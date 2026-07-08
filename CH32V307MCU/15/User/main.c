/***************************************************************************************************
 * 文件名: main_with_position_matching.c
 * 功能: 四通道称重系统 + K230位置识别匹配（整合版）
 *
 * 通道与位置映射关系:
 *   通道1 (ch1) <-> top_left (左上)
 *   通道2 (ch2) <-> top_right (右上)
 *   通道3 (ch3) <-> bottom_left (左下)
 *   通道4 (ch4) <-> bottom_right (右下)
 *
 * 数据流程:
 *   1. K230识别物品，发送JSON: {"top_left":{"item":"apple","\xc6\xbb\xb9\xfb":0.95}, ...}
 *   2. 称重系统读取4个通道重量
 *   3. 根据位置匹配重量和物品信息
 *   4. 输出合并数据: {"position":"top_left", "item":"apple", "weight":123.45g}
 *
 * 硬件连接:
 *   ADS1234: PA0(SCLK), PA1(DOUT), PB0(PDWN), PB2(A0), PB5(A1)
 *   UART1:  PA9(TX), PA10(RX) - 调试+串口屏
 *   UART3:  PB10(TX), PB11(RX) - 接收K230数据
 *   UART4:  PC10(TX), PC11(RX) - 转发到PC
 *
 * 作者: 代码整合助手
 * 日期: 2026-05-20
 ***************************************************************************************************/

#include "ch32v30x.h"
#include "System.h"
#include "Uart.h"
#include "ADS1234.h"
#include "Timer.h"
#include "Card.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <CLoude_bridge.h>
#include "Uart1.h"
#include "CLoud_protocol.h"
#include "../Driver/Audio.h"


/***************************************************************************************************
 * ===========================================
 * 第一部分: 配置定义
 * ===========================================
 */



uint8_t Default_Key[6] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};
//#define INIT_BALANCE 10000

/***************************************************************************************************
 * 物品名称映射表 - 英文到中文名称（UTF-8）
 ***************************************************************************************************/
// ============================================
// 物品名称映射表 - 英文到中文名称
// ============================================
// ============================================
// 物品名称映射表 - 英文到中文名称
// ============================================
typedef struct {
    const char *name_en;   // 英文名称（K230识别结果）
    const char *name_cn;   // 中文名称（串口屏显示）
} ItemMapping_t;

//const ItemMapping_t g_item_map[] = {
//    {"apple",       "苹果"},
//    {"banana",      "香蕉"},
//    {"orange",      "橙子"},
//    {"grape",       "葡萄"},
//    {"steamed_bun", "馒头"},
//    {"egg",         "鸡蛋"},
//    {"pure_milk",   "纯牛奶"},
//    {"shaomai",     "烧卖"},
//    // 添加更多映射...
//};

//const ItemMapping_t g_item_map[] = {
//    {"apple",                  "\xc6\xbb\xb9\xfb"},
//    {"banana",                 "\xcf\xe3\xbd\xb6"},
//    {"orange",                 "\xb3\xc8\xd7\xd3"},
//    {"grape",                  "\xc6\xcf\xcc\xd1"},
//    {"steamed_bun",            "\xc2\xf8\xcd\xb7"},
//    {"egg",                    "\xbc\xa6\xb5\xb0"},
//    {"pure_milk",              "\xb4\xbf\xc5\xa3\xc4\xcc"},
//    {"shaomai",                "\xc9\xd5\xc2\xf4"},
//    {"tomato_scrambled_egg",   "\xce\xf7\xba\xec\xca\xc1\xb3\xb4\xbc\xa6\xb5\xb0"},
//    {"spicy_sour_potato_shreds", "\xcb\xe1\xc0\xb1\xcd\xc1\xb6\xb9\xcb\xbf"},
//};
//#define ITEM_MAP_COUNT (sizeof(g_item_map) / sizeof(ItemMapping_t))

const ItemMapping_t g_item_map[] = {
    /* ---- K230 可能识别的物品 ---- */
    {"apple",                  "\xc6\xbb\xb9\xfb"},
    {"banana",                 "\xcf\xe3\xbd\xb6"},
    {"orange",                 "\xb3\xc8\xd7\xd3"},
    {"grape",                  "\xc6\xcf\xcc\xd1"},

    /* ---- 早餐 ---- */
    {"steamed_bun",            "\xc2\xf8\xcd\xb7"},
    {"egg",                    "\xbc\xa6\xb5\xb0"},
    {"pure_milk",              "\xb4\xbf\xc5\xa3\xc4\xcc"},
    {"shaomai",                "\xc9\xd5\xc2\xf4"},
    {"soy_milk",               "\xb6\xb9\xbd\xac"},
    {"fried_dough",            "\xd3\xcd\xcc\xf5"},
    {"porridge",               "\xd6\xe0"},
    {"meat_bun",               "\xc8\xe2\xb0\xfc"},
    {"pancake",                "\xbc\xe5\xb1\xfd"},
    {"tea_egg",                "\xb2\xe8\xd2\xb6\xb5\xb0"},

    /* ---- 菜品 ---- */
    {"tomato_scrambled_egg",   "\xce\xf7\xba\xec\xca\xc1\xb3\xb4\xbc\xa6\xb5\xb0"},
    {"spicy_sour_potato_shreds","\xcb\xe1\xc0\xb1\xcd\xc1\xb6\xb9\xcb\xbf"},
    {"kung_pao_chicken",       "\xb9\xac\xb1\xa3\xbc\xa6\xb6\xa1"},
    {"sweet_sour_pork",        "\xcc\xc7\xb4\xd7\xc0\xef\xbc\xb9"},
    {"mapo_tofu",              "\xc2\xe9\xc6\xc5\xb6\xb9\xb8\xaf"},
    {"twice_cooked_pork",      "\xbb\xd8\xb9\xf8\xc8\xe2"},
    {"fish_fragrant_eggplant", "\xd3\xe3\xcf\xe3\xc7\xd1\xd7\xd3"},
    {"dry_fried_beans",        "\xb8\xc9\xec\xd4\xcb\xc4\xbc\xbe\xb6\xb9"},
    {"braised_pork",           "\xba\xec\xc9\xd5\xc8\xe2"},
    {"pepper_beef",            "\xc7\xe0\xbd\xb7\xc8\xe2\xcb\xbf"},
    {"garlic_broccoli",        "\xcb\xe2\xc8\xd8\xce\xf7\xc0\xbc\xbb\xa8"},
    {"seaweed_soup",           "\xd7\xcf\xb2\xcb\xb5\xb0\xbb\xa8\xcc\xc0"},
};
#define ITEM_MAP_COUNT (sizeof(g_item_map) / sizeof(ItemMapping_t))

/* 接收缓冲区 */
#define RX_BUFFER_SIZE 2048
#define MAX_ITEMS 4

/* 位置名称定义 */
#define POS_TOP_LEFT     "top_left"
#define POS_TOP_RIGHT    "top_right"
#define POS_BOTTOM_LEFT  "bottom_left"
#define POS_BOTTOM_RIGHT "bottom_right"

/* 通道索引 */
#define CH_TOP_LEFT      0
#define CH_TOP_RIGHT     1
#define CH_BOTTOM_LEFT   2
#define CH_BOTTOM_RIGHT  3


/* ========== M1卡扇区布局定义 ========== */
#define SECTOR4_BASE_ADDR    16    // 扇区4的基地址（4 * 4 = 16）
#define BLOCK_BALANCE        (SECTOR4_BASE_ADDR + 0)  // 块0: 存储余额
#define BLOCK_LOG            (SECTOR4_BASE_ADDR + 1)  // 块1: 交易日志
#define BLOCK_BACKUP         (SECTOR4_BASE_ADDR + 2)  // 块2: 备份数据

#define INIT_BALANCE         10000    // 初始余额100元
#define USER_INFO_SECTOR     5      // 用户信息扇区


/***************************************************************************************************
 * ===========================================
 * 第二部分: 数据结构定义
 * ===========================================
 */

/* 物品信息结构体 */
typedef struct {
    char position[20];      // 位置名称: top_left, top_right, etc.
    char item_name[50];     // 物品名称: apple, banana, etc.
    float confidence;       // 识别置信度: 0.0 ~ 1.0
    uint8_t detected;       // 是否检测到物品: 1=是, 0=否
} ItemInfo_t;

/* 称重数据结构体 */
typedef struct {
    float weight;           // 重量（克）
    uint8_t valid;          // 数据是否有效: 1=是, 0=否
    uint32_t timestamp;     // 时间戳
} WeightData_t;

/* 合并数据结构体 */
typedef struct {
    char position[20];      // 位置
    char item_name[50];     // 物品名称
    float confidence;       // 置信度
    float weight;           // 重量
    uint8_t has_item;       // 是否有物品
    uint8_t has_weight;     // 是否有重量数据
} MergedData_t;

// 余额数据结构（16字节）
typedef struct {
    uint32_t balance;      // 4字节：实际余额
    uint32_t nonce;        // 4字节：随机数（防重复攻击）
    uint16_t crc;          // 2字节：CRC校验
    uint8_t reserved[4];   // 4字节：保留
    uint8_t checksum;      // 1字节：异或校验
    uint8_t flag;          // 1字节：数据有效性标志（0xA5表示有效）
} BalanceData_t;

//管理员模式
typedef enum {
    MODE_NORMAL = 0,      // 普通模式
    MODE_ADMIN_READY = 1,  // 管理员已认证，等待目标卡
    MODE_ADMIN_CHECK = 2, // 查询余额模式
    MODE_ADMIN_RECHARGE = 3,  // 充值模式
    MODE_ADMIN_INFO = 4    // 查看用户信息
} SystemMode_t;

SystemMode_t system_mode = MODE_NORMAL;  // 当前系统模式



/* 价格表：按英文名查找单价 */
typedef struct {
    char name_en[32];
    uint16_t price;          // 单价，单位：0.1元（125=12.5元）
} PriceEntry_t;



/***************************************************************************************************
   * 物品名称映射表 - 英文到GBK
   ***************************************************************************************************/

//  // 定义GBK编码的菜品名称（示例）
//  typedef struct {
//      const char *name_en;      // 英文名称（K230发送的）
//      const uint8_t *name_gbk;  // GBK编码的中文名称
//      uint8_t gbk_len;          // GBK编码长度
//  } ItemMapping_t;

  // 西红柿炒鸡蛋的GBK编码
  const uint8_t gbk_xihongshi[] = {0xCE,0xF7,0xBA,0xEC,0xCA,0xC1,0xB3,0xB4,0xBC,0xA6,0xB5,0xB0};

  // 鱼香肉丝的GBK编码
  const uint8_t gbk_yuxiangrousi[] = {0xBA,0xEC,0xCC,0xC7,0xC2,0xF8,0xCD,0xB7};

  // 宫保鸡丁的GBK编码
  const uint8_t gbk_gongbaojiding[] = {0xB9,0xAC,0xB1,0xA3,0xBC,0xA6,0xB6,0xA1};

  // 糖醋里脊的GBK编码
  const uint8_t gbk_tangculiji[] = {0xCC,0xC7,0xB4,0xD7,0xC0,0xEF,0xBC,0xB9};

  // 空的GBK编码
  const uint8_t gbk_empty[] = {0xBF,0xD5};  // "空"

  // 示例：定义一个 GBK 编码的“纯牛奶”字符串常量
  const unsigned char gbk_chunniunai[] = {0xB4, 0xBF, 0xC5, 0xA3, 0xC4, 0xCC, 0x00};

  // 苹果
  const unsigned char gbk_pingguo[] = {0xC6, 0xBB, 0xB9, 0xFB, 0x00};

//  // 物品映射表
//  const ItemMapping_t g_item_map[] = {
//      {"apple",          gbk_pingguo,      5},  // 苹果 -> 西红柿炒鸡蛋（示例）
//      {"banana",         gbk_yuxiangrousi,    8},  // 香蕉 -> 鱼香肉丝（示例）
//      {"orange",         gbk_gongbaojiding,   8},  // 橙子 -> 宫保鸡丁（示例）
//      {"steamed_bun",    gbk_xihongshi,      12},  // 馒头
//      {"egg",            gbk_yuxiangrousi,    8},  // 鸡蛋
//      {"pure_milk",      gbk_chunniunai,    7},  // 鸡蛋
//      // 添加更多映射...
//  };

  #define ITEM_MAP_COUNT (sizeof(g_item_map) / sizeof(ItemMapping_t))




//
///* 菜单价格表（单位：0.1元，35=3.5元） */
//PriceEntry_t g_price_table[] = {
//  {"steamed_bun",            15},   // 馒头   1.5元
//  {"egg",                    20},   // 鸡蛋   2.0元
//  {"pure_milk",              35},   // 纯牛奶 3.5元
//  {"shaomai",                25},   // 烧卖   2.5元
//  {"tomato_scrambled_egg",   120},  // 西红柿炒鸡蛋 12.0元
//  {"spicy_sour_potato_shreds",100}, // 酸辣土豆丝   10.0元
//};
//uint8_t g_price_count = sizeof(g_price_table) / sizeof(PriceEntry_t);
//





/***************************************************************************************************
 * ===========================================
 * 第三部分: 全局变量
 * ===========================================
 */

/* 称重系统变量 */
unsigned int Timer_2s = 0;
unsigned char Uart_Flag = 1;

/* K230数据接收变量 */
uint8_t uart3_rx_buffer[RX_BUFFER_SIZE];
volatile uint16_t uart3_rx_index = 0;
volatile uint8_t uart3_rx_complete = 0;
volatile uint8_t in_json_frame = 0;

/* 物品信息存储 */
ItemInfo_t g_items[MAX_ITEMS];

/* 称重数据存储 */
WeightData_t g_weights[4];

/* 合并数据存储 */
MergedData_t g_merged_data[4];

/* 数据更新标志 */
volatile uint8_t g_items_updated = 0;
volatile uint8_t g_weights_updated = 0;

/*刷卡*/
unsigned long int uwtick = 0;
uint8_t CARD_ID[4];
int price = 10;                              // 扣款金额
uint32_t recharge_amount = 100;              // 充值金额
uint8_t admin_uid[4];                        // 保存当前管理员UID


uint32_t g_k230_last_time = 0;

extern volatile uint32_t g_sys_tick;

volatile uint32_t g_tick = 0;


//PriceEntry_t g_price_table[20];
//uint8_t g_price_count = 0;

PriceEntry_t g_price_table[] = {
    /* ---- 早餐（10种） ---- */
    {"steamed_bun",              150},   // 馒头       1.5元
    {"egg",                      200},   // 鸡蛋       2.0元
    {"pure_milk",                350},   // 纯牛奶     3.5元
    {"shaomai",                  250},   // 烧卖       2.5元
    {"soy_milk",                 200},   // 豆浆       2.0元
    {"fried_dough",              150},   // 油条       1.5元
    {"porridge",                 200},   // 粥         2.0元
    {"meat_bun",                 250},   // 肉包       2.5元
    {"pancake",                  400},   // 煎饼       4.0元
    {"tea_egg",                  150},   // 茶叶蛋     1.5元

//    /* ---- 菜品（12种） ---- */
//    {"tomato_scrambled_egg",    120},   // 西红柿炒鸡蛋  12.0元
//    {"spicy_sour_potato_shreds",100},   // 酸辣土豆丝    10.0元
//    {"kung_pao_chicken",        150},   // 宫保鸡丁      15.0元
//    {"sweet_sour_pork",         180},   // 糖醋里脊      18.0元
//    {"mapo_tofu",               120},   // 麻婆豆腐      12.0元
//    {"twice_cooked_pork",       160},   // 回锅肉        16.0元
//    {"fish_fragrant_eggplant",  140},   // 鱼香茄子      14.0元
//    {"dry_fried_beans",         130},   // 干煸四季豆    13.0元
//    {"braised_pork",            180},   // 红烧肉        18.0元
//    {"pepper_beef",             140},   // 青椒肉丝      14.0元
//    {"garlic_broccoli",         120},   // 蒜蓉西兰花    12.0元
//    {"seaweed_soup",             80},   // 紫菜蛋花汤     8.0元


    /* ---- 菜品（12种） ---- */
    {"tomato_scrambled_egg",    2200},   // 西红柿炒鸡蛋  22.00元  [素]
    {"spicy_sour_potato_shreds",1800},   // 酸辣土豆丝    18.00元  [素]
    {"kung_pao_chicken",        1600},   // 宫保鸡丁      16.00元  [肉]
    {"sweet_sour_pork",         1900},   // 糖醋里脊      19.00元  [肉]
    {"mapo_tofu",               2000},   // 麻婆豆腐      20.00元  [素]
    {"twice_cooked_pork",       1700},   // 回锅肉        17.00元  [肉]
    {"fish_fragrant_eggplant",  2100},   // 鱼香茄子      21.00元  [素]
    {"dry_fried_beans",         1900},   // 干煸四季豆    19.00元  [素]
    {"braised_pork",            2000},   // 红烧肉        20.00元  [肉]
    {"pepper_beef",             1500},   // 青椒肉丝      15.00元  [肉]
    {"garlic_broccoli",         2200},   // 蒜蓉西兰花    22.00元  [素]
    {"seaweed_soup",            1200},   // 紫菜蛋花汤    12.00元  [汤]

};
uint8_t g_price_count = sizeof(g_price_table) / sizeof(PriceEntry_t);

extern volatile uint8_t  UART5_RxFlag;
extern volatile uint8_t  UART5_RxBuf[512];
extern volatile uint16_t UART5_RxLen;
void UART5_Init(uint32_t baudrate);
void UART4_ProcessPriceFrame(uint8_t *buf, uint16_t len);

extern volatile uint16_t uart5_bytes;
extern volatile uint32_t uart5_last_byte;

volatile uint8_t rfid_busy = 0;

volatile uint8_t show_success_page = 0;  // 全局变量
volatile uint32_t success_page_start = 0;

volatile uint8_t  show_fail_page = 0;
volatile uint32_t fail_page_start = 0;

volatile uint16_t g_total_price = 0;  // 当前总价（0.1元）

char g_saved_dishes[60] = {0};  // 保存最近一次识别的菜品名

void Send_Price_To_Screen(void);


/***************************************************************************************************
 * ===========================================
 * 第四部分: 函数声明
 * ===========================================
 */
const char* Get_Chinese_Name(const char *name_en);
void CompensateCrosstalkSimple(float weight[4]);
void Process_K230_Data(void);
uint8_t Parse_K230_JSON(char *json_str);
void Merge_Data(void);
void Print_Merged_Data(void);
void Send_Merged_Data_To_PC(void);
void Send_Merged_Data_To_Screen(void);
void Initialize_Data_Structures(void);
const char* Get_Position_Name(uint8_t channel);
uint8_t is_unit_priced_item(const char *item_name);

/* UART1函数声明 */
void USART1_SendString(char *str);

/* RFID函数声明 */
void RFID_Payment_Process(void);
void InitNewCard(uint8_t *uid, UserData_t *user_infor);
uint8_t ReadUserInfoFromCard(uint8_t sector, uint8_t *uid, UserData_t *user_data);
void DisplayUserInfo(UserData_t *user);
uint8_t ReadBalance(uint8_t block_addr, uint32_t *balance);
uint8_t WriteBalance(uint8_t block_addr, uint32_t balance);

uint16_t CalculateTotalPrice(void);  // 计算总价，返回元

//}



	/***************************************************************************************************

/***************************************************************************************************
 * ===========================================
 * 第五部分: 辅助函数 - 字符串处理
 * ===========================================
 */

/***************************************************************************************************
 * 函数名: find_json_value
 * 功能: 从JSON字符串中查找指定键的值
 * 参数:
 *   json - JSON字符串
 *   key  - 要查找的键名
 *   output - 输出缓冲区
 *   max_len - 输出缓冲区最大长度
 * 返回: 1=找到, 0=未找到
 ***************************************************************************************************/
