// ======================================================================
// 文件名: sketch_jun24a.ino
// 功能: ESP32-S3 + RC522 射频卡读卡器 + K230 智能餐盘识别 + CH32V307 通信
// 说明:
//   1. 通过 WiFi 连接 OneNET 云平台 (MQTT 协议)
//   2. 读取 RC522 射频卡 UID 并上报到云平台
//   3. 接收 K230 串口发送的菜品识别结果并上报
//   4. 接收云端下发的充值/扣费指令并执行
//   5. 与 CH32V307 通过自定义帧协议通信
// ======================================================================

// ==================== 基础库引入 ====================
#include <WiFi.h>                 // WiFi 网络连接
#define MQTT_MAX_PACKET_SIZE 512  // MQTT 最大数据包大小
#include <PubSubClient.h>         // MQTT 客户端库
#include <ArduinoJson.h>          // JSON 数据解析与构建
#include <WiFiClientSecure.h>     // SSL/TLS 加密连接
#include <time.h>                 // 时间同步
#include <mbedtls/md.h>           // MD5/SHA 加密算法
#include <mbedtls/md5.h>
#include <mbedtls/sha256.h>
#include <SPI.h>      // SPI 总线通信
#include <MFRC522.h>  // RC522 射频卡读卡器库
#include <HTTPClient.h>

// ======================================================================
// 1. 硬件引脚配置
// ======================================================================

// RC522 射频卡读卡器引脚 (SPI 接口)
// SDA  -> GPIO10  (片选)
// SCK  -> GPIO11  (时钟)
// MOSI -> GPIO12  (主机输出从机输入)
// MISO -> GPIO13  (主机输入从机输出)
// RST  -> GPIO21  (复位)

// =========== RC522 射频卡读卡器 (SPI 接口) ===========
#define SS_PIN 10
#define RST_PIN 21
MFRC522 rfid(SS_PIN, RST_PIN);

// ---------- K230 串口通信 (UART2) ----------
// 硬件串口：K230 UART1 -- ESP32 UART2 (GPIO17=RX, GPIO16=TX)
HardwareSerial k230Serial(2);  // 使用 UART2
#define K230_RX 17             // 接收引脚（K230 TX → ESP32 18）
#define K230_TX 16             // 发送引脚（K230 RX ← ESP32 17）

// --------- CH32V307 串口通信 (UART1) --------
// CH32V307 串口通信：ESP32 UART1 (GPIO19=RX, GPIO20=TX)
HardwareSerial ch32Serial(1);
#define CH32_RX 4
#define CH32_TX 5
// #define CH32_RX 15
// #define CH32_TX 14

// ======================================================================
// 2. CH32V307 帧协议常量（需与 CH32 端 cloud_protocol.h 保持一致）
// ======================================================================
// ----- 帧定界符 -----
#define CLOUD_SYNC 0xAA  // 帧头
#define CLOUD_END 0x55   // 帧尾
#define CLOUD_ESC 0xBB   // 转义字符

// ----- 转义后映射 -----
#define CLOUD_ESC_AA 0x01  // 0xAA 转义后为 0xBB 0x01
#define CLOUD_ESC_55 0x02  // 0x55 转义后为 0xBB 0x02
#define CLOUD_ESC_BB 0x00  // 0xBB 转义后为 0xBB 0x00

// ----- 帧类型 -----
#define FRAME_CARD_TAP 0x10       // 刷卡事件帧
#define FRAME_TRANSACTION 0x11    // 交易记录帧
#define FRAME_HEARTBEAT 0x12      // 心跳帧
#define FRAME_PRICE_TABLE 0x13    // 价格表帧
#define FRAME_RECHARGE_CMD 0x20   // 充值指令帧（ESP32 → CH32）
#define FRAME_HEARTBEAT_ACK 0x32  // 心跳应答帧（ESP32 → CH32）

// ----- CH32 帧接收缓冲区 -----
uint8_t ch32RxBuf[512];       // 接收缓冲区
uint16_t ch32RxIdx = 0;       // 当前接收位置
bool ch32FrameReady = false;  // 是否接收到完整帧
// float total_price = 0;  // CH32V307 传来的总价
// bool hasNewPrice = false;

// ==================== 配置区域 ====================

// ======================================================================
// 3. WiFi 与 OneNET 云平台配置
// ======================================================================

// --------- WiFi 配置 ---------
// const char* ssid = "OPPO";
// const char* password = "r6pewfjm";
const char* ssid = "wanteen";         // 新热点名
const char* password = "hulihe8893";  // 密码

// ---------- OneNET 平台配置 ---------
const char* productId = "dL26Jqg105";                                       // 产品 ID
const char* deviceName = "dish";                                            // 设备名称
const char* device_name = "rfid_reader";                                    // 在 OneNET 新建一个设备 刷卡
const char* deviceSecret = "Z2h6ckg5ODVCSWlwbjBHZW01MDhzRGJHUEl3OU5SZlI=";  // Base64 编码的设备密钥
const char* rawKey = "ghzrH985BIipn0Gem508sDbGPIw9NRfR";                    // 解码后的原始密钥（用于 HMAC-MD5 签名）

// ---------- MQTT 服务器配置 ----------
// const char* mqttServer = "mqtts.heclouds.com";  // OneNET MQTT 服务器地址
const char* mqttServer = "218.201.45.2";
const int mqttPort = 8883;  // 改回 SSL 非加密端口
// const char* mqttServer = "183.230.40.96";
// const int mqttPort = 1883;


// ----------- K230 接收缓冲区 ----------
String k230Buffer = "";

// ======================================================================
// 4. 数据结构定义
// ======================================================================

// 菜品信息结构体
struct DishData {
  String name;  // 菜品名称
  int calorie;  // 热量（千卡）
  int protein;  // 蛋白质（克）
  int weight;   // 重量（克）
};

// 菜品营养数据库结构体 (每100克含量)
struct DishDatabase {
  String name;
  int caloriePer100g;
  int proteinPer100g;
};

// RC522 相关 射频卡事件结构体
struct CardEvent {
  String uid;
  String action;  // "read", "recharge", "deduct"
  float amount;
};

// ======================================================================
// 5. 全局变量
// ======================================================================

WiFiClientSecure espClient;  // TCP 客户端（加密）
//WiFiClient espClient;                // TCP 客户端（非加密）
PubSubClient mqttClient(espClient);  // MQTT 客户端

// 标志位（用于事件触发）
volatile bool wifiConnected = false;   // WiFi 连接状态
volatile bool timeSynced = false;      // 时间同步状态
volatile bool mqttConnected = false;   // MQTT 连接状态
volatile bool hasNewDishData = false;  // 是否有新数据待上报
volatile bool hasNewCard = false;

// RC522 相关变量
String lastCardUid = "";             // 上次读取的卡号 (防重复)
unsigned long lastCardReadTime = 0;  // 上次读取时间 (防重复)
CardEvent pendingCard;               // 待上报的卡片事件

// 缓存最近一次成功解析的菜品
DishData cachedDish;
bool hasCachedDish = false;

