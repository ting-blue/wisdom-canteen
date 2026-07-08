# ==================== 导入模块 ====================

from django.http import JsonResponse
from django.views.decorators.csrf import csrf_exempt
from django.views.decorators.http import require_http_methods
from django.utils import timezone
from django.conf import settings
from rest_framework.response import Response
from rest_framework.viewsets import GenericViewSet
from rest_framework.mixins import ListModelMixin
from rest_framework.decorators import api_view
from .utils.ai_utils import call_qwen_api

from django.db import transaction

from smart.utils.onenet_auth import get_device_properties, get_device_status,send_command_to_onenet

from smart.utils.card_sync import sync_card_data_from_onenet



import base64
import json
import hashlib
import time
import logging
import requests
from datetime import datetime, timedelta
from decimal import Decimal
from pathlib import Path

from PIL import Image
from io import BytesIO


# 导入模型（使用别名避免命名冲突）
from .models import (
    Welcome as WelcomeModel,
    Banners,
    Notice,
    Food,
    UserBalance,
    Bill,
    UserMealRecord,
    User,  # 添加 User 模型导入
    RfidCard,
    CardBinding,
    BindWaitQueue,
    CardTransaction,
    CardRecord,
    ChatHistory,
    PendingCommand
)

# 导入序列化器
from .serializer import BannersSerializer, NoticeSerializer, FoodSerializer

# 导入工具函数
from .utils.ai_utils import (
    call_qwen_api,
#    recommend_recipe_by_ingredients,
    analyze_nutrition,
    get_healthy_tips
)

logger = logging.getLogger(__name__)

# ==================== 工具函数 ====================

def generate_token(user_id):
    """生成用户 token（使用 user_id + 时间戳 + 随机字符串）"""
    raw = f"{user_id}_{time.time()}_{hashlib.md5(str(user_id).encode()).hexdigest()}"
    return hashlib.sha256(raw.encode()).hexdigest()

def _get_bmi_status(bmi):
    """判断 BMI 状态（内部函数）"""
    if bmi < 18.5:
        return '偏瘦'
    if bmi < 24:
        return '正常'
    if bmi < 28:
        return '超重'
    return '肥胖'


# ==================== 欢迎图片接口 ====================
def get_welcome_image(request):
    """获取欢迎页面图片（取 order 字段值最小的那张）"""
    # 1. 查出 order 最小的一张图片（order_by默认升序，first()取第一个）
    res = WelcomeModel.objects.all().order_by('order').first()

    # 2. 处理查询结果为空的情况
    if not res:
        return JsonResponse({'code': 404, 'msg': '没有找到欢迎图片', 'result': None})

    # 3. 获取图片的完整URL（Django会自动加上MEDIA_URL前缀）
    img_url = res.img.url  # 返回类似 /media/xxx.jpg

    return JsonResponse({
        'code': 100,
        'msg': '成功',
        'result': {
            'id': res.id,
            'img': img_url,
            'order': res.order
        }
    })


def image_to_base64(image_field, default_image=None):
    """
    将图片字段转换为 Base64 编码
    """
    try:
        if not image_field:
            return default_image or ""

        # 获取图片路径
        if hasattr(image_field, 'path'):
            img_path = image_field.path
        elif hasattr(image_field, 'url'):
            # 如果是 URL，尝试从本地获取
            img_path = str(Path(settings.MEDIA_ROOT) / image_field.name)
        else:
            return default_image or ""

        # 检查文件是否存在
        if not Path(img_path).exists():
            logger.warning(f"图片文件不存在: {img_path}")
            return default_image or ""

        # 缩略图 + 编码
        img = Image.open(img_path)
        img.thumbnail((400, 400))
        buf = BytesIO()
        img.convert('RGB').save(buf, 'JPEG', quality=80)
        img_b64 = base64.b64encode(buf.getvalue()).decode('utf-8')
        return 'data:image/jpeg;base64,' + img_b64

    except FileNotFoundError:
        logger.error(f"图片文件未找到: {image_field}")
        return default_image or ""
    except Exception as e:
        logger.error(f"图片转 Base64 失败: {str(e)}")
        return default_image or ""


# ==================== 轮播图视图集 ====================
class BannersViewSet(GenericViewSet, ListModelMixin):
    """轮播图视图集：获取前3张未删除的轮播图"""
    queryset = Banners.objects.filter(is_delete=False).order_by('order')[:3]
    serializer_class = BannersSerializer

    def list(self, request, *args, **kwargs):
        """
        获取轮播图列表（返回 Base64 编码的图片）
        """
        try:
            queryset = self.get_queryset()
            data = []

            for banner in queryset:  # ✅ 修复：只遍历一次
                # 图片转 Base64
                img_b64 = image_to_base64(banner.img)

                data.append({
                    'id': banner.id,
                    'order': banner.order,
                    'img_b64': img_b64,
                    'img_url': banner.img.url if banner.img else '',  # 保留 URL 供参考
                    'is_delete': banner.is_delete,
                    'create_time': banner.create_time.strftime('%Y-%m-%d %H:%M:%S')
                })

            # 获取最新通知
            notice = Notice.objects.all().order_by('-create_time').first()

            return Response({
                'code': 100,
                'msg': '成功',
                'banners': data,
                'notice': NoticeSerializer(notice).data if notice else None
            })

        except Exception as e:
            logger.error(f"获取轮播图失败: {str(e)}", exc_info=True)
            return Response({
                'code': 500,
                'msg': f'获取轮播图失败: {str(e)}',
                'banners': [],
                'notice': None
            })


# ==================== 食物推荐视图集 ====================
class FoodViewSet(GenericViewSet, ListModelMixin):
    """食物推荐视图集：获取所有未删除的食物"""
    queryset = Food.objects.filter(is_delete=False).order_by('order', '-create_time')
    serializer_class = FoodSerializer

    def list(self, request, *args, **kwargs):
        """
        获取食物列表（返回 Base64 编码的图片）
        """
        try:
            queryset = self.get_queryset()
            data = []

            for food in queryset:
                # 基础数据
                item = {
                    'id': food.id,
                    'name': food.name,
                    'price': str(food.price),
                    'calorie_per_100g': food.calorie_per_100g,
                    'protein_per_100g': food.protein_per_100g,
                    'fat_per_100g': food.fat_per_100g,
                    'carb_per_100g': food.carb_per_100g,
                    'default_weight': food.default_weight,
                    'spiciness': food.spiciness,
                    'rating': str(food.rating),
                    'description': food.description,
                    'order': food.order,
                    'sauce_calorie': food.sauce_calorie,
                    'oil_calorie': food.oil_calorie,
                }

                # 图片转 Base64
                item['img_b64'] = image_to_base64(food.img)

                # 保留原始 URL（可选）
                item['img_url'] = food.img.url if food.img else ''

                data.append(item)

            return Response({
                'code': 100,
                'msg': '成功',
                'foods': data,
                'total': len(data)
            })

        except Exception as e:
            logger.error(f"获取食物列表失败: {str(e)}", exc_info=True)
            return Response({
                'code': 500,
                'msg': f'获取食物列表失败: {str(e)}',
                'foods': []
            })

# ==================== 余额和账单接口 ====================
@csrf_exempt
@require_http_methods(["GET"])
def get_balance(request):
    """获取用户余额（测试用户默认为 test_user）"""
    user_id = request.GET.get('user_id', 'test_user')
    balance_obj, created = UserBalance.objects.get_or_create(user_id=user_id)

    return JsonResponse({
        'code': 100,
        'msg': '成功',
        'balance': float(balance_obj.balance)
    })


@csrf_exempt
@require_http_methods(["GET"])
def get_bills(request):
    """获取用户账单列表（最多50条）"""
    user_id = request.GET.get('user_id', 'test_user')
    bills = Bill.objects.filter(user_id=user_id)[:50]

    bills_data = []
    for bill in bills:
        bills_data.append({
            'id': bill.id,
            'type': bill.type,
            'typeText': bill.type_text,
            'amount': float(bill.amount),
            'balance_after': float(bill.balance_after),
            'description': bill.description,
            'create_time': bill.create_time.strftime('%Y-%m-%d %H:%M:%S')
        })

    return JsonResponse({
        'code': 100,
        'msg': '成功',
        'bills': bills_data
    })


@csrf_exempt
@require_http_methods(["POST"])
def recharge(request):
    """用户充值接口"""
    try:
        data = json.loads(request.body)
        user_id = data.get('user_id', 'test_user')
        amount = float(data.get('amount', 0))

        if amount <= 0:
            return JsonResponse({'code': 400, 'msg': '金额必须大于0'})

        # 获取或创建余额记录
        balance_obj, created = UserBalance.objects.get_or_create(user_id=user_id)
        old_balance = float(balance_obj.balance)
        new_balance = old_balance + amount
        balance_obj.balance = new_balance
        balance_obj.save()

        # 创建账单记录
        Bill.objects.create(
            user_id=user_id,
            type='income',
            type_text='充值',
            amount=amount,
            balance_after=new_balance,
            description=f'充值{amount}元'
        )

        return JsonResponse({
            'code': 100,
            'msg': '充值成功',
            'balance': new_balance
        })


    except json.JSONDecodeError:

        return JsonResponse({'code': 400, 'msg': '请求数据格式错误'})

    except (ValueError, TypeError) as e:

        return JsonResponse({'code': 400, 'msg': f'参数错误: {str(e)}'})

    except UserBalance.DoesNotExist:

        return JsonResponse({'code': 404, 'msg': '余额记录不存在'})

    except Exception as e:

        return JsonResponse({'code': 500, 'msg': f'服务器错误: {str(e)}'})