uint8_t find_json_value(char *json, const char *key, char *output, uint16_t max_len)
{
    char search_key[100];
    char *start, *end, *value_start, *value_end;

    /* 构建搜索字符串: "key": */
    sprintf(search_key, "\"%s\"", key);

    start = strstr(json, search_key);
    if(start == NULL)
    {
        return 0;
    }

    /* 找到冒号后的值 */
    value_start = strchr(start, ':');
    if(value_start == NULL)
    {
        return 0;
    }
    value_start++;  /* 跳过冒号 */

    /* 跳过空白字符 */
    while(*value_start == ' ' || *value_start == '\t')
    {
        value_start++;
    }

    /* 判断值的类型 */
    if(*value_start == '"')  /* 字符串值 */
    {
        value_start++;  /* 跳过开始引号 */
        value_end = strchr(value_start, '"');
        if(value_end == NULL)
        {
            return 0;
        }
    }
    else if(*value_start == '{' || *value_start == '[')  /* 对象或数组 */
    {
        char end_char = (*value_start == '{') ? '}' : ']';
        int depth = 1;
        value_end = value_start + 1;

        while(*value_end && depth > 0)
        {
            if(*value_end == '{' || *value_end == '[')
            {
                depth++;
            }
            else if(*value_end == '}' || *value_end == ']')
            {
                depth--;
            }
            value_end++;
        }
        if(depth != 0)
        {
            return 0;
        }
    }
    else  /* 数字或其他 */
    {
        value_end = value_start;
        while(*value_end && *value_end != ',' && *value_end != '}' && *value_end != ']')
        {
            value_end++;
        }
    }

    /* 复制值到输出缓冲区 */
    uint16_t len = value_end - value_start;
    if(len >= max_len)
    {
        len = max_len - 1;
    }
    memcpy(output, value_start, len);
    output[len] = '\0';

    return 1;
}

/***************************************************************************************************
 * 函数名: extract_float_from_json
 * 功能: 从JSON对象中提取浮点数值
 ***************************************************************************************************/
float extract_float_from_json(char *json_obj)
{
    char *str_start = json_obj;

    /* 跳过空白 */
    while(*str_start == ' ' || *str_start == '\t' || *str_start == '\n' || *str_start == '\r')
    {
        str_start++;
    }

    /* 尝试解析为浮点数 */
    return strtof(str_start, NULL);
}

/***************************************************************************************************
 * ===========================================
 * 第六部分: K230数据处理函数
 * ===========================================
 */

/***************************************************************************************************
 * 函数名: Initialize_Data_Structures
 * 功能: 初始化所有数据结构
 ***************************************************************************************************/
void Initialize_Data_Structures(void)
{
    /* 初始化物品信息 */
    for(int i = 0; i < MAX_ITEMS; i++)
    {
        strcpy(g_items[i].position, "");
        strcpy(g_items[i].item_name, "");
        g_items[i].confidence = 0.0f;
        g_items[i].detected = 0;
    }

    /* 初始化称重数据 */
    for(int i = 0; i < 4; i++)
    {
        g_weights[i].weight = 0.0f;
        g_weights[i].valid = 0;
        g_weights[i].timestamp = 0;
    }

    /* 初始化合并数据 */
    for(int i = 0; i < 4; i++)
    {
        strcpy(g_merged_data[i].position, "");
        strcpy(g_merged_data[i].item_name, "");
        g_merged_data[i].confidence = 0.0f;
        g_merged_data[i].weight = 0.0f;
        g_merged_data[i].has_item = 0;
        g_merged_data[i].has_weight = 0;
    }
}

/***************************************************************************************************
 * 函数名: Parse_K230_JSON
 * 功能: 解析K230发送的JSON数据
 *
 * 假设K230发送的JSON格式示例:
 * {
 *   "top_left": {"item": "apple", "confidence": 0.95},
 *   "top_right": {"item": "banana", "confidence": 0.87},
 *   "bottom_left": {"item": "orange", "confidence": 0.92},
 *   "bottom_right": {}
 * }
 *
 * 或者简化格式:
 * {
 *   "top_left": "apple",
 *   "top_right": "banana",
 *   "bottom_left": "",
 *   "bottom_right": ""
 * }
 ***************************************************************************************************/
//uint8_t Parse_K230_JSON(char *json_str)
//{
//    char temp_buf[200];
//    char *obj_start, *obj_end;
//    uint8_t item_count = 0;
//
//    printf("\r\n[JSON] 开始解析K230数据...\r\n");
//
//    /* 定义要解析的位置数组 */
//    const char *positions[] = {POS_TOP_LEFT, POS_TOP_RIGHT, POS_BOTTOM_LEFT, POS_BOTTOM_RIGHT};
//    const uint8_t channels[] = {CH_TOP_LEFT, CH_TOP_RIGHT, CH_BOTTOM_LEFT, CH_BOTTOM_RIGHT};
//
//    /* 先清空物品信息 */
//    for(int i = 0; i < MAX_ITEMS; i++)
//    {
//        strcpy(g_items[i].item_name, "");
//        g_items[i].confidence = 0.0f;
//        g_items[i].detected = 0;
//    }
//
//    /* 解析每个位置 */
//    for(int i = 0; i < 4; i++)
//    {
//        /* 查找位置对象 */
//        char search_pattern[50];
//        sprintf(search_pattern, "\"%s\"", positions[i]);
//
//        char *pos_ptr = strstr(json_str, search_pattern);
//        if(pos_ptr == NULL)
//        {
//            printf("[JSON] 未找到位置: %s\r\n", positions[i]);
//            continue;
//        }
//
//        /* 找到冒号后的值 */
//        char *colon = strchr(pos_ptr, ':');
//        if(colon == NULL) continue;
//
//        /* 跳过空白 */
//        colon++;
//        while(*colon == ' ' || *colon == '\t') colon++;
//
//        /* 判断是对象格式还是字符串格式 */
//        if(*colon == '{')  /* 对象格式: {"item": "apple", "confidence": 0.95} */
//        {
//            /* 提取item名称 */
//            if(find_json_value(colon, "item", temp_buf, sizeof(temp_buf)))
//            {
//                strcpy(g_items[channels[i]].item_name, temp_buf);
//                g_items[channels[i]].detected = 1;
//                strcpy(g_items[channels[i]].position, positions[i]);
//
//                /* 提取置信度 */
//                if(find_json_value(colon, "confidence", temp_buf, sizeof(temp_buf)))
//                {
//                    g_items[channels[i]].confidence = extract_float_from_json(temp_buf);
//                }
//
//                item_count++;
//                printf("[JSON] %s: %s (置信度: %.2f)\r\n",
//                       positions[i], g_items[channels[i]].item_name, g_items[channels[i]].confidence);
//            }
//        }
//        else if(*colon == '"')  /* 字符串格式: "apple" */
//        {
//            /* 提取字符串值 */
//            char *str_start = colon + 1;
//            char *str_end = strchr(str_start, '"');
//
//            if(str_end && str_end > str_start)
//            {
//                uint16_t len = str_end - str_start;
//                if(len > 0 && len < sizeof(g_items[channels[i]].item_name))
//                {
//                    memcpy(g_items[channels[i]].item_name, str_start, len);
//                    g_items[channels[i]].item_name[len] = '\0';
//                    g_items[channels[i]].detected = 1;
//                    strcpy(g_items[channels[i]].position, positions[i]);
//                    g_items[channels[i]].confidence = 1.0f;  /* 字符串格式默认置信度为1 */
//
//                    item_count++;
//                    printf("[JSON] %s: %s\r\n", positions[i], g_items[channels[i]].item_name);
//                }
//            }
//        }
//        else if(*colon == 'n' || *colon == 'N')  /* null值 */
//        {
//            g_items[channels[i]].detected = 0;
//            printf("[JSON] %s: 空\r\n", positions[i]);
//        }
//    }
//
//    g_items_updated = 1;
//    printf("[JSON] 解析完成，共检测到 %d 个物品\r\n", item_count);
//
//    return item_count;
//}


//
//uint8_t Parse_K230_JSON(char *json_str)
//{
//    char temp_buf[100];
//    char position[20];
//    char label[50];
//    int channel = -1;
//
//    printf("\r\n[JSON] Parse K230: %s\r\n", json_str);
//
//    /* 1. 提取 position */
//    if(!find_json_value(json_str, "position", position, sizeof(position)))
//        return 0;
//
//    /* 2. 提取 label */
//    if(!find_json_value(json_str, "label", label, sizeof(label)))
//        return 0;
//
//    /* 3. 位置 → 通道映射 */
//    if(strcmp(position, "top_left") == 0)        channel = 0;
//    else if(strcmp(position, "top_right") == 0)   channel = 1;
//    else if(strcmp(position, "bottom_left") == 0) channel = 2;
//    else if(strcmp(position, "bottom_right") == 0) channel = 3;
//    else {
//        printf("[JSON] Unknown position: %s\r\n", position);
//        return 0;
//    }
//
//    /* 4. 只更新当前通道，不碰其他通道 */
//    strcpy(g_items[channel].item_name, label);
//    g_items[channel].detected = 1;
//
//    /* 5. 提取 confidence（可选） */
//    if(find_json_value(json_str, "confidence", temp_buf, sizeof(temp_buf)))
//        g_items[channel].confidence = atof(temp_buf);
//    else
//        g_items[channel].confidence = 0.0f;
//
//    strcpy(g_items[channel].position, position);
//
//    printf("[JSON] channel=%d, item=%s, confidence=%.2f\r\n",
//           channel, label, g_items[channel].confidence);
//
//    g_items_updated = 1;
//    return 1;
//}


uint8_t Parse_K230_JSON(char *json_str)
{
    char temp_buf[100];
    char position[20];
    char label[50];
    int channel = -1;

    /* 提取 position */
    if(!find_json_value(json_str, "position", position, sizeof(position)))
        return 0;

    /* 提取 label */
    if(!find_json_value(json_str, "label", label, sizeof(label)))
        return 0;

    /* 位置 → 通道 */
    if(strcmp(position, "top_left") == 0)        channel = 0;
    else if(strcmp(position, "top_right") == 0)   channel = 1;
    else if(strcmp(position, "bottom_left") == 0) channel = 2;
    else if(strcmp(position, "bottom_right") == 0) channel = 3;
    else return 0;

    /* 只更新当前通道 */
    strcpy(g_items[channel].item_name, label);
    g_items[channel].detected = 1;
    strcpy(g_items[channel].position, position);

    /* 提取 confidence */
    if(find_json_value(json_str, "confidence", temp_buf, sizeof(temp_buf)))
        g_items[channel].confidence = atof(temp_buf);

    /* 记录收到时间，等超时后统一发屏 */
    printf("[JSON] 即将设置时间戳, sys=%lu\r\n", g_tick);
    g_k230_last_time = g_tick;

    printf("[JSON] ch%d=%s\r\n", channel, label);
    return 1;
}



/***************************************************************************************************
 * 函数名: Process_K230_Data
 * 功能: 处理从K230接收的数据
 ***************************************************************************************************/
//void Process_K230_Data(void)
//{
//    if(uart3_rx_complete)
//    {
//        /* 转发原始数据到PC */
//        UART4_SendString("[K230原始] ");
//        UART4_SendString((char*)uart3_rx_buffer);
//        UART4_SendString("\r\n");
//
//        /* 解析JSON数据 */
//        uint8_t item_count = Parse_K230_JSON((char*)uart3_rx_buffer);
//
//        /* 如果解析成功，检查是否可以合并数据 */
//        if(item_count > 0)
//        {
//            printf("[K230] 收到%d个物品数据，g_weights_updated=%d\r\n", item_count, g_weights_updated);
//
//            /* 如果有称重数据，立即合并 */
//            if(g_weights_updated)
//            {
//                printf("[K230] 称重数据已就绪，开始合并...\r\n");
//                Merge_Data();
//                Print_Merged_Data();
//                Send_Merged_Data_To_PC();
////                Send_Merged_Data_To_Screen();
//            }
//            else
//            {
//                printf("[K230] 称重数据未就绪，等待下次称重周期...\r\n");
//                /* 即使没有称重数据，也发送K230原始数据到PC */
//                UART4_SendString("{\"status\":\"K230_data_received\",\"waiting_for_weight\":true}\r\n");
//            }
//        }
//
//        /* 清除标志和缓冲区 */
//        uart3_rx_index = 0;
//        uart3_rx_complete = 0;
//        in_json_frame = 0;
//
//        for(uint16_t i = 0; i < RX_BUFFER_SIZE; i++)
//        {
//            uart3_rx_buffer[i] = 0;
//        }
//    }
//}


void Process_K230_Data(void)
{
    /* 解析已在ISR中完成，这里只转发原始数据到PC */
    if(uart3_rx_complete)
    {
        UART4_SendString("[K230原始] ");
        UART4_SendString((char*)uart3_rx_buffer);
        UART4_SendString("\r\n");
        Parse_K230_JSON((char*)uart3_rx_buffer);

        uart3_rx_index = 0;
        uart3_rx_complete = 0;
        in_json_frame = 0;

        for(uint16_t i = 0; i < RX_BUFFER_SIZE; i++)
        {
            uart3_rx_buffer[i] = 0;
        }
    }
}


/***************************************************************************************************
 * ===========================================
 * 第七部分: 数据合并函数
 * ===========================================
 */

/***************************************************************************************************
 * 函数名: Get_Position_Name
 * 功能: 根据通道索引获取位置名称
 ***************************************************************************************************/
const char* Get_Position_Name(uint8_t channel)
{
    switch(channel)
    {
        case 0: return POS_TOP_LEFT;
        case 1: return POS_TOP_RIGHT;
        case 2: return POS_BOTTOM_LEFT;
        case 3: return POS_BOTTOM_RIGHT;
        default: return "unknown";
    }
}

/***************************************************************************************************
 * 函数名: Get_Chinese_Name
 * 功能: 根据英文名称查找对应的中文名称
 ***************************************************************************************************/
const char* Get_Chinese_Name(const char *name_en)
{
    /* 遍历映射表 */
    for(int i = 0; i < ITEM_MAP_COUNT; i++)
    {
        if(strcmp(name_en, g_item_map[i].name_en) == 0)
        {
            return g_item_map[i].name_cn;  // 返回中文名称
        }
    }

    /* 未找到，返回原名称 */
    return name_en;
}

/***************************************************************************************************
 * 函数名: is_unit_priced_item
 * 功能: 判断物品是否按个售卖（不需要显示重量）
 ***************************************************************************************************/
uint8_t is_unit_priced_item(const char *item_name)
{
    if(strcmp(item_name, "steamed_bun") == 0 ||
       strcmp(item_name, "egg") == 0 ||
       strcmp(item_name, "pure_milk") == 0 ||
       strcmp(item_name, "shaomai") == 0)
    {
        return 1;
    }
    return 0;
}

/***************************************************************************************************
 * 函数名: Merge_Data
 * 功能: 合并物品信息和称重数据
 ***************************************************************************************************/
void Merge_Data(void)
{
    printf("\r\n[MERGE] 开始合并数据...\r\n");

    for(int i = 0; i < 4; i++)
    {
        /* 设置位置名称 */
        strcpy(g_merged_data[i].position, Get_Position_Name(i));

        /* 复制物品信息 */
        if(g_items[i].detected)
        {
            strcpy(g_merged_data[i].item_name, g_items[i].item_name);
            g_merged_data[i].confidence = g_items[i].confidence;
            g_merged_data[i].has_item = 1;
        }
        else
        {
            strcpy(g_merged_data[i].item_name, "无物品");
            g_merged_data[i].confidence = 0.0f;
            g_merged_data[i].has_item = 0;
        }

//        /* 复制称重数据 */
//        if(g_weights[i].valid && g_weights[i].weight > 0.1f)  /* 大于0.1g认为有重量 */
//        {
//            g_merged_data[i].weight = g_weights[i].weight;
//            g_merged_data[i].has_weight = 1;
//        }
//        else
//        {
//            g_merged_data[i].weight = 0.0f;
//            g_merged_data[i].has_weight = 0;
//        }

        /* 复制称重数据 */
        if(g_items[i].detected && is_unit_priced_item(g_items[i].item_name))
        {
            // 按个卖的商品，重量强制为 0，不计入总重
            g_merged_data[i].weight = 0.0f;
            g_merged_data[i].has_weight = 0;
        }
        else if(g_weights[i].valid && g_weights[i].weight > 0.1f)
        {
            g_merged_data[i].weight = g_weights[i].weight;
            g_merged_data[i].has_weight = 1;
        }
        else
        {
            g_merged_data[i].weight = 0.0f;
            g_merged_data[i].has_weight = 0;
        }


        /* 打印合并结果 */
        printf("[MERGE] %s: 物品=%s, 重量=%.2fg (有物品:%d, 有重量:%d)\r\n",
               g_merged_data[i].position,
               g_merged_data[i].item_name,
               g_merged_data[i].weight,
               g_merged_data[i].has_item,
               g_merged_data[i].has_weight);
    }

    printf("[MERGE] 数据合并完成\r\n");
}

/***************************************************************************************************
 * 函数名: Print_Merged_Data
 * 功能: 打印合并后的数据到调试串口
 ***************************************************************************************************/
void Print_Merged_Data(void)
{
    printf("\r\n");
    printf("=============================================================================\r\n");
    printf("                         物品识别与称重数据合并                              \r\n");
    printf("-----------------------------------------------------------------------------\r\n");
    printf("  位置         | 物品名称        | 置信度   | 重量(g)   | 识别 | 称重        \r\n");
    printf("---------------|-----------------|----------|-----------|------|-------------\r\n");

    for(int i = 0; i < 4; i++)
    {
        printf(" %-13s | %-15s | %.2f     | %8.2f | %-4s | %-4s\r\n",
               g_merged_data[i].position,
               g_merged_data[i].item_name,
               g_merged_data[i].confidence,
               g_merged_data[i].weight,
               g_merged_data[i].has_item ? "✓" : "✗",
               g_merged_data[i].has_weight ? "✓" : "✗");
    }

    printf("-----------------------------------------------------------------------------\r\n");

    /* 计算总重量 */
    float total_weight = 0.0f;
    int detected_count = 0;
    for(int i = 0; i < 4; i++)
    {
        if(g_merged_data[i].has_weight)
        {
            total_weight += g_merged_data[i].weight;
        }
        if(g_merged_data[i].has_item)
        {
            detected_count++;
        }
    }

    printf("  检测到物品数: %d  |  总重量: %.2f g\r\n", detected_count, total_weight);
    printf("=============================================================================\r\n");
}

/***************************************************************************************************
 * 函数名: Send_Merged_Data_To_PC
 * 功能: 发送合并数据到PC（UART4，JSON格式）
 ***************************************************************************************************/
//void Send_Merged_Data_To_PC(void)
//{
//    char json_buffer[1024];
//    char *ptr = json_buffer;
//
//    /* 构建JSON */
//    /* 用整数运算避免浮点 printf */
//    int weight_int[4];
//    int conf_int[4];
//    for(int i=0; i<4; i++) {
//        weight_int[i] = (int)(g_merged_data[i].weight * 100.0f + 0.5f);
//        conf_int[i]   = (int)(g_merged_data[i].confidence * 100.0f + 0.5f); // 置信度转为两位小数
//    }
//
//    int total_weight_int = 0;
//    int detected_count = 0;
//    for(int i=0; i<4; i++) {
//        if(g_merged_data[i].has_weight) total_weight_int += weight_int[i];
//        if(g_merged_data[i].has_item) detected_count++;
//    }
//
//    ptr += sprintf(ptr, "{\"merged_data\":[");
//    ptr += sprintf(ptr, "{\"position\":\"%s\",\"item\":\"%s\",\"confidence\":%d.%02d,\"weight\":%d.%02d}",
//                   g_merged_data[0].position, g_merged_data[0].item_name,
//                   conf_int[0]/100, conf_int[0]%100,
//                   weight_int[0]/100, weight_int[0]%100);
//
//    for(int i = 1; i < 4; i++)
//    {
//        ptr += sprintf(ptr, ",{\"position\":\"%s\",\"item\":\"%s\",\"confidence\":%d.%02d,\"weight\":%d.%02d}",
//                       g_merged_data[i].position, g_merged_data[i].item_name,
//                       conf_int[i]/100, conf_int[i]%100,
//                       weight_int[i]/100, weight_int[i]%100);
//    }
//
//    ptr += sprintf(ptr, "],\"total_weight\":%d.%02d,\"detected_count\":%d}",
//                   total_weight_int/100, total_weight_int%100, detected_count);
//
//    /* 发送到PC */
//    UART4_SendString(json_buffer);
//    UART4_SendString("\r\n");
//
//    printf("[UART4] 已发送合并数据到PC\r\n");
//}