// 菜品数据
DishData pendingDish;      // 待上报的菜品数据
String serialBuffer = "";  // 串口数据缓冲区

// ===== 添加心跳变量 =====
unsigned long lastHeartbeat = 0;  // 上次心跳时间
unsigned long lastReconnect = 0;  // 上次重连时间（如果有的话）

// ======================================================================
// 6. 菜品营养数据库
// ======================================================================
DishDatabase dishDatabase[] = {
  { "西红柿炒鸡蛋", 95, 6 },
  { "酸辣土豆丝", 88, 3 },
  { "馒头", 223, 7 },
  { "烧麦", 220, 8 },
  { "纯牛奶", 66, 3 },
  { "鸡蛋", 144, 13 },
  { "红烧肉", 350, 15 },
  { "清炒西兰花", 55, 4 },
  { "糖醋排骨", 280, 18 },
  { "麻婆豆腐", 120, 8 },
  { "白米饭", 116, 2 },
  { "", 0, 0 }  // 结束标记
};


// ==================== 函数声明 ====================
void onWiFiEvent(WiFiEvent_t event);                                  // WiFi 事件回调
void onMQTTMessage(char* topic, byte* payload, unsigned int length);  // MQTT 消息回调
void syncTime();                                                      // 同步 NTP 时间
void checkSerialData();
void processDishData(String jsonStr);
void reportDish(DishData dish);                                      // 上报菜品数据到 OneNET
void checkRfidCard();                                                // 检测 RFID 卡片
void reportCardUid(String uid);                                      // 上报卡号到 OneNET
void handleCardCommand(String message);                              // 处理云端充值/扣费指令
String generateToken(int expireSeconds);                             // 生成 OneNET 鉴权 Token
String hmacSha256(String key, String payload);                       // HMAC-SHA256 签名
String base64Encode(const uint8_t* data, size_t length);             // Base64 编码
String base64Decode(String input);                                   // Base64 解码
DishData getDishNutrition(String dishName);                          // 根据菜名查询营养数据
String translateDishName(String engName);                            // 英文菜名 → 中文
void connectOneNET();                                                // 连接 OneNET
void checkK230Data();                                                // 读取 K230 串口数据
void processK230Data(String jsonStr);                                // 处理 K230 识别结果
void checkCH32Data();                                                // 读取 CH32 串口数据
uint8_t crc8_ch32(uint8_t* data, uint16_t len);                      // CRC8 校验
void processCH32Frame();                                             // 处理 CH32 帧
void sendFrameToCH32(uint8_t type, uint8_t* payload, uint16_t len);  // 发送帧到 CH32
void handleCH32Frame(uint8_t type, uint8_t* payload, uint16_t len);  // 处理 CH32 帧

// ==================== 翻译函数 ====================
/**
 * @brief 英文菜名 → 中文菜名
 * @param engName 英文名称 (K230 识别结果)
 * @return String 中文名称
 */
String translateDishName(String engName) {
  if (engName == "tomato_scrambled_egg") return "西红柿炒鸡蛋";
  if (engName == "spicy_sour_potato_shreds") return "酸辣土豆丝";
  if (engName == "steamed_bun") return "馒头";
  if (engName == "shaomai") return "烧麦";
  if (engName == "pure_milk") return "纯牛奶";
  if (engName == "egg") return "鸡蛋";
  return engName;
}

// ==================== HMAC-SHA256 签名计算 ====================
// 功能：使用 HMAC-MD5 算法对 payload 进行签名，返回 Base64 编码结果
String hmacSha256(String key, String payload) {
  byte hmacResult[32];
  mbedtls_md_context_t ctx;
  mbedtls_md_type_t md_type = MBEDTLS_MD_SHA256;

  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(md_type), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)key.c_str(), key.length());
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)payload.c_str(), payload.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  return base64Encode(hmacResult, 32);
}

// ==================== Base64 编码 ====================
/**
 * @brief Base64 编码
 * @param data  二进制数据指针
 * @param length 数据长度
 * @return String Base64 编码后的字符串
 */
