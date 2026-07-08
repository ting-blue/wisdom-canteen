/**
  * @file    cloud_bridge.c
  * @brief   云端通信桥接层实现
  * @note    负责将业务数据打包成协议帧，管理充值指令队列
  *          提供高层API供业务层调用，屏蔽底层协议细节
  *          物理层使用UART8通信
  */

#include "CLoude_bridge.h"
#include "cloud_protocol.h"      /* 协议层，内部使用UART8 */
#include <string.h>
#include <stdio.h>

/*==================== 静态变量定义 ====================*/

/**
  * @brief  待处理的充值指令队列
  * @note   存储从云端接收到的充值命令，等待设备执行
  *         队列长度由 MAX_RECHARGE_QUEUE 控制
  */
static RechargeCmd_t g_pending_list[MAX_RECHARGE_QUEUE];

/**
  * @brief  队列中待处理的充值指令数量
  */
static uint8_t g_pending_count = 0;

/*==================== 公有函数实现 ====================*/

/**
  * @brief  上报刷卡事件到云端
  * @param  uid:        卡片UID（4字节原始数据）
  * @param  user_id:    用户ID（32位无符号整数）
  * @param  user_name:  用户名（字符串，最大12字节）
  * @param  level:      用户等级
  * @param  balance:    当前余额
  * @retval None
  * @note   用于实时上报用户刷卡消费事件
  *         数据格式：UID(4) + UserID(4) + UserName(12) + Level(1) + Balance(4) + DeviceID(10)
  *         总长度：35字节
  */
void Cloud_ReportCardTap(uint8_t *uid, uint32_t user_id,
                         char *user_name, uint8_t level, uint32_t balance)
{
    uint8_t payload[64];          /* 载荷缓冲区，64字节足够 */
    uint16_t pos = 0;             /* 当前写入位置 */

    /* 1. 填充卡片UID（4字节原始数据） */
    memcpy(&payload[pos], uid, 4);
    pos += 4;

    /* 2. 填充用户ID（大端模式，高字节在前） */
    payload[pos++] = (user_id >> 24) & 0xFF;   /* 最高字节 */
    payload[pos++] = (user_id >> 16) & 0xFF;   /* 次高字节 */
    payload[pos++] = (user_id >> 8) & 0xFF;    /* 次低字节 */
    payload[pos++] = user_id & 0xFF;           /* 最低字节 */

    /* 3. 填充用户名（最大12字节，不足部分补0） */
    memcpy(&payload[pos], user_name, 12);
    pos += 12;

    /* 4. 填充用户等级 */
    payload[pos++] = level;

    /* 5. 填充当前余额（大端模式） */
    payload[pos++] = (balance >> 24) & 0xFF;
    payload[pos++] = (balance >> 16) & 0xFF;
    payload[pos++] = (balance >> 8) & 0xFF;
    payload[pos++] = balance & 0xFF;

    /* 6. 填充设备ID（用于云端识别设备） */
    memcpy(&payload[pos], DEVICE_ID_STR, 4);
    pos += 4;

    /* 7. 字符串结束符（安全保护） */
    payload[pos] = 0;

    /* 8. 发送帧到云端（通过UART8） */
    Cloud_SendFrame(FRAME_CARD_TAP, payload, pos);
}

/**
  * @brief  上报交易数据到云端
  * @param  txn: 交易数据结构体指针
  * @retval None
  * @note   用于详细记录每次消费交易
  *         数据格式：UID(4) + UserID(4) + TotalPrice(2) + BalanceBefore(4)
  *                  + BalanceAfter(4) + UserName(12)
  *         总长度：30字节
  */