void Send_Merged_Data_To_PC(void)
{
    char json_buffer[1024];
    char *ptr = json_buffer;

    // 重量整数化（保留1位小数）
    int weight_int[4];
    for(int i = 0; i < 4; i++) {
        weight_int[i] = (int)(g_merged_data[i].weight * 10.0f + 0.5f);
    }

    ptr += sprintf(ptr, "{\"merged_data\":[");

    // 第一个数据
    ptr += sprintf(ptr, "{\"position\":\"%s\",\"item\":\"%s\",\"weight\":%d.%d}",
                   g_merged_data[0].position,
                   g_merged_data[0].item_name,
                   weight_int[0]/10, weight_int[0]%10);

    // 后续数据
    for(int i = 1; i < 4; i++) {
        ptr += sprintf(ptr, ",{\"position\":\"%s\",\"item\":\"%s\",\"weight\":%d.%d}",
                       g_merged_data[i].position,
                       g_merged_data[i].item_name,
                       weight_int[i]/10, weight_int[i]%10);
    }

    ptr += sprintf(ptr, "]}");

    // 发送到PC
    UART4_SendString(json_buffer);
    UART4_SendString("\r\n");
}



/***************************************************************************************************
 * 函数名: Send_Merged_Data_To_Screen
 * 功能: 发送合并数据到串口屏
 ***************************************************************************************************/
//void Send_Merged_Data_To_Screen(void)
//{
//    char screen_buf[100];
//
//    for(int i = 0; i < 4; i++)
//    {
////        /* 格式: t1.txt="apple:123.45g"ÿÿÿ */
////        if(g_merged_data[i].has_item)
////        {
////            sprintf(screen_buf, "t%d.txt=\"%s\\n%.1fg\"\xff\xff\xff",
////                    i + 1, g_merged_data[i].item_name, g_merged_data[i].weight);
////        }
////        else if(g_merged_data[i].has_weight)
////        {
////            sprintf(screen_buf, "t%d.txt=\"未知\\n%.1fg\"\xff\xff\xff",
////                    i + 1, g_merged_data[i].weight);
////        }
////        else
////        {
////            sprintf(screen_buf, "t%d.txt=\"空\"\xff\xff\xff", i + 1);
////        }
//
//        if(g_merged_data[i].has_item)
//        {
//            if(is_unit_priced_item(g_merged_data[i].item_name))
//            {
//                // 按个卖的商品不显示重量
//                sprintf(screen_buf, "t%d.txt=\"%s\\n按个卖\"\xff\xff\xff",
//                        i + 1, g_merged_data[i].item_name);
//            }
//            else
//            {
//                sprintf(screen_buf, "t%d.txt=\"%s\\n%.1fg\"\xff\xff\xff",
//                        i + 1, g_merged_data[i].item_name, g_merged_data[i].weight);
//            }
//        }
//        else if(g_merged_data[i].has_weight)
//        {
//            sprintf(screen_buf, "t%d.txt=\"未知\\n%.1fg\"\xff\xff\xff",
//                    i + 1, g_merged_data[i].weight);
//        }
//        else
//        {
//            sprintf(screen_buf, "t%d.txt=\"空\"\xff\xff\xff", i + 1);
//        }
//
//        USART1_SendString(screen_buf);
//        Delay_Ms(10);
//    }
//
//    /* 总重量 */
//    float total_weight = 0.0f;
//    for(int i = 0; i < 4; i++)
//    {
//        if(g_merged_data[i].has_weight)
//        {
//            total_weight += g_merged_data[i].weight;
//        }
//    }
//
//    sprintf(screen_buf, "t5.txt=\"总计:%.1fg\"\xff\xff\xff", total_weight);
//    USART1_SendString(screen_buf);
//}

/***************************************************************************************************
   * 函数名: Send_Merged_Data_To_Screen
   * 功能: 发送合并数据到串口屏
   ***************************************************************************************************/
//  void Send_Merged_Data_To_Screen(void)
//  {
//
////      if(show_success_page) return;  // 显示成功页时不刷新数据
//
//      if(show_success_page || show_fail_page) return;
//
//      static uint8_t first_send = 1;
//      if(first_send)
//      {
//          first_send = 0;
////          UART4_SendString("page Home");
////          UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////          Delay_Ms(100);
//      }
//
//      const char *cn;
//         char buf[16];
//
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(5);
//
//         /* ---- 通道0 -> Home.t2 ---- */
//         if(g_items[0].detected) {
//             cn = Get_Chinese_Name(g_items[0].item_name);
//             UART4_SendString("Home.t2.txt=\"");
//             UART4_SendString(cn);
//             UART4_SendString("\"");
//         } else {
//             UART4_SendString("Home.t2.txt=\"\xbf\xd5\"");
//         }
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
//         /* ---- 通道1 -> Home.t3 ---- */
//         if(g_items[1].detected) {
//             cn = Get_Chinese_Name(g_items[1].item_name);
//             UART4_SendString("Home.t3.txt=\"");
//             UART4_SendString(cn);
//             UART4_SendString("\"");
//         } else {
//             UART4_SendString("Home.t3.txt=\"\xbf\xd5\"");
//         }
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
//         /* ---- 通道2 -> Home.t4 ---- */
//         if(g_items[2].detected) {
//             cn = Get_Chinese_Name(g_items[2].item_name);
//             UART4_SendString("Home.t4.txt=\"");
//             UART4_SendString(cn);
//             UART4_SendString("\"");
//         } else {
//             UART4_SendString("Home.t4.txt=\"\xbf\xd5\"");
//         }
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
//         /* ---- 通道3 -> Home.t5 ---- */
//         if(g_items[3].detected) {
//             cn = Get_Chinese_Name(g_items[3].item_name);
//             UART4_SendString("Home.t5.txt=\"");
//             UART4_SendString(cn);
//             UART4_SendString("\"");
//         } else {
//             UART4_SendString("Home.t5.txt=\"\xbf\xd5\"");
//         }
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
//         /* ---- 重量 ---- */
//         UART4_SendString("List.x4.val=");
//         sprintf(buf, "%d", (int)(g_weights[0].weight * 10));
//         UART4_SendString(buf);
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
//         UART4_SendString("List.x5.val=");
//         sprintf(buf, "%d", (int)(g_weights[1].weight * 10));
//         UART4_SendString(buf);
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
//         UART4_SendString("List.x6.val=");
//         sprintf(buf, "%d", (int)(g_weights[2].weight * 10));
//         UART4_SendString(buf);
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
//         UART4_SendString("List.x7.val=");
//         sprintf(buf, "%d", (int)(g_weights[3].weight * 10));
//         UART4_SendString(buf);
//         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//         Delay_Ms(50);
//
////         /* ---- 总重量 ---- */
////         float total = g_weights[0].weight + g_weights[1].weight +
////                       g_weights[2].weight + g_weights[3].weight;
////         UART4_SendString("Home.t6.txt=\"");
////         sprintf(buf, "%.1fg", total);
////         UART4_SendString(buf);
////         UART4_SendString("\"");
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//
//
//
//         /* ---- 匹配价格并发送到 Home.x0~x3 ---- */
////         for(int i = 0; i < 4; i++)
////         {
////             uint16_t price = 0;
////             for(uint8_t p = 0; p < g_price_count; p++)
////             {
////                 if(strcmp(g_items[i].item_name, g_price_table[p].name_en) == 0)
////                 {
////                     price = g_price_table[p].price;
////                     break;
////                 }
////             }
////             sprintf(buf, "%d", price);
////             UART4_SendString("Home.x"); UART4_SendByte('0' + i);
////             UART4_SendString(".val=");
////             UART4_SendString(buf);
////             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////             Delay_Ms(30);
////         }
//         /* ---- 发送价格 (只发匹配到的) ---- */
//         for(int i = 0; i < 4; i++)
//         {
//             uint16_t price = 0;
//             uint8_t found = 0;
//             for(uint8_t p = 0; p < g_price_count; p++)
//             {
//                 if(strcmp(g_items[i].item_name, g_price_table[p].name_en) == 0)
//                 {
//                     price = g_price_table[p].price;
//                     found = 1;
//                     break;
//                 }
//             }
//
//             printf("[PRICE_SEND] ch%d item=%s found=%d price=%d\r\n",
//                           i, g_items[i].item_name, found, price);
//
//             if(found)  // ← 只有匹配到才发
//             {
//                 sprintf(buf, "%d", price);
//                 UART4_SendString("Home.x"); UART4_SendByte('0' + i);
//                 UART4_SendString(".val=");
//                 UART4_SendString(buf);
//                 UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//                 Delay_Ms(30);
//             }
//         }
//
//
//
//         uint16_t total = CalculateTotalPrice();
//        UART4_SendString("List.x8.val=");
//        sprintf(buf, "%d", total);
//        UART4_SendString(buf);
//        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//        Delay_Ms(50);
//
//
//         Delay_Ms(100);
//}


//void Send_Merged_Data_To_Screen(void)
//{
//
////      if(show_success_page) return;  // 显示成功页时不刷新数据
//
//      if(show_success_page || show_fail_page) return;
//
//      static uint8_t first_send = 1;
//      if(first_send)
//      {
//          first_send = 0;
////          UART4_SendString("page Home");
////          UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////          Delay_Ms(100);
//      }
//
////      const char *cn;
////         char buf[16];
////
////         /* ---- 通道0 -> Home.t2 ---- */
////         if(g_items[0].detected) {
////             cn = Get_Chinese_Name(g_items[0].item_name);
////             UART4_SendString("Home.t2.txt=\"");
////             UART4_SendString(cn);
////             UART4_SendString("\"");
////         } else {
////             UART4_SendString("Home.t2.txt=\"\xbf\xd5\"");
////         }
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////         /* ---- 通道1 -> Home.t3 ---- */
////         if(g_items[1].detected) {
////             cn = Get_Chinese_Name(g_items[1].item_name);
////             UART4_SendString("Home.t3.txt=\"");
////             UART4_SendString(cn);
////             UART4_SendString("\"");
////         } else {
////             UART4_SendString("Home.t3.txt=\"\xbf\xd5\"");
////         }
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////         /* ---- 通道2 -> Home.t4 ---- */
////         if(g_items[2].detected) {
////             cn = Get_Chinese_Name(g_items[2].item_name);
////             UART4_SendString("Home.t4.txt=\"");
////             UART4_SendString(cn);
////             UART4_SendString("\"");
////         } else {
////             UART4_SendString("Home.t4.txt=\"\xbf\xd5\"");
////         }
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////         /* ---- 通道3 -> Home.t5 ---- */
////         if(g_items[3].detected) {
////             cn = Get_Chinese_Name(g_items[3].item_name);
////             UART4_SendString("Home.t5.txt=\"");
////             UART4_SendString(cn);
////             UART4_SendString("\"");
////         } else {
////             UART4_SendString("Home.t5.txt=\"\xbf\xd5\"");
////         }
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////         /* ---- 重量 通道0 ---- */
////         UART4_SendString("List.x4.val=");
////         if(g_items[0].detected)
////             sprintf(buf, "%d", (int)(g_weights[0].weight * 10));
////         else
////             sprintf(buf, "0");
////         UART4_SendString(buf);
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////         /* ---- 重量 通道1 ---- */
////         UART4_SendString("List.x5.val=");
////         if(g_items[1].detected)
////             sprintf(buf, "%d", (int)(g_weights[1].weight * 10));
////         else
////             sprintf(buf, "0");
////         UART4_SendString(buf);
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////         /* ---- 重量 通道2 ---- */
////         UART4_SendString("List.x6.val=");
////         if(g_items[2].detected)
////             sprintf(buf, "%d", (int)(g_weights[2].weight * 10));
////         else
////             sprintf(buf, "0");
////         UART4_SendString(buf);
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////         /* ---- 重量 通道3 ---- */
////         UART4_SendString("List.x7.val=");
////         if(g_items[3].detected)
////             sprintf(buf, "%d", (int)(g_weights[3].weight * 10));
////         else
////             sprintf(buf, "0");
////         UART4_SendString(buf);
////         UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////         Delay_Ms(50);
////
////        /* ---- 发送价格 (只发匹配到的) ---- */
////        for(int i = 0; i < 4; i++)
////        {
////            uint16_t price = 0;
////            uint8_t found = 0;
////            for(uint8_t p = 0; p < g_price_count; p++)
////            {
////                if(strcmp(g_items[i].item_name, g_price_table[p].name_en) == 0)
////                {
////                    price = g_price_table[p].price;
////                    found = 1;
////                    break;
////                }
////            }
////
////            printf("[PRICE_SEND] ch%d item=%s found=%d price=%d\r\n",
////                          i, g_items[i].item_name, found, price);
////
//////            if(found)
//////            {
//////                sprintf(buf, "%d", price);
//////                UART4_SendString("Home.x"); UART4_SendByte('0' + i);
//////                UART4_SendString(".val=");
//////                UART4_SendString(buf);
//////                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//////                Delay_Ms(30);
//////            }
////
////            if(found)
////                sprintf(buf, "%d", price);
////            else
////                sprintf(buf, "0");
////
////            UART4_SendString("Home.x"); UART4_SendByte('0' + i);
////            UART4_SendString(".val=");
////            UART4_SendString(buf);
////            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////            Delay_Ms(30);
////
////        }
//
//
//      const char *cn;
//         char buf[16];
//
//         /* ---- 建立显示映射：检测到的通道按顺序排到前面 ---- */
//         uint8_t disp_map[4];   // disp_map[显示位] = 通道号
//         uint8_t disp_cnt = 0;
//
//         if(disp_cnt == 0)
//                 return;
//
//         for(int ch = 0; ch < 4; ch++)
//         {
//             if(g_items[ch].detected)
//                 disp_map[disp_cnt++] = ch;
//         }
//         // 未检测到的通道填到后面
//         for(int ch = 0; ch < 4; ch++)
//         {
//             if(!g_items[ch].detected)
//                 disp_map[disp_cnt++] = ch;
//         }
//
//         /* ---- 物品名称 → Home.t2 ~ Home.t5（按显示顺序） ---- */
////         for(int pos = 0; pos < 4; pos++)
////         {
////             uint8_t ch = disp_map[pos];
////             sprintf(buf, "Home.t%d.txt=\"", pos + 2);
////             UART4_SendString(buf);
////             if(g_items[ch].detected)
////             {
////                 cn = Get_Chinese_Name(g_items[ch].item_name);
////                 UART4_SendString(cn);
////             }
////             else
////             {
////                 UART4_SendString("\xBF\xD5");
////             }
////             UART4_SendString("\"");
////             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////             Delay_Ms(50);
////         }
//
//         for(int pos = 0; pos < disp_cnt; pos++)
//         {
//             uint8_t ch = disp_map[pos];
//             sprintf(buf, "Home.t%d.txt=\"", pos + 2);
//             UART4_SendString(buf);
//             cn = Get_Chinese_Name(g_items[ch].item_name);
//             UART4_SendString(cn);
//             UART4_SendString("\"");
//             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//             Delay_Ms(50);
//         }
//
//         /* ---- 重量 → List.x4 ~ List.x7（按显示顺序） ---- */
////         for(int pos = 0; pos < 4; pos++)
////         {
////             uint8_t ch = disp_map[pos];
////             sprintf(buf, "List.x%d.val=", pos + 4);
////             UART4_SendString(buf);
////             if(g_items[ch].detected)
////                 sprintf(buf, "%d", (int)(g_weights[ch].weight * 10));
////             else
////                 sprintf(buf, "0");
////             UART4_SendString(buf);
////             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////             Delay_Ms(50);
////         }
//
//         for(int pos = 0; pos < disp_cnt; pos++)
//         {
//             uint8_t ch = disp_map[pos];
//             sprintf(buf, "List.x%d.val=", pos + 4);
//             UART4_SendString(buf);
//             sprintf(buf, "%d", (int)(g_weights[ch].weight * 10));
//             UART4_SendString(buf);
//             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//             Delay_Ms(50);
//         }
//
//        /* ---- 发送价格 → Home.x0 ~ Home.x3（按显示顺序） ---- */
////        for(int pos = 0; pos < 4; pos++)
////        {
////            uint8_t ch = disp_map[pos];
////            uint16_t price = 0;
////            uint8_t found = 0;
////            for(uint8_t p = 0; p < g_price_count; p++)
////            {
////                if(strcmp(g_items[ch].item_name, g_price_table[p].name_en) == 0)
////                {
////                    price = g_price_table[p].price;
////                    found = 1;
////                    break;
////                }
////            }
////
////            printf("[PRICE_SEND] pos%d ch%d item=%s found=%d price=%d\r\n",
////                          pos, ch, g_items[ch].item_name, found, price);
////
////            if(found)
////                sprintf(buf, "%d", price);
////            else
////                sprintf(buf, "0");
////
////            UART4_SendString("Home.x"); UART4_SendByte('0' + pos);
////            UART4_SendString(".val=");
////            UART4_SendString(buf);
////            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////            Delay_Ms(30);
////        }
//
////         for(int pos = 0; pos < disp_cnt; pos++)
////         {
////             uint8_t ch = disp_map[pos];
////             uint16_t price = 0;
////             for(uint8_t p = 0; p < g_price_count; p++)
////             {
////                 if(strcmp(g_items[ch].item_name, g_price_table[p].name_en) == 0)
////                 {
////                     price = g_price_table[p].price;
////                     break;
////                 }
////             }
////
////             printf("[PRICE_SEND] pos%d ch%d item=%s price=%d\r\n",
////                           pos, ch, g_items[ch].item_name, price);
////
////             sprintf(buf, "%d", price);
////
////             UART4_SendString("Home.x"); UART4_SendByte('0' + pos);
////             UART4_SendString(".val=");
////             UART4_SendString(buf);
////             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////             Delay_Ms(30);
////         }
//
//
////        /* ---- 单位显示 → Home.t6 ~ Home.t9 ---- */
////        {
////            const char *breakfast_list[] = {"steamed_bun", "egg", "pure_milk", "shaomai"};
////            uint8_t bf_count = 4;
////
////            for(int i = 0; i < 4; i++)
////            {
////                if(g_items[i].detected)
////                {
////                    uint8_t is_bf = 0;
////                    for(uint8_t b = 0; b < bf_count; b++)
////                    {
////                        if(strcmp(g_items[i].item_name, breakfast_list[b]) == 0)
////                        { is_bf = 1; break; }
////                    }
////
////                    UART4_SendString("Home.t"); UART4_SendByte('0' + i + 6);
////                    if(is_bf)
////                        UART4_SendString(".txt=\"\xD4\xAA/\xB8\xF6\"");   // 元/个
////                    else
////                        UART4_SendString(".txt=\"\xD4\xAA/\xBD\xEF\"");   // 元/斤
////                }
////                else
////                {
////                    UART4_SendString("Home.t"); UART4_SendByte('0' + i + 6);
////                    UART4_SendString(".txt=\"\xBF\xD5\"");                 // 空
////                }
////                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////                Delay_Ms(30);
////            }
////        }
//
//
//
//        /* ---- 单位显示 → Home.t6 ~ Home.t9（按显示顺序） ---- */
//        {
//            const char *breakfast_list[] = {"steamed_bun", "egg", "pure_milk", "shaomai"};
//            uint8_t bf_count = 4;
//
//            for(int pos = 0; pos < 4; pos++)
//            {
//                uint8_t ch = disp_map[pos];
//                if(g_items[ch].detected)
//                {
//                    uint8_t is_bf = 0;
//                    for(uint8_t b = 0; b < bf_count; b++)
//                    {
//                        if(strcmp(g_items[ch].item_name, breakfast_list[b]) == 0)
//                        { is_bf = 1; break; }
//                    }
//
//                    UART4_SendString("Home.t"); UART4_SendByte('0' + pos + 6);
//                    if(is_bf)
//                        UART4_SendString(".txt=\"\xD4\xAA/\xB8\xF6\"");
//                    else
//                        UART4_SendString(".txt=\"\xD4\xAA/\xBD\xEF\"");
//                }
//                else
//                {
//                    UART4_SendString("Home.t"); UART4_SendByte('0' + pos + 6);
//                    UART4_SendString(".txt=\"\xBF\xD5\"");
//                }
//                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//                Delay_Ms(30);
//            }
//
////            for(int pos = 0; pos < disp_cnt; pos++)
////            {
////                uint8_t ch = disp_map[pos];
////                uint8_t is_bf = 0;
////                for(uint8_t b = 0; b < bf_count; b++)
////                {
////                    if(strcmp(g_items[ch].item_name, breakfast_list[b]) == 0)
////                    { is_bf = 1; break; }
////                }
////
////                UART4_SendString("Home.t"); UART4_SendByte('0' + pos + 6);
////                if(is_bf)
////                    UART4_SendString(".txt=\"\xD4\xAA/\xB8\xF6\"");
////                else
////                    UART4_SendString(".txt=\"\xD4\xAA/\xBD\xEF\"");
////                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////                Delay_Ms(30);
////            }
//
//        }
//
//
//        /* ---- 总价 → List.x8 ---- */
//        {
//            uint16_t total = CalculateTotalPrice();
//
////            g_total_price = total;   // ← 加这行，存到全局
//
//            if(total > 0) g_total_price = total;
//
//            UART4_SendString("List.x8.val=");
//            sprintf(buf, "%d", total);
//            UART4_SendString(buf);
//            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//            Delay_Ms(50);
//        }
//
//        Delay_Ms(100);
//}