String base64Encode(const uint8_t* data, size_t length) {
  const char base64Chars[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  String result;
  int i = 0, j = 0;
  uint8_t charArray3[3];
  uint8_t charArray4[4];

  while (length--) {
    charArray3[i++] = *(data++);
    if (i == 3) {
      // 将 3 个字节 (24位) 转换为 4 个 Base64 字符 (每个6位)
      charArray4[0] = (charArray3[0] & 0xfc) >> 2;
      charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
      charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
      charArray4[3] = charArray3[2] & 0x3f;

      for (i = 0; i < 4; i++)
        result += base64Chars[charArray4[i]];
      i = 0;
    }
  }

  // 处理剩余字节 (补 '=' 填充)
  if (i) {
    for (j = i; j < 3; j++)
      charArray3[j] = '\0';

    charArray4[0] = (charArray3[0] & 0xfc) >> 2;
    charArray4[1] = ((charArray3[0] & 0x03) << 4) + ((charArray3[1] & 0xf0) >> 4);
    charArray4[2] = ((charArray3[1] & 0x0f) << 2) + ((charArray3[2] & 0xc0) >> 6);
    charArray4[3] = charArray3[2] & 0x3f;

    for (j = 0; j < i + 1; j++)
      result += base64Chars[charArray4[j]];

    while ((i++ < 3))
      result += '=';
  }

  return result;
}

// ==================== 解码 Base64 编码 ====================
/**
 * @brief Base64 解码
 * @param input Base64 编码的字符串
 * @return String 解码后的原始字符串
 */
String base64Decode(String input) {
  const char* base64Chars = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  int inLen = input.length();
  int i = 0, j = 0, in_ = 0;
  uint8_t charArray4[4], charArray3[3];
  String result;

  while (inLen-- && input[in_] != '=') {
    charArray4[i++] = input[in_];
    in_++;
    if (i == 4) {
      for (i = 0; i < 4; i++)
        charArray4[i] = strchr(base64Chars, charArray4[i]) - base64Chars;

      // 将 4 个 Base64 字符 (24位) 还原为 3 个字节
      charArray3[0] = (charArray4[0] << 2) + ((charArray4[1] & 0x30) >> 4);
      charArray3[1] = ((charArray4[1] & 0x0F) << 4) + ((charArray4[2] & 0x3C) >> 2);
      charArray3[2] = ((charArray4[2] & 0x03) << 6) + charArray4[3];

      for (i = 0; i < 3; i++)
        result += (char)charArray3[i];
      i = 0;
    }
  }
  return result;
}


// =============== CRC8 函数 ==============
/**
 * @brief CRC8 校验计算
 * @param data 数据指针
 * @param len 数据长度
 * @return uint8_t CRC8 校验值
 * @note 用于 CH32 帧协议的数据完整性校验
 */
uint8_t crc8_ch32(uint8_t* data, uint16_t len) {
  uint8_t crc = 0;
  while (len--) {
    uint8_t inbyte = *data++;
    for (uint8_t i = 8; i; i--) {
      uint8_t mix = (crc ^ inbyte) & 0x01;
      crc >>= 1;
      if (mix) crc ^= 0x8C;
      inbyte >>= 1;
    }
  }
  return crc;
}

// ==================== 生成 OneNET Token ====================
/**
 * @brief 生成 OneNET MQTT 连接鉴权 Token
 * @param expireSeconds Token 有效期 (秒)
 * @return String Token 字符串
 * @note 使用 HMAC-SHA1 算法 + URL 编码
 */
String generateToken(int expireSeconds = 3600) {
  time_t now = time(nullptr);
  long expireTime = now + expireSeconds;

  String res = "products/" + String(productId) + "/devices/" + String(deviceName);

  // 签名参数
  String version = "2018-10-31";
  String method = "sha1";
  String stringToSign = String(expireTime) + "\n" + method + "\n" + res + "\n" + version;

  // 使用 HMAC-SHA1 计算签名
  byte hmacResult[20];  // SHA1 = 20字节
  mbedtls_md_context_t ctx;
  mbedtls_md_init(&ctx);
  mbedtls_md_setup(&ctx, mbedtls_md_info_from_type(MBEDTLS_MD_SHA1), 1);
  mbedtls_md_hmac_starts(&ctx, (const unsigned char*)rawKey, strlen(rawKey));
  mbedtls_md_hmac_update(&ctx, (const unsigned char*)stringToSign.c_str(), stringToSign.length());
  mbedtls_md_hmac_finish(&ctx, hmacResult);
  mbedtls_md_free(&ctx);

  // Base64 编码签名
  String signature = base64Encode(hmacResult, 20);

  // URL 编码 (转义特殊字符)
  res.replace("/", "%2F");
  signature.replace("+", "%2B");
  signature.replace("/", "%2F");
  signature.replace("=", "%3D");

  return "version=" + version + "&res=" + res + "&et=" + String(expireTime)
         + "&method=" + method + "&sign=" + signature;
}


// ==================== 事件监听：WiFi 事件 ====================
/**
 * @brief WiFi 事件回调函数
 * @param event WiFi 事件类型
 */
void onWiFiEvent(WiFiEvent_t event) {
  switch (event) {
    case ARDUINO_EVENT_WIFI_STA_CONNECTED:
      Serial0.println("[事件] WiFi 已连接");
      break;

    case ARDUINO_EVENT_WIFI_STA_GOT_IP:
      Serial0.print("[事件] 获取到IP: ");
      Serial0.println(WiFi.localIP());
      wifiConnected = true;
      break;

    case ARDUINO_EVENT_WIFI_STA_DISCONNECTED:
      Serial0.println("[事件] WiFi 断开连接");
      wifiConnected = false;
      mqttConnected = false;
      // 尝试重连
      WiFi.reconnect();
      break;

    default:
      break;
  }
}

// ==================== 事件监听：MQTT 消息回调 ====================
/**
 * @brief MQTT 消息回调函数
 * @param topic   主题
 * @param payload 消息内容
 * @param length  消息长度
 * @note 处理平台下发的指令，解析 JSON 数据
 */
void onMQTTMessage(char* topic, byte* payload, unsigned int length) {
  Serial0.print("[事件] 收到 MQTT 消息，主题： ");
  Serial0.println(topic);

  String message;
  for (unsigned int i = 0; i < length; i++) {
    message += (char)payload[i];
  }

  String topicStr = String(topic);

  // ===== 上报回执（云平台有没有接收数据） =====
  if (topicStr.indexOf("/thing/property/post/reply") >= 0) {
    Serial0.print("[回执-RAW] ");
    Serial0.println(message);

    // StaticJsonDocument<256> doc;
    StaticJsonDocument<512> doc;
    DeserializationError err = deserializeJson(doc, message);
    if (!err) {
      int code = doc["code"] | -1;
      String msg = doc["msg"] | "";
      Serial0.printf("[回执] code=%d, msg=%s\n", code, msg.c_str());
      if (code == 200 || code == 0) {
        Serial0.printf("[回执] 成功 code=%d msg=%s\n", code, msg.c_str());
        Serial0.println(message);
      }
    } else {
      // ★ 解析失败也要打出来
      Serial0.print("[回执] JSON解析失败: ");
      Serial0.println(err.c_str());
    }
    return;
  }

  Serial0.print("[事件] 收到 MQTT 消息，主题： ");
  Serial0.println(topic);

  Serial0.print("消息内容: ");
  Serial0.println(message);

  // ===== 云端下发属性设置（充值/扣费指令） =====
  if (String(topic).indexOf("/thing/property/set") >= 0) {
    handleCardCommand(message);
    return;
  }

  // ===== 其他消息处理 =====
  StaticJsonDocument<256> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, message);
  if (!error) {
    if (jsonDoc["method"] == "get") {
      Serial0.println("[事件] 收到云端查询请求");
    }
  }
}

// ==================== 读取 K230 串口数据 ====================
/**
 * @brief 读取 K230 串口数据
 * @note 从 UART2 读取 JSON 格式的菜品识别结果
 */
void checkK230Data() {
  while (k230Serial.available()) {

    String line = k230Serial.readStringUntil('\n');
    line.trim();
    if (line.length() < 20) continue;

    // 提取 JSON 部分（去除前后非 JSON 字符）
    int brace = line.indexOf('{');
    int endBrace = line.lastIndexOf('}');
    if (brace >= 0 && endBrace > brace) {
      line = line.substring(brace, endBrace + 1);
      // 过滤非打印字符
      String cleanLine = "";
      for (int i = 0; i < line.length(); i++) {
        char c = line[i];
        if (c >= 32 && c <= 126) cleanLine += c;
      }
      if (cleanLine.length() > 0) {
        processK230Data(cleanLine);
      }
    }
  }
}

// ==================== 轮询串口数据 ====================
void checkSerialData() {
  while (Serial0.available()) {
    char c = Serial0.read();
    if (c == '\n') {
      if (serialBuffer.length() > 0) {
        Serial0.print("[事件] 收到串口数据: ");
        Serial0.println(serialBuffer);
        processDishData(serialBuffer);
        serialBuffer = "";
      }
    } else {
      serialBuffer += c;
    }
  }
}

// ==================== 处理菜品数据 ====================
void processDishData(String jsonStr) {
  StaticJsonDocument<1024> jsonDoc;
  DeserializationError error = deserializeJson(jsonDoc, jsonStr);

  if (error) {
    Serial0.print("[错误] JSON 解析失败: ");
    Serial0.println(error.c_str());
    return;
  }

  // 适配 K230 的格式
  int cnt = jsonDoc["cnt"] | 0;
  JsonArray items = jsonDoc["items"].as<JsonArray>();

  if (cnt > 0 && items) {
    for (JsonVariant item : items) {
      String label = item["label"].as<String>();
      String position = item["position"].as<String>();
      float confidence = item["confidence"] | 0;

      // 翻译英文菜名
      String dishName = translateDishName(label);

      // 获取营养数据
      pendingDish = getDishNutrition(dishName);
      pendingDish.weight = 200;
      hasNewDishData = true;

      // 可以加个延时避免同时上报多个
      delay(500);
    }
  }
}

