# smart/utils/onenet_auth.py
"""
OneNET 平台 API 工具函数
功能说明：
    1. 生成 OneNET API 鉴权 Token
    2. 查询设备最新属性数据
    3. 获取设备在线状态

使用场景：
    Django 后端通过此模块与 OneNET 云平台交互，获取设备上报的数据
"""

import time
import hmac
import hashlib
import base64
import requests
from urllib.parse import quote
from django.conf import settings


def generate_onenet_token(access_key, product_id):
    """
    生成 OneNET API 鉴权 Token
    使用 products 方式（与成功代码一致）

    Args:
        access_key (str): Base64 格式的产品级 AccessKey
        product_id (str): 产品 ID，如 "dL26Jqg105"

    Returns:
        str: 完整的鉴权 Token，格式为 "version=...&res=...&et=...&method=...&sign=..."

    工作原理：
        1. 使用产品级 AccessKey 和产品 ID 生成 Token
        2. Token 包含签名信息，用于 OneNET 服务器验证请求合法性
        3. 有效期设置为 1 年，避免频繁过期
    """
    version = '2018-10-31'  # ✅ 与成功代码一致
    res = f'products/{product_id}'  # ✅ 使用 products，不是 userid
    et = str(int(time.time()) + 3600 * 24 * 365)  # 一年后过期
    method = 'sha1'  # ✅ 使用 sha1

    # ✅ AccessKey 是 Base64 格式，需要解码
    key = base64.b64decode(access_key)

    # 计算 sign
    sign_str = et + '\n' + method + '\n' + res + '\n' + version
    sign_bytes = hmac.new(key, sign_str.encode('utf-8'), hashlib.sha1).digest()   # 使用 HMAC-SHA1 算法计算签名
    sign = base64.b64encode(sign_bytes).decode('utf-8')  # 将签名结果进行 Base64 编码

    # URL 编码
    sign = quote(sign, safe='')
    res = quote(res, safe='')

    # 组装最终的 Token
    return f'version={version}&res={res}&et={et}&method={method}&sign={sign}'


def get_device_properties(product_id, device_name):
    """
    查询设备最新属性数据
    使用与成功代码一致的接口

    Args:
        product_id (str): 产品 ID，如 "dL26Jqg105"
        device_name (str): 设备名称，如 "dish"

    Returns:
        dict: 包含查询结果的字典
            - 成功时: {"success": True, "data": {"属性名": "属性值", ...}}
            - 失败时: {"success": False, "msg": "错误信息"}

    数据流程:
        1. 从 settings.py 获取 AccessKey 和产品 ID
        2. 生成鉴权 Token
        3. 向 OneNET API 发送 GET 请求
        4. 解析返回的 JSON 数据，提取属性值
        5. 返回格式化的结果
    """
    access_key = settings.ONENET_ACCESS_KEY
    product_id = settings.ONENET_PRODUCT_ID  # ✅ 使用产品ID

    if not access_key or not product_id:
        return {"success": False, "msg": "请在 settings.py 中配置 ONENET_ACCESS_KEY 和 ONENET_PRODUCT_ID"}

    #  1. 生成 Token（传入 product_id）
    token = generate_onenet_token(access_key, product_id)
    print("生成的 token:", token)

    # 2. 构造 API 请求
    # 使用 iot-api.heclouds.com 域名（与成功代码一致）
    url = "https://iot-api.heclouds.com/thingmodel/query-device-property"
    params = {
        "product_id": product_id,
        "device_name": device_name,
    }
    headers = {
        "Authorization": token,
        "Accept": "application/json",
    }

    try:
        # 3. 发送 GET 请求
        # timeout=10 表示 10 秒超时
        response = requests.get(url, params=params, headers=headers, timeout=10)
        print("状态码:", response.status_code)
        print("原始内容:", repr(response.text[:300]))

        # 4. 检查响应是否为空
        if not response.text:
            return {
                "success": False,
                "msg": f"OneNET 返回空内容 | 状态码:{response.status_code}"
            }

        data = response.json()

        if data.get("code") == 0:
            properties = data.get("data", [])
            result = {}
            for prop in properties:
                # 安全取值：如果 value 不存在，使用空字符串或 None
                identifier = prop.get("identifier")
                # 如果 value 不存在或为 None，用空字符串代替
                value = prop.get("value", "")
                if identifier:
                    result[identifier] = value
                # result[prop["identifier"]] = prop["value"]
            return {"success": True, "data": result}
        else:
            return {"success": False, "msg": data.get("msg", "未知错误")}

    except requests.exceptions.Timeout:
        return {"success": False, "msg": "请求超时"}
    except ValueError:
        return {"success": False, "msg": f"OneNET 返回非 JSON: {response.text[:200]}"}
    except requests.exceptions.RequestException as e:
        return {"success": False, "msg": f"请求异常: {str(e)}"}


def get_device_status(product_id, device_name):
    """
    获取设备在线状态
    简化实现：通过查询属性判断

    Args:
        product_id (str): 产品 ID
        device_name (str): 设备名称

    Returns:
        dict: 设备状态信息
            - {"success": True, "online": True, "last_time": "在线"}
            - {"success": True, "online": False, "last_time": "离线"}

    实现方式:
        通过调用 get_device_properties 来判断设备是否在线。
        如果能够成功获取属性数据，则认为设备在线；否则视为离线。

    注意:
        这是一种间接判断方式，仅用于快速检测。更精确的方式是调用
        OneNET 的设备状态查询 API。
    """
    result = get_device_properties(product_id, device_name)

    if result.get("success"):
        return {
            "success": True,
            "online": True,
            "last_time": "在线"
        }
    else:
        return {
            "success": True,
            "online": False,
            "last_time": "离线"
        }


def send_command_to_onenet(device_name, cmd_value):
    """
    向 OneNET 下发属性设置指令
    """
    access_key = settings.ONENET_ACCESS_KEY
    product_id = settings.ONENET_PRODUCT_ID

    token = generate_onenet_token(access_key, product_id)

    url = "https://open.iot.10086.cn/thingmodel/set-device-property"

    body = {
        "product_id": product_id,
        "device_name": device_name,
        "params": {
            "card_cmd": {
                "value": cmd_value  # 格式: "recharge:20.00" 或 "deduct:15.50"
            }
        }
    }

    headers = {
        "Authorization": token,
        "Accept": "application/json, text/plain, */*",
        "Content-Type": "application/json",
    }

    try:
        response = requests.post(url, headers=headers, json=body, timeout=10)
        return response.json()
    except Exception as e:
        return {"code": 500, "msg": str(e)}