@csrf_exempt
@require_http_methods(["POST"])
def withdraw(request):
    """用户提现接口（需要检查余额是否充足）"""
    try:
        data = json.loads(request.body)
        user_id = data.get('user_id', 'test_user')
        amount = float(data.get('amount', 0))

        if amount <= 0:
            return JsonResponse({'code': 400, 'msg': '金额必须大于0'})

        balance_obj = UserBalance.objects.get(user_id=user_id)
        old_balance = float(balance_obj.balance)

        if amount > old_balance:
            return JsonResponse({'code': 400, 'msg': '余额不足'})

        new_balance = old_balance - amount
        balance_obj.balance = new_balance
        balance_obj.save()

        # 创建账单记录
        Bill.objects.create(
            user_id=user_id,
            type='expense',
            type_text='提现',
            amount=amount,
            balance_after=new_balance,
            description=f'提现{amount}元'
        )

        return JsonResponse({
            'code': 100,
            'msg': '提现申请已提交',
            'balance': new_balance
        })


    except json.JSONDecodeError:

        return JsonResponse({'code': 400, 'msg': '请求数据格式错误'})

    except (ValueError, TypeError) as e:

        return JsonResponse({'code': 400, 'msg': f'参数错误: {str(e)}'})

    except UserBalance.DoesNotExist:

        return JsonResponse({'code': 404, 'msg': '余额记录不存在'})

    except Exception as e:

        return JsonResponse({'code': 500, 'msg': f'服务器错误: {str(e)}'})


# ==================== AI 相关接口 ====================
def build_health_context(health_info):
    """构建健康档案的文字描述"""
    if not health_info or not any(health_info.values()):
        return "用户未提供健康档案信息，请根据一般健康饮食原则推荐。"

    context_parts = ["用户健康档案："]

    # 基础信息
    if health_info.get('height') and health_info.get('weight'):
        height = health_info['height']
        weight = health_info['weight']
        bmi = weight / ((height / 100) ** 2)
        bmi_status = _get_bmi_status(bmi)
        context_parts.append(f"- 身高{height}cm，体重{weight}kg，BMI指数{bmi:.1f}（{bmi_status}）")

    if health_info.get('age'):
        context_parts.append(f"- 年龄{health_info['age']}岁")

    if health_info.get('gender'):
        gender_text = '男' if health_info['gender'] == 'male' else '女'
        context_parts.append(f"- 性别：{gender_text}")

    # 活动水平
    activity_map = {
        'sedentary': '久坐少动（很少运动）',
        'light': '轻度活动（每周运动1-2次）',
        'moderate': '中度活动（每周运动3-4次）',
        'active': '非常活跃（每天运动）'
    }
    activity = health_info.get('activity_level')
    if activity:
        context_parts.append(f"- 活动水平：{activity_map.get(activity, activity)}")

    # 饮食目标
    goal_map = {
        'lose': '减脂（需要控制热量，增加蛋白质）',
        'maintain': '保持体重（均衡营养）',
        'gain': '增肌（需要增加蛋白质和适量碳水）'
    }
    goal = health_info.get('goal')
    if goal:
        context_parts.append(f"- 饮食目标：{goal_map.get(goal, goal)}")

    # 忌口
    allergies = health_info.get('allergies', [])
    if allergies:
        allergy_names = {
            'peanuts': '花生', 'seafood': '海鲜', 'milk': '乳制品',
            'gluten': '麸质', 'soy': '大豆', 'egg': '鸡蛋'
        }
        allergy_text = '、'.join([allergy_names.get(a, str(a)) for a in allergies])
        context_parts.append(f"- ⚠️ 忌口/过敏：{allergy_text}（绝对不能推荐这些食材）")

    # 疾病
    diseases = health_info.get('diseases', [])
    if diseases:
        disease_advice = {
            'diabetes': '控制碳水，避免高升糖指数食物',
            'hypertension': '控制钠摄入，少吃盐和腌制食品',
            'hyperlipidemia': '减少饱和脂肪和胆固醇',
            'gout': '避免高嘌呤食物（内脏、海鲜、浓汤）',
            'gi': '选择易消化的食物，避免辛辣刺激',
            'kidney': '控制蛋白质和磷的摄入'
        }
        disease_list = [disease_advice.get(d, '') for d in diseases if disease_advice.get(d)]
        if disease_list:
            context_parts.append(f"- 🏥 注意事项：{'；'.join(disease_list)}")

    return '\n'.join(context_parts)


@csrf_exempt
@require_http_methods(["POST"])
def ai_recommend_recipe(request):
    """AI推荐食谱：根据用户输入的食材和健康档案推荐菜谱"""
    try:
        data = json.loads(request.body)
        user_input = data.get("message", "")
        health_info = data.get("health_info", {})  # 接收健康档案

        if not user_input:
            return JsonResponse({"code": 400, "msg": "请输入内容"})

        # 构建健康档案上下文
        health_context = build_health_context(health_info)

        # 构建系统提示词
        system_prompt = f"""你是智慧食堂的AI营养师助手，需要根据用户的健康状况提供个性化的饮食建议。

{health_context}

请遵循以下原则：
1. **个性化推荐**：根据用户的BMI、年龄、性别、活动水平给出定制建议
2. **目标导向**：围绕用户的饮食目标（减脂/保持/增肌）推荐
3. **避开忌口**：绝对不要推荐用户忌口的食物
4. **注意疾病**：考虑用户的健康状况，给出适合的食材建议
5. **热量控制**：根据活动水平估算每日所需热量

请用友好、专业的语气回答，给出2-3个具体的食谱推荐，并说明推荐理由。"""

        # 调用 AI
        result = call_qwen_api(user_input, system_prompt)

        if result["success"]:
            return JsonResponse({
                "code": 100,
                "msg": "成功",
                "data": {"reply": result["content"]}
            })
        else:
            return JsonResponse({"code": 500, "msg": result.get("error", "AI服务异常")})

    except (json.JSONDecodeError, KeyError) as e:
        return JsonResponse({"code": 400, "msg": f"请求参数错误: {str(e)}"})
    except Exception as e:
        return JsonResponse({"code": 500, "msg": f"服务器内部错误: {str(e)}"})


@csrf_exempt
@require_http_methods(["POST"])
def ai_analyze_nutrition(request):
    """AI营养分析：分析菜品的营养成分"""
    try:
        data = json.loads(request.body)
        dish_name = data.get("dish_name", "")

        if not dish_name:
            return JsonResponse({"code": 400, "msg": "请输入菜品名称"})

        result = analyze_nutrition(dish_name)

        if result["success"]:
            return JsonResponse({
                "code": 100,
                "msg": "成功",
                "data": {"reply": result["content"]}
            })
        else:
            return JsonResponse({"code": 500, "msg": result.get("error", "AI服务异常")})

    except Exception as e:
        return JsonResponse({"code": 500, "msg": str(e)})


@csrf_exempt
@require_http_methods(["GET"])
def ai_health_tips(request):
    """获取健康小贴士（随机返回一条健康建议）"""
    result = get_healthy_tips()

    if result["success"]:
        return JsonResponse({
            "code": 100,
            "msg": "成功",
            "data": {"tip": result["content"]}
        })
    else:
        return JsonResponse({"code": 500, "msg": result.get("error", "AI服务异常")})


@csrf_exempt
@require_http_methods(["POST"])
def ai_recommend_from_menu(request):
    """根据现有菜单推荐食谱（基于食堂现有菜品）"""
    try:
        data = json.loads(request.body)
        menu_items = data.get("menu_items", [])
        user_preference = data.get("preference", "")

        if not menu_items:
            return JsonResponse({"code": 400, "msg": "请提供菜单数据"})

        # 构建菜品信息文本
        menu_text = "\n".join(
            [f"- {item.get('name', '')}：{item.get('calorie_per_100g', 0)}千卡/100g"
             for item in menu_items]
        )

        system_prompt = """你是智慧食堂的AI营养师助手。根据用户食堂现有的菜品，推荐一份营养均衡的餐单。
        你需要考虑：
        1. 营养均衡（蛋白质、碳水、脂肪、维生素）
        2. 热量控制
        3. 口味搭配
        4. 给出具体的搭配建议和理由

        请用友好的语气回答，推荐2-3种不同的搭配方案。"""

        if user_preference:
            user_message = f"食堂现有以下菜品：\n{menu_text}\n\n用户偏好：{user_preference}\n\n请根据这些菜品，推荐一份营养均衡的餐单。"
        else:
            user_message = f"食堂现有以下菜品：\n{menu_text}\n\n请根据这些菜品，推荐一份营养均衡的餐单。"

        result = call_qwen_api(user_message, system_prompt)

        if result["success"]:
            return JsonResponse({
                "code": 100,
                "msg": "成功",
                "data": {"reply": result["content"]}
            })
        else:
            return JsonResponse({"code": 500, "msg": result.get("error", "AI服务异常")})

    except Exception as e:
        return JsonResponse({"code": 500, "msg": str(e)})


