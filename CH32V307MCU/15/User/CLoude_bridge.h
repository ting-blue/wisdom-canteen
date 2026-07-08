/**
  * @file    cloud_bridge.h
  * @brief   云端通信桥接层头文件
  * @note    提供高层业务接口，将业务数据转换为协议帧
  *          管理云端下发的充值指令队列
  *          物理层使用UART8通信
  *
  * @section 使用方法
  *  1. 初始化：调用 CloudProtocol_Init() 初始化UART8和协议栈
  *  2. 数据上报：调用 Cloud_ReportCardTap() / Cloud_ReportTransaction() 上报数据
  *  3. 心跳维护：在主循环中定时调用 Cloud_SendHeartbeat()
  *  4. 充值处理：在主循环中调用 Cloud_ProcessPendingRecharges() 接收充值指令
  *  5. 充值消费：刷卡时调用 Cloud_GetPendingRecharge() 查询并消费充值
  */

#ifndef CLOUD_BRIDGE_H
#define CLOUD_BRIDGE_H

#include "cloud_protocol.h"      /* 协议层定义，包含帧类型和数据结构 */

/*==================== 公有函数声明 ====================*/

/**
  * @brief  上报交易数据到云端
  * @param  txn: 交易数据结构体指针
  *              - uid: 卡片UID（4字节）
  *              - user_id: 用户ID
  *              - total_price: 消费金额
  *              - balance_before: 交易前余额
  *              - balance_after: 交易后余额
  *              - user_name: 用户名
  * @retval None
  * @note   用于上报详细消费记录
  *         帧类型：FRAME_TRANSACTION (0x11)
  *         数据格式：UID(4) + UserID(4) + TotalPrice(2) + BalanceBefore(4)
  *                  + BalanceAfter(4) + UserName(12)
  *         总长度：30字节
  * @warning 确保 TransactionData_t 结构体中的所有字段都已正确填充
  */
void Cloud_ReportTransaction(TransactionData_t *txn);

/**
  * @brief  上报刷卡事件到云端
  * @param  uid:       卡片UID（4字节原始数据）
  * @param  user_id:   用户ID（32位无符号整数）
  * @param  user_name: 用户名（字符串，最大12字节）
  * @param  level:     用户等级（如：1-普通，2-会员，3-VIP等）
  * @param  balance:   当前余额（单位：分）
  * @retval None
  * @note   用于实时上报用户刷卡消费事件
  *         帧类型：FRAME_CARD_TAP (0x10)
  *         数据格式：UID(4) + UserID(4) + UserName(12) + Level(1) + Balance(4) + DeviceID(10)
  *         总长度：35字节
  * @example
  *   uint8_t uid[4] = {0xA1, 0xB2, 0xC3, 0xD4};
  *   Cloud_ReportCardTap(uid, 10001, "张三", 2, 50000);
  */
void Cloud_ReportCardTap(uint8_t *uid, uint32_t user_id,
                         char *user_name, uint8_t level, uint32_t balance);

/**
  * @brief  上报充值完成事件到云端
  * @param  uid:         卡片UID（4字节原始数据）
  * @param  amount:      充值金额（单位：分）
  * @param  new_balance: 充值后的新余额（单位：分）
  * @retval None
  * @note   用于通知云端充值已成功执行
  *         帧类型：FRAME_HEARTBEAT (0x12) - 复用心跳帧类型
  *         数据格式：UID(4) + Amount(4) + NewBalance(4)
  *         总长度：12字节
  * @example
  *   Cloud_ReportRechargeComplete(uid, 10000, 60000);  // 充值100元，新余额600元
  */
void Cloud_ReportRechargeComplete(uint8_t *uid, uint32_t amount, uint32_t new_balance);

/**
  * @brief  获取指定UID的待处理充值金额
  * @param  uid: 卡片UID（4字节原始数据）
  * @retval 充值金额（单位：分）
  *         0: 没有待处理充值或充值金额为0
  * @note   从待处理队列中查找匹配的UID
  *         找到后自动从队列中移除该充值指令
  *         通常在使用者刷卡时调用，自动消费充值
  * @example
  *   uint32_t amount = Cloud_GetPendingRecharge(uid);
  *   if (amount > 0) {
  *       // 执行充值逻辑
  *       AddBalance(uid, amount);
  *   }
  */
uint32_t Cloud_GetPendingRecharge(uint8_t *uid);

/**
  * @brief  发送心跳包到云端
  * @param  None
  * @retval None
  * @note   用于保持设备与云端的连接活跃
  *         帧类型：FRAME_HEARTBEAT (0x12)
  *         数据格式：DeviceID(10) + PendingFlag(1) + Reserved(1)
  *         总长度：12字节
  *         PendingFlag: 1表示有待处理的充值指令，0表示无
  * @warning 建议每隔30-60秒调用一次，防止云端超时断开连接
  * @example
  *   // 在主循环中定时调用
  *   if (heartbeat_timer >= 30000) {  // 30秒
  *       Cloud_SendHeartbeat();
  *       heartbeat_timer = 0;
  *   }
  */
void Cloud_SendHeartbeat(void);

/**
  * @brief  处理云端下发的待处理充值指令
  * @param  None
  * @retval None
  * @note   应在主循环中周期性调用（建议每隔100ms调用一次）
  *         从协议层获取所有下发的帧，如果是充值命令则加入待处理队列
  *         充值指令格式：UID(4) + UserID(4) + Amount(4) + OrderNo(32)
  *         总长度：44字节
  *         队列大小由 MAX_RECHARGE_QUEUE 控制（默认8条）
  * @warning 如果队列已满，新的充值指令将被丢弃
  * @example
  *   // 在主循环中调用
  *   while (1) {
  *       Cloud_ProcessPendingRecharges();
  *       // 其他任务...
  *       delay_ms(100);
  *   }
  */
void Cloud_ProcessPendingRecharges(void);

#endif /* CLOUD_BRIDGE_H */
