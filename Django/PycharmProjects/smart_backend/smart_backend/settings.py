

from pathlib import Path

BASE_DIR = Path(__file__).resolve().parent.parent

SECRET_KEY = 'django-insecure-(yug(_7(l2^n=(3=515ux#@_3&5xb0zp3xahrr$*@ii@bx(!9='

DEBUG = True

ALLOWED_HOSTS = ['*']

INSTALLED_APPS = [
    'simpleui',
    'django_extensions',
    'django.contrib.admin',
    'django.contrib.auth',
    'django.contrib.contenttypes',
    'django.contrib.sessions',
    'django.contrib.messages',
    'django.contrib.staticfiles',
    'smart.apps.SmartConfig',
    'rest_framework'
]

MIDDLEWARE = [
    'django.middleware.security.SecurityMiddleware',
    'django.contrib.sessions.middleware.SessionMiddleware',
    'django.middleware.common.CommonMiddleware',
    'django.middleware.csrf.CsrfViewMiddleware',
    'django.contrib.auth.middleware.AuthenticationMiddleware',
    'django.contrib.messages.middleware.MessageMiddleware',
    'django.middleware.clickjacking.XFrameOptionsMiddleware',
]

ROOT_URLCONF = 'smart_backend.urls'

TEMPLATES = [
    {
        'BACKEND': 'django.template.backends.django.DjangoTemplates',
        'DIRS': [BASE_DIR / 'templates'],
        'APP_DIRS': True,
        'OPTIONS': {
            'context_processors': [
                'django.template.context_processors.request',
                'django.contrib.auth.context_processors.auth',
                'django.contrib.messages.context_processors.messages',
            ],
        },
    },
]

WSGI_APPLICATION = 'smart_backend.wsgi.application'

DATABASES = {
    'default': {
        'ENGINE': 'django.db.backends.sqlite3',
        'NAME': BASE_DIR / 'db.sqlite3',
    }
}

AUTH_PASSWORD_VALIDATORS = [
    {
        'NAME': 'django.contrib.auth.password_validation.UserAttributeSimilarityValidator',
    },
    {
        'NAME': 'django.contrib.auth.password_validation.MinimumLengthValidator',
    },
    {
        'NAME': 'django.contrib.auth.password_validation.CommonPasswordValidator',
    },
    {
        'NAME': 'django.contrib.auth.password_validation.NumericPasswordValidator',
    },
]

LANGUAGE_CODE = 'zh-hans'

TIME_ZONE = 'Asia/Shanghai'

USE_I18N = True
USE_L10N = True
USE_TZ = False

STATIC_URL = 'static/'
DEFAULT_AUTO_FIELD = 'django.db.models.BigAutoField'


import os

MEDIA_URL = '/media/'
MEDIA_ROOT = os.path.join(BASE_DIR, 'media')

# 阿里云百炼API配置
DASHSCOPE_API_KEY = "sk-085c41f1aba74bfbadf77d34e197b375"  # 替换为你的API Key

# 微信小程序配置
WECHAT_APPID = 'wx3fcb6ebc34325eb7'
WECHAT_SECRET = '08a0e9513f34cd7e47b91d4a9e60d728'

# OneNET 配置
ONENET_ACCESS_KEY = "uZ9dqwsBYR03UykvIZHBJV2zKQd7/83Fg0ONJTWl07U="  # 从 OneNET 控制台获取  产品级
# ONENET_ACCESS_KEY = "b3fa6a7e97b140c7b716add78523009c"  # 你的 32 位十六进制 AccessKey 用户级别
# ONENET_ACCESS_KEY = "29bc3f4379814311bf4885a1583650c5"
ONENET_PRODUCT_ID = "dL26Jqg105"
ONENET_DEVICE_NAME = "dish"
ONENET_USER_ID = "479519"            # 从 OneNET 控制台右上角获取
ONENET_PROJECT_ID = "26QXZeTBwggpcof13350"