# smart/utils/card_sync.py
from decimal import Decimal
import logging

from django.conf import settings
from django.db import transaction  # 修复：正确导入 transaction

from .onenet_auth import get_device_properties
from smart.models import RfidCard, Bill, CardBinding

# 获取日志记录器
logger = logging.getLogger(__name__)

def sync_card_data_from_onenet():
    """
    从 OneNET 拉取最新刷卡数据，同步到 Django 数据库

    功能说明：
        1. 从 OneNET 获取设备属性（卡号、余额、消费金额、用户ID）
        2. 同步到 RfidCard 表
        3. 如果有消费，写入 Bill 表
        4. 返回同步结果

    """
    try:
        # 1. 从 OneNET 获取设备数据
        result = get_device_properties(
            settings.ONENET_PRODUCT_ID,
            settings.ONENET_DEVICE_NAME
        )

        if not result.get("success"):
            logger.warning(f"OneNET 查询失败: {result.get('msg')}")
            return {
                "success": False,
                "msg": result.get("msg", "OneNET 查询失败")
            }

        data = result.get("data", {})

        # 2. 提取数据
        card_uid = data.get("card_uid", "")
        balance_str = data.get("balance", "0")
        total_price_str = data.get("total_price", "0")
        user_id = data.get("user_id", 0)

        # 验证数据有效性
        if not card_uid:
            logger.info("暂无刷卡数据")
            return {
                "success": False,
                "msg": "暂无刷卡数据"
            }

        # 3. 转换数据类型
        new_balance = Decimal(balance_str)
        total_price = Decimal(total_price_str)

        # 4. 获取或创建 RfidCard 记录
        card, created = RfidCard.objects.get_or_create(
            uid=card_uid,
            defaults={"balance": new_balance}
        )
        old_balance = card.balance

        # 5. 确定用户标识（用于 Bill 记录）
        user_identifier = card_uid  # 默认使用卡号

        # 查找绑定的用户
        binding = CardBinding.objects.filter(card=card, status='active').first()
        if binding and binding.user:
            user_identifier = binding.user.openid  # 修复：这里 safe 访问
            logger.info(f"找到绑定用户: {binding.user.nickname} ({user_identifier})")
        else:
            logger.info(f"卡 {card_uid} 未绑定用户，使用卡号作为标识")

        # 6. 处理消费记录（余额发生变化且有消费金额）
        if old_balance != new_balance and total_price > 0:
            # 写入 Bill 表
            Bill.objects.create(
                user_id=user_identifier,
                type='expense',
                type_text='食堂消费',
                amount=total_price,
                balance_after=new_balance,
                description=f'卡号 {card_uid} 消费 {total_price}元'
            )
            logger.info(f"创建消费记录: 卡 {card_uid}, 金额 {total_price}元")

        # 7. 更新卡余额
        if created or old_balance != new_balance:
            card.balance = new_balance
            card.save()
            logger.info(f"更新卡余额: {card_uid}, 旧余额 {old_balance}, 新余额 {new_balance}")

        # 8. 返回成功结果
        return {
            "success": True,
            "msg": "同步成功",
            "data": {
                "card_uid": card_uid,
                "balance": str(card.balance),
                "user_id": user_id,
                "total_price": total_price_str,
                "created": created,
                "balance_changed": old_balance != new_balance
            }
        }

    except RfidCard.DoesNotExist:
        logger.error(f"RFID卡不存在: {card_uid}")
        return {"success": False, "msg": "RFID卡不存在"}

    except Exception as e:
        logger.error(f"同步数据失败: {str(e)}", exc_info=True)
        return {"success": False, "msg": f"同步失败: {str(e)}"}