//void Send_Merged_Data_To_Screen(void)
//{
//
//      if(show_success_page || show_fail_page) return;
//
//      static uint8_t first_send = 1;
//      if(first_send)
//      {
//          first_send = 0;
//      }
//
//      const char *cn;
//         char buf[16];
//
//         /* ---- 建立显示映射：检测到的通道按顺序排到前面 ---- */
//         uint8_t disp_map[4];
//         uint8_t disp_cnt = 0;
//         for(int ch = 0; ch < 4; ch++)
//         {
//             if(g_items[ch].detected)
//                 disp_map[disp_cnt++] = ch;
//         }
//         for(int ch = 0; ch < 4; ch++)
//         {
//             if(!g_items[ch].detected)
//                 disp_map[disp_cnt++] = ch;
//         }
//
//         /* ---- 物品名称 → Home.t2 ~ Home.t5（按显示顺序） ---- */
//         for(int pos = 0; pos < 4; pos++)
//         {
//             uint8_t ch = disp_map[pos];
//             sprintf(buf, "Home.t%d.txt=\"", pos + 2);
//             UART4_SendString(buf);
//             if(g_items[ch].detected)
//             {
//                 cn = Get_Chinese_Name(g_items[ch].item_name);
//                 UART4_SendString(cn);
//             }
//             UART4_SendString("\"");
//             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//             Delay_Ms(50);
//         }
//
//         /* ---- 重量 → List.x4 ~ List.x7（按显示顺序） ---- */
//         for(int pos = 0; pos < 4; pos++)
//         {
//             uint8_t ch = disp_map[pos];
//             sprintf(buf, "List.x%d.val=", pos + 4);
//             UART4_SendString(buf);
//             if(g_items[ch].detected)
//                 sprintf(buf, "%d", (int)(g_weights[ch].weight * 10));
//             else
//                 sprintf(buf, "0");
//             UART4_SendString(buf);
//             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//             Delay_Ms(50);
//         }
//
//        /* ---- 发送价格 → Home.x0 ~ Home.x3（按显示顺序） ---- */
//        for(int pos = 0; pos < 4; pos++)
//        {
//            uint8_t ch = disp_map[pos];
//            uint16_t price = 0;
//            uint8_t found = 0;
//            for(uint8_t p = 0; p < g_price_count; p++)
//            {
//                if(strcmp(g_items[ch].item_name, g_price_table[p].name_en) == 0)
//                {
//                    price = g_price_table[p].price;
//                    found = 1;
//                    break;
//                }
//            }
//
//            printf("[PRICE_SEND] pos%d ch%d item=%s found=%d price=%d\r\n",
//                          pos, ch, g_items[ch].item_name, found, price);
//
//            if(found)
//                sprintf(buf, "%d", price);
//            else
//                sprintf(buf, "0");
//
//            UART4_SendString("Home.x"); UART4_SendByte('0' + pos);
//            UART4_SendString(".val=");
//            UART4_SendString(buf);
//            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//            Delay_Ms(30);
//        }
//
//        /* ---- 单位显示 → Home.t6 ~ Home.t9（按显示顺序） ---- */
//        {
//            const char *breakfast_list[] = {"steamed_bun", "egg", "pure_milk", "shaomai"};
//            uint8_t bf_count = 4;
//
//            for(int pos = 0; pos < 4; pos++)
//            {
//                uint8_t ch = disp_map[pos];
//                UART4_SendString("Home.t"); UART4_SendByte('0' + pos + 6);
//                if(g_items[ch].detected)
//                {
//                    uint8_t is_bf = 0;
//                    for(uint8_t b = 0; b < bf_count; b++)
//                    {
//                        if(strcmp(g_items[ch].item_name, breakfast_list[b]) == 0)
//                        { is_bf = 1; break; }
//                    }
//                    if(is_bf)
//                        UART4_SendString(".txt=\"\xD4\xAA/\xB8\xF6\"");
//                    else
//                        UART4_SendString(".txt=\"\xD4\xAA/\xBD\xEF\"");
//                }
//                else
//                {
//                    UART4_SendString(".txt=\"\"");
//                }
//                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//                Delay_Ms(30);
//            }
//        }
//
//        /* ---- 总价 → List.x8 ---- */
//        {
//            uint16_t total = CalculateTotalPrice();
//
//            if(total > 0) g_total_price = total;
//
//            UART4_SendString("List.x8.val=");
//            sprintf(buf, "%d", total);
//            UART4_SendString(buf);
//            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//            Delay_Ms(50);
//        }
//
//        Delay_Ms(100);
//}




void Send_Merged_Data_To_Screen(void)
{

      if(show_success_page || show_fail_page) return;

      static uint8_t first_send = 1;
      if(first_send)
      {
          first_send = 0;
      }

      const char *cn;
         char buf[16];

         /* ---- 建立显示映射 ---- */
         uint8_t disp_map[4];
         uint8_t disp_cnt = 0;
         for(int ch = 0; ch < 4; ch++)
         {
             if(g_items[ch].detected)
                 disp_map[disp_cnt++] = ch;
         }
         for(int ch = 0; ch < 4; ch++)
         {
             if(!g_items[ch].detected)
                 disp_map[disp_cnt++] = ch;
         }

//
//         uint8_t disp_map[4];
//         uint8_t disp_cnt = 0;
//
//         /* 位置0 固定为通道1 */
//         disp_map[disp_cnt++] = 0;
//
//         /* 通道2~4：检测到的优先，未检测到的靠后 */
//         for(int ch = 1; ch < 4; ch++)
//         {
//             if(g_items[ch].detected)
//                 disp_map[disp_cnt++] = ch;
//         }
//         for(int ch = 1; ch < 4; ch++)
//         {
//             if(!g_items[ch].detected)
//                 disp_map[disp_cnt++] = ch;
//         }




         /* ---- 物品名称 → Home.t2 ~ Home.t5 ---- */
         for(int pos = 0; pos < 4; pos++)
         {
             uint8_t ch = disp_map[pos];
             sprintf(buf, "Home.t%d.txt=\"", pos + 2);
             UART4_SendString(buf);
             if(g_items[ch].detected)
             {
                 cn = Get_Chinese_Name(g_items[ch].item_name);
                 UART4_SendString(cn);
             }
             UART4_SendString("\"");
             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
             Delay_Ms(50);
         }

//         /* ---- 重量 → List.x4 ~ List.x7 ---- */
//         for(int pos = 0; pos < 4; pos++)
//         {
//             uint8_t ch = disp_map[pos];
//             sprintf(buf, "List.x%d.val=", pos + 4);
//             UART4_SendString(buf);
//             if(g_items[ch].detected)
//                 sprintf(buf, "%d", (int)(g_weights[ch].weight * 10));
//             else
//                 sprintf(buf, "%d", (int)(g_weights[0].weight * 10));
//             UART4_SendString(buf);
//             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//             Delay_Ms(50);
//         }

         /* ---- 称重 → List.t18 ~ List.t21（按显示顺序） ---- */
         for(int pos = 0; pos < 4; pos++)
         {
             uint8_t ch = disp_map[pos];
             if(g_items[ch].detected)
                 sprintf(buf, "List.t%d.txt=\"%d.%d\"", pos + 18,
                         (int)g_weights[ch].weight,
                         (int)(g_weights[ch].weight * 10) % 10);
             else
                 sprintf(buf, "List.t%d.txt=\"\"", pos + 18);
             UART4_SendString(buf);
             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
             Delay_Ms(30);
         }

//         /* ---- 称重 → List.t18 ~ List.t21（固定通道1~4，不管K230） ---- */
//         for(int ch = 0; ch < 4; ch++)
//         {
//             sprintf(buf, "List.t%d.txt=\"%d.%d\"", ch + 18,
//                     (int)g_weights[ch].weight,
//                     (int)(g_weights[ch].weight * 10) % 10);
//             UART4_SendString(buf);
//             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//             Delay_Ms(30);
//         }


//        /* ---- 价格 → Home.x0 ~ Home.x3（有物品才发） ---- */
//        for(int pos = 0; pos < 4; pos++)
//        {
//            uint8_t ch = disp_map[pos];
//            if(!g_items[ch].detected) continue;
//
//            uint16_t price = 0;
//            uint8_t found = 0;
//            for(uint8_t p = 0; p < g_price_count; p++)
//            {
//                if(strcmp(g_items[ch].item_name, g_price_table[p].name_en) == 0)
//                {
//                    price = g_price_table[p].price;
//                    found = 1;
//                    break;
//                }
//            }
//
//            if(found)
//            {
//                printf("[PRICE_SEND] pos%d ch%d item=%s price=%d\r\n",
//                              pos, ch, g_items[ch].item_name, price);
//
//                UART4_SendString("Home.x"); UART4_SendByte('0' + pos);
//                UART4_SendString(".val=");
//                sprintf(buf, "%d", price);
//                UART4_SendString(buf);
//                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//                Delay_Ms(30);
//            }
//        }

//         /* ---- 价格 → Home.x0 ~ Home.x3（按显示顺序） ---- */
//         for(int pos = 0; pos < 4; pos++)
//         {
//             uint8_t ch = disp_map[pos];
//             uint16_t price = 0;
//             uint8_t found = 0;
//
//             if(g_items[ch].detected)
//             {
//                 for(uint8_t p = 0; p < g_price_count; p++)
//                 {
//                     if(strcmp(g_items[ch].item_name, g_price_table[p].name_en) == 0)
//                     {
//                         price = g_price_table[p].price;
//                         found = 1;
//                         break;
//                     }
//                 }
//             }
//
//             printf("[PRICE_SEND] pos%d ch%d item=%s found=%d price=%d\r\n",
//                           pos, ch, g_items[ch].item_name, found, price);
//
//             UART4_SendString("Home.x"); UART4_SendByte('0' + pos);
//             if(found)
//             {
//                 UART4_SendString(".txt=\"");
//                 sprintf(buf, "%d.%02d", price/100, price%100);
//                 UART4_SendString(buf);
//                 UART4_SendString("\"");
//             }
//             else
//             {
//                 UART4_SendString(".txt=\"\"");
//             }
//             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//             Delay_Ms(30);
//         }

         /* ---- 菜品单价 → Home.t10 ~ Home.t13（按显示顺序） ---- */
         for(int pos = 0; pos < 4; pos++)
         {
             uint8_t ch = disp_map[pos];
             uint16_t price = 0;
             uint8_t found = 0;

             if(g_items[ch].detected)
             {
                 for(uint8_t p = 0; p < g_price_count; p++)
                 {
                     if(strcmp(g_items[ch].item_name, g_price_table[p].name_en) == 0)
                     {
                         price = g_price_table[p].price;
                         found = 1;
                         break;
                     }
                 }
             }

             if(found)
             {
                 sprintf(buf, "Home.t%d.txt=\"%d.%02d\"", pos + 10, price/100, price%100);
                 UART4_SendString(buf);
             }
             else
             {
                 sprintf(buf, "Home.t%d.txt=\"\"", pos + 10);
                 UART4_SendString(buf);
             }
             UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
             Delay_Ms(30);
         }

        /* ---- 单位 → Home.t6 ~ Home.t9（有物品发单位，没物品发空格） ---- */
        {
            const char *breakfast_list[] = {"steamed_bun", "egg", "pure_milk", "shaomai"};
            uint8_t bf_count = 4;

            for(int pos = 0; pos < 4; pos++)
            {
                uint8_t ch = disp_map[pos];
                UART4_SendString("Home.t"); UART4_SendByte('0' + pos + 6);
                if(g_items[ch].detected)
                {
                    uint8_t is_bf = 0;
                    for(uint8_t b = 0; b < bf_count; b++)
                    {
                        if(strcmp(g_items[ch].item_name, breakfast_list[b]) == 0)
                        { is_bf = 1; break; }
                    }
//                    if(is_bf)
//                        UART4_SendString(".txt=\"\xD4\xAA/\xB8\xF6\"");
//                    else
//                        UART4_SendString(".txt=\"\xD4\xAA/\xBD\xEF\"");

                    if(is_bf)
                        UART4_SendString(".txt=\"\xD4\xAA/\xB8\xF6\"");    // 元/个 不变
                    else
                        UART4_SendString(".txt=\"\xD4\xAA/500g\"");         // 元/500g

                }
                else
                {
                    UART4_SendString(".txt=\" \"");
                }
                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
                Delay_Ms(30);
            }
        }

        /* ---- 总价 → List.x8 ---- */
        {
            uint16_t total = CalculateTotalPrice();

            if(total > 0) g_total_price = total;

            UART4_SendString("List.x8.val=");
            sprintf(buf, "%d", total);
            UART4_SendString(buf);
            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
            Delay_Ms(50);
        }

        Delay_Ms(100);
}




/***************************************************************************************************
 * ===========================================
 * 第八部分: 串扰补偿函数
 * ===========================================
 ***************************************************************************************************/
void CompensateCrosstalkSimple(float weight[4])
{
    int active_count = 0;
    int main_ch = -1;
    float sum_small = 0;

    /* 找出实际使用的通道（重量 > 200g 视为活跃通道） */
    for(int i = 0; i < 4; i++)
    {
        if(weight[i] >= 200.0f)
        {
            active_count++;
            main_ch = i;
        }
    }

    /* 只有1个通道活跃时才进行补偿 */
    if(active_count == 1)
    {
        for(int i = 0; i < 4; i++)
        {
            if(i != main_ch && weight[i] < 40.0f && weight[i] > 0)
            {
                sum_small += weight[i];
                weight[i] = 0;
            }
        }
        weight[main_ch] += sum_small;
    }
}

/***************************************************************************************************
 * ===========================================
 * 第九部分: 中断服务函数
 * ===========================================
 ***************************************************************************************************/
void TIM3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void TIM3_IRQHandler(void)
{
    if(TIM_GetITStatus(TIM3, TIM_IT_Update) != RESET)
    {
        Timer_2s++;
        g_tick++;

        if(Timer_2s >= 2)
        {
            Timer_2s = 0;
            Uart_Flag = 1;
        }
    }
    TIM_ClearITPendingBit(TIM3, TIM_IT_Update);
}

void USART3_IRQHandler(void) __attribute__((interrupt("WCH-Interrupt-fast")));
void USART3_IRQHandler(void)
{
    uint8_t data;

    if(USART_GetITStatus(USART3, USART_IT_RXNE) != RESET)
    {
        data = USART_ReceiveData(USART3);

        /* 检测JSON帧开始 */
        if(data == '{')
        {
            uart3_rx_index = 0;
            in_json_frame = 1;
        }

        /* 存入缓冲区 */
        if(in_json_frame && uart3_rx_index < RX_BUFFER_SIZE - 1)
        {
            uart3_rx_buffer[uart3_rx_index++] = data;
        }

        /* 检测JSON帧结束(\n) */
//        if(data == '\n' && in_json_frame)
//        {
//            uart3_rx_buffer[uart3_rx_index] = '\0';
//            uart3_rx_complete = 1;
//            in_json_frame = 0;
//        }

        /* 检测JSON帧结束(\n) */
        if(data == '\n' && in_json_frame)
        {
            uart3_rx_buffer[uart3_rx_index] = '\0';
            in_json_frame = 0;
            Parse_K230_JSON((char*)uart3_rx_buffer);
            uart3_rx_index = 0;
        }

        /* 防止缓冲区溢出 */
        if(uart3_rx_index >= RX_BUFFER_SIZE - 1)
        {
            uart3_rx_index = 0;
            in_json_frame = 0;
        }
    }
}


void UART8_IRQHandler(void)__attribute__((interrupt("WCH-Interrupt-fast")));
void UART8_IRQHandler(void)
{
  if (USART_GetITStatus(UART8, USART_IT_RXNE) != RESET)
  {
      uint8_t data = (uint8_t)(USART_ReceiveData(UART8) &
0xFF);
      Cloud_FeedRXByte(data);
  }
}

/***************************************************************************************************
 * ===========================================
 * 第十部分: UART初始化函数
 * ===========================================
 ***************************************************************************************************/
//void UART4_Init(void)
//{
//    GPIO_InitTypeDef GPIO_InitStructure;
//    USART_InitTypeDef USART_InitStructure;
//
//    RCC_APB1PeriphClockCmd(RCC_APB1Periph_UART4, ENABLE);
//    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
//
//    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
//    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
//    GPIO_Init(GPIOC, &GPIO_InitStructure);
//
//    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
//    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
//    GPIO_Init(GPIOC, &GPIO_InitStructure);
//
//    USART_InitStructure.USART_BaudRate = 115200;
//    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
//    USART_InitStructure.USART_StopBits = USART_StopBits_1;
//    USART_InitStructure.USART_Parity = USART_Parity_No;
//    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
//    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
//    USART_Init(UART4, &USART_InitStructure);
//
//    USART_Cmd(UART4, ENABLE);
//
//    printf("[INIT] UART4初始化完成 (PC10/TX, PC11/RX, 115200)\r\n");
//}

