/**
  * @file    cloud_protocol.h
  * @brief   云端通信协议头文件
  * @note    定义与ESP8266/ESP32通信的帧格式、命令类型和数据结构
  *          物理层使用UART8进行通信
  */

#ifndef CLOUD_PROTOCOL_H
#define CLOUD_PROTOCOL_H

#include "ch32v30x.h"          /* 沁恒CH32V307标准外设库 */

/*==================== 协议帧格式定义 ====================*/
/**
  * @brief  帧格式：同步头 + 类型 + 长度 + 数据 + CRC + 结束符
  * @note   同步头: 0xAA (1字节)
  *         类型:   1字节
  *         长度:   2字节 (高字节在前)
  *         数据:   N字节 (最大256字节)
  *         CRC:    1字节 (CRC8校验)
  *         结束符: 0x55 (1字节)
  */

/* 帧标志字节定义 */
#define CLOUD_SYNC_BYTE     0xAA    /*!< 帧同步头（帧起始标志） */
#define CLOUD_END_BYTE      0x55    /*!< 帧结束标志 */
#define CLOUD_ESC_BYTE      0xBB    /*!< 转义字节（用于数据转义） */

/* 转义后的数据映射 */
#define CLOUD_ESC_AA        0x01    /*!< 0xAA 转义后的值 */
#define CLOUD_ESC_55        0x02    /*!< 0x55 转义后的值 */
#define CLOUD_ESC_BB        0x00    /*!< 0xBB 转义后的值（注意：这里是0x00而非0x03） */

/*==================== 帧类型定义（MCU -> ESP32） ====================*/
/**
  * @brief  MCU发送给ESP32的帧类型
  * @note   MCU作为客户端，主动上报数据给云端
  */
#define FRAME_CARD_TAP      0x10    /*!< 刷卡事件上报（用户刷卡消费） */
#define FRAME_TRANSACTION   0x11    /*!< 交易数据上报（消费记录） */
#define FRAME_HEARTBEAT     0x12    /*!< 心跳包（保持连接活跃） */
#define FRAME_PRICE_TABLE   0x13    /*!< 价格表上报（菜品价格更新） */

/*==================== 帧类型定义（ESP32 -> MCU） ====================*/
/**
  * @brief  ESP32发送给MCU的帧类型
  * @note   ESP32作为服务器，下发指令或响应给MCU
  */
#define FRAME_RECHARGE_CMD  0x20    /*!< 充值指令（ESP32下发充值命令） */
#define FRAME_ACK           0x30    /*!< 应答成功（MCU上报数据后ESP32确认） */
#define FRAME_NACK          0x31    /*!< 应答失败（MCU上报数据后ESP32拒绝） */
#define FRAME_HEARTBEAT_ACK 0x32    /*!< 心跳应答（ESP32回复心跳包） */

/*==================== 系统配置参数 ====================*/
#define MAX_RECHARGE_QUEUE  8       /*!< 充值指令队列最大长度（支持8条待处理充值） */
#define DEVICE_ID_STR       "dish"  /*!< 设备ID字符串（用于云端识别设备） */
#define DEVICE_ID_LEN       4           /*!< 设备ID长度 */

/*==================== 数据结构定义 ====================*/

/**
  * @brief  充值指令结构体（ESP32下发）
  * @note   用于云端向设备下发充值命令
  */
typedef struct {
    char     card_uid_hex[9];      /*!< 卡片UID的十六进制字符串（如 "A1B2C3D4"） */
    uint32_t user_id;              /*!< 用户ID（云端用户唯一标识） */
    uint32_t recharge_amount;      /*!< 充值金额（单位：分） */
    char     order_no[32];         /*!< 订单号（云端生成的唯一订单标识） */
} RechargeCmd_t;

/**
  * @brief  交易数据结构体（MCU上报）
  * @note   用于向云端上报刷卡消费记录
  */
typedef struct {
    uint8_t  uid[4];               /*!< 卡片UID（原始字节数据） */
    uint32_t user_id;              /*!< 用户ID（从卡片读取） */
    char     user_name[13];        /*!< 用户名（最大12字节+结束符） */
    uint8_t  user_level;           /*!< 用户等级（如：普通/会员/VIP等） */
    uint16_t total_price;          /*!< 消费总金额（单位：分） */
    uint32_t balance_before;       /*!< 交易前余额（单位：分） */
    uint32_t balance_after;        /*!< 交易后余额（单位：分） */
    char     dishes[60];
} TransactionData_t;

/*==================== 公有函数声明 ====================*/

/**
  * @brief  初始化云端通信协议模块
  * @param  None
  * @retval None
  * @note   内部会初始化UART8外设，波特率默认115200
  *         清空接收缓冲区，重置状态机
  */
void CloudProtocol_Init(void);

/**
  * @brief  发送一帧数据到云端（ESP32）
  * @param  type:    帧类型（见上方FRAME_xxx定义）
  * @param  payload: 有效载荷数据指针
  * @param  len:     有效载荷数据长度
  * @retval 1: 发送成功  0: 发送失败
  * @note   自动添加帧头、帧尾、CRC校验，并对数据中的特殊字节进行转义
  *         阻塞发送，直到数据全部发送完毕
  */
uint8_t Cloud_SendFrame(uint8_t type, uint8_t *payload, uint16_t len);

/**
  * @brief  处理接收到的云端数据帧
  * @param  None
  * @retval None
  * @note   应在主循环中周期性调用
  *         自动完成：去转义 → CRC校验 → 提取数据
  *         校验成功的数据存入内部缓冲区，供上层通过GetNextFrame获取
  */
void Cloud_ProcessRX(void);

/**
  * @brief  检查是否有待处理的帧
  * @param  None
  * @retval 1: 有待处理帧  0: 无待处理帧
  * @note   用于轮询检测是否有新帧到达
  *         建议在主循环中调用，避免阻塞
  */
uint8_t Cloud_HasPendingFrame(void);

/**
  * @brief  获取下一帧数据
  * @param  type:    输出参数，返回帧类型
  * @param  payload: 输出参数，返回载荷数据指针（需确保缓冲区足够大）
  * @param  len:     输出参数，返回载荷数据长度
  * @retval 1: 获取成功  0: 没有待处理的帧
  * @note   调用后会清除内部帧就绪标志，确保每帧只被处理一次
  *         建议先调用 Cloud_HasPendingFrame() 确认有数据后再调用此函数
  */
uint8_t Cloud_GetNextFrame(uint8_t *type, uint8_t *payload, uint16_t *len);

void CloudProtocol_Init(void);

#endif /* CLOUD_PROTOCOL_H */