@csrf_exempt
@require_http_methods(["POST"])
def ai_analyze_meal(request):
    """分析用户选择的餐品组合（营养评估）"""
    try:
        data = json.loads(request.body)
        selected_items = data.get("selected_items", [])

        if not selected_items:
            return JsonResponse({"code": 400, "msg": "请选择餐品"})

        # 构建餐品信息文本
        items_text = "\n".join(
            [f"- {item.get('name', '')}：{item.get('calorie_per_100g', 0)}千卡/100g，约{item.get('weight', 200)}克"
             for item in selected_items]
        )

        system_prompt = """你是智慧食堂的AI营养师助手。分析用户选择的餐品组合，给出：
        1. 总热量估算
        2. 营养分析（蛋白质、脂肪、碳水的比例）
        3. 健康评分（满分100分）
        4. 改进建议

        请用友好的语气回答，简洁明了。"""

        user_message = f"用户选择了以下餐品：\n{items_text}\n\n请分析这份餐单的营养情况。"

        result = call_qwen_api(user_message, system_prompt)

        if result["success"]:
            return JsonResponse({
                "code": 100,
                "msg": "成功",
                "data": {"analysis": result["content"]}
            })
        else:
            return JsonResponse({"code": 500, "msg": result.get("error", "AI服务异常")})

    except Exception as e:
        return JsonResponse({"code": 500, "msg": str(e)})


# ==================== 用餐记录接口 ====================
@csrf_exempt
@require_http_methods(["POST"])
def add_meal_record(request):
    """记录用户吃过的菜品"""
    try:
        data = json.loads(request.body)
        user_id = data.get('user_id', 'test_user')
        food_id = data.get('food_id')
        food_name = data.get('food_name')
        food_img = data.get('food_img', '')
        price = data.get('price', 0)
        weight = data.get('weight', 200)
        calorie = data.get('calorie', 0)
        meal_type = data.get('meal_type', 'lunch')

        if not food_id or not food_name:
            return JsonResponse({'code': 400, 'msg': '缺少必要参数'})

        record = UserMealRecord.objects.create(
            user_id=user_id,
            food_id=food_id,
            food_name=food_name,
            food_img=food_img,
            price=price,
            weight=weight,
            calorie=calorie,
            meal_type=meal_type
        )

        return JsonResponse({
            'code': 100,
            'msg': '记录成功',
            'data': {'record_id': record.id}
        })

    except Exception as e:
        return JsonResponse({'code': 500, 'msg': str(e)})


@csrf_exempt
@require_http_methods(["GET"])
def get_meal_records(request):
    """获取用户用餐记录（支持按天数筛选）"""
    user_id = request.GET.get('user_id', 'test_user')
    days = int(request.GET.get('days', 7))

    start_date = datetime.now().date() - timedelta(days=days)
    records = UserMealRecord.objects.filter(
        user_id=user_id,
        eat_date__gte=start_date
    ).order_by('-eat_time')

    records_data = []
    for record in records:
        meal_type_text = record.get_meal_type_display() if hasattr(record,'get_meal_type_display') \
            else record.meal_type

        records_data.append({
            'id': record.id,
            'food_id': record.food_id,
            'food_name': record.food_name,
            'food_img': record.food_img,
            'price': float(record.price),
            'weight': record.weight,
            'calorie': record.calorie,
            'meal_type': record.meal_type,
            'meal_type_text': meal_type_text,
            'eat_date': record.eat_date.strftime('%Y-%m-%d'),
            'eat_time': record.eat_time.strftime('%H:%M')
        })

    # 按日期分组
    grouped = {}
    for record in records_data:
        date = record['eat_date']
        if date not in grouped:
            grouped[date] = []
        grouped[date].append(record)

    return JsonResponse({
        'code': 100,
        'msg': '成功',
        'data': {
            'records': records_data,
            'grouped': grouped,
            'total_days': len(grouped),
            'total_calorie': sum(r['calorie'] for r in records_data),
            'total_count': len(records_data)
        }
    })


@csrf_exempt
@require_http_methods(["POST"])
def delete_meal_record(request):
    """删除指定用餐记录"""
    try:
        data = json.loads(request.body)
        record_id = data.get('record_id')
        user_id = data.get('user_id', 'test_user')

        record = UserMealRecord.objects.filter(id=record_id, user_id=user_id).first()
        if not record:
            return JsonResponse({'code': 404, 'msg': '记录不存在'})

        record.delete()
        return JsonResponse({'code': 100, 'msg': '删除成功'})

    except Exception as e:
        return JsonResponse({'code': 500, 'msg': str(e)})


# ==================== 微信登录接口 ====================
@csrf_exempt
@require_http_methods(["POST"])
def wx_login(request):
    """微信小程序登录：用 code 换取 openid 和 token"""
    try:
        # 解析前端传来的数据
        body = json.loads(request.body)
        code = body.get('code')
        user_info = body.get('userInfo', {})
        if not code:
            return JsonResponse({'code': 400, 'msg': '缺少code参数'})

        # 向微信服务器请求 openid 和 session_key
        url = 'https://api.weixin.qq.com/sns/jscode2session'
        params = {
            'appid': settings.WECHAT_APPID,
            'secret': settings.WECHAT_SECRET,
            'js_code': code,
            'grant_type': 'authorization_code'
        }
        resp = requests.get(url, params=params, timeout=10).json()

        if 'errcode' in resp and resp['errcode'] != 0:
            return JsonResponse({'code': 500, 'msg': resp.get('errmsg', '微信登录失败')})

        openid = resp.get('openid')
        if not openid:
            return JsonResponse({'code': 500, 'msg':'获取openid失败'})

        # 查询或创建用户
        user, created = User.objects.get_or_create(openid=openid)

        # 更新用户信息（前端传来的头像和昵称）
        if user_info.get('nickName'):
            user.nickname = user_info.get('nickName')
        if user_info.get('avatarUrl'):
            user.avatar = user_info.get('avatarUrl')

        # 生成 token
        token = generate_token(user.id)
        user.token = token
        user.save()

        return JsonResponse({
            'code': 100,
            'msg': '登录成功',
            'token': token,
            'userInfo': {
                'id': user.id,
                'nickname': user.nickname or '微信用户',
                'avatar': user.avatar or '',
                'openid': user.openid,
                'is_profile_completed': user.is_profile_completed
            }
        })
    except Exception as e:
        return JsonResponse({'code': 500, 'msg': str(e)})


@csrf_exempt
@require_http_methods(["POST"])
def check_token(request):
    """检查用户 token 是否有效"""
    try:
        data = json.loads(request.body)
        token = data.get('token')

        if not token:
            return JsonResponse({'code': 400, 'msg': '缺少token'})

        user = User.objects.filter(token=token, token_expire__gt=timezone.now()).first()

        if user:
            return JsonResponse({'code': 100, 'msg': 'token有效'})
        else:
            return JsonResponse({'code': 401, 'msg': 'token无效或已过期'})

    except Exception as e:
        return JsonResponse({'code': 500, 'msg': str(e)})


# ==================== 获取用户信息接口 ====================
@csrf_exempt
@require_http_methods(["GET"])
def get_user_profile(request):
    """获取当前登录用户的个人信息"""
    # 获取 token（从请求头）
    token = request.headers.get('Authorization')

    if not token:
        return JsonResponse({'code': 401, 'msg': '未登录，缺少token'})

    # 查找用户
    user = User.objects.filter(token=token).first()
    if not user:
        return JsonResponse({'code': 401, 'msg': '用户不存在或token无效'})

    # 返回用户信息
    return JsonResponse({
        'code': 100,
        'msg': '成功',
        'data': {
            'id': user.id,
            'nickname': user.nickname or '微信用户',
            'avatar': user.avatar or '/images/default-avatar.png',
            'username': user.username if hasattr(user, 'username') else '',
            'email': user.email if hasattr(user, 'email') else '',
            'score': getattr(user, 'score', 100),
            'is_profile_completed': user.is_profile_completed
        }
    })


@csrf_exempt
@require_http_methods(["POST"])
def update_profile(request):
    """更新用户头像和昵称"""
    try:
        # 获取 token（优先从请求头获取，没有再从 body 获取）
        token = request.headers.get('Authorization')
        if not token:
            data = json.loads(request.body)
            token = data.get('token')

        if not token:
            return JsonResponse({'code': 401, 'msg': '未登录'})

        # 查找用户
        user = User.objects.filter(token=token).first()
        if not user:
            return JsonResponse({'code': 401, 'msg': '用户不存在或已过期'})

        # 解析请求数据
        data = json.loads(request.body)

        if data.get('nickname'):
            user.nickname = data['nickname']
        if data.get('avatar'):
            user.avatar = data['avatar']

        user.save()

        return JsonResponse({
            'code': 100,
            'msg': '更新成功',
            'userInfo': {
                'id': user.id,
                'nickname': user.nickname,
                'avatar': user.avatar,
                'openid': user.openid
            }
        })

    except Exception as e:
        return JsonResponse({'code': 500, 'msg': str(e)})