void UART3_Init_For_K230(void)
{
    GPIO_InitTypeDef GPIO_InitStructure;
    USART_InitTypeDef USART_InitStructure;
    NVIC_InitTypeDef NVIC_InitStructure;

    RCC_APB1PeriphClockCmd(RCC_APB1Periph_USART3, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOB, ENABLE);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_10;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
    GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_11;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IN_FLOATING;
    GPIO_Init(GPIOB, &GPIO_InitStructure);

    USART_InitStructure.USART_BaudRate = 115200;
    USART_InitStructure.USART_WordLength = USART_WordLength_8b;
    USART_InitStructure.USART_StopBits = USART_StopBits_1;
    USART_InitStructure.USART_Parity = USART_Parity_No;
    USART_InitStructure.USART_HardwareFlowControl = USART_HardwareFlowControl_None;
    USART_InitStructure.USART_Mode = USART_Mode_Rx | USART_Mode_Tx;
    USART_Init(USART3, &USART_InitStructure);

    USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);

    NVIC_InitStructure.NVIC_IRQChannel = USART3_IRQn;
    NVIC_InitStructure.NVIC_IRQChannelPreemptionPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelSubPriority = 1;
    NVIC_InitStructure.NVIC_IRQChannelCmd = ENABLE;
    NVIC_Init(&NVIC_InitStructure);

    USART_Cmd(USART3, ENABLE);

    printf("[INIT] UART3初始化完成 (PB10/TX, PB11/RX, 115200, 中断已启用)\r\n");
}

//void UART4_SendByte(uint8_t data)
//{
//    while(USART_GetFlagStatus(UART4, USART_FLAG_TXE) == RESET);
//    USART_SendData(UART4, data);
//}
//
//void UART4_SendString(char *str)
//{
//    while(*str)
//    {
//        UART4_SendByte(*str++);
//    }
//}

/***************************************************************************************************
 * ===========================================
 * 第十一部分: 辅助函数
 * ===========================================
 ***************************************************************************************************/
void Print_Main_Header(void)
{
    printf("\r\n\r\n");
    printf("=============================================================\r\n");
    printf("     四通道称重 + K230位置识别匹配系统 V4.0            \r\n");
    printf("     MCU: CH32V307  时钟: %dMHz                        \r\n", SystemCoreClock / 1000000);
    printf("=============================================================\r\n");
    printf("  通道映射:                                              \r\n");
    printf("    通道1 <-> top_left (左上)                            \r\n");
    printf("    通道2 <-> top_right (右上)                           \r\n");
    printf("    通道3 <-> bottom_left (左下)                         \r\n");
    printf("    通道4 <-> bottom_right (右下)                        \r\n");
    printf("=============================================================\r\n");
    printf("\r\n");
}

void USART1_SendString(char *str)
{
    while (*str)
    {
        while (USART_GetFlagStatus(USART1, USART_FLAG_TXE) == RESET);
        USART_SendData(USART1, (uint8_t)*str++);
    }
}



//uint16_t CalculateTotalPrice(void)
//{
//    uint16_t total = 0;  // 单位：元（price 表存的是 0.1元，所以除以10）
//
//    for(int i = 0; i < 4; i++)
//    {
//        if(g_items[i].detected)
//        {
//            for(uint8_t p = 0; p < g_price_count; p++)
//            {
//                if(strcmp(g_items[i].item_name, g_price_table[p].name_en) == 0)
//                {
//                    total += g_price_table[p].price / 10;  // 0.1元 → 元
//                    break;
//                }
//            }
//        }
//    }
//
////    if(total == 0) total = 10;  // 没匹配到任何价格时，兜底扣10元
//    return total;
//}



uint16_t CalculateTotalPrice(void)
{
    uint16_t total = 0;  // 单位：0.1元

    /* 早餐列表（英文名，按个卖，不乘重量） */
    const char *breakfast[] = {"steamed_bun", "egg", "pure_milk", "shaomai"};
    uint8_t bf_count = 4;

    for(int i = 0; i < 4; i++)
    {
        if(!g_items[i].detected) continue;

        for(uint8_t p = 0; p < g_price_count; p++)
        {
            if(strcmp(g_items[i].item_name, g_price_table[p].name_en) == 0)
            {
                uint8_t is_breakfast = 0;
                for(uint8_t b = 0; b < bf_count; b++)
                {
                    if(strcmp(g_items[i].item_name, breakfast[b]) == 0)
                    { is_breakfast = 1; break; }
                }

                if(is_breakfast)
                {
                    total += g_price_table[p].price;   // 早餐：直接加单价
                }
                else
                {
                    // 菜品：单价(0.1元) × 重量(g) / 500 (按斤计价)
                    total += (uint16_t)(g_price_table[p].price * g_weights[i].weight / 500);
                }
                break;
            }
        }
    }
    return total;
}



/***************************************************************************************************
 * 函数名: Send_Price_To_Screen
 * 功能: 发送菜单价格到串口屏 Price1/Price2 页面
 ***************************************************************************************************/
//void Send_Price_To_Screen(void)
//{
//    char buf[80];
//    uint8_t i;
//    const char *cn;
//
//    delay_ms(2000);
//
//    /* ---- Price1 页面（前6个） ---- */
//    UART4_SendString("page Price1");
//    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//    Delay_Ms(200);
//
//    for(i = 0; i < g_price_count && i < 6; i++)
//    {
//        cn = Get_Chinese_Name(g_price_table[i].name_en);
//
//        /* 名称 → Price1.t1 ~ Price1.t6 */
//        sprintf(buf, "Price1.t%d.txt=\"%s\"", i + 1, cn);
//        UART4_SendString(buf);
//        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//        Delay_Ms(30);
//
//        /* 单价 → Price1.x0 ~ Price1.x5 */
//        sprintf(buf, "Price1.x%d.val=%d", i, g_price_table[i].price);
//        UART4_SendString(buf);
//        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//        Delay_Ms(30);
//    }
//
//    /* ---- Price2 页面（第7个起） ---- */
//    if(g_price_count > 6)
//    {
//        UART4_SendString("page Price2");
//        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//        Delay_Ms(200);
//
//        for(i = 6; i < g_price_count && i < 12; i++)
//        {
//            cn = Get_Chinese_Name(g_price_table[i].name_en);
//
//            sprintf(buf, "Price2.t%d.txt=\"%s\"", i - 5, cn);
//            UART4_SendString(buf);
//            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//            Delay_Ms(30);
//
//            sprintf(buf, "Price2.x%d.val=%d", i - 6, g_price_table[i].price);
//            UART4_SendString(buf);
//            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//            Delay_Ms(30);
//        }
//    }
//
//    UART4_SendString("page Home");
//    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//}

//void Send_Price_To_Screen(void)
//{
//    char buf[30];
//    uint8_t i;
//    const char *cn;
////
////    delay_ms(2000);
//
//
//    Delay_Ms(10);
//    UART4_SendFrame("Price1.t2.txt=\"西红柿炒鸡蛋\"\xff\xff\xff");
//    Delay_Ms(100);
//    UART4_SendFrame("Price1.x0.val=133\xff\xff\xff");
//    Delay_Ms(100);
//
//    /* 先不切回 Home，停在 Price1 看效果 */
////    UART4_SendString("page Home\xff\xff\xff");
//    Delay_Ms(10000);   // 停10秒
//
//
////    /* ---- Price1 ---- */
////    UART4_SendString("page Price1");
////    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////    Delay_Ms(200);
////
////    for(i = 0; i < g_price_count && i < 6; i++)
////    {
////        cn = Get_Chinese_Name(g_price_table[i].name_en);
////
////        /* 名称：三段发 */
////        sprintf(buf, "Price1.t%d.txt=\"", i + 1);
////        UART4_SendString(buf);
////        UART4_SendString(cn);
////        UART4_SendString("\"");
////        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////        Delay_Ms(50);
////
////        /* 单价 */
////        sprintf(buf, "Price1.x%d.val=", i);
////        UART4_SendString(buf);
////        sprintf(buf, "%d", g_price_table[i].price);
////        UART4_SendString(buf);
////        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////        Delay_Ms(50);
////    }
////
////    /* ---- Price2 ---- */
////    if(g_price_count > 6)
////    {
////        UART4_SendString("page Price2");
////        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////        Delay_Ms(200);
////
////        for(i = 6; i < g_price_count && i < 12; i++)
////        {
////            cn = Get_Chinese_Name(g_price_table[i].name_en);
////
////            sprintf(buf, "Price2.t%d.txt=\"", i - 5);
////            UART4_SendString(buf);
////            UART4_SendString(cn);
////            UART4_SendString("\"");
////            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////            Delay_Ms(50);
////
////            sprintf(buf, "Price2.x%d.val=", i - 6);
////            UART4_SendString(buf);
////            sprintf(buf, "%d", g_price_table[i].price);
////            UART4_SendString(buf);
////            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////            Delay_Ms(50);
////        }
////    }
////
////    UART4_SendString("page Home");
////    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//}
//
////void Test_Price1(void)
////{
////    delay_ms(3000);
////
////    /* 切到 Price1 */
////    UART4_SendString("page Price1");
////    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////    Delay_Ms(500);
////
////    /* 防丢垫一条 */
////    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////    Delay_Ms(10);
////
////    /* 名称 */
////    UART4_SendString("Price1.t1.txt=\"馒头\"");
////    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////    Delay_Ms(30);
////
////    /* 单价 */
////    UART4_SendString("Price1.x0.val=15");
////    UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
////}


//void Send_Price_To_Screen(void)
//{
//    UART4_SendFrame("page Price1\xff\xff\xff");
//    Delay_Ms(300);
//
//    UART4_SendFrame("Price1.t1.txt=\"西红柿炒鸡蛋\"\xff\xff\xff");
//    Delay_Ms(100);
//    UART4_SendFrame("Price1.x0.val=133\xff\xff\xff");
//    Delay_Ms(100);
//}

//void Send_Price_To_Screen(void)
//{
//    char buf[120];
//    uint8_t i;
//    const char *cn;
//
//    UART4_SendFrame("page Price1\xff\xff\xff");
//    Delay_Ms(300);
//
////    for(i = 0; i < g_price_count && i < 6; i++)
////    {
////        cn = Get_Chinese_Name(g_price_table[i].name_en);
////
////        /* 名称 */
////        sprintf(buf, "Price1.t%d.txt=\"", i + 1);
////        UART4_SendString(buf);
////        UART4_SendString(cn);               // GBK，不乱码
////        UART4_SendString("\"\xff\xff\xff");
////        Delay_Ms(80);
////
////        /* 单价 */
////        sprintf(buf, "Price1.x%d.val=%d\xff\xff\xff", i, g_price_table[i].price);
////        UART4_SendString(buf);
////        Delay_Ms(80);
////    }
//
//    for(i = 4; i < g_price_count && i < 10; i++)    // ← 从4开始，跳过早餐
//    {
//        cn = Get_Chinese_Name(g_price_table[i].name_en);
//
//        sprintf(buf, "Price1.t%d.txt=\"", i - 3);    // ← i-3 → t1,t2...
//        UART4_SendString(buf);
//        UART4_SendString(cn);
//        UART4_SendString("\"\xff\xff\xff");
//        Delay_Ms(80);
//
//        sprintf(buf, "Price1.x%d.val=%d\xff\xff\xff", i - 4, g_price_table[i].price);
//        UART4_SendString(buf);
//        Delay_Ms(80);
//    }
//
//    if(g_price_count > 6)
//    {
//        UART4_SendFrame("page Price2\xff\xff\xff");
//        Delay_Ms(300);
//
//        for(i = 6; i < g_price_count && i < 12; i++)
//        {
//            cn = Get_Chinese_Name(g_price_table[i].name_en);
//
//            sprintf(buf, "Price2.t%d.txt=\"", i - 5);
//            UART4_SendString(buf);
//            UART4_SendString(cn);
//            UART4_SendString("\"\xff\xff\xff");
//            Delay_Ms(80);
//
//            sprintf(buf, "Price2.x%d.val=%d\xff\xff\xff", i - 6, g_price_table[i].price);
//            UART4_SendString(buf);
//            Delay_Ms(80);
//        }
//    }
//}

void Send_Price_To_Screen(void)
{
    char buf[120];
    uint8_t i;
    const char *cn;

    /* ---- Price1（菜品 1~6） ---- */
//    UART4_SendFrame("page Price1\xff\xff\xff");
//    Delay_Ms(300);

    for(i = 10; i < 16; i++)    // 第11~16个 = 菜品1~6
    {
        cn = Get_Chinese_Name(g_price_table[i].name_en);

        sprintf(buf, "Price1.t%d.txt=\"", i - 9);
        UART4_SendString(buf);
        UART4_SendString(cn);
        UART4_SendString("\"\xff\xff\xff");
//        Delay_Ms(30);

        sprintf(buf, "Price1.x%d.val=%d\xff\xff\xff", i - 10, g_price_table[i].price);
        UART4_SendString(buf);
//        Delay_Ms(30);
    }

    /* ---- Price2（菜品 7~12） ---- */
//    UART4_SendFrame("page Price2\xff\xff\xff");
//    Delay_Ms(300);

    for(i = 16; i < 22; i++)    // 第17~22个 = 菜品7~12
    {
        cn = Get_Chinese_Name(g_price_table[i].name_en);

        sprintf(buf, "Price2.t%d.txt=\"", i - 15);
        UART4_SendString(buf);
        UART4_SendString(cn);
        UART4_SendString("\"\xff\xff\xff");
//        Delay_Ms(30);

        sprintf(buf, "Price2.x%d.val=%d\xff\xff\xff", i - 16, g_price_table[i].price);
        UART4_SendString(buf);
//        Delay_Ms(30);
    }
}

//void Send_Breakfast_To_Screen(void)
//{
//    char buf[120];
//    uint8_t i;
//    const char *cn;
//
//    UART4_SendFrame("page Breakfast\xff\xff\xff");
//    Delay_Ms(300);
//
//    for(i = 0; i < 4; i++)          // ← 只前4个
//    {
//        cn = Get_Chinese_Name(g_price_table[i].name_en);
//
//        sprintf(buf, "Breakfast.t%d.txt=\"", i + 1);
//        UART4_SendString(buf);
//        UART4_SendString(cn);
//        UART4_SendString("\"\xff\xff\xff");
//        Delay_Ms(80);
//
//        sprintf(buf, "Breakfast.x%d.val=%d\xff\xff\xff", i, g_price_table[i].price);
//        UART4_SendString(buf);
//        Delay_Ms(80);
//    }
//}


void Send_Breakfast_To_Screen(void)
{
    char buf[120];
    uint8_t i;
    const char *cn;

//    UART4_SendFrame("page Breakfast\xff\xff\xff");
//    Delay_Ms(300);

    for(i = 0; i < 10 && i < g_price_count; i++)
    {
        cn = Get_Chinese_Name(g_price_table[i].name_en);

        /* 名称 → Breakfast.t1 ~ Breakfast.t4 */
        sprintf(buf, "Breakfast.t%d.txt=\"", i + 1);
        UART4_SendString(buf);
        UART4_SendString(cn);
        UART4_SendString("\"\xff\xff\xff");
//        Delay_Ms(30);

        /* 单价 → Breakfast.x1 ~ Breakfast.x4 */
        sprintf(buf, "Breakfast.x%d.val=%d\xff\xff\xff", i + 1, g_price_table[i].price);
        UART4_SendString(buf);
//        Delay_Ms(30);
    }
}



/***************************************************************************************************
 * ===========================================
 * 第十二部分: 主函数
 * ===========================================
 ***************************************************************************************************/
int main(void)
{
    SystemInit();
    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
    SystemCoreClockUpdate();
    Delay_Init();
//    USART_Printf_Init(115200);
//    UART4_Init(115200);

    /* 系统初始化 */
//    SystemClock_Config();
//    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);

    /* UART1初始化 */
    USART1_GPIO_Config();
    USART1_Config(115200);

    /* UART4初始化 — 必须在任何printf之前，因为fputc重定向到了UART4 */
    UART4_Init(115200);

    /* 打印启动信息 */
    Print_Main_Header();

    /* 初始化数据结构 */
    Initialize_Data_Structures();
    printf("[INIT] 数据结构初始化完成\r\n");

    /* UART3初始化 */
    UART3_Init_For_K230();

    /* UART5初始化 */
    UART5_Init(115200);
//    UART5_TX_Init(115200);

    /* 配置DOUT引脚 */
    GPIO_InitTypeDef GPIO_InitStructure;
    RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);
    GPIO_InitStructure.GPIO_Pin = GPIO_Pin_1;
    GPIO_InitStructure.GPIO_Mode = GPIO_Mode_IPU;
    GPIO_Init(GPIOA, &GPIO_InitStructure);

    uint8_t dout_state = GPIO_ReadInputDataBit(GPIOA, GPIO_Pin_1);
    printf("[INFO] DOUT(PA1)当前电平: %s\r\n", dout_state ? "高电平" : "低电平");

    /* 初始化云通信 UART8 */
    CloudProtocol_Init();
    printf("[INIT] UART8 Cloud Protocol initialized\r\n");
    // ==== 测试：立刻发一帧 ====
    uint8_t test[] = "HELLO";
    Cloud_SendFrame(0x12, test, 5);  // 发心跳帧，payload="HELLO"
    printf("[TEST] UART8 test frame sent\r\n");

    /* 初始化ADS1234 */
    ADS1234_GPIO_Config();
    ADS1234_Init();
    Delay_Ms(200);


//    /* 初始化ADS1234 */
//    UART4_SendString("[DBG] before ADS GPIO\r\n");
//    ADS1234_GPIO_Config();
//    UART4_SendString("[DBG] before ADS Init\r\n");
//    ADS1234_Init();
//    UART4_SendString("[DBG] after ADS Init\r\n");

    /* 初始化定时器 */
    Tim3_Init(1000 - 1, 48000 - 1);

    /* 初始化SysTick */

    printf("[INIT] SystemCoreClock=%lu\r\n", SystemCoreClock);

//    SysTick->CTLR = 0;
//    SysTick->SR = 0;
//    SysTick->CNT = 0;
//    SysTick->CMP = SystemCoreClock / 1000 - 1;
//    SysTick->CTLR = 0x00000007;

    /* 初始化SysTick */
    SysTick->CTLR = 0;
    SysTick->SR = 0;
    SysTick->CNT = 0;
    SysTick->CMP = 143999;         // 1ms @ 144MHz，先写死
    SysTick->CTLR = 0x00000007;    // 使能+中断+HCLK时钟


    /* 初始化语音模块 */
    audio_init();
    BusyPin_Init();
    Delay_Ms(100);

    /* ========== RC522初始化 ========== */
    printf("[INIT] Initializing RC522...\r\n");
    RC522_Init();
    Delay_Ms(5);
    RC522_Rese();
    Delay_Ms(5);
    RC522_Config_Type('A');
    Delay_Ms(5);

//    // ========== 增强天线驱动 ==========
//    RC522_Write_Register(TxModeReg, 0x8D);      // 100% 调制
//    RC522_Write_Register(TxControlReg, 0x83);   // 确保 TX1/TX2 使能
//    RC522_Write_Register(0x15, 0x00);       // 不翻转 ASK
//    // ==================================

    // Read version register
    uint8_t version = RC522_Read_Register(VersionReg);

    printf("RC522 VersionReg = 0x%02X\r\n", version);

    char buf[64];
    sprintf(buf, "VersionReg=0x%02X\r\n", version);
    UART4_SendString(buf);

    printf("(Note: 0x82 is acceptable, RC522 will work normally)\r\n");

    // Enable antenna
    RC522_Antenna_On();

    uint8_t tx = RC522_Read_Register(TxControlReg);

    char buf2[64];
    sprintf(buf2,"TxControlReg=0x%02X\r\n",tx);
    UART4_SendString(buf2);

    /* 通过UART4发送RC522初始化信息 */
    UART4_SendString("========================================");
    UART4_SendString("       RC522 RFID System Ready");
    UART4_SendString("========================================");
    UART4_SendString("Please place card on antenna...");

    printf("Antenna ON success\r\n");
    printf("[INIT] RC522 initialization complete\r\n");

    printf("\r\n========== System Parameters ==========\r\n");
    printf("User info sector: %d\r\n", USER_INFO_SECTOR);
    printf("========================================\r\n\r\n");

    printf("Please place the card on the antenna area...\r\n");
    printf("========================================\r\n");
    /* ================================== */

    /* 自动校零 */
    printf("\r\n[INFO] 开始自动校零...\r\n");
    ADS1234_AutoZero();

    /* 设置校准系数 */
    g_scale_factor[0] = 0.0007442f;
    g_scale_factor[1] = 0.00074183f;
    g_scale_factor[2] = 0.00070175f;
    g_scale_factor[3] = 0.0004076640847f;

    printf("\r\n[INFO] 校准系数设置完成\r\n");
    printf("[INFO] 系统初始化完成，开始运行...\r\n\r\n");