void Cloud_ReportTransaction(TransactionData_t *txn)
{
    uint8_t payload[64];          /* 载荷缓冲区 */
    uint16_t pos = 0;             /* 当前写入位置 */

    /* 1. 填充卡片UID（4字节） */
    memcpy(&payload[pos], txn->uid, 4);
    pos += 4;

    /* 2. 填充用户ID（大端模式） */
    payload[pos++] = (txn->user_id >> 24) & 0xFF;
    payload[pos++] = (txn->user_id >> 16) & 0xFF;
    payload[pos++] = (txn->user_id >> 8) & 0xFF;
    payload[pos++] = txn->user_id & 0xFF;

    /* 3. 填充消费总金额（大端模式，2字节） */
    payload[pos++] = (txn->total_price >> 8) & 0xFF;   /* 高字节 */
    payload[pos++] = txn->total_price & 0xFF;          /* 低字节 */

    /* 4. 填充交易前余额（大端模式） */
    payload[pos++] = (txn->balance_before >> 24) & 0xFF;
    payload[pos++] = (txn->balance_before >> 16) & 0xFF;
    payload[pos++] = (txn->balance_before >> 8) & 0xFF;
    payload[pos++] = txn->balance_before & 0xFF;

    /* 5. 填充交易后余额（大端模式） */
    payload[pos++] = (txn->balance_after >> 24) & 0xFF;
    payload[pos++] = (txn->balance_after >> 16) & 0xFF;
    payload[pos++] = (txn->balance_after >> 8) & 0xFF;
    payload[pos++] = txn->balance_after & 0xFF;

    /* 6. 填充用户名（12字节） */
    memcpy(&payload[pos], txn->user_name, 12);
    pos += 12;

    memcpy(&payload[pos], txn->dishes, 60);
    pos += 60;

    /* 7. 发送帧到云端（通过UART8） */
    Cloud_SendFrame(FRAME_TRANSACTION, payload, pos);
}

/**
  * @brief  上报充值完成事件到云端
  * @param  uid:         卡片UID（4字节原始数据）
  * @param  amount:      充值金额
  * @param  new_balance: 充值后的新余额
  * @retval None
  * @note   复用FRAME_HEARTBEAT帧类型上报充值完成
  *         数据格式：UID(4) + Amount(4) + NewBalance(4)
  *         总长度：12字节
  */
void Cloud_ReportRechargeComplete(uint8_t *uid, uint32_t amount, uint32_t new_balance)
{
    uint8_t payload[32];          /* 载荷缓冲区 */

    /* 1. 填充卡片UID（4字节） */
    memcpy(payload, uid, 4);

    /* 2. 填充充值金额（大端模式） */
    payload[4] = (amount >> 24) & 0xFF;
    payload[5] = (amount >> 16) & 0xFF;
    payload[6] = (amount >> 8) & 0xFF;
    payload[7] = amount & 0xFF;

    /* 3. 填充新余额（大端模式） */
    payload[8] = (new_balance >> 24) & 0xFF;
    payload[9] = (new_balance >> 16) & 0xFF;
    payload[10] = (new_balance >> 8) & 0xFF;
    payload[11] = new_balance & 0xFF;

    /* 4. 发送帧到云端（复用心跳帧类型） */
    Cloud_SendFrame(FRAME_HEARTBEAT, payload, 12);
}

/**
  * @brief  发送心跳包到云端
  * @param  None
  * @retval None
  * @note   用于保持设备与云端的连接活跃
  *         数据格式：DeviceID(10) + PendingFlag(1) + Reserved(1)
  *         总长度：12字节
  *         PendingFlag指示是否有待处理的充值指令
  */
void Cloud_SendHeartbeat(void)
{
    uint8_t payload[16];          /* 载荷缓冲区 */

    /* 1. 填充设备ID（10字节，用于云端识别） */
    memcpy(payload, DEVICE_ID_STR, 4);
    payload[4] = 0;              /* 字符串结束符（安全保护） */

    /* 2. 填充待处理标志（指示是否有充值指令等待执行） */
    payload[11] = (g_pending_count > 0) ? 1 : 0;

    /* 3. 发送心跳帧到云端（通过UART8） */
    Cloud_SendFrame(FRAME_HEARTBEAT, payload, 6);
}