// ==================== 根据菜品名称获取营养数据 ====================
/**
 * @brief 根据菜品名称查询营养数据
 * @param dishName 中文菜品名称
 * @return DishData 包含热量、蛋白质等数据的结构体
 */
DishData getDishNutrition(String dishName) {
  DishData dish;
  dish.name = dishName;
  dish.weight = 200;
  dish.calorie = 100;
  dish.protein = 5;

  int i = 0;
  while (dishDatabase[i].name != "") {
    if (dishDatabase[i].name == dishName) {
      dish.calorie = dishDatabase[i].caloriePer100g * 2;
      dish.protein = dishDatabase[i].proteinPer100g * 2;
      break;
    }
    i++;
  }

  if (dishDatabase[i].name == "") {
    Serial0.print("[警告] 未找到菜品? ");
    Serial0.println(dishName);
  }

  return dish;
}

// ==================== 连接 OneNET MQTT ====================
/**
 * @brief 连接 OneNET MQTT 服务器
 * @note 自动重试 5 次，连接成功后订阅主题
 */
void connectOneNET() {
  String clientId = deviceName;
  String username = productId;

  mqttClient.setServer(mqttServer, mqttPort);
  // mqttClient.setKeepAlive(30);
  // mqttClient.setSocketTimeout(15);
  mqttClient.setKeepAlive(60);  // 120 → 30，30秒发一次 PING
  mqttClient.setSocketTimeout(10);
  mqttClient.setCallback(onMQTTMessage);

  for (int retry = 0; retry < 5; retry++) {
    espClient.stop();
    delay(500);

    String pwd = generateToken();
    Serial0.print("[事件] 正在连接 OneNET (");
    Serial0.print(retry + 1);
    Serial0.print("/5)...");
    Serial0.print("\n[调试] ClientId: ");
    Serial0.println(clientId);
    Serial0.print("[调试] Username: ");
    Serial0.println(username);
    Serial0.print("[调试] Token: ");
    Serial0.println(pwd);

    Serial0.print("pwd hex: ");
    for (int i = 0; i < pwd.length(); i++) {
      Serial0.print((uint8_t)pwd[i], HEX);
      Serial0.print(" ");
    }
    Serial0.println();

    espClient.setInsecure();
    if (mqttClient.connect(clientId.c_str(), username.c_str(), pwd.c_str())) {
      Serial0.println("  连接成功！");
      mqttConnected = true;

      // 订阅平台下发的指令主题
      String subscribeTopic = String("$sys/") + productId + "/" + deviceName + "/thing/property/set";
      mqttClient.subscribe(subscribeTopic.c_str());
      String replyTopic = String("$sys/") + productId + "/" + deviceName + "/thing/property/post/reply";
      mqttClient.subscribe(replyTopic.c_str());
      Serial0.print("[Event] subscribed: ");
      Serial0.println(subscribeTopic);
      Serial0.print("[Event] subscribed reply: ");
      Serial0.println(replyTopic);

      // Immediately send cached dish if available
      if (hasCachedDish) {
        mqttClient.loop();
        reportDish(cachedDish);
        hasCachedDish = false;
      }
      return;
    } else {
      Serial0.print(" 失败，状态码: ");
      Serial0.println(mqttClient.state());
      delay(2000);
    }
  }
  mqttConnected = false;
}

// ==================== 处理 K230 识别结果 ====================
/**
 * @brief 处理 K230 识别结果
 * @param jsonStr K230 发送的 JSON 字符串
 * @note 解析识别结果，提取菜品信息并上报到 OneNET
 */
void processK230Data(String jsonStr) {
  StaticJsonDocument<1024> doc;
  DeserializationError err = deserializeJson(doc, jsonStr);
  if (err) {
    Serial0.print("[K230] JSON解析失败: ");
    Serial0.println(err.c_str());
    return;
  }

  int cnt = doc["cnt"];  // 识别到的菜品数量
  JsonArray items = doc["items"].as<JsonArray>();

  // 如果有多条识别结果，循环上报
  for (int i = 0; i < items.size(); i++) {
    String enLabel = items[i]["label"] | "unknown";  // 英文标签
    float conf = items[i]["confidence"] | 0.0;       // 置信度
    String pos = items[i]["position"] | "unknown";   // 位置（top_left等）
    String cnName = translateDishName(enLabel);      // 翻译为中文

    // 从数据库查营养信息
    DishData dish = getDishNutrition(cnName);

    Serial0.println("======================");
    Serial0.print("[K230] 食物: ");
    Serial0.println(cnName);
    Serial0.print("[K230] 置信度: ");
    Serial0.println(conf);
    Serial0.print("[K230] 位置: ");
    Serial0.println(pos);

    // 上报到 OneNET
    if (mqttConnected) {
      String topic = String("$sys/") + productId + "/" + deviceName + "/thing/property/post";
      StaticJsonDocument<512> report;
      report["id"] = String(millis());
      report["version"] = "1.0";

      //使用物模型定义的字段名
      JsonObject params = report.createNestedObject("params");
      params["dish_name"]["value"] = cnName;      // string 类型，直接赋值
      params["calorie"]["value"] = dish.calorie;  // int32 类型
      params["protein"]["value"] = dish.protein;  // int32 类型
      params["weight"]["value"] = dish.weight;    // 重量
      params["position"]["value"] = pos;          // 位置
      params["confidence"]["value"] = conf;       // 置信度
      // 注意：物模型中 dish_name 长度限制为10个字符，请确保中文名称长度不超过10
      // 如果菜品名称超过10个字符，需要截断或使用缩写

      char buf[512];
      serializeJson(report, buf);

      if (mqttClient.publish(topic.c_str(), buf)) {
        Serial0.println("[OneNET] 上报成功");
        Serial0.print("[OneNET] 数据: ");
        Serial0.println(buf);
      } else {
        Serial0.println("[OneNET] 上报失败");
      }
    } else {
      Serial0.println("[OneNET] 未连接，跳过上报");
    }
  }
}

// ==================== 上报菜品数据 ====================
/**
 * @brief 上报菜品数据到 OneNET
 * @param dish 菜品数据结构
 * @note 使用 MQTT 发布到属性上报主题
 */
void reportDish(DishData dish) {
  mqttClient.loop();
  if (!mqttClient.connected()) {
    mqttConnected = false;
    return;
  }

  String topic = String("$sys/") + productId + "/" + deviceName + "/thing/property/post";
  StaticJsonDocument<256> doc;
  doc["id"] = String(millis());
  doc["version"] = "1.0";
  doc["params"]["temp"]["value"] = dish.name;
  doc["params"]["temp1"]["value"] = dish.calorie;
  doc["params"]["temp2"]["value"] = dish.protein;
  doc["params"]["temp3"]["value"] = dish.weight;
  doc["params"]["temp4"]["value"] = dish.name;
  char buf[512];
  size_t len = serializeJson(doc, buf);

  if (mqttClient.publish(topic.c_str(), buf, len)) {
    Serial0.print("[OneNET] OK: ");
    Serial0.write(buf, len);
    Serial0.println();
    mqttClient.loop();  // ← 加这行
    yield();
  } else {
    Serial0.println("[OneNET] FAIL");
    mqttConnected = false;
  }
}