//    /* 发送准备指令到屏幕 */
//    UART4_SendString("t1.txt=\"Ready\"\xff\xff\xff");

    /* 主循环 */
    printf("[MAIN] 进入主循环...\r\n");
    printf("        - 称重数据每2秒更新一次\r\n");
    printf("        - K230数据实时接收并匹配\r\n\r\n");





//    // 测试数据：模拟 K230 识别结果
//    test_screen();
    __enable_irq();




    Send_Breakfast_To_Screen();   // 发早餐
    Send_Price_To_Screen();

//    Test_Price1();


//    NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);
//    SystemCoreClockUpdate();
//    Delay_Init();
//    USART_Printf_Init(115200);
//    UART4_Init(115200);

//    UART4_SendString("HELLO");



    while (1)
    {
//        UART4_SendString("[LOOP]\r\n");
//        {
//            char t[32];
//            sprintf(t, "B=%d T=%lu\r\n", uart5_bytes, g_tick);
//
//            if(uart5_bytes > 0)
//            {
//                for(uint16_t i = 0; i < uart5_bytes; i++)
//                {
//                    sprintf(t, "%02X ", UART5_RxBuf[i]);
//                    UART4_SendString(t);
//                }
//                UART4_SendString("\r\n");
//            }
//        }



//        Delay_Ms(10);
//        UART4_SendFrame("Price1.t1.txt=\"西红柿炒鸡蛋\"\xff\xff\xff");
//        Delay_Ms(100);
//        UART4_SendFrame("Price1.x0.val=133\xff\xff\xff");
//        Delay_Ms(100);



        if(uart5_bytes && GetSysTick() - uart5_last_byte >= 20)
        {
            UART5_RxBuf[uart5_bytes] = '\0';
            UART5_RxLen = uart5_bytes;
            UART5_RxFlag = 1;
            uart5_bytes = 0;
        }


        if(UART5_RxFlag)
        {
            UART5_RxFlag = 0;
            uint8_t *buf = (uint8_t*)UART5_RxBuf;
            uint16_t len = UART5_RxLen;


            /* 打印原始数据 hex */
            printf("[UART5_RAW] len=%d: ", UART5_RxLen);
            for(uint16_t i = 0; i < UART5_RxLen && i < 256; i++)
                printf("%02X ", UART5_RxBuf[i]);
            printf("\r\n");

            /* 找分号位置 */
            uint16_t semi = 0;
            for(uint16_t i = 0; i < len; i++) { if(buf[i] == ';') { semi = i; break; } }
            if(semi == 0) goto done_price;

            /* 解析菜名：从帧头0x55 0xAA后面开始 */
            uint16_t pos = 3, cnt = 0;
            char names[10][32];
            while(pos < semi && cnt < 10) {
                uint8_t nl = 0;
                while(pos + nl < semi && buf[pos + nl] != ',') nl++;
                memcpy(names[cnt], &buf[pos], nl); names[cnt][nl] = '\0';
                pos += nl; if(buf[pos] == ',') pos++;
                cnt++;
            }

            /* 解析价格：分号后面，每2字节一个价格（小端） */
//            pos = semi + 1;
//            for(uint8_t pc = 0; pc < cnt && pos + 1 < len; pc++)
//            {
//                uint16_t price = buf[pos] | ((uint16_t)buf[pos+1] << 8);
//                pos += 2;
//
//                /* 用GBK菜名匹配g_item_map的name_cn */
//                g_price_table[g_price_count].price = price;
//                for(uint8_t m = 0; m < ITEM_MAP_COUNT; m++)
//                {
//                    if(strcmp(names[pc], g_item_map[m].name_cn) == 0)
//                    {
//                        strcpy(g_price_table[g_price_count].name_en, g_item_map[m].name_en);
//                        break;
//                    }
//                }
//                g_price_count++;
//            }

            /* 解析价格：每条 0x55 xx xx idx price_lo price_hi */
//            pos = semi + 1;
//            for(uint8_t pc = 0; pc < cnt; pc++)
//            {
//                while(pos < len && buf[pos] != 0x55) pos++;
//                if(pos + 5 >= len) break;
//                uint16_t price = buf[pos + 4] | ((uint16_t)buf[pos + 5] << 8);
//                pos += 6;
//
//                g_price_table[g_price_count].price = price;
//                for(uint8_t m = 0; m < ITEM_MAP_COUNT; m++)
//                {
//                    if(strcmp(names[pc], g_item_map[m].name_cn) == 0)
//                    {
//                        strcpy(g_price_table[g_price_count].name_en, g_item_map[m].name_en);
//                        break;
//                    }
//                }
//                printf("[PRICE] %s -> %d\r\n", g_price_table[g_price_count].name_en, price);
//                g_price_count++;
//            }

//            /* 解析价格: [idx] [price_lo] [price_hi] 00 00 00 [55 AA]... */
//            pos = semi + 1;
//            for(uint8_t pc = 0; pc < cnt; pc++)
//            {
//                if(pos + 2 >= len) break;
//                uint16_t price = buf[pos + 1] | ((uint16_t)buf[pos + 2] << 8);
//                pos += 6;  /* 跳过 idx + price(2) + pad(3) */
//                if(pos + 1 < len && buf[pos] == 0x55 && buf[pos+1] == 0xAA) pos += 2;
//
//                g_price_table[g_price_count].price = price;
//                for(uint8_t m = 0; m < ITEM_MAP_COUNT; m++)
//                {
//                    if(strcmp(names[pc], g_item_map[m].name_cn) == 0)
//                    {
//                        strcpy(g_price_table[g_price_count].name_en, g_item_map[m].name_en);
//                        break;
//                    }
//                }
//                printf("[PRICE] %s -> %d\r\n", g_price_table[g_price_count].name_en, price);
//                g_price_count++;
//            }

            /* 解析价格: 跳过 ; 0D 0A, 然后 55 AA [idx] [price_lo] [price_hi] 00 00 */
            pos = semi + 1;
            if(pos + 1 < len && buf[pos] == 0x0D && buf[pos+1] == 0x0A) pos += 2;

            for(uint8_t pc = 0; pc < cnt; pc++)
            {
                /* 找下一个 55 AA */
                while(pos + 1 < len && (buf[pos] != 0x55 || buf[pos+1] != 0xAA)) pos++;
                if(pos + 5 >= len) break;
                uint16_t price = buf[pos + 3] | ((uint16_t)buf[pos + 4] << 8);
                pos += 6;

                g_price_table[g_price_count].price = price;
                for(uint8_t m = 0; m < ITEM_MAP_COUNT; m++)
                {
                    if(strcmp(names[pc], g_item_map[m].name_cn) == 0)
                    {
                        strcpy(g_price_table[g_price_count].name_en, g_item_map[m].name_en);
                        break;
                    }
                }
                printf("[PRICE] %s -> %d\r\n", g_price_table[g_price_count].name_en, price);
                g_price_count++;
            }


            printf("[PRICE] parsed %d items\r\n", cnt);

        done_price:
            memset(UART5_RxBuf, 0, 512);
        }

//
//        Send_Merged_Data_To_Screen();

//        if(show_success_page && (g_tick - success_page_start >= 2000))
//        {
//            show_success_page = 0;
//            UART4_SendString("\xff\xff\xff");
//            Delay_Ms(100);
//            UART4_SendString("page Home");
//            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//        }


        if(show_success_page && (g_tick - success_page_start >= 2))
        {
            show_success_page = 0;
            audio_play(1);
            UART4_SendString("\xff\xff\xff");
            UART4_SendString("page Home");
            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
        }




//        if(show_success_page && (g_tick - success_page_start >= 2000))
//        {
//            show_success_page = 0;
//            UART4_SendString("\xff\xff\xff");
//            UART4_SendString("page Home");
//            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
//        }

        if(show_fail_page && (g_tick - fail_page_start >= 2))
        {
            show_fail_page = 0;
            UART4_SendString("\xff\xff\xff");
            UART4_SendString("page Home");
            UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
        }

//        if(!rfid_busy)
//            Send_Merged_Data_To_Screen();
//
//
//        /* 发完立刻清空，K230在Process_K230_Data()里重新填充 */
//        for(int i = 0; i < 4; i++)
//        {
//            strcpy(g_items[i].item_name, "");
//            g_items[i].detected = 0;
//            g_items[i].confidence = 0.0f;
//        }
//
//        /* 发完立刻清空，K230在Process_K230_Data()里重新填充 */
//        if(!show_success_page && !show_fail_page)
//        {
//            for(int i = 0; i < 4; i++)
//            {
//                strcpy(g_items[i].item_name, "");
//                g_items[i].detected = 0;
//                g_items[i].confidence = 0.0f;
//            }
//        }
//
//
//
//
////        UART4_SendString("after RFID\r\n");
//
////        Send_Merged_Data_To_Screen();
//
////        UART4_SendAllDishes();
//
//        printf("[TICK] tick=%lu\r\n", g_sys_tick);
//
//        Delay_Ms(1000);
//
//        /* 任务1: 处理K230数据 */
//        Process_K230_Data();


        /* 任务1: 处理K230数据（累积，不清空） */
        Process_K230_Data();
        /* 处理ESP32发来的数据帧 */
        Cloud_ProcessRX();
        Cloud_ProcessPendingRecharges();

        /* 每10秒发一次心跳 */
        static uint32_t last_heartbeat = 0;
        if (g_tick - last_heartbeat >= 10) {
            last_heartbeat = g_tick;
            Cloud_SendHeartbeat();
        }


        // ← 把 RFID 代码粘贴到这里 ↓
        static uint32_t rfid_div = 0;
        rfid_div++;
        if(rfid_div >= 5)
        {
            rfid_div = 0;
            RFID_Payment_Process();
            rfid_busy = 0;
        }

        /* 定期重发菜单数据（每10次循环≈10秒） */
        static uint32_t menu_div = 0;
        menu_div++;
        if(menu_div >= 1)
        {
            menu_div = 0;
            Send_Price_To_Screen();
            Send_Breakfast_To_Screen();
        }


        /* 每2秒发送一次累积的数据到串口屏 */
        static uint32_t last_screen = 0;
        if(!rfid_busy && !show_success_page && !show_fail_page&& g_tick - last_screen >= 1)
        {
            if(g_tick - last_screen >= 2)
            {
                last_screen = g_tick;
                Send_Merged_Data_To_Screen();

                // 保存菜名到静态缓存，供后续 RFID 使用
//                static char saved_dishes[60] = {0};
                int sp = 0;
                memset(g_saved_dishes, 0, 60);
                for(int i = 0; i < 4; i++) {
                    if (g_items[i].detected && strlen(g_items[i].item_name) > 0) {
                      if (sp > 0 && sp < 59) g_saved_dishes[sp++] = ',';
                      int nl = strlen(g_items[i].item_name);
                      if (sp + nl > 59) nl = 59 - sp;
                      memcpy(&g_saved_dishes[sp], g_items[i].item_name, nl);
                      sp += nl;
                  }
              }

                /* 发完后清空，准备下一个2秒窗口 */
                for(int i = 0; i < 4; i++)
                {
                    strcpy(g_items[i].item_name, "");
                    g_items[i].detected = 0;
                    g_items[i].confidence = 0.0f;
                }
            }
        }

        printf("[TICK] tick=%lu\r\n", g_sys_tick);

//        static uint32_t last_uart5_dbg = 0;
//        if(GetSysTick() - last_uart5_dbg >= 1000)
//        {
//            last_uart5_dbg = GetSysTick();
//            printf("[U5DBG] bytes=%d\r\n", uart5_bytes);
//        }

        delay_ms(1000);





        /* 任务2: 称重数据读取（每2秒） */
        if(Uart_Flag == 1)
        {
            Uart_Flag = 0;

            int32_t adc[4];
            float weight[4];

            printf("\r\n========== 开始称重 ==========\r\n");

            /* 读取4个通道 */
            for(int ch = 0; ch < 4; ch++)
            {
                adc[ch] = ADS1234_ReadData((ADS1234_Channel_t)ch);
                weight[ch] = ADS1234_ToWeight((ADS1234_Channel_t)ch, adc[ch]);
            }

            /* 检查读取是否成功 */
            uint8_t all_ok = 1;
            for(int ch = 0; ch < 4; ch++)
            {
                if(adc[ch] == 0)
                {
                    printf("[错误] 通道%d读取失败(ADC=0)!\r\n", ch + 1);
                    all_ok = 0;
                }
            }

            if(all_ok)
            {
                /* 串扰补偿 */
                CompensateCrosstalkSimple(weight);

                /* 存储称重数据 */
                for(int i = 0; i < 4; i++)
                {
                    g_weights[i].weight = weight[i];
                    g_weights[i].valid = 1;
                    g_weights[i].timestamp = GetSysTick();
                }
                g_weights_updated = 1;

                printf("\r\n[WEIGHT] 称重完成: ch1=%.2fg, ch2=%.2fg, ch3=%.2fg, ch4=%.2fg\r\n",
                       weight[0], weight[1], weight[2], weight[3]);
                printf("[WEIGHT] g_weights_updated=1, g_items_updated=%d\r\n", g_items_updated);

//                /* 如果有物品数据，进行合并 */
//                if(g_items_updated)
//                {
////                    printf("[MERGE] 检测到K230物品数据，开始合并...\r\n");
////                    Merge_Data();
////                    Print_Merged_Data();
////                    Send_Merged_Data_To_PC();
////                    Send_Merged_Data_To_Screen();
////                    g_items_updated = 0;   // ★ 关键：用完后立即清零，防止下次重复发送旧数据
//                }



//                /* ---- K230数据超时触发 ---- */
//                if(g_k230_last_time > 0 && (g_tick - g_k230_last_time >= 1))
//                {
//                    /* 500ms没收到新数据，认为一帧结束 */
//                    Send_Merged_Data_To_Screen();
//
//                    /* 清空所有通道，准备下一帧 */
//                    for(int i = 0; i < 4; i++)
//                    {
//                        strcpy(g_items[i].item_name, "");
//                        g_items[i].detected = 0;
//                        g_items[i].confidence = 0.0f;
//                    }
//
//                    g_k230_last_time = 0;  /* 等K230下一帧数据 */
//                }
//                else
//                {
//                    /* 没有物品数据，发送重量到串口屏和PC */
//                    printf("[INFO] 没有K230物品数据，发送纯称重数据\r\n");
//
//                    char screen_buf[80];
//
//                    /* 发送到串口屏 */
//                    for(int i = 0; i < 4; i++)
//                    {
//                        int32_t weight_int = (int32_t)(weight[i] * 10);
//                        sprintf(screen_buf, "Home.t%d.txt=\"%.1fg\"\xff\xff\xff",
//                                i + 1, weight[i]);
//                        UART4_SendString(screen_buf);
//                        Delay_Ms(10);
//                    }
//
//                    float total = weight[0] + weight[1] + weight[2] + weight[3];
//                    sprintf(screen_buf, "Home.t5.txt=\"%.1fg\"\xff\xff\xff", total);
//                    UART4_SendString(screen_buf);

//                else
//                {
                    /* 没有物品数据，发送重量到串口屏 (List.x4~x7，不覆盖Home) */
                    printf("[INFO] 没有K230物品数据，发送纯称重数据\r\n");

                    char screen_buf[80];

                    /* 重量发到 List.x4~x7，和 Send_Merged_Data_To_Screen 保持一致 */
                    sprintf(screen_buf, "List.x4.val=%d\xff\xff\xff", (int)(weight[0] * 10));
                    UART4_SendString(screen_buf);
                    delay_ms(10);
                    sprintf(screen_buf, "List.x5.val=%d\xff\xff\xff", (int)(weight[1] * 10));
                    UART4_SendString(screen_buf);
                    delay_ms(10);
                    sprintf(screen_buf, "List.x6.val=%d\xff\xff\xff", (int)(weight[2] * 10));
                    UART4_SendString(screen_buf);
                    delay_ms(10);
                    sprintf(screen_buf, "List.x7.val=%d\xff\xff\xff", (int)(weight[3] * 10));
                    UART4_SendString(screen_buf);

                    /* 同时发送到PC (UART4) */
                    /* 用整数运算避免浮点 printf 问题 */
//                    int ch1_int = (int)(weight[0] * 100.0f + 0.5f);
//                    int ch2_int = (int)(weight[1] * 100.0f + 0.5f);
//                    int ch3_int = (int)(weight[2] * 100.0f + 0.5f);
//                    int ch4_int = (int)(weight[3] * 100.0f + 0.5f);
//                    int total_int = ch1_int + ch2_int + ch3_int + ch4_int;
//
//                    sprintf(screen_buf,
//                            "{\"weight_only\":{"
//                            "\"ch1\":%d.%02d,\"ch2\":%d.%02d,\"ch3\":%d.%02d,\"ch4\":%d.%02d,"
//                            "\"total\":%d.%02d}}\r\n",
//                            ch1_int/100, ch1_int%100,
//                            ch2_int/100, ch2_int%100,
//                            ch3_int/100, ch3_int%100,
//                            ch4_int/100, ch4_int%100,
//                            total_int/100, total_int%100);
//                    UART4_SendString(screen_buf);
//                    printf("[UART4] 已发送称重数据到PC: %s", screen_buf);
                }
            }
            else
            {
                printf("[错误] 称重读取失败!\r\n");
            }
        }




//    if(all_ok)
//      {
//          /* 串扰补偿 */
//          CompensateCrosstalkSimple(weight);
//
//          /* 存储称重数据 */
//          for(int i = 0; i < 4; i++)
//          {
//              g_weights[i].weight = weight[i];
//              g_weights[i].valid = 1;
//              g_weights[i].timestamp = GetSysTick();
//          }
//          g_weights_updated = 1;
//
//          printf("\r\n[WEIGHT] 称重完成: ch1=%.2fg, ch2=%.2fg, ch3=%.2fg, ch4=%.2fg\r\n",
//                 weight[0], weight[1], weight[2], weight[3]);
//
//          /* 如果有物品数据，进行合并 */
//          if(g_items_updated)
//          {
//              printf("[MERGE] 检测到K230物品数据，开始合并...\r\n");
//              Merge_Data();
//              Print_Merged_Data();
//              Send_Merged_Data_To_PC();
//              Send_Merged_Data_To_Screen();
//              g_items_updated = 0;
//          }
//          else
//          {
//              /* 没有K230数据时，也发送称重数据到串口屏 */
//              printf("[INFO] 发送称重数据到串口屏\r\n");
//
//              char screen_buf[80];
//              for(int i = 0; i < 4; i++)
//              {
//                  sprintf(screen_buf, "Home.t%d.txt=\"%.1fg\"\xff\xff\xff",
//                          i + 2, weight[i]);
//                  UART4_SendFrame(screen_buf);
//                  Delay_Ms(50);
//              }
//
//              float total = weight[0] + weight[1] + weight[2] + weight[3];
//              sprintf(screen_buf, "Home.t6.txt=\"%.1fg\"\xff\xff\xff", total);
//              UART4_SendFrame(screen_buf);
//          }
//      }

        /* 任务3: RC522 RFID刷卡处理（每200ms检测一次） */