/**
  * @brief  获取指定UID的待处理充值金额
  * @param  uid: 卡片UID（4字节原始数据）
  * @retval 充值金额（如果该UID有待处理充值）
  *         0: 没有待处理充值或充值金额为0
  * @note   从待处理队列中查找匹配的UID
  *         找到后自动从队列中移除该充值指令
  *         使用UID十六进制字符串进行匹配
  */
uint32_t Cloud_GetPendingRecharge(uint8_t *uid)
{
    char uid_hex[9];              /* UID十六进制字符串缓冲区 */

    /* 1. 将UID转换为十六进制字符串（格式：XX-XX-XX-XX） */
    sprintf(uid_hex, "%02X-%02X-%02X-%02X", uid[0], uid[1], uid[2], uid[3]);

    /* 2. 遍历待处理队列，查找匹配的UID */
    for (uint8_t i = 0; i < g_pending_count; i++) {
        if (strcmp(g_pending_list[i].card_uid_hex, uid_hex) == 0) {
            /* 3. 找到匹配项，取出充值金额 */
            uint32_t amount = g_pending_list[i].recharge_amount;

            /* 4. 从队列中移除该充值指令（数组前移） */
            for (uint8_t j = i; j < g_pending_count - 1; j++)
                g_pending_list[j] = g_pending_list[j + 1];
            g_pending_count--;     /* 队列长度减1 */

            return amount;         /* 返回充值金额 */
        }
    }

    return 0;  /* 未找到匹配的充值指令 */
}

/**
  * @brief  处理云端下发的待处理充值指令
  * @param  None
  * @retval None
  * @note   应在主循环中周期性调用
  *         从协议层获取所有下发的帧，如果是充值命令则加入待处理队列
  *         数据格式：UID(4) + UserID(4) + Amount(4) + OrderNo(32)
  *         总长度：44字节
  */
void Cloud_ProcessPendingRecharges(void)
{
    uint8_t type;                 /* 帧类型 */
    uint8_t payload[256];         /* 载荷数据缓冲区 */
    uint16_t len;                 /* 载荷数据长度 */

    /* 1. 循环处理所有待处理的帧 */
    while (Cloud_GetNextFrame(&type, payload, &len)) {
        /* 2. 判断是否为充值命令，且队列未满 */
        if (type == FRAME_RECHARGE_CMD && g_pending_count < MAX_RECHARGE_QUEUE) {
            /* 3. 获取队列尾部指针 */
            RechargeCmd_t *cmd = &g_pending_list[g_pending_count];
            uint16_t pos = 0;     /* 载荷数据读取位置 */

            /* 4. 解析UID（4字节），转换为十六进制字符串 */
            sprintf(cmd->card_uid_hex, "%02X-%02X-%02X-%02X",
                    payload[0], payload[1], payload[2], payload[3]);
            pos += 4;

            /* 5. 解析用户ID（大端模式） */
            cmd->user_id = ((uint32_t)payload[pos] << 24) |
                          ((uint32_t)payload[pos+1] << 16) |
                          ((uint32_t)payload[pos+2] << 8) |
                          payload[pos+3];
            pos += 4;

            /* 6. 解析充值金额（大端模式） */
            cmd->recharge_amount = ((uint32_t)payload[pos] << 24) |
                                   ((uint32_t)payload[pos+1] << 16) |
                                   ((uint32_t)payload[pos+2] << 8) |
                                   payload[pos+3];
            pos += 4;

            /* 7. 复制订单号（32字节），确保字符串安全结束 */
            memcpy(cmd->order_no, &payload[pos], 32);
            cmd->order_no[31] = '\0';  /* 强制字符串结束 */

            /* 8. 增加队列计数 */
            g_pending_count++;
        }
        /* 其他帧类型在此可以忽略或扩展处理 */
    }
}