// ==================== 同步时间 ====================
/**
 * @brief 同步 NTP 网络时间
 * @note 用于 Token 签名的时间戳验证
 */
void syncTime() {
  Serial0.print("[事件] 正在同步时间...");
  configTime(8 * 3600, 0, "pool.ntp.org", "time.windows.com", "ntp.aliyun.com");

  time_t now = time(nullptr);
  int retry = 0;
  while (now < 8 * 3600 * 2 && retry < 20) {
    delay(500);
    Serial0.print(".");
    now = time(nullptr);
    retry++;
  }

  if (now >= 8 * 3600 * 2) {
    Serial0.println(" 完成！");
    struct tm* timeinfo = localtime(&now);
    char timeStr[30];
    strftime(timeStr, sizeof(timeStr), "%Y-%m-%d %H:%M:%S", timeinfo);
    Serial0.print("[事件] 当前时间: ");
    Serial0.println(timeStr);
    timeSynced = true;
  } else {
    Serial0.println(" 失败！");
    timeSynced = false;
  }
}

// ==================== 初始化====================
/**
 * @brief 系统初始化函数
 * @note 执行顺序：串口 → 外设串口 → RC522 → WiFi → 时间同步 → MQTT
 */
void setup() {
  // --- 串口初始化 ---
  Serial0.begin(115200);
  // 强行等待 3 秒，给串口芯片和电脑足够的建立连接时间
  for (int i = 0; i < 3; i++) {
    delay(1000);
    Serial0.println("--- 芯片正在启动 ---");
  }

  Serial0.println("\n╔════════════════════════════════════════╗");
  Serial0.println("║   ESP32-S3 OneNET 事件驱动版 v2.0      ║");
  Serial0.println("╚════════════════════════════════════════╝\n");

  // --- 初始化 CH32V307 串口 ---
  ch32Serial.begin(115200, SERIAL_8N1, CH32_RX, CH32_TX);
  Serial0.println("[ESP32] ch32Serial ready, waiting for 0xAA...");
  Serial0.println("[CH32] 串口已初始化 (UART1: RX=4, TX=5)");
  // Serial0.println("[CH32] 串口已初始化 (UART1: RX=15, TX=14)");

  //  --- 初始化 K230 串口  ---
  k230Serial.begin(115200, SERIAL_8N1, K230_RX, K230_TX);

  //  --- 初始化 RC522  ---
  SPI.begin(11, 13, 12, 10);  // SCK=11, MISO=13, MOSI=12,SDA=10
  SPI.setFrequency(250000);   // ← 250kHz
  rfid.PCD_Init(10, 21);      // SDA=10, RST=22
  SPI.setFrequency(50000);
  delay(200);

  // 强制开启天线（0x83 = TX1 + TX2 输出使能）
  //  rfid.PCD_SetRegisterBitMask(MFRC522::TxControlReg, 0x03);
  rfid.PCD_WriteRegister(MFRC522::TxControlReg, 0x83);
  rfid.PCD_AntennaOn();  // 备用：显式开天线
  delay(50);

  // 验证
  byte ver = rfid.PCD_ReadRegister(MFRC522::VersionReg);
  byte tx = rfid.PCD_ReadRegister(MFRC522::TxControlReg);
  Serial0.printf("[RC522] Version=0x%02X  TxControl=0x%02X\n", ver, tx);

  if (tx == 0x83) {
    Serial0.println("[RC522]  天线已开启");
  } else {
    Serial0.println("[RC522]  天线未正常开启");
  }

  Serial0.println("[RC522] RFID 读卡器已初始化");

  // 注册 WiFi 事件监听
  WiFi.onEvent(onWiFiEvent);

  // --- 连接 WiFi ---
  Serial0.print("[事件] 正在连接 WiFi: ");
  Serial0.println(ssid);
  WiFi.begin(ssid, password);

  WiFi.setAutoReconnect(true);  // WiFi自动恢复
  WiFi.setSleep(false);

  // 等待 WiFi 连接（事件会处理后续）
  int timeout = 30;
  while (!wifiConnected && timeout > 0) {
    delay(500);
    timeout--;
    Serial0.print(".");
  }

  // DNS 测试
  Serial0.print("[测试] DNS 解析 mqtts.heclouds.com...");
  IPAddress ip;
  if (WiFi.hostByName("mqtts.heclouds.com", ip)) {
    Serial0.print(" 成功: ");
    Serial0.println(ip);
  } else {
    Serial0.println(" 失败！DNS 不通");
  }

  // --- 时间同步 & MQTT 连接 ---
  if (wifiConnected) {
    syncTime();
    if (timeSynced) {
      connectOneNET();
    }
  }

  Serial0.println("\n[事件] 系统就绪，等待菜品识别..");
  Serial0.println("[提示] 请通过串口发送JSON: {\"dish\":\"红烧肉\",\"weight\":150}");
}


// ==================== 主循环  ====================
/**
 * @brief 主循环函数
 * @note 执行频率：读卡/读串口实时执行，其他任务每 500ms 执行一次
 */
