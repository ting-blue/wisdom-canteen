/**
  * @file    cloud_protocol.c
  * @brief   云端通信协议处理模块
  * @note    支持数据转义、CRC校验、帧解析等功能
  *          通信格式: 0xAA + 类型(1B) + 长度(2B) + 数据(nB) + CRC(1B) + 0x55
  *          转义规则: 0xAA -> 0xBB 0x01, 0x55 -> 0xBB 0x02, 0xBB -> 0xBB 0x03
  */

#include "cloud_protocol.h"
#include "Uart.h"          // 使用UART8进行物理层通信
#include <string.h>
#include <stdio.h>

/* 静态变量定义（仅在本文件内使用） */

/**
  * @brief  接收缓冲区，用于存储从UART8接收的原始数据（含转义字符）
  */
static uint8_t g_cloud_rx_buf[512];

/**
  * @brief  当前接收缓冲区中的数据索引（写入位置）
  */
static uint16_t g_cloud_rx_idx = 0;

/**
  * @brief  帧接收完成标志
  * @note   0: 未收到完整帧  1: 已收到完整帧
  */
static volatile uint8_t g_cloud_frame_ready = 0;

/**
  * @brief  当前接收帧的类型
  */
static uint8_t g_cloud_frame_type = 0;

/**
  * @brief  当前接收帧的有效载荷数据缓冲区
  */
static uint8_t g_cloud_frame_payload[256];

/**
  * @brief  当前接收帧的有效载荷数据长度
  */
static uint16_t g_cloud_frame_len = 0;

/* 私有函数（仅在本文件内使用） */

/**
  * @brief  计算CRC8校验值
  * @param  data: 要计算校验的数据指针
  * @param  len:  数据长度（字节数）
  * @retval 计算得到的8位CRC校验值
  * @note   使用多项式0x8C，初始值0x00
  *         算法：逐位计算，异或多项式
  */
static uint8_t CRC8(uint8_t *data, uint16_t len)
{
    uint8_t crc = 0;  // CRC初始值为0

    // 遍历每个字节
    while (len--) {
        uint8_t inbyte = *data++;  // 取出当前字节

        // 逐位处理（8位）
        for (uint8_t i = 8; i; i--) {
            // 判断CRC最高位与当前字节最低位的异或结果
            uint8_t mix = (crc ^ inbyte) & 0x01;
            crc >>= 1;              // CRC右移一位

            // 如果异或结果为1，则与多项式0x8C进行异或
            if (mix) crc ^= 0x8C;
            inbyte >>= 1;           // 处理下一位
        }
    }
    return crc;
}

/**
  * @brief  发送转义后的单字节数据
  * @param  b: 要发送的原始字节数据
  * @retval None
  * @note   转义规则：
  *         0xAA (同步字节) -> 0xBB 0x01
  *         0x55 (结束字节) -> 0xBB 0x02
  *         0xBB (转义字节) -> 0xBB 0x03
  *         其他字节 -> 原样发送
  */
static void Cloud_SendEscaped(uint8_t b)
{
    // 判断是否需要转义发送
    if (b == CLOUD_SYNC_BYTE) {                    // 同步字节 0xAA
        UART8_SendByte(CLOUD_ESC_BYTE);            // 先发送转义字节 0xBB
        UART8_SendByte(CLOUD_ESC_AA);              // 再发送转义后的数据 0x01
    } else if (b == CLOUD_END_BYTE) {              // 结束字节 0x55
        UART8_SendByte(CLOUD_ESC_BYTE);            // 先发送转义字节 0xBB
        UART8_SendByte(CLOUD_ESC_55);              // 再发送转义后的数据 0x02
    } else if (b == CLOUD_ESC_BYTE) {              // 转义字节 0xBB
        UART8_SendByte(CLOUD_ESC_BYTE);            // 先发送转义字节 0xBB
        UART8_SendByte(CLOUD_ESC_BB);              // 再发送转义后的数据 0x03
    } else {
        UART8_SendByte(b);                         // 其他字节直接发送
    }
}

/* 公有函数（提供给外部调用） */