@csrf_exempt
def health_info(request):
    """获取或保存用户健康信息"""
    # 获取 token（统一处理）
    token = request.headers.get('Authorization')
    if not token:
        # 兼容 body 中传 token 的方式
        if request.method == 'POST':
            try:
                data = json.loads(request.body)
                token = data.get('token')
            except:
                pass

    if not token:
        return JsonResponse({'code': 401, 'msg': '未登录'})

    user = User.objects.filter(token=token).first()
    if not user:
        return JsonResponse({'code': 401, 'msg': '用户不存在'})

    # ========== GET：获取健康信息 ==========
    if request.method == 'GET':
        return JsonResponse({
            'code': 100,
            'data': {
                'height': user.height,
                'weight': user.weight,
                'body_fat': user.body_fat,
                'age': user.age,
                'gender': user.gender,
                'activity_level': user.activity_level,
                'goal': user.diet_goal,
                'allergies': user.allergies.split(',') if user.allergies else [],
                'diseases': user.diseases.split(',') if user.diseases else []
            }
        })

    # ========== POST：保存健康信息 ==========
    if request.method == 'POST':
        try:
            data = json.loads(request.body)

            user.height = data.get('height')
            user.weight = data.get('weight')
            user.body_fat = data.get('body_fat')
            user.age = data.get('age')
            user.gender = data.get('gender')
            user.activity_level = data.get('activity_level')
            user.diet_goal = data.get('goal')
            user.allergies = ','.join(data.get('allergies', []))
            user.diseases = ','.join(data.get('diseases', []))
            user.is_profile_completed = True
            user.save()

            return JsonResponse({
                'code': 100,
                'msg': '保存成功',
                'userInfo': {
                    'id': user.id,
                    'nickname': user.nickname,
                    'avatar': user.avatar,
                    'is_profile_completed': user.is_profile_completed
                }
            })
        except Exception as e:
            return JsonResponse({'code': 500, 'msg': str(e)})

    # 其他请求方法
    return JsonResponse({'code': 405, 'msg': '方法不允许'})


"""
OneNET 数据接口视图
功能：为小程序提供设备数据查询 API
"""
@api_view(['GET'])
def get_dish_data(request):
    """
    获取最新菜品数据（供小程序调用）

    请求方式: GET
    返回格式:
        {
            "code": 100,
            "msg": "成功",
            "data": {
                "dish_name": "纯牛奶",
                "calorie": 132,
                "protein": 6,
                "weight": 200,
                "position": "top_left",
                "confidence": 0.56
            }
        }
    """
    product_id = settings.ONENET_PRODUCT_ID
    device_name = settings.ONENET_DEVICE_NAME

    result = get_device_properties(product_id, device_name)

    if result.get("success"):
        return Response({
            "code": 100,
            "msg": "成功",
            "data": result["data"]
        })
    else:
        return Response({
            "code": 500,
            "msg": result.get("msg", "获取数据失败")
        })


@api_view(['GET'])
def get_device_status(request):
    """
    获取设备在线状态

    返回格式:
        {
            "code": 100,
            "msg": "成功",
            "data": {
                "online": true,
                "last_time": "2026-06-17 19:49:02"
            }
        }
    """
    product_id = settings.ONENET_PRODUCT_ID
    device_name = settings.ONENET_DEVICE_NAME

    result = get_device_status(product_id, device_name)

    if result.get("success"):
        return Response({
            "code": 100,
            "msg": "成功",
            "data": {
                "online": result["online"],
                "last_time": result["last_time"]
            }
        })
    else:
        return Response({
            "code": 500,
            "msg": result.get("msg", "获取状态失败")
        })



def get_card_or_create(uid):
    """获取或创建卡记录"""
    card, created = RfidCard.objects.get_or_create(
        uid=uid,
        defaults={'balance': Decimal('0')}
    )
    return card, created


# ==================== 查询余额 ====================
def card_query(request):
    """
        查询RFID卡余额
        功能说明：
            根据传入的uid查询对应RFID卡的余额信息
    """
    uid = request.GET.get('uid') # 从GET请求中获取uid参数
    if not uid:
        return JsonResponse({'code': 1, 'message': '缺少 uid 参数'})

    # card, created = get_card_or_create(uid)
    card = RfidCard.objects.filter(uid=uid).first()
    if not card:
        return JsonResponse({'code': 0, 'data': {'balance': 0, 'is_new': True}})   # 新卡余额为0  # 标记为新卡
    return JsonResponse({'code': 0, 'data': {'uid': card.uid, 'balance': str(card.balance)}})


# 充值
@csrf_exempt
def card_recharge(request):
    """
        RFID卡充值
        ============================================================
        功能说明：
            为指定的RFID卡进行充值操作，如果卡不存在则自动创建
    """
    # 验证请求方法是否为POST
    if request.method != 'POST':
        return JsonResponse({'code': 1, 'message': '请使用 POST 请求'})

    try:
        data = json.loads(request.body)      # 解析请求体中的JSON数据
        uid = data.get('uid')                # 卡UID
        amount_str = data.get('amount')      # 充值金额（字符串格式）
        device_name = data.get('device_name', 'dish')  # 默认设备名

        if not uid or amount_str is None:
            return JsonResponse({'code': 1, 'message': '缺少 uid 或 amount 参数'})

        amount = Decimal(str(amount_str))
        if amount <= 0:
            return JsonResponse({'code': 1, 'message': '充值金额必须大于 0'})

        # card, created = RfidCard.objects.get_or_create(uid=uid, defaults={'balance': Decimal('0')})  # 获取或创建RFID卡记录
        # ===== 步骤1：更新数据库余额 =====
        # card, created = get_card_or_create(uid)
        # card.balance += amount  # 增加余额
        # card.save()
        #
        # # ===== 步骤2：通过 OneNET 下发指令到 ESP32 =====
        # # 指令格式: "recharge:20.00"
        # cmd_value = f"recharge:{amount:.2f}"
        #
        # command_result = send_command_to_onenet(device_name, cmd_value)
        # if command_result.get('code') == 0:
        #     return JsonResponse({
        #         'code': 0,
        #         'msg': '充值指令已下发',
        #         'data': {
        #             'uid': card.uid,
        #             'balance': str(card.balance),
        #             'command': cmd_value,
        #             'status': 'pending'  # 等待 ESP32 执行
        #         }
        #     })
        # else:
        #     # 数据库已更新，但指令下发失败
        #     return JsonResponse({
        #         'code': 1,
        #         'msg': f'充值成功但指令下发失败: {command_result.get("msg", "未知错误")}',
        #         'data': {
        #             'uid': card.uid,
        #             'balance': str(card.balance)
        #         }
        #     })
        # 更新数据库余额
        card, created = get_card_or_create(uid)
        card.balance += amount
        card.save()

        # 存待执行指令，等 ESP32 轮询
        PendingCommand.objects.create(
            card_uid=uid,
            cmd_type='recharge',
            amount=amount
        )

        CardRecord.objects.create(
            card_uid=uid,
            user_id=0,
            user_name='充值',
            balance=card.balance,
            total_price=amount
        )

        return JsonResponse({
            'code': 0,
            'msg': '充值指令已提交',
            'data': {
                'uid': card.uid,
                'balance': str(card.balance),
                'status': 'pending'
            }
        })
    except json.JSONDecodeError:
        return JsonResponse({'code': 1, 'message': '无效的 JSON 数据'})
    except Exception as e:
        return JsonResponse({'code': 1, 'message': f'充值失败: {str(e)}'})


# ==================== 扣费 ====================
@csrf_exempt
def card_deduct(request):
    """
    RFID 卡扣费
    功能说明：
        为指定的RFID卡进行扣费操作，如果卡不存在则返回错误
    """
    if request.method != 'POST':
        return JsonResponse({'code': 1, 'message': '请使用 POST 请求'})

    try:
        data = json.loads(request.body)
        uid = data.get('uid')
        amount_str = data.get('amount')
        device_name = data.get('device_name', 'dish')

        if not uid or amount_str is None:
            return JsonResponse({'code': 1, 'message': '缺少 uid 或 amount 参数'})

        amount = Decimal(str(amount_str))
        if amount <= 0:
            return JsonResponse({'code': 1, 'message': '扣费金额必须大于 0'})

        # ===== 步骤1：检查余额是否充足 =====
        card = RfidCard.objects.filter(uid=uid).first()
        if not card:
            return JsonResponse({'code': 1, 'message': '卡不存在'})

        if card.balance < amount:
            return JsonResponse({
                'code': 1,
                'message': f'余额不足，当前余额: {card.balance}'
            })

        # 步骤2：扣款
        card.balance -= amount
        card.save()

        # 步骤3：写账单记录
        CardRecord.objects.create(
            card_uid=uid,
            user_id=0,
            user_name='扫码支付',
            balance=card.balance,
            total_price=amount
        )

        return JsonResponse({
            'code': 0,
            'msg': '扣费成功',
            'data': {
                'uid': card.uid,
                'balance': str(card.balance)
            }
        })

    except json.JSONDecodeError:
        return JsonResponse({'code': 1, 'message': '无效的 JSON 数据'})
    except Exception as e:
        return JsonResponse({'code': 1, 'message': f'扣费失败: {str(e)}'})