def sync_card_data_from_onenet_v2():
    """
    增强版：支持批量同步和更详细的日志
    """
    try:
        result = get_device_properties(
            settings.ONENET_PRODUCT_ID,
            settings.ONENET_DEVICE_NAME
        )

        if not result.get("success"):
            return {"success": False, "msg": result.get("msg", "OneNET 查询失败")}

        data = result.get("data", {})
        card_uid = data.get("card_uid", "")
        balance_str = data.get("balance", "0")
        total_price_str = data.get("total_price", "0")
        user_id = data.get("user_id", 0)

        if not card_uid:
            return {"success": False, "msg": "暂无刷卡数据"}

        new_balance = Decimal(balance_str)
        total_price = Decimal(total_price_str)

        # 使用事务保证数据一致性
        with transaction.atomic():
            # 获取或创建卡
            card, created = RfidCard.objects.get_or_create(
                uid=card_uid,
                defaults={"balance": new_balance}
            )
            old_balance = card.balance

            # 获取绑定的用户信息（使用 select_related 优化查询）
            binding = CardBinding.objects.filter(
                card=card,
                status='active'
            ).select_related('user').first()

            user_info = {
                'openid': card_uid,
                'nickname': '未知用户',
                'is_bound': False
            }

            if binding and binding.user:
                user_info = {
                    'openid': binding.user.openid,
                    'nickname': binding.user.nickname or '微信用户',
                    'is_bound': True
                }

            # 记录消费
            if old_balance != new_balance and total_price > 0:
                Bill.objects.create(
                    user_id=user_info['openid'],
                    type='expense',
                    type_text='食堂消费',
                    amount=total_price,
                    balance_after=new_balance,
                    description=f'卡号 {card_uid} 消费 {total_price}元，用户: {user_info["nickname"]}'
                )

            # 更新余额
            if created or old_balance != new_balance:
                card.balance = new_balance
                card.save()

            return {
                "success": True,
                "msg": "同步成功",
                "data": {
                    "card_uid": card_uid,
                    "balance": str(card.balance),
                    "user_id": user_id,
                    "total_price": total_price_str,
                    "user_info": user_info,
                    "is_new_card": created,
                    "balance_changed": old_balance != new_balance
                }
            }

    except RfidCard.DoesNotExist:
        return {"success": False, "msg": "RFID卡不存在"}
    except Exception as e:
        logger.error(f"同步失败: {str(e)}", exc_info=True)
        return {"success": False, "msg": f"同步失败: {str(e)}"}


# ==================== 定时任务函数（配合 Celery） ====================

def sync_card_data_periodic():
    """
    定时同步任务（用于 Celery 定时任务）
    每 5 秒执行一次
    """
    result = sync_card_data_from_onenet()

    if result.get("success"):
        logger.info(f"定时同步成功: {result.get('data')}")
    else:
        logger.warning(f"定时同步失败: {result.get('msg')}")

    return result


def sync_card_data_from_onenet_with_history():
    """
    带历史记录的同步（记录每次变化）
    """
    try:
        from smart.models import CardTransaction  # 延迟导入避免循环引用

        result = get_device_properties(
            settings.ONENET_PRODUCT_ID,
            settings.ONENET_DEVICE_NAME
        )

        if not result.get("success"):
            return {"success": False, "msg": result.get("msg", "OneNET 查询失败")}

        data = result.get("data", {})
        card_uid = data.get("card_uid", "")
        balance_str = data.get("balance", "0")
        total_price_str = data.get("total_price", "0")
        user_id = data.get("user_id", 0)

        if not card_uid:
            return {"success": False, "msg": "暂无刷卡数据"}

        new_balance = Decimal(balance_str)
        total_price = Decimal(total_price_str)

        with transaction.atomic():
            card, created = RfidCard.objects.get_or_create(
                uid=card_uid,
                defaults={"balance": new_balance}
            )
            old_balance = card.balance

            # 获取绑定的用户
            binding = CardBinding.objects.filter(
                card=card,
                status='active'
            ).select_related('user').first()
            user = binding.user if binding else None

            # 记录交易
            if old_balance != new_balance:
                # 确定交易类型
                trans_type = 'deduct' if total_price > 0 else 'recharge'
                trans_amount = total_price if total_price > 0 else (new_balance - old_balance)

                # 创建交易记录
                CardTransaction.objects.create(
                    card=card,
                    user=user,
                    transaction_type=trans_type,
                    amount=abs(trans_amount),  # 使用绝对值
                    balance_before=old_balance,
                    balance_after=new_balance,
                    description=f'设备刷卡同步 - 卡 {card_uid}'
                )

                # 创建账单记录
                Bill.objects.create(
                    user_id=user.openid if user else card_uid,
                    type='expense' if total_price > 0 else 'income',
                    type_text='食堂消费' if total_price > 0 else '其他',
                    amount=abs(trans_amount),
                    balance_after=new_balance,
                    description=f'卡号 {card_uid} {"消费" if total_price > 0 else "充值"} {abs(trans_amount)}元'
                )

                # 更新余额
                card.balance = new_balance
                card.save()

            return {
                "success": True,
                "msg": "同步成功",
                "data": {
                    "card_uid": card_uid,
                    "balance": str(card.balance),
                    "old_balance": str(old_balance),
                    "user_id": user_id,
                    "total_price": total_price_str,
                    "is_new_card": created,
                    "balance_changed": old_balance != new_balance
                }
            }

    except Exception as e:
        logger.error(f"同步失败: {str(e)}", exc_info=True)
        return {"success": False, "msg": f"同步失败: {str(e)}"}


# ==================== 安全辅助函数 ====================

def safe_get_user_openid(binding):
    """
    安全获取用户 openid（避免 None 访问错误）
    """
    if binding and binding.user:
        return binding.user.openid
    return None


def safe_get_user_nickname(binding):
    """
    安全获取用户昵称（避免 None 访问错误）
    """
    if binding and binding.user:
        return binding.user.nickname or '微信用户'
    return '未知用户'