/**
  * @brief  发送一帧数据到云端
  * @param  type:     帧类型（1字节）
  * @param  payload:  有效载荷数据指针
  * @param  len:      有效载荷数据长度
  * @retval 1: 发送成功
  * @note   发送格式：同步头 + 类型 + 长度(高字节) + 长度(低字节) + 数据 + CRC + 结束符
  *         所有内容（除同步头和结束符外）都需要进行转义处理
  */
uint8_t Cloud_SendFrame(uint8_t type, uint8_t *payload, uint16_t len)
{
    /* 1. 发送帧头 - 同步字节（不转义） */
    UART8_SendByte(CLOUD_SYNC_BYTE);               // 发送帧起始标志 0xAA

    /* 2. 发送帧类型（需要转义） */
    Cloud_SendEscaped(type);                        // 发送类型字段

    /* 3. 发送帧长度（需要转义）- 大端模式：高字节在前 */
    Cloud_SendEscaped((len >> 8) & 0xFF);           // 发送长度高字节
    Cloud_SendEscaped(len & 0xFF);                  // 发送长度低字节

    /* 4. 发送有效载荷数据（每个字节都需要转义） */
    for (uint16_t i = 0; i < len; i++)
        Cloud_SendEscaped(payload[i]);              // 逐个发送数据字节

    /* 5. 计算并发送CRC校验值（需要转义） */
    uint8_t crc = CRC8(&type, 1);                   // 计算类型字段的CRC
    uint8_t len_bytes[2] = {(len >> 8) & 0xFF, len & 0xFF};
    crc = CRC8(len_bytes, 2);                       // 累加长度字段的CRC
    crc = CRC8(payload, len);                       // 累加载荷数据的CRC
    Cloud_SendEscaped(crc);                         // 发送最终的CRC校验值

    /* 6. 发送帧尾 - 结束字节（不转义） */
    UART8_SendByte(CLOUD_END_BYTE);                 // 发送帧结束标志 0x55

    return 1;  // 发送成功
}

/**
  * @brief  处理接收到的云端数据帧
  * @param  None
  * @retval None
  * @note   对接收缓冲区中的数据进行解析：去转义、校验CRC、提取数据
  *         解析成功后，数据存入 g_cloud_frame_payload 供上层使用
  *         解析失败会自动清空缓冲区，准备接收下一帧
  */
void Cloud_ProcessRX(void)
{
    /* 检查是否有完整的帧需要处理 */
    if (!g_cloud_frame_ready) return;

    /* 定义解码缓冲区，用于存储去转义后的数据 */
    uint8_t decoded[256];              // 解码后的数据缓冲区
    uint16_t di = 0;                   // 解码缓冲区的写入索引
    uint8_t esc = 0;                   // 转义标志：0=普通模式，1=转义模式

    /* 1. 去除转义字符，还原原始数据（跳过帧头0xAA和帧尾0x55） */
    for (uint16_t i = 1; i < g_cloud_rx_idx - 1 && di < 255; i++) {
        uint8_t b = g_cloud_rx_buf[i];        // 获取当前字节

        if (esc) {                             // 如果处于转义模式
            // 根据转义值还原原始数据
            if (b == CLOUD_ESC_AA)             // 转义值0x01 -> 还原为0xAA
                decoded[di++] = CLOUD_SYNC_BYTE;
            else if (b == CLOUD_ESC_55)        // 转义值0x02 -> 还原为0x55
                decoded[di++] = CLOUD_END_BYTE;
            else if (b == CLOUD_ESC_BB)        // 转义值0x03 -> 还原为0xBB
                decoded[di++] = CLOUD_ESC_BYTE;
            esc = 0;                           // 清除转义标志
        } else if (b == CLOUD_ESC_BYTE) {      // 如果遇到转义字节0xBB
            esc = 1;                           // 设置转义标志，下一个字节需要还原
        } else {
            decoded[di++] = b;                 // 普通字节直接保存
        }
    }

    /* 2. 检查帧最小长度：至少需要 类型(1) + 长度(2) + CRC(1) = 4字节 */
    if (di < 4) {
        g_cloud_frame_ready = 0;               // 清空就绪标志
        g_cloud_rx_idx = 0;                    // 重置接收索引
        return;
    }

    /* 3. 提取帧的各个字段 */
    uint8_t  type   = decoded[0];              // 帧类型（第1字节）
    uint16_t len    = ((uint16_t)decoded[1] << 8) | decoded[2];  // 数据长度（第2-3字节）
    uint8_t *payload = &decoded[3];            // 有效载荷数据（第4字节开始）
    uint8_t  crc_rcv = decoded[3 + len];       // CRC校验值（位于载荷之后）

    /* 4. 检查帧长度是否匹配（防止数据不完整） */
    if (3 + len + 1 > di) {                    // 3(类型+长度) + len(数据) + 1(CRC) > 实际长度
        g_cloud_frame_ready = 0;
        g_cloud_rx_idx = 0;
        return;
    }

    /* 5. 验证CRC校验值 */
    uint8_t crc_calc = CRC8(&type, 1);         // 计算类型字段的CRC
    crc_calc = CRC8(&decoded[1], 2);           // 累加长度字段的CRC
    crc_calc = CRC8(payload, len);             // 累加载荷数据的CRC

    if (crc_calc != crc_rcv) {                 // 校验失败
        g_cloud_frame_ready = 0;
        g_cloud_rx_idx = 0;
        return;
    }

    /* 6. 校验通过，保存帧数据供上层使用 */
    g_cloud_frame_type = type;                 // 保存帧类型
    memcpy(g_cloud_frame_payload, payload, len); // 复制载荷数据
    g_cloud_frame_len = len;                   // 保存数据长度

    /* 7. 清空接收缓冲区，准备接收下一帧 */
    g_cloud_rx_idx = 0;                        // 注意：不清除 g_cloud_frame_ready，供上层查询
}

