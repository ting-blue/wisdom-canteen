# smart/utils/ai_utils.py
import requests
import json
from django.conf import settings


def call_qwen_api(user_message, system_prompt=None):
    """
    调用阿里云通义千问API
    user_message: 用户输入的消息
    system_prompt: 系统提示词（定义AI角色）
    """

    # 默认系统提示词 - 智慧食堂营养师
    if system_prompt is None:
        system_prompt = """你是智慧食堂的AI营养师助手。你的任务是：
        1. 根据用户提供的食材或需求，推荐健康食谱
        2. 分析菜品的营养价值
        3. 给出健康的饮食建议
        保持回答简洁、专业、有帮助。"""

    #  使用兼容模式 API 地址（支持 messages 格式）
    url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"

    # 请求头
    headers = {
        "Authorization": f"Bearer {settings.DASHSCOPE_API_KEY}",
        "Content-Type": "application/json"
    }

    #  兼容模式请求体格式
    data = {
        "model": "qwen-max",
        "messages": [
            {"role": "system", "content": system_prompt},
            {"role": "user", "content": user_message}
        ]
    }

    try:
        response = requests.post(url, headers=headers, json=data, timeout=60)
        result = response.json()

        if response.status_code == 200:
            #  兼容模式的返回格式
            ai_response = result.get("choices", [{}])[0].get("message", {}).get("content", "")
            return {
                "success": True,
                "content": ai_response,
                "raw": result
            }
        else:
            #  兼容模式的错误格式
            error_msg = result.get("error", {}).get("message", f"API请求失败: {response.status_code}")
            return {
                "success": False,
                "error": error_msg
            }

    except Exception as e:
        return {
            "success": False,
            "error": str(e)
        }


def recommend_recipe_by_ingredients(ingredients):
    """
    根据食材推荐食谱
    ingredients: 用户输入的食材列表或描述
    """
    user_message = f"我手头有以下食材：{ingredients}。请推荐一道可以用这些食材制作的健康菜品，并给出简单的制作步骤。"
    return call_qwen_api(user_message)

def recommend_from_menu(menu_items, preference=''):
    """
    根据菜单推荐餐单
    """
    menu_text = "\n".join(
        [f"- {item.get('name')}（{item.get('calorie_per_100g', 0)}千卡/100g，{item.get('price', 0)}元）" for item in
         menu_items])

    system_prompt = """你是智慧食堂的AI营养师。用户想从食堂现有菜品中选一份午餐。
    请根据提供的菜单，推荐2-3种营养均衡的搭配方案。

    要求：
    1. 必须从菜单中选菜，不能自己编菜
    2. 推荐时要说明为什么这样搭配
    3. 给出总热量估算
    4. 用友好的语气回答

    例如：推荐一份套餐：主菜X + 副菜Y + 主食Z，热量约XXX千卡。
    """

    user_message = f"食堂现有以下菜品：\n{menu_text}\n\n请为我推荐一份营养均衡的午餐搭配。"

    return call_qwen_api(user_message, system_prompt)

def analyze_nutrition(dish_name):
    """
    分析菜品的营养信息
    dish_name: 菜品名称
    """
    user_message = f"请分析菜品【{dish_name}】的营养价值，包括热量、蛋白质、脂肪、碳水化合物等信息。"
    return call_qwen_api(user_message)


def get_healthy_tips():
    """获取健康饮食小贴士"""
    user_message = "请给我一条关于健康饮食的小贴士，简短实用，50字以内。"
    return call_qwen_api(user_message)