# ==================== 充值/扣费结果回调 ====================
@csrf_exempt
def card_command_result(request):
    """
    ESP32 执行充值/扣费后的结果回调
    或 OneNET 物模型上报的回复
    """
    if request.method != 'POST':
        return JsonResponse({'code': 1, 'message': '请使用 POST 请求'})

    try:
        data = json.loads(request.body)
        uid = data.get('uid')
        action = data.get('action')  # 'recharge' 或 'deduct'
        amount = data.get('amount')
        status = data.get('status')  # 'success' 或 'failed'
        msg = data.get('msg', '')

        # 可以记录到日志或更新状态
        print(f"命令结果: uid={uid}, action={action}, amount={amount}, status={status}, msg={msg}")

        return JsonResponse({
            'code': 0,
            'msg': '结果已记录'
        })

    except Exception as e:
        return JsonResponse({'code': 1, 'message': str(e)})


# ==================== 辅助函数 ====================
def generate_qrcode_token(openid):
    """生成二维码token"""
    raw = f"{openid}_{time.time()}_{hashlib.md5(openid.encode()).hexdigest()}"
    return hashlib.sha256(raw.encode()).hexdigest()[:32]


def create_transaction(card, user, trans_type, amount, balance_before, balance_after, description=''):
    """创建交易记录"""
    return CardTransaction.objects.create(
        card=card,
        user=user,
        transaction_type=trans_type,
        amount=amount,
        balance_before=balance_before,
        balance_after=balance_after,
        description=description
    )


def get_user_by_token(request):
    """从请求中获取用户"""
    token = request.headers.get('Authorization')
    if not token:
        return None, JsonResponse({'code': 401, 'message': '未登录'})

    user = User.objects.filter(token=token).first()
    if not user:
        user, _ = User.objects.get_or_create(
            openid='dev_user',
            defaults={'token': token, 'nickname': '新用户'}
        )
        if user.token != token:
            user.token = token
            user.save()

    return user, None


def get_user_by_openid(openid):
    """通过openid获取用户"""
    try:
        return User.objects.get(openid=openid)
    except User.DoesNotExist:
        return None


# ==================== 1. 获取绑定状态 ====================

@csrf_exempt
@require_http_methods(["GET"])
def card_binding_status(request):
    """
    获取用户卡绑定状态
    请求: GET?openid=xxx 或 通过token
    返回: {
        "code": 0,
        "data": {
            "is_bound": true/false,
            "card_uid": "xxx",
            "balance": 100.50,
            "bind_time": "2026-07-02 10:30:00",
            "status": "active"
        }
    }
    """
    openid = request.GET.get('openid')

    # 如果没有openid，尝试从token获取
    if not openid:
        user, error = get_user_by_token(request)
        if error:
            return error
        openid = user.openid

    user = get_user_by_openid(openid)
    if not user:
        return JsonResponse({'code': 1, 'message': '用户不存在'})

    # 查询绑定记录
    binding = CardBinding.objects.filter(user=user, status='active').first()

    if not binding:
        return JsonResponse({
            'code': 0,
            'data': {
                'is_bound': False,
                'message': '未绑定卡'
            }
        })

    return JsonResponse({
        'code': 0,
        'data': {
            'is_bound': True,
            'card_uid': binding.card.uid,
            'balance': float(binding.card.balance),
            'bind_time': binding.bind_time.strftime('%Y-%m-%d %H:%M:%S'),
            'status': binding.status,
            'initial_balance': float(binding.initial_balance)
        }
    })


# ==================== 2. 开始绑定（生成二维码） ====================

@csrf_exempt
@require_http_methods(["POST"])
def card_bind_start(request):
    """
    开始绑定流程：生成二维码
    请求: {
        "openid": "xxx"  # 可选，如果不传则从token获取
    }
    返回: {
        "code": 0,
        "data": {
            "qrcode_token": "xxx",
            "expire_at": "2026-07-02 10:35:00",
            "qrcode_data": "smart://bind?token=xxx"
        }
    }
    """
    try:
        data = json.loads(request.body) if request.body else {}
        openid = data.get('openid')

        # 如果没有openid，从token获取
        if not openid:
            user, error = get_user_by_token(request)
            if error:
                return error
            openid = user.openid
        else:
            user = get_user_by_openid(openid)
            if not user:
                return JsonResponse({'code': 1, 'message': '用户不存在'})

        # 检查是否已绑定激活的卡
        if CardBinding.objects.filter(user=user, status='active').exists():
            return JsonResponse({'code': 1, 'message': '您已绑定卡，请先解绑再重新绑定'})

        # 生成二维码token
        qrcode_token = generate_qrcode_token(openid)

        # 设置过期时间（5分钟后）
        expire_at = timezone.now() + timedelta(minutes=5)

        # 删除旧的等待记录
        BindWaitQueue.objects.filter(openid=openid).delete()

        # 创建等待记录
        BindWaitQueue.objects.create(
            openid=openid,
            qrcode_token=qrcode_token,
            expire_at=expire_at
        )

        # 生成二维码内容（小程序扫码解析用）
        qrcode_data = f"smart://bind?token={qrcode_token}"

        return JsonResponse({
            'code': 0,
            'data': {
                'qrcode_token': qrcode_token,
                'expire_at': expire_at.strftime('%Y-%m-%d %H:%M:%S'),
                'qrcode_data': qrcode_data,
                'expire_seconds': 300  # 5分钟
            }
        })

    except json.JSONDecodeError:
        return JsonResponse({'code': 1, 'message': '无效的JSON数据'})
    except Exception as e:
        return JsonResponse({'code': 1, 'message': f'生成二维码失败: {str(e)}'})


# ==================== 3. 设备端确认绑定（刷卡） ====================

@csrf_exempt
@require_http_methods(["POST"])
def card_bind_confirm(request):
    """
    设备端确认绑定：刷卡后绑定
    请求: {
        "card_uid": "A1B2C3D4",
        "qrcode_token": "xxx"  # 扫码获取的token
    }
    返回: {
        "code": 0,
        "message": "绑定成功",
        "data": {
            "openid": "xxx",
            "card_uid": "xxx",
            "balance": 0
        }
    }
    """
    try:
        data = json.loads(request.body)
        card_uid = data.get('card_uid')
        qrcode_token = data.get('qrcode_token')

        if not card_uid:
            return JsonResponse({'code': 1, 'message': '缺少卡号'})

        if not qrcode_token:
            return JsonResponse({'code': 1, 'message': '缺少二维码token'})

        # 查找等待记录
        wait_record = BindWaitQueue.objects.filter(
            qrcode_token=qrcode_token,
            is_used=False
        ).first()

        if not wait_record:
            return JsonResponse({'code': 1, 'message': '无效的二维码'})

        # 检查是否过期
        if wait_record.is_expired():
            wait_record.delete()
            return JsonResponse({'code': 1, 'message': '二维码已过期，请重新生成'})

        # 获取用户
        user = get_user_by_openid(wait_record.openid)
        if not user:
            return JsonResponse({'code': 1, 'message': '用户不存在'})

        # 检查卡是否存在
        rfid_card = RfidCard.objects.filter(uid=card_uid).first()
        if not rfid_card:
            # 卡不存在，自动创建
            rfid_card = RfidCard.objects.create(
                uid=card_uid,
                balance=0
            )

        # 检查卡是否已被其他用户绑定
        existing_binding = CardBinding.objects.filter(
            card=rfid_card,
            status='active'
        ).exclude(user=user).first()
        if existing_binding:
            return JsonResponse({
                'code': 1,
                'message': f'该卡已被用户 {existing_binding.user.nickname} 绑定'
            })

        # 检查用户是否已绑定其他卡
        if CardBinding.objects.filter(user=user, status='active').exists():
            return JsonResponse({'code': 1, 'message': '您已绑定其他卡，请先解绑'})

        # 开始事务
        with transaction.atomic():
            # 创建绑定记录
            binding = CardBinding.objects.create(
                user=user,
                card=rfid_card,
                status='active',
                initial_balance=rfid_card.balance,
                remark='通过扫码绑定'
            )

            # 更新用户的关联
            user.rfid_card = rfid_card
            user.save()

            # 标记等待记录为已使用
            wait_record.is_used = True
            wait_record.card_uid = card_uid
            wait_record.save()

            # 创建交易记录
            create_transaction(
                card=rfid_card,
                user=user,
                trans_type='bind',
                amount=Decimal('0'),
                balance_before=rfid_card.balance,
                balance_after=rfid_card.balance,
                description=f'用户 {user.nickname} 绑定卡'
            )

        return JsonResponse({
            'code': 0,
            'message': '绑定成功',
            'data': {
                'openid': user.openid,
                'nickname': user.nickname,
                'card_uid': rfid_card.uid,
                'balance': float(rfid_card.balance)
            }
        })

    except json.JSONDecodeError:
        return JsonResponse({'code': 1, 'message': '无效的JSON数据'})
    except Exception as e:
        return JsonResponse({'code': 1, 'message': f'绑定失败: {str(e)}'})


# ==================== 4. 手动输入卡号绑定 ====================