/**
  * @brief  检查是否有待处理的帧
  * @param  None
  * @retval 1: 有待处理帧  0: 无待处理帧
  * @note   此函数用于轮询检测是否有新帧到达
  */
uint8_t Cloud_HasPendingFrame(void)
{
    return g_cloud_frame_ready && g_cloud_frame_len > 0;
}

/**
  * @brief  获取下一帧数据
  * @param  type:    输出参数，返回帧类型
  * @param  payload: 输出参数，返回载荷数据（需确保缓冲区足够大）
  * @param  len:     输出参数，返回载荷数据长度
  * @retval 1: 获取成功  0: 没有待处理的帧
  * @note   调用后会自动清除就绪标志，确保每帧只被处理一次
  */
uint8_t Cloud_GetNextFrame(uint8_t *type, uint8_t *payload, uint16_t *len)
{
    /* 检查是否有待处理的帧 */
    if (!g_cloud_frame_ready) return 0;

    /* 返回帧数据 */
    *type = g_cloud_frame_type;                    // 返回帧类型
    memcpy(payload, g_cloud_frame_payload, g_cloud_frame_len);  // 复制载荷数据
    *len = g_cloud_frame_len;                      // 返回数据长度

    /* 清除帧就绪标志和缓冲区，准备接收下一帧 */
    g_cloud_frame_ready = 0;
    g_cloud_frame_len = 0;

    return 1;  // 获取成功
}

/**
  * @brief  向云端协议模块喂入一个接收字节
  * @param  b: 从UART8接收到的原始字节
  * @retval None
  * @note   此函数应在UART8的接收中断服务函数中调用
  *         会自动过滤无效数据，检测帧头和帧尾
  *         当检测到完整帧时，设置 g_cloud_frame_ready = 1
  */
void Cloud_FeedRXByte(uint8_t b)
{
    /* 1. 如果缓冲区为空，等待同步字节0xAA（丢弃无效数据） */
    if (g_cloud_rx_idx == 0 && b != CLOUD_SYNC_BYTE) return;

    /* 2. 将字节存入接收缓冲区（防止溢出） */
    if (g_cloud_rx_idx < 512) {
        g_cloud_rx_buf[g_cloud_rx_idx++] = b;
    }

    /* 3. 检测帧尾0x55，且帧长度至少为最小帧（6字节：头+类型+长度+CRC+尾） */
    if (b == CLOUD_END_BYTE && g_cloud_rx_idx >= 6) {
        g_cloud_frame_ready = 1;  // 标记帧接收完成
    }
}


void CloudProtocol_Init(void)
{
  UART8_TX_Init(115200);
}

