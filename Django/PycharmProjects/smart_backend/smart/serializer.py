
from rest_framework import serializers
from .models import Banners,Notice,Food

class BannersSerializer(serializers.ModelSerializer):
    class Meta:
        model = Banners
        fields = '__all__'

class NoticeSerializer(serializers.ModelSerializer):
    class Meta:
        model = Notice
        fields = '__all__'

class FoodSerializer(serializers.ModelSerializer):
    total_calorie = serializers.SerializerMethodField()
    nutrition_info = serializers.SerializerMethodField()

    class Meta:
        model = Food
        fields = ['id', 'name', 'price', 'calorie_per_100g', 'protein_per_100g',
                  'fat_per_100g', 'carb_per_100g', 'sauce_calorie', 'oil_calorie',
                  'default_weight', 'spiciness', 'rating', 'img', 'description',
                  'total_calorie', 'nutrition_info']

    @staticmethod
    def get_total_calorie(obj):
        """计算总热量"""
        weight = obj.default_weight
        food_calorie = (obj.calorie_per_100g / 100) * weight
        total = food_calorie + obj.sauce_calorie + obj.oil_calorie
        return round(total)

    @staticmethod
    def get_nutrition_info(obj):
        """获取完整营养信息"""
        weight = obj.default_weight
        ratio = weight / 100
        return {
            'weight': weight,
            'calorie': round((obj.calorie_per_100g * ratio) + obj.sauce_calorie + obj.oil_calorie),
            'protein': round(obj.protein_per_100g * ratio),
            'fat': round(obj.fat_per_100g * ratio + obj.oil_calorie / 9),
            'carb': round(obj.carb_per_100g * ratio),
            'sauce_calorie': obj.sauce_calorie,
            'oil_calorie': obj.oil_calorie
        }