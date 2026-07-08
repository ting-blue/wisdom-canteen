
from . import views
from django.urls import path
from django.conf import settings
from django.conf.urls.static import static
from rest_framework.routers import SimpleRouter

from .views import get_welcome_image, BannersViewSet,FoodViewSet
from .views import get_balance, get_bills, recharge, withdraw

from .views import ai_recommend_recipe, ai_analyze_nutrition, ai_health_tips
from .views import ai_recommend_from_menu, ai_analyze_meal

from .views import add_meal_record, get_meal_records, delete_meal_record

from .views import wx_login, check_token, update_profile, health_info, get_user_profile

from .views import get_dish_data   #, get_device_status

from .views import card_query,card_recharge, card_deduct, card_command_result,card_sync_and_query
from .views import card_record_create, card_record_latest
from .views import pending_commands, complete_command

# 创建路由器
router=SimpleRouter()
router.register(r'banners', BannersViewSet, 'banners')
router.register(r'foods', FoodViewSet, basename='foods')  # 新增食物路由


urlpatterns = [
    #http://192.168.31.101:8000/smart/get_welcome_image/
    path('smart/get_welcome_image/', get_welcome_image),
    path('api/balance/', get_balance),
    path('api/bills/', get_bills),
    path('api/recharge/', recharge),
    path('api/withdraw/', withdraw),
    path('api/ai/recommend/', ai_recommend_recipe),
    path('api/ai/nutrition/', ai_analyze_nutrition),
    path('api/ai/tips/', ai_health_tips),
    path('api/ai/recommend-from-menu/', ai_recommend_from_menu),
    path('api/ai/analyze-meal/', ai_analyze_meal),
    path('api/meal/add/', add_meal_record),
    path('api/meal/records/', get_meal_records),
    path('api/meal/delete/', delete_meal_record),
    path('api/wx-login/', wx_login),
    path('api/check-token/', check_token),
    path('api/user/profile/', get_user_profile),
    path('api/update-profile/', update_profile),
    path('api/health-info/', health_info),
    path('api/get_dish_data/', get_dish_data, name='get_dish_data'),
    # path('api/get_device_status/', get_device_status, name='get_device_status'),
    path('api/card/query/', card_query, name='card_query'),
    path('api/card/recharge/', card_recharge, name='card_recharge'),
    path('api/card/deduct/', card_deduct, name='card_deduct'),
    path('api/card/result/', card_command_result, name='card_command_result'),
    # 卡绑定相关
    path('card/binding/status', views.card_binding_status, name='card_binding_status'),
    path('card/bind/start', views.card_bind_start, name='card_bind_start'),
    path('card/bind/confirm', views.card_bind_confirm, name='card_bind_confirm'),
    path('card/bind/manual', views.card_bind_manual, name='card_bind_manual'),
    path('card/bind/refresh', views.card_bind_refresh, name='card_bind_refresh'),
    path('card/unbind', views.card_unbind, name='card_unbind'),
    path('card/bind/history', views.card_bind_history, name='card_bind_history'),
    path('card/bind/wait/status', views.card_bind_wait_status, name='card_bind_wait_status'),
    path('api/card/sync/', views.card_sync_and_query,name='card_sync'),
    path('smart/api/card/record/', card_record_create,name='card_record_create'),
    path('api/card/latest/', card_record_latest,name='card_record_latest'),
    path('api/chat/save/', views.chat_save),
    path('api/chat/load/', views.chat_load),
    path('api/report/daily/', views.daily_report),
    path('api/card/pending/', pending_commands),
    path('api/card/complete/', complete_command),
]

urlpatterns += router.urls

# 开发环境下提供媒体文件服务
if settings.DEBUG:
    urlpatterns += static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)
