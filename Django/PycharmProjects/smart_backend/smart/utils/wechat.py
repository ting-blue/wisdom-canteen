# smart/utils/wechat.py
import requests
import hashlib
import time
import random
import string
from django.conf import settings


def get_wechat_session(code):
    """用 code 换取 openid 和 session_key"""
    url = 'https://api.weixin.qq.com/sns/jscode2session'
    params = {
        'appid': settings.WECHAT_APPID,
        'secret': settings.WECHAT_SECRET,
        'js_code': code,
        'grant_type': 'authorization_code'
    }

    try:
        response = requests.get(url, params=params, timeout=10)
        result = response.json()

        if 'errcode' in result and result['errcode'] != 0:
            return None, result.get('errmsg', '微信接口错误')

        return result, None
    except Exception as e:
        return None, str(e)


def generate_token(user_id):
    """生成简单的 token"""
    timestamp = str(int(time.time()))
    random_str = ''.join(random.choices(string.ascii_letters + string.digits, k=16))
    raw = f"{user_id}_{timestamp}_{random_str}"
    token = hashlib.sha256(raw.encode()).hexdigest()
    return token