void loop() {
  // ===== 1. MQTT 保活（必须每次调用） =====
  mqttClient.loop();
  yield();

  // ===== 2. 实时任务（每次循环都执行，不加 delay） =====
  //  checkRfidCard();  // RC522 刷卡检测

  // 有新卡立即上报
  if (hasNewCard && mqttConnected) {
    reportCardUid(pendingCard.uid);
    hasNewCard = false;
    yield();  // 让出 CPU 给 WiFi 和网络栈
  }

  // CH32 数据处理
  checkCH32Data();     // CH32 串口数据接收
  processCH32Frame();  // 处理 CH32 帧

  // ===== 轮询 Django 拿充值指令，发给 CH32 =====
  static unsigned long lastPoll = 0;
  if (wifiConnected && millis() - lastPoll > 3000) {
    lastPoll = millis();
    HTTPClient http;
    // http.begin("http://172.20.10.10:8000/api/card/pending/");
    http.begin("https://wisplike-scoop-elk.ngrok-free.dev/api/card/pending/");
    int code = http.GET();
    Serial0.printf("[POLL] code=%d, error=%s\n", code, http.errorToString(code).c_str());
    if (code == 200) {
      String body = http.getString();
      StaticJsonDocument<512> doc;
      if (!deserializeJson(doc, body)) {
        JsonArray cmds = doc["data"].as<JsonArray>();
         Serial0.printf("[POLL] cmds count=%d\n", cmds.size());
        for (JsonVariant c : cmds) {
          int cmdId = c["id"];
          String cardUidStr = c["card_uid"];
          String cmdType = c["cmd_type"];
          float amtYuan = c["amount"].as<float>();

          uint8_t pkt[44] = { 0 };
          if (cmdType == "recharge") {
            // 拼装 44 字节充值帧：UID(4) + UserID(4) + Amount(4) + OrderNo(32) uint8_t pkt[44] = { 0 };
            // UID: hex 字符串转 4 字节
            for (int i = 0; i < 4; i++) {
              pkt[i] = (uint8_t)strtol(cardUidStr.substring(i * 2, i * 2 + 2).c_str(), NULL, 16);
            }
            // Amount: 大端序，单位分，放在 pkt[8..11]（跳过UserID 4 字节）
            uint32_t amtFen = (uint32_t)(amtYuan * 100);
            pkt[8] = (amtFen >> 24) & 0xFF;
            pkt[9] = (amtFen >> 16) & 0xFF;
            pkt[10] = (amtFen >> 8) & 0xFF;
            pkt[11] = amtFen & 0xFF;

            sendFrameToCH32(FRAME_RECHARGE_CMD, pkt, 44);
            Serial0.printf("[POLL] 充值帧已发送: uid=%s, amount=%.2f\n", cardUidStr.c_str(), amtYuan);

            // 标记完成
            HTTPClient http2;
            // http2.begin("http://172.20.10.10:8000/api/card/complete/");
            http2.begin("https://wisplike-scoop-elk.ngrok-free.dev/api/card/complete/");
            http2.addHeader("Content-Type", "application/json");
            http2.POST("{\"id\":" + String(cmdId) + "}");
            http2.end();
          }
        }
      }
    }
    http.end();
  }

  // ===== 3. 定时任务（每 500ms 执行一次） =====
  // static unsigned long lastLoop = 0;
  // if (millis() - lastLoop > 5000) {
  //   lastLoop = millis();
  //   Serial0.println("[心跳] loop运行中...");
  // }

  // Serial0.println("[A]");
  // // ===== 每次循环都调用 MQTT.loop()，确保保活心跳不丢 =====
  // mqttClient.loop();
  // Serial0.println("[B]");
  // yield();  // 让出 CPU 给 WiFi 和网络栈

  // // =====  RC522 刷卡 =====
  // checkRfidCard();  // RC522 刷卡检测
  // checkCH32Data();  // CH32 串口数据接收

  // // CH32 数据上报
  // processCH32Frame();  // 处理 CH32 帧

  // // ===== 如果有新卡且 MQTT 已连接，上报卡号 =====
  // if (hasNewCard && mqttConnected) {
  //   reportCardUid(pendingCard.uid);
  //   hasNewCard = false;
  //   delay(100);
  // }

  // ===== 以下逻辑每500ms才跑一次，避免阻塞 MQTT =====
  static unsigned long lastTick = 0;
  if (millis() - lastTick < 500) return;
  lastTick = millis();

  // --- WiFi 状态检查与重连 ---
  if (WiFi.status() != WL_CONNECTED) {
    static unsigned long lastReconnect = 0;
    if (millis() - lastReconnect > 10000) {
      lastReconnect = millis();
      Serial0.println("[WiFi] 断开，重连...");
      WiFi.reconnect();
      mqttConnected = false;
    }
  } else {
    wifiConnected = true;
  }

  // --- MQTT 重连 ---
  if (wifiConnected && !mqttClient.connected()) {
    if (mqttConnected) {
      Serial0.printf("[MQTT] 断连 state=%d, 重连...\n", mqttClient.state());
    }
    mqttConnected = false;
    connectOneNET();
    return;
  }

  // --- 菜品上报 ---
  if (hasCachedDish && mqttConnected) {
    reportDish(cachedDish);
    hasCachedDish = false;
  }

  // --- K230 数据 ---
  checkK230Data();
}


// ==================== RC522 读卡 ====================
/**
 * @brief 检测 RFID 卡片
 * @note 读取卡片 UID，防重复读取 (3秒内同一张卡不重复上报)
 */
void checkRfidCard() {
  static unsigned long lastDbg = 0;

  if (millis() - lastDbg > 3000) {
    lastDbg = millis();

    // 读 RC522 版本寄存器，确认 SPI 是否通畅
    byte version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    if (version == 0x00 || version == 0xFF) {
      // SPI断连，重新初始化
      Serial0.println("[RC522] SPI断连，重新初始化...");
      rfid.PCD_Init(10, 21);
      rfid.PCD_WriteRegister(MFRC522::TxControlReg, 0x83);
      rfid.PCD_AntennaOn();
      delay(50);
      version = rfid.PCD_ReadRegister(MFRC522::VersionReg);
    }
    Serial0.printf("[RC522] VersionReg=0x%02X,监听中...\n", version);
  }

  //  if (!rfid.PICC_IsNewCardPresent() || !rfid.PICC_ReadCardSerial()) return;
  if (!rfid.PICC_IsNewCardPresent()) return;
  Serial0.println("[RC522] 检测到卡片接近！");

  if (!rfid.PICC_ReadCardSerial()) {
    Serial0.println("[RC522] 读取序列号失败");
    return;
  }
  Serial0.println("[RC522] 序列号读取成功");

  // 读取 UID (十六进制字符串)
  String uid = "";
  for (byte i = 0; i < rfid.uid.size; i++) {
    if (rfid.uid.uidByte[i] < 0x10) uid += "0";
    uid += String(rfid.uid.uidByte[i], HEX);
  }

  rfid.PICC_HaltA();  // 停止当前卡片通信

  // 同一张卡 3 秒内不重复上报
  if (uid == lastCardUid && (millis() - lastCardReadTime) < 3000) return;

  lastCardUid = uid;
  lastCardReadTime = millis();

  Serial0.print("[RC522] 读到卡: ");
  Serial0.println(uid);

  // 保存卡片事件，待主循环上报
  pendingCard.uid = uid;
  pendingCard.action = "read";
  pendingCard.amount = 0;
  hasNewCard = true;
}


// ==================== 上报卡号到 OneNET ====================
/**
 * @brief 上报卡号到 OneNET
 * @param uid 卡片 UID
 * @note 发布到属性上报主题，字段名为 card_uid
 */
void reportCardUid(String uid) {
  if (!mqttClient.connected()) {
    Serial0.println("[RC522] MQTT断开,跳过");
    return;
  }

  String topic = "$sys/" + String(productId) + "/" + String(deviceName) + "/thing/property/post";

  // ===== 上报 card_uid =====
  StaticJsonDocument<256> doc;
  doc["id"] = String(millis());
  doc["version"] = "1.0";
  doc["params"]["card_uid"]["value"] = uid;

  char buf[256];
  serializeJson(doc, buf);
  Serial0.print("[RC522] card_uid: ");
  Serial0.println(buf);

  if (mqttClient.publish(topic.c_str(), buf)) {
    Serial0.println("[RC522] 上报成功");
  } else {
    Serial0.println("[RC522] 上报失败");
  }
}

// ==================== 处理云端充值/扣费指令 ====================
/**
 * @brief 处理云端下发的充值/扣费指令
 * @param message JSON 格式的指令消息
 * @note 格式: {"params":{"card_cmd":{"value":"recharge:20.00"}}}
 */
