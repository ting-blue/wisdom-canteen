from django.contrib import admin

# Register your models here.


from .models import Welcome,Banners,Notice,Food

admin.site.register(Welcome)
admin.site.register(Banners)
admin.site.register(Notice)
# 注册食物模型（自定义显示）
@admin.register(Food)
class FoodAdmin(admin.ModelAdmin):
    list_display = [
        'id',
        'name',
        'price',
        'calorie_per_100g',
        'protein_per_100g',
        'fat_per_100g',
        'carb_per_100g',
        'sauce_calorie',
        'oil_calorie',
        'default_weight',
        'spiciness',
        'rating',
        'order',
        'is_delete'
    ]
    list_editable = ['order']
    list_filter = ['is_delete', 'spiciness']
    search_fields = ['name']
    list_per_page = 20