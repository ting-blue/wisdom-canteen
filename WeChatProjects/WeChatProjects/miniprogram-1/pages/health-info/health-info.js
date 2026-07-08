// pages/health-info/health-info.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    // 基础信息
    height: '',
    heightSlider: 170,
    weight: '',
    weightSlider: 65,
    bodyFat: '',
    bodyFatSlider: 20,
    age: '',
    ageSlider: 30,
    gender: '',
    
    // 活动水平
    activityLevel: '',
    
    // 饮食目标
    goal: '',
    
    // 过敏项（包含自定义）
    allergies: [],
    customAllergy: '',
    customAllergies: [],
    
    // 疾病项（包含自定义）
    diseases: [],
    customDisease: '',
    customDiseases: [],
    
    userId: ''
  },

  onLoad(options) {
    const userInfo = wx.getStorageSync('userInfo')
    this.setData({ userId: userInfo?.id || 'test_user' })
  },

  // 身高输入
  onHeightInput(e) {
    const val = parseFloat(e.detail.value) || 0
    this.setData({ height: val, heightSlider: val })
  },
  onHeightSlider(e) {
    const val = e.detail.value
    this.setData({ height: val, heightSlider: val })
  },

  // 体重输入
  onWeightInput(e) {
    const val = parseFloat(e.detail.value) || 0
    this.setData({ weight: val, weightSlider: val })
  },
  onWeightSlider(e) {
    const val = e.detail.value
    this.setData({ weight: val, weightSlider: val })
  },

  // 体脂率输入
  onBodyFatInput(e) {
    const val = parseFloat(e.detail.value) || 0
    this.setData({ bodyFat: val, bodyFatSlider: val })
  },
  onBodyFatSlider(e) {
    const val = e.detail.value
    this.setData({ bodyFat: val, bodyFatSlider: val })
  },

  // 年龄输入
  onAgeInput(e) {
    const val = parseInt(e.detail.value) || 0
    this.setData({ age: val, ageSlider: val })
  },
  onAgeSlider(e) {
    const val = e.detail.value
    this.setData({ age: val, ageSlider: val })
  },

  // 性别选择
  selectGender(e) {
    this.setData({ gender: e.currentTarget.dataset.gender })
  },

  // 活动水平选择
  selectActivity(e) {
    this.setData({ activityLevel: e.currentTarget.dataset.level })
  },

  // 饮食目标选择
  selectGoal(e) {
    this.setData({ goal: e.currentTarget.dataset.goal })
  },

  // 切换过敏选项
  toggleAllergy(e) {
    const allergy = e.currentTarget.dataset.allergy
    let allergies = [...this.data.allergies]
    const index = allergies.indexOf(allergy)

    console.log('点击忌口：', allergy)
    console.log('当前 allergies：', allergies)

    if (index === -1) {
      allergies.push(allergy)
    } else {
      allergies.splice(index, 1)
    }
    // 特殊处理：如果选了"无"，清空其他选项
    if (allergy === 'none' && index === -1) {
      allergies = ['none']
    } else if (allergies.includes('none') && allergy !== 'none') {
      // 如果已有"无"，再选其他时，移除"无"
      const noneIndex = allergies.indexOf('none')
      if (noneIndex !== -1) {
        allergies.splice(noneIndex, 1)
      }
    }
    
    console.log('新的 allergies：', allergies)
    this.setData({ allergies })
  },

  // 自定义忌口输入
  onCustomAllergyInput(e) {
    this.setData({ customAllergy: e.detail.value })
  },

  // 添加自定义忌口
  addCustomAllergy() {
    const text = this.data.customAllergy.trim()
    if (!text) return
    if (this.data.customAllergies.includes(text)) {
      wx.showToast({ title: '已存在', icon: 'none' })
      return
    }
    this.setData({
      customAllergies: [...this.data.customAllergies, text],
      customAllergy: ''
    })
  },

  // 删除自定义忌口
  removeCustomAllergy(e) {
    const index = e.currentTarget.dataset.index
    const customAllergies = [...this.data.customAllergies]
    customAllergies.splice(index, 1)
    this.setData({ customAllergies })
  },

  // 切换疾病选项
  toggleDisease(e) {
    const disease = e.currentTarget.dataset.disease
    let diseases = [...this.data.diseases]
    const index = diseases.indexOf(disease)

    console.log('点击健康状况：', disease)
    console.log('当前 diseases：', diseases)

    if (index === -1) {
      diseases.push(disease)
    } else {
      diseases.splice(index, 1)
    }
    // 特殊处理：如果选了"无"，清空其他选项
    if (disease === 'none' && index === -1) {
      diseases = ['none']
    } else if (diseases.includes('none') && disease !== 'none') {
      const noneIndex = diseases.indexOf('none')
      if (noneIndex !== -1) {
         diseases.splice(noneIndex, 1)
      }
    }
        
    console.log('新的 diseases：', diseases)
    this.setData({ diseases })
  },

  // 自定义健康状况输入
  onCustomDiseaseInput(e) {
    this.setData({ customDisease: e.detail.value })
  },

  // 添加自定义健康状况
  addCustomDisease() {
    const text = this.data.customDisease.trim()
    if (!text) return
    if (this.data.customDiseases.includes(text)) {
      wx.showToast({ title: '已存在', icon: 'none' })
      return
    }
    this.setData({
      customDiseases: [...this.data.customDiseases, text],
      customDisease: ''
    })
  },

  // 删除自定义健康状况
  removeCustomDisease(e) {
    const index = e.currentTarget.dataset.index
    const customDiseases = [...this.data.customDiseases]
    customDiseases.splice(index, 1)
    this.setData({ customDiseases })
  },

  // 提交健康信息
  submitHealthInfo() {
    const { height, weight, age, gender, activityLevel, goal, userId } = this.data
    
    if (!height) { wx.showToast({ title: '请输入身高', icon: 'none' }); return }
    if (!weight) { wx.showToast({ title: '请输入体重', icon: 'none' }); return }
    if (!age) { wx.showToast({ title: '请输入年龄', icon: 'none' }); return }
    if (!gender) { wx.showToast({ title: '请选择性别', icon: 'none' }); return }
    
    // 合并所有忌口（预设 + 自定义）
    const allAllergies = [...this.data.allergies, ...this.data.customAllergies]
    // 合并所有健康状况（预设 + 自定义）
    const allDiseases = [...this.data.diseases, ...this.data.customDiseases]
    
    const token = wx.getStorageSync('token')
    wx.showLoading({ title: '保存中...' })
    
    wx.request({
      url: `${BASE_URL}/api/health-info/`,
      method: 'POST',
      header: { 'Authorization': token },
      data: {
        user_id: userId,
        height: this.data.height,
        weight: this.data.weight,
        body_fat: this.data.bodyFat || null,
        age: this.data.age,
        gender: this.data.gender,
        activity_level: this.data.activityLevel,
        goal: this.data.goal,
        allergies: this.data.allergies,
        diseases: this.data.diseases,
        token: token
      },
      success: (res) => {
        wx.hideLoading()
        if (res.data.code === 100) {
          wx.showToast({ title: '保存成功', icon: 'success' })
          setTimeout(() => {
            wx.switchTab({ url: '/pages/index/index' })
          }, 1500)
        } else {
          wx.showToast({ title: res.data.msg || '保存失败', icon: 'none' })
        }
      },
      fail: () => {
        wx.hideLoading()
        wx.showToast({ title: '网络请求失败', icon: 'none' })
      }
    })
  },

  // 跳过
  skipHealthInfo() {
    wx.showModal({
      title: '提示',
      content: '健康信息可以帮助我们为您提供更精准的推荐，确定要跳过吗？',
      success: (res) => {
        if (res.confirm) {
          wx.switchTab({ url: '/pages/index/index' })
        }
      }
    })
  }
})