@csrf_exempt
@require_http_methods(["POST"])
def card_bind_manual(request):
    """
    手动输入卡号绑定
    请求: {
        "card_uid": "A1B2C3D4"
    }
    需要登录（从token获取用户）
    """
    try:
        data = json.loads(request.body)
        card_uid = data.get('card_uid')

        if not card_uid:
            return JsonResponse({'code': 1, 'message': '缺少卡号'})

        # 获取当前用户
        user, error = get_user_by_token(request)
        if error:
            return error

        # 检查用户是否已绑定
        if CardBinding.objects.filter(user=user, status='active').exists():
            return JsonResponse({'code': 1, 'message': '您已绑定卡，请先解绑'})

        # 检查卡是否存在
        rfid_card, created = RfidCard.objects.get_or_create(
            uid=card_uid,
            defaults={'balance': 0}
        )
        # rfid_card, created = RfidCard.objects.filter(uid=card_uid).first()
        # if not rfid_card:
        #     return JsonResponse({'code': 1, 'message': '卡号不存在，请确认卡号是否正确'})

        # 检查卡是否已被绑定
        if CardBinding.objects.filter(card=rfid_card, status='active').exists():
            return JsonResponse({'code': 1, 'message': '该卡已被绑定'})

        with transaction.atomic():
            # 创建绑定记录
            binding = CardBinding.objects.create(
                user=user,
                card=rfid_card,
                status='active',
                initial_balance=rfid_card.balance,
                remark='手动输入绑定'
            )

            user.rfid_card = rfid_card
            user.save()

            create_transaction(
                card=rfid_card,
                user=user,
                trans_type='bind',
                amount=Decimal('0'),
                balance_before=rfid_card.balance,
                balance_after=rfid_card.balance,
                description=f'用户 {user.nickname} 手动绑定卡'
            )

        return JsonResponse({
            'code': 0,
            'message': '绑定成功',
            'data': {
                'card_uid': rfid_card.uid,
                'balance': float(rfid_card.balance)
            }
        })

    except json.JSONDecodeError:
        return JsonResponse({'code': 1, 'message': '无效的JSON数据'})
    except Exception as e:
        return JsonResponse({'code': 1, 'message': f'绑定失败: {str(e)}'})


# ==================== 5. 解绑卡 ====================

@csrf_exempt
@require_http_methods(["POST"])
def card_unbind(request):
    """
    解绑卡
    请求: {
        "openid": "xxx"  # 可选，不传则从token获取
    }
    """
    try:
        data = json.loads(request.body) if request.body else {}
        openid = data.get('openid')

        # 获取用户
        if openid:
            user = get_user_by_openid(openid)
            if not user:
                return JsonResponse({'code': 1, 'message': '用户不存在'})
        else:
            user, error = get_user_by_token(request)
            if error:
                return error

        # 查询绑定记录
        binding = CardBinding.objects.filter(user=user, status='active').first()
        if not binding:
            return JsonResponse({'code': 1, 'message': '未绑定卡'})

        # 检查余额
        if binding.card.balance > 0:
            return JsonResponse({
                'code': 1,
                'message': f'卡内还有余额 {binding.card.balance} 元，请先消费或提现再解绑'
            })

        with transaction.atomic():
            # 更新绑定状态
            binding.status = 'unbound'
            binding.unbind_time = timezone.now()
            binding.save()

            # 清除用户关联
            user.rfid_card = None
            user.save()

            # 记录解绑交易
            create_transaction(
                card=binding.card,
                user=user,
                trans_type='unbind',
                amount=Decimal('0'),
                balance_before=binding.card.balance,
                balance_after=binding.card.balance,
                description=f'用户 {user.nickname} 解绑卡'
            )

        return JsonResponse({
            'code': 0,
            'message': '解绑成功',
            'data': {
                'card_uid': binding.card.uid,
                'unbind_time': binding.unbind_time.strftime('%Y-%m-%d %H:%M:%S')
            }
        })

    except json.JSONDecodeError:
        return JsonResponse({'code': 1, 'message': '无效的JSON数据'})
    except Exception as e:
        return JsonResponse({'code': 1, 'message': f'解绑失败: {str(e)}'})


# ==================== 6. 查询绑定历史 ====================

@csrf_exempt
@require_http_methods(["GET"])
def card_bind_history(request):
    """
    查询用户的绑定历史
    请求: GET?openid=xxx 或 通过token
    """
    openid = request.GET.get('openid')

    if not openid:
        user, error = get_user_by_token(request)
        if error:
            return error
        openid = user.openid
    else:
        user = get_user_by_openid(openid)
        if not user:
            return JsonResponse({'code': 1, 'message': '用户不存在'})

    bindings = CardBinding.objects.filter(user=user).order_by('-bind_time')

    data = []
    for binding in bindings:
        data.append({
            'id': binding.id,
            'card_uid': binding.card.uid,
            'status': binding.status,
            'status_text': binding.get_status_display(),
            'initial_balance': float(binding.initial_balance),
            'current_balance': float(binding.card.balance),
            'bind_time': binding.bind_time.strftime('%Y-%m-%d %H:%M:%S'),
            'unbind_time': binding.unbind_time.strftime('%Y-%m-%d %H:%M:%S') if binding.unbind_time else None,
            'remark': binding.remark
        })

    return JsonResponse({
        'code': 0,
        'data': {
            'total': len(data),
            'bindings': data
        }
    })


# ==================== 7. 获取等待队列状态（小程序端） ====================

@csrf_exempt
@require_http_methods(["GET"])
def card_bind_wait_status(request):
    """
    查询当前绑定等待状态
    请求: GET?openid=xxx 或 通过token
    """
    openid = request.GET.get('openid')

    if not openid:
        user, error = get_user_by_token(request)
        if error:
            return error
        openid = user.openid

    wait_record = BindWaitQueue.objects.filter(
        openid=openid,
        is_used=False
    ).first()

    if not wait_record:
        return JsonResponse({
            'code': 0,
            'data': {
                'waiting': False,
                'message': '没有等待中的绑定'
            }
        })

    return JsonResponse({
        'code': 0,
        'data': {
            'waiting': True,
            'qrcode_token': wait_record.qrcode_token,
            'expire_at': wait_record.expire_at.strftime('%Y-%m-%d %H:%M:%S'),
            'is_expired': wait_record.is_expired(),
            'created_at': wait_record.created_at.strftime('%Y-%m-%d %H:%M:%S')
        }
    })


# ==================== 8. 刷新二维码（重新生成） ====================

@csrf_exempt
@require_http_methods(["POST"])
def card_bind_refresh(request):
    """
    刷新二维码
    请求: {
        "openid": "xxx"  # 可选
    }
    """
    try:
        data = json.loads(request.body) if request.body else {}
        openid = data.get('openid')

        if not openid:
            user, error = get_user_by_token(request)
            if error:
                return error
            openid = user.openid

        # 删除旧的等待记录
        BindWaitQueue.objects.filter(openid=openid).delete()

        # 生成新的二维码
        qrcode_token = generate_qrcode_token(openid)
        expire_at = timezone.now() + timedelta(minutes=5)

        BindWaitQueue.objects.create(
            openid=openid,
            qrcode_token=qrcode_token,
            expire_at=expire_at
        )

        return JsonResponse({
            'code': 0,
            'data': {
                'qrcode_token': qrcode_token,
                'expire_at': expire_at.strftime('%Y-%m-%d %H:%M:%S'),
                'qrcode_data': f"smart://bind?token={qrcode_token}"
            }
        })

    except Exception as e:
        return JsonResponse({'code': 1, 'message': f'刷新失败: {str(e)}'})



@csrf_exempt
@require_http_methods(["GET"])
def card_sync_and_query(request):
      """小程序查询卡余额：先从 OneNET 同步最新数据，再返回"""
      from smart.utils.card_sync import sync_card_data_from_onenet

      result = sync_card_data_from_onenet()

      if result.get("success"):
          return JsonResponse({
              "code": 0,
              "msg": "同步成功",
              "data": result["data"]
          })
      else:
          # 同步失败时，降级查本地数据库
          uid = request.GET.get("uid")
          if uid:
              card = RfidCard.objects.filter(uid=uid).first()
              if card:
                  return JsonResponse({
                      "code": 0,
                      "msg": "返回本地数据",
                      "data": {"card_uid": uid, "balance":str(card.balance)}
                  })
          return JsonResponse({"code": 1, "msg":result.get("msg", "无数据")})


# ==================== 卡交易记录（小程序账单）====================

@csrf_exempt
@require_http_methods(["GET"])
def card_sync_and_query(request):
    """
    同步 OneNET 数据并返回最新卡信息

    功能说明：
        1. 从 OneNET 设备拉取最新的刷卡数据
        2. 同步到本地数据库（更新余额、创建账单）
        3. 返回最新的卡信息给前端
    """
    try:
        # 1. 同步 OneNET 数据
        result = sync_card_data_from_onenet()

        if not result.get("success"):
            return JsonResponse({
                "code": 1,
                "msg": result.get("msg", "同步失败")
            })

        data = result["data"]
        card_uid = data.get("card_uid", "")

        # 2. 查找绑定的用户信息
        binding = CardBinding.objects.filter(
            card__uid=card_uid,
            status='active'
        ).select_related('user').first()

        # 3. 获取用户昵称
        nickname = ""
        if binding and binding.user:
            nickname = binding.user.nickname or binding.user.openid or ""

        # 4. 返回数据
        return JsonResponse({
            "code": 0,
            "msg": "ok",
            "data": {
                "card_uid": card_uid,
                "balance": data.get("balance", "0"),
                "total_price": data.get("total_price", "0"),
                "user_name": data.get("user_name", ""),
                "nickname": nickname,
                "is_bound": bool(binding),
                "sync_time": timezone.now().strftime('%Y-%m-%d %H:%M:%S')
            }
        })

    except Exception as e:
        logger.error(f"同步查询失败: {str(e)}", exc_info=True)
        return JsonResponse({
            "code": 1,
            "msg": f"查询失败: {str(e)}"
        })


