
from django.contrib import admin
from django.urls import path, include
from django.conf import settings
from django.views.static import serve   # 必须导入
from django.conf.urls.static import static

urlpatterns = [
    path('admin/', admin.site.urls),
    path('', include('smart.urls')),

]

# 开发环境下提供媒体文件服务
if settings.DEBUG:
    urlpatterns += static(settings.MEDIA_URL, document_root=settings.MEDIA_ROOT)
