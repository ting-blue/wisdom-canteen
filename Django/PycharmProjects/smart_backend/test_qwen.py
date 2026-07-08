# test_qwen.py
import requests
import json

# 你的 API Key
API_KEY = "sk-085c41f1aba74bfbadf77d34e197b375"

url = "https://dashscope.aliyuncs.com/compatible-mode/v1/chat/completions"

headers = {
    "Authorization": f"Bearer {API_KEY}",
    "Content-Type": "application/json"
}

data = {
    "model": "qwen-max",
    "messages": [
        {"role": "system", "content": "你是智慧食堂的营养师助手"},
        {"role": "user", "content": "我有鸡蛋和番茄，推荐一道菜"}
    ]
}

try:
    response = requests.post(url, headers=headers, json=data, timeout=30)
    result = response.json()

    print("状态码:", response.status_code)
    print("\n返回结果:")
    print(json.dumps(result, indent=2, ensure_ascii=False))

    if "choices" in result:
        content = result["choices"][0]["message"]["content"]
        print(f"\n🤖 AI回复:\n{content}")
    elif "error" in result:
        print(f"\n❌ API错误: {result['error'].get('message', '未知错误')}")

except Exception as e:
    print(f"\n❌ 请求失败: {e}")