@csrf_exempt
@require_http_methods(["GET"])
def card_bill_list(request):
    """
    获取卡消费账单（按月分组，含月度总计）

    功能说明：
        1. 根据卡号或用户ID查询消费记录
        2. 按月分组统计（包含月度总计和消费次数）
        3. 返回本月总消费和分组数据

    请求参数（GET）：
        方式一（按卡号查询）：
            - uid: 卡号（如：A1B2C3D4）
        方式二（按用户ID查询）：
            - user_id: 用户ID（如：oX8m4xxx）
    """
    try:
        # 1. 获取参数
        card_uid = request.GET.get("uid", "")
        user_id = request.GET.get("user_id", "")

        # 2. 确定 user_id
        if card_uid:
            # 通过卡号查找绑定的用户
            binding = CardBinding.objects.filter(
                card__uid=card_uid,
                status='active'
            ).select_related('user').first()

            if not binding:
                return JsonResponse({
                    "code": 1,
                    "msg": "该卡未绑定用户或已解绑"
                })

            if not binding.user:
                return JsonResponse({
                    "code": 1,
                    "msg": "绑定记录中的用户不存在"
                })

            user_id = binding.user.openid

        # 如果仍然没有 user_id，返回错误
        if not user_id:
            return JsonResponse({
                "code": 1,
                "msg": "缺少参数：请提供 uid 或 user_id"
            })

        # 3. 查询账单记录（限制100条）
        bills = Bill.objects.filter(
            user_id=user_id
        ).order_by('-create_time')[:100]

        if not bills.exists():
            return JsonResponse({
                "code": 0,
                "msg": "暂无消费记录",
                "data": {
                    "this_month_total": "0.00",
                    "monthly": []
                }
            })

        # 4. 按月分组统计
        monthly_stats = {}
        bill_list = []

        for bill in bills:
            month_key = bill.create_time.strftime('%Y-%m')

            # 统计月度数据
            if month_key not in monthly_stats:
                monthly_stats[month_key] = {
                    "total": Decimal('0'),
                    "count": 0
                }
            monthly_stats[month_key]["total"] += bill.amount
            monthly_stats[month_key]["count"] += 1

            # 构建账单列表
            bill_list.append({
                "id": bill.id,
                "type": bill.type,
                "type_text": bill.type_text,
                "amount": str(bill.amount),
                "balance_after": str(bill.balance_after),
                "create_time": bill.create_time.strftime('%Y-%m-%d %H:%M')
            })

        # 5. 按月份分组整理数据
        grouped = {}
        for bill in bill_list:
            month = bill["create_time"][:7]  # 取 YYYY-MM
            if month not in grouped:
                grouped[month] = {
                    "month": month,
                    "month_total": str(monthly_stats.get(month, {}).get("total", 0)),
                    "bill_count": monthly_stats.get(month, {}).get("count", 0),
                    "bills": []
                }
            grouped[month]["bills"].append(bill)

        # 6. 计算本月总消费
        this_month = timezone.now().strftime('%Y-%m')
        this_month_total = str(monthly_stats.get(this_month, {}).get("total", 0))

        # 7. 返回结果（按月份排序，最新的在前）
        return JsonResponse({
            "code": 0,
            "msg": "ok",
            "data": {
                "this_month_total": this_month_total,
                "this_month_count": monthly_stats.get(this_month, {}).get("count", 0),
                "total_bills": len(bill_list),
                "monthly": list(grouped.values())
            }
        })

    except CardBinding.DoesNotExist:
        return JsonResponse({
            "code": 1,
            "msg": "卡绑定记录不存在"
        })
    except Exception as e:
        logger.error(f"查询账单失败: {str(e)}", exc_info=True)
        return JsonResponse({
            "code": 1,
            "msg": f"查询失败: {str(e)}"
        })


# ==================== 获取账单详情（单个账单） ====================

@csrf_exempt
@require_http_methods(["GET"])
def card_bill_detail(request):
    """
    获取单个账单详情

    请求参数（GET）：
        - bill_id: 账单ID（必需）
    """
    try:
        bill_id = request.GET.get("bill_id")

        if not bill_id:
            return JsonResponse({"code": 1, "msg": "缺少 bill_id 参数"})

        bill = Bill.objects.get(id=bill_id)

        return JsonResponse({
            "code": 0,
            "msg": "ok",
            "data": {
                "id": bill.id,
                "type": bill.type,
                "type_text": bill.type_text,
                "amount": str(bill.amount),
                "balance_after": str(bill.balance_after),
                "description": bill.description,
                "create_time": bill.create_time.strftime('%Y-%m-%d %H:%M:%S')
            }
        })

    except Bill.DoesNotExist:
        return JsonResponse({"code": 1, "msg": "账单不存在"})
    except Exception as e:
        logger.error(f"查询账单详情失败: {str(e)}", exc_info=True)
        return JsonResponse({"code": 1, "msg": f"查询失败: {str(e)}"})


# ==================== 获取月度消费统计 ====================
@csrf_exempt
@require_http_methods(["GET"])
def card_monthly_stats(request):
    """
    获取月度消费统计（用于图表展示）

    请求参数（GET）：
        - uid: 卡号（可选）
        - user_id: 用户ID（可选）
        - months: 查询月数（默认6个月）
    """
    try:
        # 获取参数
        card_uid = request.GET.get("uid", "")
        user_id = request.GET.get("user_id", "")
        months = int(request.GET.get("months", 6))

        # 确定 user_id
        if card_uid and not user_id:
            binding = CardBinding.objects.filter(
                card__uid=card_uid,
                status='active'
            ).select_related('user').first()

            if binding and binding.user:
                user_id = binding.user.openid

        if not user_id:
            return JsonResponse({"code": 1, "msg": "请提供 uid 或 user_id"})

        # 计算起始日期
        start_date = timezone.now().date() - timedelta(days=months * 30)

        # 查询账单
        bills = Bill.objects.filter(
            user_id=user_id,
            type='expense',
            create_time__gte=start_date
        ).order_by('-create_time')

        # 按月统计
        monthly_data = {}
        for bill in bills:
            month_key = bill.create_time.strftime('%Y-%m')
            if month_key not in monthly_data:
                monthly_data[month_key] = {
                    "total": Decimal('0'),
                    "count": 0
                }
            monthly_data[month_key]["total"] += bill.amount
            monthly_data[month_key]["count"] += 1

        # 构建返回数据
        stats = []
        total_spent = Decimal('0')
        total_count = 0

        for month, data in sorted(monthly_data.items(), reverse=True):
            avg = data["total"] / data["count"] if data["count"] > 0 else 0
            stats.append({
                "month": month,
                "total": str(data["total"]),
                "count": data["count"],
                "avg": str(round(avg, 2))
            })
            total_spent += data["total"]
            total_count += data["count"]

        return JsonResponse({
            "code": 0,
            "msg": "ok",
            "data": {
                "stats": stats,
                "total_spent": str(total_spent),
                "total_count": total_count
            }
        })

    except Exception as e:
        logger.error(f"获取月度统计失败: {str(e)}", exc_info=True)
        return JsonResponse({"code": 1, "msg": f"查询失败: {str(e)}"})



@csrf_exempt
@require_http_methods(["POST"])
def card_record_create(request):
    """ESP32 上报刷卡数据"""
    try:
        data = json.loads(request.body)
        card_uid = data.get('card_uid', '')
        user_id = data.get('user_id', 0)
        user_name = data.get('user_name', '')
        # balance = int(data.get('balance', 0))
        # total_price = int(data.get('total_price', 0))
        balance = Decimal(str(int(data.get('balance', 0)))) / 100
        total_price = Decimal(str(int(data.get('total_price', 0)))) /  100

        if not card_uid:
            return JsonResponse({'code': 1, 'msg':
                '缺少卡号'})

        card, created = RfidCard.objects.get_or_create(
            uid=card_uid,
            defaults={'balance': balance}
            # defaults={'balance': Decimal(str(balance))}
        )
        if not created:
            # card.balance = Decimal(str(balance))
            card.balance = balance
            card.save()

        CardRecord.objects.create(
            card_uid=card_uid, user_id=user_id,
            user_name=user_name,
            balance=balance, total_price=total_price
        )

        # 同步创建用餐记录（如果有菜品数据才建）
        if user_name and card_uid:
            UserMealRecord.objects.create(
                user_id=card_uid,
                food_id=0,
                food_name=f'{user_name}的消费',
                price=total_price,
                weight=200,
                calorie=0,
                protein=0,
                meal_type='lunch'
            )

        # 英文菜名→(中文名, 热量/100g, 蛋白质/100g)
        def translate_dish(en):
            m = {
                'tomato_scrambled_egg': ('西红柿炒鸡蛋', 95, 6),
                'spicy_sour_potato_shreds': ('酸辣土豆丝', 88, 3),
                'steamed_bun': ('馒头', 223, 7),
                'shaomai': ('烧麦', 220, 8),
                'pure_milk': ('纯牛奶', 66, 3),
                'egg': ('鸡蛋', 144, 13),
                'braised_pork': ('红烧肉', 350, 15),
                'rice': ('白米饭', 116, 2),
            }
            return m.get(en, (en, 0, 0))

        # 创建用餐记录（新增）
        dishes = data.get('dishes', '')
        if dishes:
            dish_names = [d.strip() for d in dishes.split(',') if d.strip()]
            for dname in dish_names:
                cn_name, cal100, pro100 = translate_dish(dname)
                UserMealRecord.objects.create(
                    user_id=card_uid,
                    food_id=0,
                    food_name=cn_name,
                    price=total_price / len(dish_names) if
                    dish_names else total_price,
                    weight=200,
                    calorie=cal100 * 2,
                    protein=pro100 * 2,
                    meal_type='lunch'
                )

        return JsonResponse({'code': 0, 'msg': 'ok'})
    except Exception as e:
        return JsonResponse({'code': 1, 'msg': str(e)})