//        static uint32_t last_rfid_check = 0;
////        if(g_tick - last_rfid_check >= 200)
////        {
////            last_rfid_check = g_tick;
////            RFID_Payment_Process();
////        }
//        RFID_Payment_Process();

//        static uint32_t rfid_div = 0;
//        rfid_div++;
//        if(rfid_div >= 5)
//        {
//            rfid_div = 0;
//
//            printf("[RFID] calling, tick=%lu\r\n", g_tick);
//
//            RFID_Payment_Process();
//            rfid_busy = 0;
//        }
        /* UART4测试：每5秒发送一次心跳 */
        static uint32_t last_uart4_test = 0;
        if(g_tick - last_uart4_test >= 5000)
        {
            last_uart4_test = g_tick;
            UART4_SendString("[UART4] Heartbeat - System running");
        }
        Delay_Ms(100);
    }
//   }

/***************************************************************************************************
 * ===========================================
 * RC522 RFID刷卡处理函数
 * ===========================================
 */

/* 全局变量 */
uint8_t card_is_activated = 0;
uint8_t user_state = 0;
// 全局变量（在函数外定义）

//void RFID_Payment_Process(void)
//{
//    UserData_t user_info;
//    uint8_t uid[4];
//    uint8_t tagType[2];
//
//    int select_ok = 0;
//    static int init_attempt = 0;
//    static uint8_t last_uid[4] = {0};
//    static int admin_processing = 0;
//	    static int call_count = 0;
//
//	    /* 调试：每50次调用输出一次心跳信息 */
//	    call_count++;
//	    if(call_count >= 50)  /* 每10秒 */
//	    {
//	        call_count = 0;
//	        UART4_SendString("[RFID] Scanning for card...");
//	    }
//        /* 每10秒读取RC522寄存器状态 */
//        static int reg_check_count = 0;
//        reg_check_count++;
//        if(reg_check_count >= 50)  /* 每10秒 */
//        {
//            reg_check_count = 0;
//            char reg_buf[64];
//            uint8_t tx_ctrl = RC522_Read_Register(0x14);  /* TxControlReg */
//            uint8_t cmd = RC522_Read_Register(0x01);      /* CommandReg */
//            sprintf(reg_buf, "[REG] TxCtrl=0x%02X, Cmd=0x%02X", tx_ctrl, cmd);
//            UART4_SendString(reg_buf);
//        }
//
//
//    /* 发送REQA命令检测卡片 */
////    if(PcdRequest(0x52, tagType) != MI_OK)
////    {
////        init_attempt = 0;
////        admin_processing = 0;
////
////        UART4_SendString("[RFID] PcdRequest failed\r\n");
////
////        return;
////        /* 调试：输出PcdRequest结果 */
////        char req_buf[32];
////        sprintf(req_buf, "[DEBUG] PcdRequest failed");
////        UART4_SendString(req_buf);
////
////    }
//
//        /* 发送REQA命令检测卡片 */
//        if(PcdRequest(0x26, tagType) != MI_OK && PcdRequest(0x52, tagType) != MI_OK)
//        {
//            init_attempt = 0;
//            admin_processing = 0;
//             UART4_SendString("[RFID] No card\r\n");  // 可选，调试时取消注释
//            return;
////            char req_buf[32];
////            sprintf(req_buf, "[DEBUG] PcdRequest failed");
////            UART4_SendString(req_buf);
//        }
//        UART4_SendString("[RFID] Card found, UID: ");
//        // 后续代码...
//
//
//    /* 防冲突检测获取UID */
//    if(PcdAnticoll(uid) != MI_OK)
//    {
//        return;
//    }
//
//    /* 避免重复读取同一张卡片 */
//    if(memcmp(uid, last_uid, 4) == 0 && admin_processing == 0)
//    {
//        return;
//    }
//    memcpy(last_uid, uid, 4);
//
//    /* 发送卡片检测信息到UART4 */
//    UART4_SendString("\r\n====\r\n");
//    UART4_SendString("Card detected, UID: ");
//    char uid_buf[32];
//    sprintf(uid_buf, "%02X-%02X-%02X-%02X\r\n", uid[0], uid[1], uid[2], uid[3]);
//    UART4_SendString(uid_buf);
//
//    /* 选卡 */
//    for(int retry = 0; retry < 3; retry++)
//    {
//        if(PcdSelect(uid) == MI_OK)
//        {
//            select_ok = 1;
//            UART4_SendString("Card selection success\r\n");
//            break;
//        }
//        Delay_Ms(50);
//    }
//
//    if(!select_ok)
//    {
//        UART4_SendString("Card selection failed\r\n");
//        return;
//    }
//
//    /* 尝试读取用户信息 */
//    int read_success = 0;
//    for(int retry = 0; retry < 3; retry++)
//    {
//        if(ReadUserInfoFromCard(USER_INFO_SECTOR, uid, &user_info))
//        {
//            read_success = 1;
//            break;
//        }
//        Delay_Ms(50);
//    }
//
//    if(!read_success)
//    {
//        if(init_attempt == 0)
//        {
//            UART4_SendString("Card not initialized, initializing...\r\n");
//            UserData_t default_user;
//            default_user.user_id = 888888;
//            strcpy((char*)default_user.user_name, "NewUser");
//            default_user.user_level = 1;
//            default_user.department = 100;
//            memset(default_user.reserved, 0, 5);
//            InitNewCard(uid, &default_user);
//            init_attempt = 1;
//            UART4_SendString("Initialization complete, please place the card again\r\n");
//        }
//        PcdHalt();
//        return;
//    }
//
//    init_attempt = 0;
//    DisplayUserInfo(&user_info);
//    PcdHalt();
//}


//void RFID_Payment_Process(void)
//{
//    UserData_t user_info;
//    uint8_t uid[4];
//    uint8_t tagType[2];
//    int select_ok = 0;
//    static int init_attempt = 0;
//    static uint8_t last_uid[4] = {0};
//    static int admin_processing = 0;
//    static int call_count = 0;
//    static int reset_counter = 0;
//
//    // 每10次调用复位一次RC522
//    reset_counter++;
//    if(reset_counter > 10) {
//        RC522_Rese();
//        Delay_Ms(50);
//        RC522_Antenna_On();
//        reset_counter = 0;
//    }
//
////    call_count++;
////    if(call_count >= 50) {
////        call_count = 0;
////        UART4_SendString("[RFID] Scanning...\r\n");
////        // 打印关键寄存器
////        char reg_buf[64];
////        uint8_t tx_ctrl = RC522_Read_Register(0x14);  // TxControlReg
////        uint8_t cmd = RC522_Read_Register(0x01);      // CommandReg
////        uint8_t com_irq = RC522_Read_Register(0x04);  // ComIrqReg
////        uint8_t err_reg = RC522_Read_Register(0x06);  // ErrorReg
////        sprintf(reg_buf, "[REG] TxCtrl=0x%02X, Cmd=0x%02X, ComIrq=0x%02X, Err=0x%02X\r\n",
////                tx_ctrl, cmd, com_irq, err_reg);
////        UART4_SendString(reg_buf);
////    }
//
//    // 先尝试 0x26，再尝试 0x52
////    int r1 = PcdRequest(0x26, tagType);
////    int r2 = PcdRequest(0x52, tagType);
////
////    char buf3[64];
////
////    sprintf(buf3,
////            "REQA=%d WUPA=%d TAG=%02X %02X\r\n",
////            r1,
////            r2,
////            tagType[0],
////            tagType[1]);
////
////    UART4_SendString(buf3);
////
////    if(r1 != MI_OK && r2 != MI_OK)
////    {
////        UART4_SendString("[RFID] No card detected\r\n");
////        return;
////    }
////
////    UART4_SendString("[RFID] Card found!\r\n");
//    if(PcdRequest(0x52, tagType) != MI_OK)
//        {
//            // 无卡，直接返回
//            return;
//        }
//    // 打印寻卡成功信息
//   char buf[64];
//   sprintf(buf, "[RFID] Card found, type=0x%02X%02X\r\n", tagType[0], tagType[1]);
//   UART4_SendString(buf);
//
//   // 防冲突获取UID
//   if(PcdAnticoll(uid) != MI_OK)
//   {
//       UART4_SendString("[RFID] Anticoll failed\r\n");
//       return;
//   }
//
//   // 打印UID
//   sprintf(buf, "[RFID] UID: %02X-%02X-%02X-%02X\r\n", uid[0], uid[1], uid[2], uid[3]);
//   UART4_SendString(buf);
//
//    // 防冲突
////    if(PcdAnticoll(uid) != MI_OK) {
////        UART4_SendString("[RFID] Anticoll failed\r\n");
////        return;
////    }
//
////    char anti_ret;
////
////    anti_ret = PcdAnticoll(uid);
////
////    char dbg[128];
////
////    sprintf(dbg,
////            "[RFID] Anticoll ret=0x%02X\r\n",
////            (unsigned char)anti_ret);
////
////    UART4_SendString(dbg);
////
////    if(anti_ret != MI_OK)
////    {
////        UART4_SendString("[RFID] Anticoll failed\r\n");
////        return;
////    }
//
//
////    // 去重
////    if(memcmp(uid, last_uid, 4) == 0 && admin_processing == 0) {
////        UART4_SendString("[RFID] Same card ignored\r\n");
////        return;
////    }
////    memcpy(last_uid, uid, 4);
//
//    // 去重
//    static uint32_t last_uid_time = 0;
//    if(memcmp(uid, last_uid, 4) == 0 && (GetSysTick() - last_uid_time) < 2000)  // 2秒内忽略
//    {
//        UART4_SendString("[RFID] Same card ignored (within 2s)\r\n");
//        return;
//    }
//    memcpy(last_uid, uid, 4);
//    last_uid_time = GetSysTick();
//
//    UART4_SendString("\r\n==== Card detected ====\r\n");
//    char uid_buf[32];
//    sprintf(uid_buf, "UID: %02X-%02X-%02X-%02X\r\n", uid[0], uid[1], uid[2], uid[3]);
//    UART4_SendString(uid_buf);
//
//    // 选卡
//    for(int retry = 0; retry < 3; retry++) {
//        if(PcdSelect(uid) == MI_OK) {
//            select_ok = 1;
//            UART4_SendString("Select success\r\n");
//            break;
//        }
//        Delay_Ms(50);
//    }
//    if(!select_ok) {
//        UART4_SendString("Select failed\r\n");
//        return;
//    }
//
//    // 读取用户信息（假设已实现真正的读写函数）
//    int read_success = 0;
//    for(int retry = 0; retry < 3; retry++) {
//        if(ReadUserInfoFromCard(USER_INFO_SECTOR, uid, &user_info)) {
//            read_success = 1;
//            break;
//        }
//        Delay_Ms(50);
//    }
//
//    if(!read_success) {
//        if(init_attempt == 0) {
//            UART4_SendString("Card not initialized, initializing...\r\n");
//            UserData_t default_user;
//            default_user.user_id = 888888;
//            strcpy((char*)default_user.user_name, "NewUser");
//            default_user.user_level = 1;
//            default_user.department = 100;
//            memset(default_user.reserved, 0, 5);
//            InitNewCard(uid, &default_user);
//            init_attempt = 1;
//            UART4_SendString("Initialization complete, please place the card again\r\n");
//        }
//        PcdHalt();
//        return;
//    }
//
//    init_attempt = 0;
//    DisplayUserInfo(&user_info);
//    PcdHalt();
//
//
//}


/* 刷卡支付函数 - 支持管理员给普通用户充值 */
void RFID_Payment_Process(void)
{
    UserData_t user_info;           // 用户信息结构体
    uint8_t uid[4];                 // 卡片UID存储数组
    uint8_t tagType[2];             // 卡片类型存储数组
    int select_ok = 0;              // 选卡成功标志
    static int init_attempt = 0;    // 初始化尝试计数（防止重复初始化）
    static uint8_t last_uid[4] = {0};   // 上一次处理的卡片UID（用于去重）
    static int admin_processing = 0;    // 管理员模式处理标志（防止重复处理）
    char buf[128];                  // 临时缓冲区
    // 播放"请选择支付方式"
      if (!IsAudioBusy()) {
          audio_play(1);
      }
    rfid_busy = 1;

    // ========== 1. 寻卡 ==========
    // 寻卡：搜索天线区域内的卡片
    if (PcdRequest(0x52, tagType) != MI_OK) {
        // 没有检测到卡片，重置相关标志
        init_attempt = 0;
        admin_processing = 0;

        // 管理员模式超时退出（5秒无操作）
        if(system_mode == MODE_ADMIN_READY) {
            static int timeout_cnt = 0;
            timeout_cnt++;
            if(timeout_cnt > 50) {
                UART4_SendString("Admin mode timeout, exiting...\r\n");  // 管理员模式超时，退出
                system_mode = MODE_NORMAL;
                timeout_cnt = 0;
            }
        }
        return;
    }

    // ========== 2. 防冲突（获取 UID）==========
    // 防冲突：当多张卡同时在场时，选出一张卡并获取其UID
    if (PcdAnticoll(uid) != MI_OK) {
        return;
    }

    // 防止重复处理同一张卡（去重）
    if(memcmp(uid, last_uid, 4) == 0 && admin_processing == 0) {
        return;
    }
    memcpy(last_uid, uid, 4);

    // 打印检测到的卡片UID
    sprintf(buf, "\r\nCard detected, UID: %02X-%02X-%02X-%02X\r\n",  // 检测到卡片
           uid[0], uid[1], uid[2], uid[3]);
    UART4_SendString(buf);

    // ========== 3. 选卡 ==========
    // 选卡：选中当前卡片，准备后续操作
    for(int retry = 0; retry < 3; retry++)
    {
        if(PcdSelect(uid) == MI_OK) {
            select_ok = 1;
            UART4_SendString("Card selection success\r\n");  // 选卡成功
            break;
        }
        sprintf(buf, "Card selection retry %d/3...\r\n", retry + 1);  // 选卡重试
        UART4_SendString(buf);
        Delay_Ms(50);
    }

    if(!select_ok) {
        UART4_SendString("Card selection failed\r\n");  // 选卡失败
        return;
    }

    // ========== 管理员模式处理（等待目标卡） ==========
    if(system_mode == MODE_ADMIN_READY) {
        admin_processing = 1;

        // 读取当前卡片的用户信息，确认是普通用户卡
        if (!ReadUserInfoFromCard(USER_INFO_SECTOR, uid, &user_info))
        {
            UART4_SendString("Failed to read target card info!\r\n");  // 读取目标卡片信息失败
            system_mode = MODE_NORMAL;
            admin_processing = 0;
            PcdHalt();
            return;
        }

        // 当前卡是目标卡（普通用户卡），进行充值操作
//        if(user_info.user_level == 1) {
//            UART4_SendString("\r\n>>> Target card (Normal User) detected! <<<\r\n");  // 检测到目标卡（普通用户）
//            UART4_SendString(">>> Recharging 100 yuan to this card... <<<\r\n");      // 正在给本卡充值100元
//
//            // 直接充值（当前卡已经选中）
//            uint32_t current_balance;
//            if(PcdAuthState(0x60, BLOCK_BALANCE, Default_Key, uid) == MI_OK)
//            {
//                if(ReadBalance(BLOCK_BALANCE, &current_balance)) {
//                    uint32_t new_balance = current_balance + recharge_amount;

        if(user_info.user_level == 1) {
            UART4_SendString("\r\n>>> Target card (Normal User) detected! <<<\r\n");
            UART4_SendString(">>> Resetting balance to default... <<<\r\n");   // 提示语改一下

            // 重置余额
            uint32_t current_balance;
            if(PcdAuthState(0x60, BLOCK_BALANCE, Default_Key, uid) == MI_OK)
            {
                if(ReadBalance(BLOCK_BALANCE, &current_balance)) {
                    uint32_t new_balance = INIT_BALANCE;   // ← 直接设为初始值，不管原来多少

                    sprintf(buf, "Old balance: %lu yuan\r\n", current_balance);  // 原余额
                    UART4_SendString(buf);

                    if(WriteBalance(BLOCK_BALANCE, new_balance))
//                    if(WriteBalance(BLOCK_BALANCE, new_balance / 100))
                    {
                        UART4_SendString("\r\n========================================\r\n");
                        UART4_SendString(">>> Recharge successful! <<<\r\n");        // >>> 充值成功！<<<
                        sprintf(buf, "  Target card UID: %02X-%02X-%02X-%02X\r\n",  // 目标卡片UID
                               uid[0], uid[1], uid[2], uid[3]);
                        UART4_SendString(buf);
//                        sprintf(buf, "  Recharge amount: %lu yuan\r\n", recharge_amount);  // 充值金额

                        sprintf(buf, "  Balance reset to: %d.%02d yuan\r\n", INIT_BALANCE/100, INIT_BALANCE%100);

                        UART4_SendString(buf);
//                        sprintf(buf, "  New balance: %lu yuan\r\n", new_balance);   // 新余额

                        sprintf(buf, "  New balance: %lu.%02lu yuan\r\n", new_balance/100, new_balance%100);

                        UART4_SendString(buf);
                        UART4_SendString("========================================\r\n");
                    } else {
                        UART4_SendString("Write balance failed!\r\n");               // 写入余额失败
                    }
                } else {
                    UART4_SendString("Read balance failed!\r\n");                    // 读取余额失败
                }
            } else {
                UART4_SendString("Balance sector authentication failed!\r\n");  // 余额扇区验证失败
            }
        } else {
            sprintf(buf, "Target card is not a normal user card! Current level: %d\r\n",
                   user_info.user_level);  // 目标卡不是普通用户卡！当前等级
            UART4_SendString(buf);
        }

        // 退出管理员模式
        system_mode = MODE_NORMAL;
        admin_processing = 0;
        UART4_SendString("\r\nAdmin mode exited.\r\n");  // 管理员模式已退出
        PcdHalt();
        return;
    }

    // ========== 4. 读取用户信息（扇区5）- 带重试机制 ==========
    int read_retry;
    int read_success = 0;

    for(read_retry = 0; read_retry < 3; read_retry++)
    {
        if (ReadUserInfoFromCard(USER_INFO_SECTOR, uid, &user_info))
        {
            read_success = 1;
            break;
        }
        sprintf(buf, "Read user info retry %d/3...\r\n", read_retry + 1);  // 读取用户信息重试
        UART4_SendString(buf);
        Delay_Ms(50);
    }

    if (!read_success)
    {
        if(init_attempt == 0) {
            UART4_SendString("Card not initialized, starting initialization...\r\n");  // 卡片未初始化，开始初始化
            UserData_t default_user;
            default_user.user_id = 888888;
            strcpy((char*)default_user.user_name, "zhangsan");
            default_user.user_level = 1;
            default_user.department = 100;
            memset(default_user.reserved, 0, 5);
            InitNewCard(uid, &default_user);
            init_attempt = 1;
            UART4_SendString("Initialization complete, please place the card again\r\n");  // 初始化完成，请重新放卡
        }
//        PcdHalt();
//        return;

        show_fail_page = 1;
        fail_page_start = g_tick;
        UART4_SendString("\xff\xff\xff");
        UART4_SendString("page Card_Fail");
        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
        goto rfid_done;

    }

    init_attempt = 0;

    // ========== 显示用户信息 ==========
    DisplayUserInfo(&user_info);

    // 上报刷卡事件到ESP32（读余额前先报0，交易后再补完整数据）
    Cloud_ReportCardTap(uid, user_info.user_id,(char*)user_info.user_name,user_info.user_level, 0);
    UART4_SendString("[CLOUD] CardTap frame sent\r\n");

    // ========== 检查是否为管理员卡 ==========
    if(user_info.user_level == 2) {
        UART4_SendString("\r\n>>> Administrator card detected! <<<\r\n");  // 检测到管理员卡
        UART4_SendString(">>> Please place the target card (Normal User) <<<\r\n");  // 请放置目标卡（普通用户卡）
        UART4_SendString(">>> Waiting for target card... <<<\r\n");        // 正在等待目标卡

        // 进入管理员模式，等待目标卡
        system_mode = MODE_ADMIN_READY;
        memcpy(admin_uid, uid, 4);
        admin_processing = 0;

        PcdHalt();
        return;
    }

    // ========== 普通用户扣款流程 ==========
    // 验证余额扇区
//    if(PcdAuthState(0x60, BLOCK_BALANCE, Default_Key, uid) != MI_OK)
//    {
//        UART4_SendString("Balance sector authentication failed!\r\n");  // 余额扇区验证失败
//        PcdHalt();
//        return;
//    }

    if(PcdAuthState(0x60, BLOCK_BALANCE, Default_Key, uid) != MI_OK)
    {
        UART4_SendString("Balance sector authentication failed!\r\n");
        show_fail_page = 1;
        fail_page_start = g_tick;
        UART4_SendString("\xff\xff\xff");
        UART4_SendString("page Card_Fail");
        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
        goto rfid_done;
    }


    // 读取余额并扣款
    uint32_t current_balance;
    if(ReadBalance(BLOCK_BALANCE, &current_balance))
    {
        // 检查云端待处理充值
        uint32_t cloud_recharge = Cloud_GetPendingRecharge(uid);
        if (cloud_recharge > 0) {
            current_balance += cloud_recharge;
            WriteBalance(BLOCK_BALANCE, current_balance);
            sprintf(buf, "[CLOUD] Recharge %lu, new balance%lu\r\n", cloud_recharge, current_balance);
            UART4_SendString(buf);
            Cloud_ReportRechargeComplete(uid, cloud_recharge,current_balance);
        }
//        current_balance *= 100;   // ← 加这行
        sprintf(buf, "User: %s\r\n", user_info.user_name);  // 用户
        UART4_SendString(buf);
//        sprintf(buf, "Current balance: %lu yuan\r\n", current_balance);  // 当前余额
//        UART4_SendString(buf);
//
//        if(current_balance >= price)
//        {
//             uint32_t new_balance = current_balance - price;
//        int total_price = CalculateTotalPrice();  // ← 动态计算总价

        int total_price = g_total_price;

//        int total_price = g_total_price * 10;
//        int total_price = (g_total_price + 50) / 100;
//        sprintf(buf, "Total price: %d yuan\r\n", total_price);

        sprintf(buf, "Total price: %d.%02d yuan\r\n", total_price/100, total_price%100);

        UART4_SendString(buf);
//        sprintf(buf, "Current balance: %lu yuan\r\n", current_balance);

        sprintf(buf, "Current balance: %lu.%02lu yuan\r\n", current_balance/100, current_balance%100);

        UART4_SendString(buf);

        if(current_balance >= total_price)
        {
            uint32_t new_balance = current_balance - total_price;
            if(WriteBalance(BLOCK_BALANCE, new_balance))
//            if(WriteBalance(BLOCK_BALANCE, new_balance / 100))
            {
//                sprintf(buf, "Deduction successful! New balance: %lu yuan\r\n", new_balance);  // 扣款成功！新余额

                sprintf(buf, "Deduction successful! New balance: %lu.%02lu yuan\r\n", new_balance/100, new_balance%100);

                UART4_SendString(buf);
//                sprintf(buf, "Transaction record: %s spent %d yuan\r\n",  // 消费记录
//                       user_info.user_name, total_price);

                sprintf(buf, "Transaction record: %s spent %d.%02d yuan\r\n", user_info.user_name, total_price/100, total_price%100);

                UART4_SendString(buf);

                // 上报交易记录到ESP32
                TransactionData_t txn;
                memcpy(txn.uid, uid, 4);
                txn.user_id = user_info.user_id;
                memcpy(txn.user_name, user_info.user_name, 12);
                txn.user_name[12] = '\0';
                txn.user_level = user_info.user_level;
                txn.total_price = total_price;
                txn.balance_before = current_balance;
                txn.balance_after = new_balance;
                // 构建菜品列表
                memset(txn.dishes, 0, 60);
                memcpy(txn.dishes, g_saved_dishes, 60);
//                int dp = 0;
//                for (int i = 0; i < 4; i++) {
//                    if (g_items[i].detected && strlen(g_items[i].item_name) > 0) {
//                        if (dp > 0 && dp < 59) txn.dishes[dp++] = ',';
//                          int nl = strlen(g_items[i].item_name);
//                          if (dp + nl > 59) nl = 59 - dp;
//                          memcpy(&txn.dishes[dp], g_items[i].item_name, nl);
//                          dp += nl;
//                    }
//                }
                Cloud_ReportTransaction(&txn);

                // 扣款成功后：
                UART4_SendString("\xff\xff\xff");
                Delay_Ms(100);
                UART4_SendString("page Card_Success");
                audio_play(2);  // 播放"支付成功"
                UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);

                show_success_page = 1;
                success_page_start = g_tick;
                // 不在这里发 page Home，交给主循环处理

            }
        } else {
            sprintf(buf, "Insufficient balance! Current: %lu yuan, need %d yuan\r\n",  // 余额不足！当前余额，需要
                   current_balance, total_price);
            UART4_SendString(buf);

            show_fail_page = 1;
                        fail_page_start = g_tick;
                        UART4_SendString("\xff\xff\xff");
                        UART4_SendString("page Card_Fail");
                        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);

        }
    } else {
        UART4_SendString("Failed to read balance!\r\n");  // 读取余额失败

        show_fail_page = 1;
        fail_page_start = g_tick;
        UART4_SendString("\xff\xff\xff");
        UART4_SendString("page Card_Fail");
        UART4_SendByte(0xFF); UART4_SendByte(0xFF); UART4_SendByte(0xFF);
        goto rfid_done;

    }
