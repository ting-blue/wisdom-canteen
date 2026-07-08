# add_foods.py

import os
import django

os.environ.setdefault('DJANGO_SETTINGS_MODULE', 'smart_backend.settings')
django.setup()

from smart.models import Food

foods_data = [
    # ========== 荤菜 ==========
    {
        'name': '青椒炒肉',
        'price': 8.50,
        'calorie_per_100g': 165,
        'protein_per_100g': 14,
        'fat_per_100g': 10,
        'carb_per_100g': 6,
        'sauce_calorie': 30,
        'oil_calorie': 90,
        'default_weight': 250,
        'spiciness': 2,
        'rating': 4.5,
        'description': '鲜香可口，下饭好菜。含青椒、瘦肉，营养均衡。'
    },
    {
        'name': '红烧排骨',
        'price': 18.00,
        'calorie_per_100g': 280,
        'protein_per_100g': 18,
        'fat_per_100g': 20,
        'carb_per_100g': 8,
        'sauce_calorie': 50,
        'oil_calorie': 70,
        'default_weight': 350,
        'spiciness': 1,
        'rating': 4.7,
        'description': '软烂入味，肉香四溢。精选猪小排。'
    },
    {
        'name': '小炒黄牛肉',
        'price': 18.50,
        'calorie_per_100g': 190,
        'protein_per_100g': 22,
        'fat_per_100g': 11,
        'carb_per_100g': 5,
        'sauce_calorie': 35,
        'oil_calorie': 80,
        'default_weight': 280,
        'spiciness': 3,
        'rating': 4.9,
        'description': '香辣嫩滑，牛肉鲜美。湘菜经典，下饭神器。'
    },
    {
        'name': '宫保鸡丁',
        'price': 24.00,
        'calorie_per_100g': 155,
        'protein_per_100g': 16,
        'fat_per_100g': 9,
        'carb_per_100g': 6,
        'sauce_calorie': 35,
        'oil_calorie': 70,
        'default_weight': 280,
        'spiciness': 2,
        'rating': 4.7,
        'description': '酸甜微辣，鸡肉嫩滑。花生酥脆，下饭好菜。'
    },
    {
        'name': '糖醋里脊',
        'price': 26.00,
        'calorie_per_100g': 280,
        'protein_per_100g': 15,
        'fat_per_100g': 18,
        'carb_per_100g': 18,
        'sauce_calorie': 60,
        'oil_calorie': 100,
        'default_weight': 250,
        'spiciness': 0,
        'rating': 4.6,
        'description': '酸甜酥脆，老少皆宜。外酥里嫩，经典家常菜。'
    },
    {
        'name': '酸菜鱼',
        'price': 28.00,
        'calorie_per_100g': 110,
        'protein_per_100g': 15,
        'fat_per_100g': 4,
        'carb_per_100g': 5,
        'sauce_calorie': 45,
        'oil_calorie': 70,
        'default_weight': 500,
        'spiciness': 3,
        'rating': 4.8,
        'description': '酸爽开胃，鱼肉嫩滑。巴沙鱼无刺，老少皆宜。'
    },

    # ========== 素菜 ==========
    {
        'name': '清炒时蔬',
        'price': 4.00,
        'calorie_per_100g': 35,
        'protein_per_100g': 2,
        'fat_per_100g': 2,
        'carb_per_100g': 4,
        'sauce_calorie': 10,
        'oil_calorie': 45,
        'default_weight': 200,
        'spiciness': 0,
        'rating': 4.2,
        'description': '清淡健康，富含维生素。选用当季新鲜时蔬。'
    },
    {
        'name': '麻婆豆腐',
        'price': 3.00,
        'calorie_per_100g': 120,
        'protein_per_100g': 8,
        'fat_per_100g': 8,
        'carb_per_100g': 5,
        'sauce_calorie': 40,
        'oil_calorie': 80,
        'default_weight': 300,
        'spiciness': 3,
        'rating': 4.8,
        'description': '麻辣鲜香，经典川菜。豆腐富含植物蛋白。'
    },
    {
        'name': '番茄炒蛋',
        'price': 4.00,
        'calorie_per_100g': 95,
        'protein_per_100g': 6,
        'fat_per_100g': 6,
        'carb_per_100g': 5,
        'sauce_calorie': 15,
        'oil_calorie': 54,
        'default_weight': 250,
        'spiciness': 0,
        'rating': 4.6,
        'description': '酸甜可口，家常美味。富含维生素C和蛋白质。'
    },
    {
        'name': '蒜蓉西兰花',
        'price': 5.00,
        'calorie_per_100g': 35,
        'protein_per_100g': 3,
        'fat_per_100g': 1,
        'carb_per_100g': 5,
        'sauce_calorie': 10,
        'oil_calorie': 45,
        'default_weight': 200,
        'spiciness': 1,
        'rating': 4.5,
        'description': '清脆爽口，营养健康。富含维生素C和膳食纤维。'
    },
    {
        'name': '红烧茄子',
        'price': 5.00,
        'calorie_per_100g': 130,
        'protein_per_100g': 2,
        'fat_per_100g': 10,
        'carb_per_100g': 8,
        'sauce_calorie': 30,
        'oil_calorie': 80,
        'default_weight': 250,
        'spiciness': 1,
        'rating': 4.3,
        'description': '软糯入味，下饭神器。茄子吸味，酱香浓郁。'
    },
    {
        'name': '清炒生菜',
        'price': 5.00,
        'calorie_per_100g': 25,
        'protein_per_100g': 1,
        'fat_per_100g': 2,
        'carb_per_100g': 3,
        'sauce_calorie': 5,
        'oil_calorie': 40,
        'default_weight': 200,
        'spiciness': 0,
        'rating': 4.5,
        'description': '清脆爽口，简单健康。生菜富含维生素C和水分。'
    },

    # ========== 主食 ==========
    {
        'name': '白米饭',
        'price': 1.00,
        'calorie_per_100g': 116,
        'protein_per_100g': 2,
        'fat_per_100g': 0,
        'carb_per_100g': 26,
        'sauce_calorie': 0,
        'oil_calorie': 0,
        'default_weight': 200,
        'spiciness': 0,
        'rating': 4.5,
        'description': '香软可口，每日主食。提供充足碳水化合物。'
    },
    {
        'name': '杂粮米饭',
        'price': 2.00,
        'calorie_per_100g': 118,
        'protein_per_100g': 3,
        'fat_per_100g': 1,
        'carb_per_100g': 24,
        'sauce_calorie': 0,
        'oil_calorie': 0,
        'default_weight': 200,
        'spiciness': 0,
        'rating': 4.7,
        'description': '糙米、小米、燕麦混合，膳食纤维丰富，饱腹感强。'
    },
    {
        'name': '全麦馒头',
        'price': 1.50,
        'calorie_per_100g': 230,
        'protein_per_100g': 8,
        'fat_per_100g': 2,
        'carb_per_100g': 46,
        'sauce_calorie': 0,
        'oil_calorie': 0,
        'default_weight': 80,
        'spiciness': 0,
        'rating': 4.2,
        'description': '健康粗粮，饱腹感强。全麦制作，膳食纤维丰富。'
    },
    {
        'name': '葱花卷',
        'price': 1.50,
        'calorie_per_100g': 250,
        'protein_per_100g': 7,
        'fat_per_100g': 5,
        'carb_per_100g': 45,
        'sauce_calorie': 5,
        'oil_calorie': 20,
        'default_weight': 100,
        'spiciness': 0,
        'rating': 4.3,
        'description': '松软咸香，传统中式面点。'
    },
    {
        'name': '玉米段',
        'price': 2.00,
        'calorie_per_100g': 86,
        'protein_per_100g': 3,
        'fat_per_100g': 1,
        'carb_per_100g': 19,
        'sauce_calorie': 0,
        'oil_calorie': 0,
        'default_weight': 150,
        'spiciness': 0,
        'rating': 4.4,
        'description': '香甜软糯，粗粮健康选择。'
    },
    {
        'name': '红薯',
        'price': 2.50,
        'calorie_per_100g': 86,
        'protein_per_100g': 1,
        'fat_per_100g': 0,
        'carb_per_100g': 20,
        'sauce_calorie': 0,
        'oil_calorie': 0,
        'default_weight': 150,
        'spiciness': 0,
        'rating': 4.6,
        'description': '香甜软糯，富含膳食纤维和维生素A。'
    },
    {
        'name': '葱油拌面',
        'price': 7.00,
        'calorie_per_100g': 250,
        'protein_per_100g': 8,
        'fat_per_100g': 10,
        'carb_per_100g': 35,
        'sauce_calorie': 20,
        'oil_calorie': 60,
        'default_weight': 200,
        'spiciness': 0,
        'rating': 4.4,
        'description': '葱香浓郁，简单美味。上海特色小吃。'
    },
    {
        'name': '贝贝南瓜',
        'price': 5.00,
        'calorie_per_100g': 78,
        'protein_per_100g': 2,
        'fat_per_100g': 0,
        'carb_per_100g': 18,
        'sauce_calorie': 0,
        'oil_calorie': 10,
        'default_weight': 200,
        'spiciness': 0,
        'rating': 4.7,
        'description': '香甜粉糯，营养丰富。富含胡萝卜素和膳食纤维，健康粗粮首选。'
    },

    # ========== 粥汤 ==========
    {
        'name': '皮蛋瘦肉粥',
        'price': 9.00,
        'calorie_per_100g': 60,
        'protein_per_100g': 5,
        'fat_per_100g': 2,
        'carb_per_100g': 8,
        'sauce_calorie': 5,
        'oil_calorie': 10,
        'default_weight': 400,
        'spiciness': 0,
        'rating': 4.5,
        'description': '绵密顺滑，暖心暖胃。早餐夜宵首选。'
    },
    {
        'name': '玉米排骨汤',
        'price': 22.00,
        'calorie_per_100g': 55,
        'protein_per_100g': 6,
        'fat_per_100g': 3,
        'carb_per_100g': 4,
        'sauce_calorie': 5,
        'oil_calorie': 10,
        'default_weight': 400,
        'spiciness': 0,
        'rating': 4.6,
        'description': '清甜滋润，营养汤品。玉米香甜，排骨软烂。'
    },
    {
        'name': '紫菜蛋花汤',
        'price': 5.00,
        'calorie_per_100g': 20,
        'protein_per_100g': 2,
        'fat_per_100g': 1,
        'carb_per_100g': 2,
        'sauce_calorie': 5,
        'oil_calorie': 5,
        'default_weight': 300,
        'spiciness': 0,
        'rating': 4.3,
        'description': '清淡鲜美，简单开胃。'
    },

    # ========== 小吃点心 ==========
    {
        'name': '水煮鸡蛋',
        'price': 1.50,
        'calorie_per_100g': 155,
        'protein_per_100g': 13,
        'fat_per_100g': 11,
        'carb_per_100g': 1,
        'sauce_calorie': 0,
        'oil_calorie': 0,
        'default_weight': 50,
        'spiciness': 0,
        'rating': 4.0,
        'description': '简单营养，优质蛋白来源。水煮无油更健康。'
    },
    {
        'name': '香菇烧麦',
        'price': 2.00,
        'calorie_per_100g': 220,
        'protein_per_100g': 7,
        'fat_per_100g': 12,
        'carb_per_100g': 22,
        'sauce_calorie': 10,
        'oil_calorie': 20,
        'default_weight': 150,
        'spiciness': 0,
        'rating': 4.3,
        'description': '皮薄馅香，早餐首选。香菇提鲜，口感丰富。'
    },
    {
        'name': '韭菜盒子',
        'price': 4.00,
        'calorie_per_100g': 210,
        'protein_per_100g': 7,
        'fat_per_100g': 12,
        'carb_per_100g': 20,
        'sauce_calorie': 5,
        'oil_calorie': 40,
        'default_weight': 120,
        'spiciness': 0,
        'rating': 4.2,
        'description': '外酥里嫩，韭菜鲜香。传统面点，趁热吃最佳。'
    },
    {
        'name': '油条',
        'price': 2.00,
        'calorie_per_100g': 388,
        'protein_per_100g': 7,
        'fat_per_100g': 20,
        'carb_per_100g': 43,
        'sauce_calorie': 0,
        'oil_calorie': 150,
        'default_weight': 100,
        'spiciness': 0,
        'rating': 4.0,
        'description': '金黄酥脆，早餐经典。建议适量食用，搭配豆浆更佳。'
    },
    {
        'name': '豆浆',
        'price': 1.00,
        'calorie_per_100g': 32,
        'protein_per_100g': 3,
        'fat_per_100g': 1.5,
        'carb_per_100g': 2,
        'sauce_calorie': 5,
        'oil_calorie': 0,
        'default_weight': 300,
        'spiciness': 0,
        'rating': 4.6,
        'description': '香浓顺滑，植物蛋白饮品。现磨豆浆，营养早餐搭档。'
    },
]

# ========== 去重添加逻辑 ==========
added_count = 0
skipped_count = 0

for data in foods_data:
    # 检查是否已存在同名菜品
    existing = Food.objects.filter(name=data['name']).first()
    if existing:
        print(f"⏭️ 跳过（已存在）: {data['name']} (ID: {existing.id})")
        skipped_count += 1
    else:
        food = Food.objects.create(**data)
        print(f"✅ 已添加: {food.name} (ID: {food.id}) - ¥{food.price} - {food.calorie_per_100g}kcal/100g")
        added_count += 1

print(f"\n{'='*50}")
print(f"添加完成！")
print(f"  ✅ 新增: {added_count} 道菜")
print(f"  ⏭️ 跳过: {skipped_count} 道菜（已存在）")
print(f"  📊 总计: {Food.objects.count()} 道菜")
print(f"{'='*50}")

# 打印所有菜品ID和名称
print("\n当前所有菜品列表：")
for food in Food.objects.all().order_by('id'):
    print(f"  ID {food.id}: {food.name} - ¥{food.price}")