@csrf_exempt
@require_http_methods(["GET"])
def card_record_latest(request):
    """小程序查询最近3张刷卡记录"""
    records = CardRecord.objects.all()[:3]
    data = []
    for r in records:
        data.append({
            'card_uid': r.card_uid,
            'user_id': r.user_id,
            'user_name': r.user_name,
            'balance': r.balance,
            'total_price': r.total_price,
            'create_time': r.create_time.strftime('%Y-%m-%d%H:%M:%S')
        })
    return JsonResponse({'code': 0, 'data': data})


# @csrf_exempt
# @require_http_methods(["POST"])
# def card_record_create(request):
#     """ESP32 上报刷卡数据，自动更新 RfidCard 余额"""
#     try:
#         data = json.loads(request.body)
#         card_uid = data.get('card_uid', '')
#         user_id = int(data.get('user_id', 0))
#         user_name = data.get('user_name', '')
#         # balance = int(data.get('balance', 0))
#         # total_price = int(data.get('total_price', 0))
#         balance = Decimal(str(int(data.get('balance', 0)))) / 100
#         total_price = Decimal(str(int(data.get('total_price', 0)))) / 100
#
#         if not card_uid:
#             return JsonResponse({'code': 1, 'msg': '缺少卡号'})
#
#         # 更新卡余额（Django 数据库）
#         card, created = RfidCard.objects.get_or_create(
#             uid=card_uid,
#             # defaults={'balance': Decimal(str(balance))}
#             defaults={'balance': balance}
#         )
#         if not created:
#             # card.balance = Decimal(str(balance))
#             card.balance = balance
#             card.save()
#
#         # 存刷卡记录
#         CardRecord.objects.create(
#             card_uid=card_uid,
#             user_id=user_id,
#             user_name=user_name,
#             balance=balance,
#             total_price=total_price
#         )
#         return JsonResponse({'code': 0, 'msg': 'ok'})
#     except Exception as e:
#         return JsonResponse({'code': 1, 'msg': str(e)})
#
#
# @csrf_exempt
# @require_http_methods(["GET"])
# def card_record_latest(request):
#     """返回最近 3 条刷卡记录"""
#     records = CardRecord.objects.all()[:3]
#     data = []
#     for r in records:
#         data.append({
#             'card_uid': r.card_uid,
#             'user_id': r.user_id,
#             'user_name': r.user_name,
#             'balance': r.balance,
#             'total_price': r.total_price,
#             'create_time': r.create_time.strftime('%m-%d %H:%M:%S')})
#     return JsonResponse({'code': 0, 'data': data})


@csrf_exempt
@require_http_methods(["POST"])
def chat_save(request):
    """保存聊天记录"""
    try:
        data = json.loads(request.body)
        token = request.headers.get('Authorization', '')
        user = User.objects.filter(token=token).first()
        if not user:
            return JsonResponse({'code': 1, 'msg': '未登录'})
        chat, _ = ChatHistory.objects.get_or_create(openid=user.openid)
        chat.messages = json.dumps(data.get('messages', []))
        chat.save()
        return JsonResponse({'code': 0, 'msg': 'ok'})
    except Exception as e:
        return JsonResponse({'code': 1, 'msg': str(e)})


@csrf_exempt
@require_http_methods(["GET"])
def chat_load(request):
    """加载聊天记录"""
    token = request.headers.get('Authorization', '')
    user = User.objects.filter(token=token).first()
    if not user:
        return JsonResponse({'code': 1, 'msg': '未登录'})
    chat = ChatHistory.objects.filter(openid=user.openid).first()
    messages = json.loads(chat.messages) if chat else []
    return JsonResponse({'code': 0, 'data': messages})


@csrf_exempt
@require_http_methods(["GET"])
def daily_report(request):
    """生成今日营养报告"""
    token = request.headers.get('Authorization', '')
    user = User.objects.filter(token=token).first()
    if not user:
        user, _ = User.objects.get_or_create(
            openid='dev_user',
            defaults={'token': token, 'nickname': '新用户'}
        )

    today = timezone.now().date()
    records = UserMealRecord.objects.filter(eat_date=today)

    if records.count() == 0:
        return JsonResponse({'code': 1, 'msg': '今天还没有用餐记录'})

    # 营养数据库（每100g）
    nutrition_db = {
        '西红柿炒鸡蛋': (95, 6), '酸辣土豆丝': (88, 3),
        '馒头': (223, 7), '烧麦': (220, 8),
        '纯牛奶': (66, 3), '鸡蛋': (144, 13),
        '红烧肉': (350, 15), '白米饭': (116, 2),
    }

    total_cal = 0
    total_protein = 0
    dishes = []
    for r in records:
        name = r.food_name
        if '的消费' in name:
            continue  # 跳过通用记录
        cal, pro = nutrition_db.get(name, (0, 0))
        total_cal += cal * 2   # 默认200g
        total_protein += pro * 2
        dishes.append(name)

    if not dishes:
        return JsonResponse({'code': 1, 'msg': '今天没有有效用餐记录'})

    # 获取健康档案
    health_ctx = ''
    if user.height and user.weight:
        bmi = user.weight / ((user.height / 100) ** 2)
        health_ctx += f'身高{user.height}cm体重{user.weight}kg BMI{bmi:.1f}。'
    if user.age: health_ctx += f'年龄{user.age}岁。'
    if user.gender: health_ctx += f'性别: {"男" if user.gender=="male" else "女"}。'
    if user.diet_goal:
        goals = {'lose': '减脂', 'maintain': '保持', 'gain': '增肌'}
        health_ctx += f'目标: {goals.get(user.diet_goal, user.diet_goal)}。'

    # 调AI生成报告
    dish_list = '、'.join(dishes)
    prompt = f"用户今天吃了：{dish_list}。总热量{total_cal}千卡，总蛋白质{total_protein}g。"
    if health_ctx:
        prompt += f"健康档案：{health_ctx}"
    prompt += "请结合健康档案分析营养摄入是否合理，给出评价和改进建议。"

    result = call_qwen_api(prompt,"你是营养师助手，请简短评估")

    return JsonResponse({
        'code': 0,
        'data': {
            'date': str(today),
            'total_calorie': total_cal,
            'total_protein': total_protein,
            'meal_count': records.count(),
            'dishes': dishes,
            'report': result.get('content', '')
            if result.get('success')
            else '报告生成失败'
          }
      })


@csrf_exempt
@require_http_methods(["GET"])
def pending_commands(request):
    """ESP32 轮询待执行指令"""
    cmds = PendingCommand.objects.filter(status='pending')[:5]
    data = []
    for c in cmds:
        data.append({
            'id': c.id,
            'card_uid': c.card_uid,
            'cmd_type': c.cmd_type,
            'amount': str(c.amount)
        })
    return JsonResponse({'code': 0, 'data': data})


@csrf_exempt
@require_http_methods(["POST"])
def complete_command(request):
    """ESP32 标记指令已完成"""
    try:
        data = json.loads(request.body)
        cmd_id = data.get('id')
        cmd = PendingCommand.objects.filter(id=cmd_id).first()
        if cmd:
            cmd.status = 'done'
            cmd.save()
        return JsonResponse({'code': 0, 'msg': 'ok'})
    except Exception as e:
        return JsonResponse({'code': 1, 'msg': str(e)})


_latest_price = {'amount': '0.00', 'dishes': ''}

@csrf_exempt
@require_http_methods(["POST"])
def update_current_price(request):
    """ESP32 更新当前待付总价"""
    try:
        data = json.loads(request.body)
        _latest_price['amount'] = data.get('amount', '0.00')
        _latest_price['dishes'] = data.get('dishes', '')
        return JsonResponse({'code': 0, 'msg': 'ok'})
    except:
        return JsonResponse({'code': 1})

@csrf_exempt
@require_http_methods(["GET"])
def get_current_price(request):
    """返回当前待付总价"""
    return JsonResponse({'code': 0, 'data': _latest_price})