void handleCardCommand(String message) {
  Serial0.println("══════════════════════════════");
  Serial0.println("[RC522] ===== 收到云端指令 =====");
  Serial0.print("[RC522] 原始消息: ");
  Serial0.println(message);

  StaticJsonDocument<512> doc;
  DeserializationError err = deserializeJson(doc, message);
  if (err) {
    Serial0.print("[RC522]  JSON解析失败: ");
    Serial0.println(err.c_str());
    Serial0.println("══════════════════════════════");
    return;
  }

  // 从物模型 params 中取 card_cmd，格式: "recharge:20.00" 或 "deduct:15.50"
  String cmd = doc["params"]["card_cmd"]["value"] | "";
  Serial0.print("[RC522] 解析cmd字段: ");
  Serial0.println(cmd.length() > 0 ? cmd : "(空)");
  if (cmd.length() == 0) return;

  // 解析指令格式: "recharge:20.00" 或 "deduct:15.50"
  int sep = cmd.indexOf(':');
  if (sep < 0) return;

  String action = cmd.substring(0, sep);
  float amount = cmd.substring(sep + 1).toFloat();

  // 如果是充值指令，转发给 CH32V307 组装充值帧发给CH32
  if (action == "recharge" && amount > 0) {
    uint8_t pkt[44] = { 0 };

    // UID:从cmd里取不到，暂用00000000，真正需要从业务逻辑获取 // 此处保留框架，实际使用时需关联到具体卡片
    uint32_t uid_int = 0;
    for (int i = 0; i < 4; i++) pkt[i] = (uid_int >> (24 - i * 8)) & 0xFF;

    // UserID
    uint32_t userId = doc["params"]["user_id"]["value"] | 0;
    pkt[4] = (userId >> 24) & 0xFF;
    pkt[5] = (userId >> 16) & 0xFF;
    pkt[6] = (userId >> 8) & 0xFF;
    pkt[7] = userId & 0xFF;

    // Amount (单位：分)
    uint32_t amt = (uint32_t)(amount * 100);
    pkt[8] = (amt >> 24) & 0xFF;
    pkt[9] = (amt >> 16) & 0xFF;
    pkt[10] = (amt >> 8) & 0xFF;
    pkt[11] = amt & 0xFF;

    // OrderNo: 用当前时间戳
    String order = String(millis());
    memcpy(&pkt[12], order.c_str(), order.length() > 31 ? 31 : order.length());

    sendFrameToCH32(FRAME_RECHARGE_CMD, pkt, 44);
    Serial0.printf("[CH32] 充值指令已转发: %.2f元\n", amount);
  }

  Serial0.print("[RC522] 操作类型: ");
  Serial0.println(action);
  Serial0.print("[RC522] 金额: ");
  Serial0.println(amount);

  Serial0.printf("[RC522] 收到指令: %s %.2f 元\n", action.c_str(), amount);
}


// ==================== CH32V307 串口通信 ====================
/**
 * @brief 读取 CH32V307 单片机发送的数据
 * @note 通过 UART1 接收 CH32V307 计算的总价数据
 * @warning 数据格式要求：单独的浮点数，以换行符结尾，例如 "19.50\n"
 */
void checkCH32Data() {
  while (ch32Serial.available()) {
    uint8_t b = ch32Serial.read();
    Serial0.printf("[RAW] 0x%02X\n", b);
    if (ch32RxIdx == 0 && b != CLOUD_SYNC) continue;
    // 等帧头
    if (ch32RxIdx < 512) ch32RxBuf[ch32RxIdx++] = b;
    // 检测帧尾 0x55
    if (b == CLOUD_END && ch32RxIdx >= 6) {
      ch32FrameReady = true;
      break;
    }
  }
}

// ==================== CH32V307 处理完整帧 ====================
/**
 * @brief 处理 CH32V307 完整帧
 * @note 去转义 → CRC校验 → 分发处理
 */
void processCH32Frame() {
  if (!ch32FrameReady) return;

  // ===== 1. 去转义 =====
  uint8_t decoded[256];
  uint16_t di = 0;
  bool esc = false;
  for (uint16_t i = 1; i < ch32RxIdx - 1 && di < 255;
       i++) {
    uint8_t b = ch32RxBuf[i];
    if (esc) {
      if (b == CLOUD_ESC_AA) decoded[di++] = CLOUD_SYNC;
      else if (b == CLOUD_ESC_55) decoded[di++] = CLOUD_END;
      else if (b == CLOUD_ESC_BB) decoded[di++] = CLOUD_ESC;
      esc = false;
    } else if (b == CLOUD_ESC) {
      esc = true;
    } else {
      decoded[di++] = b;
    }
  }

  // 清空接收缓冲区
  ch32RxIdx = 0;
  ch32FrameReady = false;
  if (di < 4) return;

  // ===== 2. 取字段 =====
  uint8_t type = decoded[0];                                // 帧类型
  uint16_t len = ((uint16_t)decoded[1] << 8) | decoded[2];  // 数据长度
  uint8_t* payload = &decoded[3];                           // 数据负载
  uint8_t crc_rcv = decoded[3 + len];                       // CRC 校验值
  if (3 + len + 1 > di) return;

  // ===== 3. 校验CRC =====
  uint8_t crc = crc8_ch32(&type, 1);
  crc = crc8_ch32(&decoded[1], 2);
  crc = crc8_ch32(payload, len);
  if (crc != crc_rcv) return;

  // ===== 4. 分发 =====
  handleCH32Frame(type, payload, len);
}

// ================ 帧处理函数 ======================
/**
 * @brief 发送帧到 CH32V307
 * @param type 帧类型
 * @param payload 数据负载
 * @param len 数据长度
 * @note 自动添加帧头、帧尾、CRC和转义
 */
void sendFrameToCH32(uint8_t type, uint8_t* payload, uint16_t len) {
  ch32Serial.write(CLOUD_SYNC);  // 帧头

  // 转义写入函数
  auto escWrite = [](uint8_t b) {
    if (b == CLOUD_SYNC) {
      ch32Serial.write(CLOUD_ESC);
      ch32Serial.write(CLOUD_ESC_AA);
    } else if (b == CLOUD_END) {
      ch32Serial.write(CLOUD_ESC);
      ch32Serial.write(CLOUD_ESC_55);
    } else if (b == CLOUD_ESC) {
      ch32Serial.write(CLOUD_ESC);
      ch32Serial.write(CLOUD_ESC_BB);
    } else ch32Serial.write(b);
  };

  escWrite(type);  // 写入帧类型
  escWrite((len >> 8) & 0xFF);
  escWrite(len & 0xFF);
  for (uint16_t i = 0; i < len; i++) {
    escWrite(payload[i]);
  }

  // 计算并写入 CRC
  uint8_t crc = crc8_ch32(&type, 1);
  uint8_t lb[2] = { (uint8_t)(len >> 8), (uint8_t)(len & 0xFF) };
  crc = crc8_ch32(lb, 2);
  crc = crc8_ch32(payload, len);
  escWrite(crc);
  ch32Serial.write(CLOUD_END);  // 帧尾
}

