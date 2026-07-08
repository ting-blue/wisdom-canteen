// pages/tuijian/tuijian.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    foods:[]
  },

  onLoad() {
    this.getFoods()
  },

  computeSpiciness(level) {
    const icons = ['', '🌶️', '🌶️🌶️', '🌶️🌶️🌶️', '🌶️🌶️🌶️🌶️', '🌶️🌶️🌶️🌶️🌶️']
    return icons[level] || '不辣'
  },

  getFoods() {
    wx.request({
      url: `${BASE_URL}/foods/`,
      method: 'GET',
      success: (res) => {
        console.log('食物数据：', res.data)
        
        if (res.data.code === 100) {
          const foods = res.data.foods.map(item => {
            const ratio = (item.default_weight || 200) / 100
            return {
              ...item,
              img: item.img_b64,
              price: parseFloat(item.price).toFixed(2),
              spicinessText:
this.computeSpiciness(item.spiciness),
              selectedWeight: item.default_weight || 200,
              showDetail: false,
              nutrition_info: {
                calorie: Math.round((item.calorie_per_100g *ratio) + (item.sauce_calorie || 0) + (item.oil_calorie || 0)),
                protein: Math.round((item.protein_per_100g || 0) * ratio),
                fat: Math.round(((item.fat_per_100g || 0) * ratio) + ((item.oil_calorie || 0) / 9)),
                carb: Math.round((item.carb_per_100g || 0) * ratio),
                sauce_calorie: item.sauce_calorie || 0,
                oil_calorie: item.oil_calorie || 0
              }
            }
          })
          this.setData({ foods })
        }
      },
      fail: (err) => {
        console.error('请求失败：', err)
      }
    })
  },

  // 重量变化时重新计算热量
  onWeightInput(e) {
    const id = e.currentTarget.dataset.id
    const newWeight = parseInt(e.detail.value) || 200
    const foods = this.data.foods.map(item => {
      if (item.id === id) {
        const ratio = newWeight / 100
        const nutritionInfo = {
          weight: newWeight,
          calorie: Math.round((item.calorie_per_100g * ratio)
 + item.sauce_calorie + item.oil_calorie),
          protein: Math.round(item.protein_per_100g * ratio),
          fat: Math.round(item.fat_per_100g * ratio +
item.oil_calorie / 9),
          carb: Math.round(item.carb_per_100g * ratio),
          sauce_calorie: item.sauce_calorie,
          oil_calorie: item.oil_calorie
        }
        return { ...item, selectedWeight: newWeight,
nutrition_info: nutritionInfo }
      }
      return item
    })
    this.setData({ foods })
  },

  // 切换热量详情显示
  toggleDetail(e) {
    const id = e.currentTarget.dataset.id
    const foods = this.data.foods.map(item => {
      if (item.id === id) {
        return { ...item, showDetail: !item.showDetail }
      }
      return item
    })
    this.setData({ foods })
  },

  //AI营养师推荐食谱
  openAIChat() {
    wx.navigateTo({
      url: '/pages/yunshipu/yunshipu'
    })
  },

  // 记录用餐
  addToMealRecord(food, weight, mealType = 'lunch') {
    const userId = wx.getStorageSync('userId') || 'test_user'
  
  // 计算实际热量
    const ratio = weight / 100
    const calorie = Math.round((food.calorie_per_100g * ratio) + food.sauce_calorie + food.oil_calorie)
  
    wx.request({
      url: `${BASE_URL}/api/meal/add/`,
      method: 'POST',
      data: {
        user_id: userId,
        food_id: food.id,
        food_name: food.name,
        food_img: food.img,
        price: parseFloat(food.price),
        weight: weight,
        calorie: calorie,
        meal_type: mealType
      },
      success: (res) => {
        if (res.data.code === 100) {
          wx.showToast({ title: '已记录', icon: 'success' })
        }
      },
      fail: () => {
        console.error('记录失败')
      }
    })
  }
})