//    PcdHalt();
//    rfid_busy = 0;
    rfid_done:
    PcdHalt();

    rfid_busy = 0;

}


//
//void InitNewCard(uint8_t *uid, UserData_t *user_infor)
//{
//    UART4_SendString("InitNewCard called\r\n");
    // 实现新卡初始化逻辑
//    uint8_t buffer[16];
//    uint8_t block_addr = USER_INFO_SECTOR * 4;  // 扇区5块0地址 = 20
//    uint8_t new_uid[4];
//    uint8_t tagType[2];
//    int retry;
//    int select_ok = 0;
//    char buf[128];
//
//    UART4_SendString("\r\n--- Starting new card initialization ---\r\n");
//    sprintf(buf, "Target UID: %02X-%02X-%02X-%02X\r\n", uid[0], uid[1], uid[2], uid[3]);
//    UART4_SendString(buf);
//
//    // 1. 先寻卡
//    for(retry = 0; retry < 5; retry++)
//    {
//        if(PcdRequest(0x52, tagType) == MI_OK) {
//            UART4_SendString("Request success\r\n");
//            break;
//        }
//        sprintf(buf, "Request retry %d/5...\r\n", retry + 1);
//        UART4_SendString(buf);
//        Delay_Ms(100);
//    }
//
//    if(retry >= 5) {
//        UART4_SendString("Request failed, cannot initialize\r\n");
//        return;
//    }
//
//    // 2. 防冲突获取UID
//    if(PcdAnticoll(new_uid) != MI_OK) {
//        UART4_SendString("Anti-collision failed, cannot initialize\r\n");
//        return;
//    }
//
//    sprintf(buf, "Card UID: %02X-%02X-%02X-%02X\r\n", new_uid[0], new_uid[1], new_uid[2], new_uid[3]);
//    UART4_SendString(buf);
//
//    // 3. 选卡（带重试）
//    for(retry = 0; retry < 5; retry++)
//    {
//        if(PcdSelect(new_uid) == MI_OK) {
//            select_ok = 1;
//            UART4_SendString("Card selection success\r\n");
//            break;
//        }
//        sprintf(buf, "Card selection retry %d/5...\r\n", retry + 1);
//        UART4_SendString(buf);
//        Delay_Ms(50);
//    }
//
//    if(!select_ok) {
//        UART4_SendString("Card selection failed, initialization aborted\r\n");
//        return;
//    }
//
//    // 4. 写入用户信息到扇区5
//    sprintf(buf, "Writing user info to sector %d...\r\n", USER_INFO_SECTOR);
//    UART4_SendString(buf);
//    for(retry = 0; retry < 3; retry++)
//    {
//        if(PcdAuthState(0x60, block_addr, Default_Key, new_uid) == MI_OK) {
//            UART4_SendString("Sector 5 authentication success\r\n");
//            break;
//        }
//        sprintf(buf, "Sector 5 auth retry %d/3...\r\n", retry + 1);
//        UART4_SendString(buf);
//        Delay_Ms(50);
//    }
//
//    if(retry >= 3) {
//        UART4_SendString("Sector 5 authentication failed!\r\n");
//        return;
//    }
//
//    // 块0: 用户ID和姓名
//    memset(buffer, 0, 16);
//    buffer[0] = (user_infor->user_id >> 24) & 0xFF;
//    buffer[1] = (user_infor->user_id >> 16) & 0xFF;
//    buffer[2] = (user_infor->user_id >> 8) & 0xFF;
//    buffer[3] = user_infor->user_id & 0xFF;
//    memcpy(&buffer[4], user_infor->user_name, 12);
//
//    if(PcdWrite(block_addr, buffer) == MI_OK) {
//        UART4_SendString("User ID and name write success\r\n");
//    } else {
//        UART4_SendString("User ID and name write failed\r\n");
//    }
//
//    // 块1: 用户等级和部门
//    memset(buffer, 0, 16);
//    buffer[0] = user_infor->user_level;
//    buffer[1] = (user_infor->department >> 8) & 0xFF;
//    buffer[2] = user_infor->department & 0xFF;
//    memcpy(buffer + 3, user_infor->reserved, 5);
//
//    if(PcdWrite(block_addr + 1, buffer) == MI_OK) {
//        UART4_SendString("User permission write success\r\n");
//    } else {
//        UART4_SendString("User permission write failed\r\n");
//    }
//
//    // 5. 初始化余额（扇区4块16）
//    UART4_SendString("Initializing balance in sector 4...\r\n");
//    for(retry = 0; retry < 3; retry++)
//    {
//        if(PcdAuthState(0x60, BLOCK_BALANCE, Default_Key, new_uid) == MI_OK) {
//            UART4_SendString("Sector 4 authentication success\r\n");
//            break;
//        }
//        sprintf(buf, "Sector 4 auth retry %d/3...\r\n", retry + 1);
//        UART4_SendString(buf);
//        Delay_Ms(50);
//    }
//
//    if(retry < 3) {
//        memset(buffer, 0, 16);
//        buffer[0] = (INIT_BALANCE >> 24) & 0xFF;
//        buffer[1] = (INIT_BALANCE >> 16) & 0xFF;
//        buffer[2] = (INIT_BALANCE >> 8) & 0xFF;
//        buffer[3] = INIT_BALANCE & 0xFF;
//
//        if(PcdWrite(BLOCK_BALANCE, buffer) == MI_OK) {
//            sprintf(buf, "Balance initialization success: %d yuan\r\n", INIT_BALANCE);
//            UART4_SendString(buf);
//        } else {
//            UART4_SendString("Balance initialization failed\r\n");
//        }
//    } else {
//        UART4_SendString("Sector 4 authentication failed!\r\n");
//    }
//
//    UART4_SendString("Card initialization complete!\r\n");
//}

void InitNewCard(uint8_t *uid, UserData_t *user_infor)
{
    uint8_t buffer[16];
    uint8_t block_addr = USER_INFO_SECTOR * 4;
    uint8_t new_uid[4];
    uint8_t tagType[2];
    int retry;
    int select_ok = 0;
    char buf[128];

    UART4_SendString("\r\n--- Starting new card initialization ---\r\n");
    sprintf(buf, "Target UID: %02X-%02X-%02X-%02X\r\n", uid[0], uid[1], uid[2], uid[3]);
    UART4_SendString(buf);

    // 1. 先寻卡
    for(retry = 0; retry < 5; retry++)
    {
        if(PcdRequest(0x52, tagType) == MI_OK) {
            break;
        }
        Delay_Ms(100);
    }
    if(retry >= 5) {
        UART4_SendString("Request failed, cannot initialize\r\n");
        return;
    }

    // 2. 防冲突获取UID
    if(PcdAnticoll(new_uid) != MI_OK) {
        UART4_SendString("Anti-collision failed\r\n");
        return;
    }

    // 3. 选卡
    for(retry = 0; retry < 5; retry++)
    {
        if(PcdSelect(new_uid) == MI_OK) {
            select_ok = 1;
            break;
        }
        Delay_Ms(50);
    }
    if(!select_ok) {
        UART4_SendString("Card selection failed\r\n");
        return;
    }

    // 4. 写入用户信息
    for(retry = 0; retry < 3; retry++)
    {
        if(PcdAuthState(0x60, block_addr, Default_Key, new_uid) == MI_OK) {
            break;
        }
        Delay_Ms(50);
    }
    if(retry >= 3) {
        UART4_SendString("Sector 5 authentication failed!\r\n");
        return;
    }

    // 块0: 用户ID和姓名
    memset(buffer, 0, 16);
    buffer[0] = (user_infor->user_id >> 24) & 0xFF;
    buffer[1] = (user_infor->user_id >> 16) & 0xFF;
    buffer[2] = (user_infor->user_id >> 8) & 0xFF;
    buffer[3] = user_infor->user_id & 0xFF;
    memcpy(&buffer[4], user_infor->user_name, 12);
    PcdWrite(block_addr, buffer);

    // 块1: 用户等级
    memset(buffer, 0, 16);
    buffer[0] = user_infor->user_level;
    buffer[1] = (user_infor->department >> 8) & 0xFF;
    buffer[2] = user_infor->department & 0xFF;
    PcdWrite(block_addr + 1, buffer);

    // 5. 初始化余额
//    for(retry = 0; retry < 3; retry++)
//    {
//        if(PcdAuthState(0x60, BLOCK_BALANCE, Default_Key, new_uid) == MI_OK) {
//            memset(buffer, 0, 16);
//            buffer[3] = INIT_BALANCE;
//            PcdWrite(BLOCK_BALANCE, buffer);
//            sprintf(buf, "Balance initialized: %d yuan\r\n", INIT_BALANCE);
//            UART4_SendString(buf);
//            break;
//        }
//        Delay_Ms(50);
//    }

    // 5. 初始化余额（扇区4块16）
    for(retry = 0; retry < 3; retry++)
    {
        if(PcdAuthState(0x60, BLOCK_BALANCE, Default_Key, new_uid) == MI_OK) {
            memset(buffer, 0, 16);
            // 写入完整的 4 字节余额（大端序：高位在前）
            buffer[0] = (INIT_BALANCE >> 24) & 0xFF;
            buffer[1] = (INIT_BALANCE >> 16) & 0xFF;
            buffer[2] = (INIT_BALANCE >> 8) & 0xFF;
            buffer[3] = INIT_BALANCE & 0xFF;

            if(PcdWrite(BLOCK_BALANCE, buffer) == MI_OK) {
                sprintf(buf, "Balance initialized: %d yuan\r\n", INIT_BALANCE);
                UART4_SendString(buf);
            } else {
                UART4_SendString("Balance write failed!\r\n");
            }
            break;
        }
        Delay_Ms(50);
    }


    UART4_SendString("Card initialization complete!\r\n");
    PcdHalt();
}

uint8_t ReadUserInfoFromCard(uint8_t sector, uint8_t *uid, UserData_t *user_data)
{
    uint8_t buffer[16];
    uint8_t block_addr = sector * 4;
    uint8_t retry;

    // 验证扇区（带重试）
    for(retry = 0; retry < 3; retry++)
    {
        if(PcdAuthState(PICC_AUTHENT1A, block_addr, Default_Key, uid) == MI_OK) {
            break;
        }
        Delay_Ms(20);
    }
    if(retry >= 3) {
        UART4_SendString("Sector authentication failed!\r\n");
        return 0;
    }

    // 读取块0(用户ID和姓名)
    if(PcdRead(block_addr, buffer) != MI_OK) {
        UART4_SendString("Read user ID failed!\r\n");
        return 0;
    }

    // 检查数据有效性（用户ID不能为0）
    uint32_t user_id = (buffer[0] << 24) | (buffer[1] << 16) | (buffer[2] << 8) | buffer[3];
    if(user_id == 0) {
        UART4_SendString("User ID is 0, card not initialized\r\n");
        return 0;
    }

    // 解析用户ID
    user_data->user_id = user_id;
    memcpy(user_data->user_name, &buffer[4], 12);
    user_data->user_name[11] = '\0';

    // 读取块1(等级和部门)
    if(PcdRead(block_addr + 1, buffer) != MI_OK) {
        UART4_SendString("Read user permission failed!\r\n");
        return 0;
    }

    user_data->user_level = buffer[0];
    user_data->department = (buffer[1] << 8) | buffer[2];
    memcpy(user_data->reserved, &buffer[3], 5);

    return 1;
}

void DisplayUserInfo(UserData_t *user)
{
    UART4_SendString("========== User Details ==========\r\n");
    char buf[64];

    sprintf(buf, "User ID: %lu\r\n", user->user_id);
    UART4_SendString(buf);

    sprintf(buf, "User Name: %s\r\n", user->user_name);
    UART4_SendString(buf);

    sprintf(buf, "User Level: %s\r\n", (user->user_level == 1) ? "Normal User" : "Administrator");
    UART4_SendString(buf);

    sprintf(buf, "Department Code: %d\r\n", user->department);
    UART4_SendString(buf);

    UART4_SendString("==================================\r\n");
}


/* 读取余额 - 4字节版 */
//uint8_t ReadBalance(uint8_t block_addr, uint32_t *balance)
//{
//    uint8_t buffer[16];
//    char buf[64];
//
//    if(PcdRead(block_addr, buffer) == MI_OK) {
//        // 使用4个字节存储余额（大端序：高字节在前）
//        *balance = ((uint32_t)buffer[0] << 24) |
//                   ((uint32_t)buffer[1] << 16) |
//                   ((uint32_t)buffer[2] << 8) |
//                   (uint32_t)buffer[3];
//        sprintf(buf, "[BALANCE] Read: %lu yuan\r\n", *balance);
//        UART4_SendString(buf);
//        return 1;
//    }
//    UART4_SendString("[BALANCE] Read failed!\r\n");
//    return 0;
//}

uint8_t ReadBalance(uint8_t block_addr, uint32_t *balance)
{
    uint8_t buffer[16];
    if(PcdRead(block_addr, buffer) == MI_OK) {
        // 大端序解析
        *balance = ((uint32_t)buffer[0] << 24) |
                   ((uint32_t)buffer[1] << 16) |
                   ((uint32_t)buffer[2] << 8)  |
                   (uint32_t)buffer[3];
        return 1;
    }
    return 0;
}


/* 写入余额 - 4字节版 */
uint8_t WriteBalance(uint8_t block_addr, uint32_t balance)
{
    uint8_t buffer[16];
    char buf[64];

    memset(buffer, 0, 16);
    // 使用4个字节存储余额（大端序：高字节在前）
    buffer[0] = (balance >> 24) & 0xFF;
    buffer[1] = (balance >> 16) & 0xFF;
    buffer[2] = (balance >> 8) & 0xFF;
    buffer[3] = balance & 0xFF;

    if(PcdWrite(block_addr, buffer) == MI_OK) {
//        sprintf(buf, "[BALANCE] Write success: %lu yuan\r\n", balance);

        sprintf(buf, "[BALANCE] Write success: %lu.%02lu yuan\r\n", balance/100, balance%100);

        UART4_SendString(buf);
        return 1;
    }
    UART4_SendString("[BALANCE] Write failed!\r\n");
    return 0;
}