// ============= 处理 CH32V307 帧 ===========
/**
 * @brief 处理 CH32V307 帧（根据类型分发）
 * @param type 帧类型
 * @param payload 数据负载
 * @param len 数据长度
 */
void handleCH32Frame(uint8_t type, uint8_t* payload, uint16_t len) {
  Serial0.printf("[CH32帧] type=0x%02X len=%d\n", type, len);
  switch (type) {
    case FRAME_CARD_TAP:  // 刷卡事件帧
      {                   // 格式：UID(4)+UserID(4)+Name(12)+Level(1)+Balance(4)+DevID(4)=29
        if (len < 29) return;

        char uidHex[9];
        sprintf(uidHex, "%02X%02X%02X%02X", payload[0], payload[1], payload[2], payload[3]);
        uint32_t userId = ((uint32_t)payload[4] << 24) | ((uint32_t)payload[5] << 16) | ((uint32_t)payload[6] << 8) | payload[7];

        char userName[13];
        memcpy(userName, &payload[8], 12);
        userName[12] = '\0';
        uint8_t level = payload[20];
        uint32_t balance = ((uint32_t)payload[21] << 24) | ((uint32_t)payload[22] << 16) | ((uint32_t)payload[23] << 8) | payload[24];
        Serial0.printf("[CH32] 刷卡事件: UID=%s, UserID=%u, Name=%s, Level=%d, Balance=%u\n", uidHex, userId, userName, level, balance);

        // 上报刷卡记录到 Django
        if (wifiConnected) {
          HTTPClient http;
          http.begin("https://wisplike-scoop-elk.ngrok-free.dev/smart/api/card/record/");
          http.addHeader("Content-Type", "application/json");
          String body = "{\"card_uid\":\"" + String(uidHex) + "\",\"user_id\":" + String(userId) + ",\"user_name\":\"" + String(userName) + "\",\"balance\":" + String(balance) + ",\"total_price\":0}";
          http.POST(body);
          http.end();
        }

        // 上报到OneNET
        if (mqttConnected) {
          String topic = "$sys/" + String(productId) + "/" + String(deviceName) + "/thing/property/post";
          StaticJsonDocument<512> doc;
          doc["id"] = String(millis());
          doc["version"] = "1.0";
          doc["params"]["card_uid"]["value"] = String(uidHex);
          doc["params"]["user_id"]["value"] = userId;
          doc["params"]["user_name"]["value"] = String(userName);
          doc["params"]["balance"]["value"] = balance;
          char buf[512];
          serializeJson(doc, buf);
          mqttClient.publish(topic.c_str(), buf);
        }
        break;
      }
    case FRAME_TRANSACTION:  // 交易记录帧
      {                      // 格式：UID(4)+UserID(4)+Price(2) + BalBef(4) + BalAft(4) + Name(12) = 30
        if (len < 30) return;
        char uidHex[9];
        sprintf(uidHex, "%02X%02X%02X%02X", payload[0], payload[1], payload[2], payload[3]);
        ((uint32_t)payload[4] << 24) | ((uint32_t)payload[5] << 16) | ((uint32_t)payload[6] << 8) | payload[7];
        uint16_t price = ((uint16_t)payload[8] << 8) | payload[9];
        uint32_t balBef = ((uint32_t)payload[10] << 24) | ((uint32_t)payload[11] << 16) | ((uint32_t)payload[12] << 8) | payload[13];
        uint32_t balAft = ((uint32_t)payload[14] << 24) | ((uint32_t)payload[15] << 16) | ((uint32_t)payload[16] << 8) | payload[17];

        char userName[13];
        memcpy(userName, &payload[18], 12);
        userName[12] = '\0';
        Serial0.printf("[CH32] 交易记录: UID=%s, Price=%.2f元, Balance=%u, Name=%s\n", uidHex, price / 100.0, balAft, userName);

        // 解析菜品列表（payload 第 30 字节起，最多 60 字节）
        String dishList = "";
        if (len >= 90) {
          char dishes[61];
          memcpy(dishes, &payload[30], 60);
          dishes[60] = '\0';
          dishList = String(dishes);
          Serial0.printf("[CH32] 菜品: %s\n", dishes);
        }

        // 上报交易记录到 Django
        if (wifiConnected) {
          HTTPClient http;
          http.begin("https://wisplike-scoop-elk.ngrok-free.dev/smart/api/card/record/");
          http.addHeader("Content-Type", "application/json");
          String body = "{\"card_uid\":\"" + String(uidHex) + "\",\"user_id\":0,\"user_name\":\"" + String(userName) + "\",\"balance\":" + String(balAft) + ",\"total_price\":" + String(price) + ",\"dishes\":\"" + dishList + "\"}";
          int code = http.POST(body);
          // http.POST(body);
          Serial0.printf("[HTTP] POST card record, code=%d\n", code);
          http.end();
        }

        // 更新当前总价到 Django
        HTTPClient http3;
        http3.begin("https://wisplike-scoop-elk.ngrok-free.dev/api/price/update/");
        http3.addHeader("Content-Type", "application/json");
        String priceBody = "{\"amount\":\"" + String(price / 100.0, 2) + "\",\"dishes\":\"" + dishList + "\"}";
        http3.POST(priceBody);
        http3.end();

        // 交易上报 OneNET
        if (mqttConnected) {
          String topic = "$sys/" + String(productId) + "/" + String(deviceName) + "/thing/property/post";
          StaticJsonDocument<1024> doc;
          doc["id"] = String(millis());
          doc["version"] = "1.0";
          doc["params"]["card_uid"]["value"] = String(uidHex);
          doc["params"]["total_price"]["value"] = String(price / 100.0, 2);
          doc["params"]["balance"]["value"] = balAft;
          doc["params"]["dish_name"]["value"] = dishList;
          char buf[1024];
          serializeJson(doc, buf);
          mqttClient.publish(topic.c_str(), buf);
        }
        break;
      }
    case FRAME_HEARTBEAT:  // 心跳帧
      {
        // 回复心跳应答
        uint8_t ack = FRAME_HEARTBEAT_ACK;
        sendFrameToCH32(FRAME_HEARTBEAT_ACK, &ack, 1);
        break;
      }
    default:
      Serial0.printf("[CH32] 未知帧类型: 0x%02X\n", type);
      break;
  }
}


void postCardRecord(String cardUid, uint32_t userId, String userName, uint32_t balance, uint32_t totalPrice) {
  if (!wifiConnected) return;

  HTTPClient http;
  http.begin("https://wisplike-scoop-elk.ngrok-free.dev/smart/api/card/record/");
  http.addHeader("Content-Type", "application/json");

  String body = "{";
  body += "\"card_uid\":\"" + cardUid + "\",";
  body += "\"user_id\":" + String(userId) + ",";
  body += "\"user_name\":\"" + userName + "\",";
  body += "\"balance\":" + String(balance) + ",";
  body += "\"total_price\":" + String(totalPrice);
  body += "}";

  http.POST(body);
  http.end();
}
