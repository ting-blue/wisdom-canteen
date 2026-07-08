// pages/health/health.js

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
    
    // 生活方式
    activityLevel: '',
    goal: '',
    
    // 忌口/过敏
    allergies: [],
    customAllergy: '',
    customAllergies: [],
    
    // 健康状况
    diseases: [],
    customDisease: '',
    customDiseases: [],
    reportData: null
  },

  onLoad(options) {
    const mode = options.mode || ''
    if (mode === 'report') {
      this.loadDailyReport()
    } else {
      this.loadHealthInfo()
    }
  },

  // 加载健康信息
  loadHealthInfo() {
    const token = wx.getStorageSync('token')
    wx.request({
      url: `${BASE_URL}/api/health-info/`,
      method: 'GET',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 100) {
          const info = res.data.data
          this.setData({
            height: info.height || '',
            heightSlider: info.height || 170,
            weight: info.weight || '',
            weightSlider: info.weight || 65,
            bodyFat: info.body_fat || '',
            bodyFatSlider: info.body_fat || 20,
            age: info.age || '',
            ageSlider: info.age || 30,
            gender: info.gender || '',
            activityLevel: info.activity_level || '',
            goal: info.goal || '',
            allergies: info.allergies || [],
            diseases: info.diseases || []
          })
          console.log('当前 allergies 数组:', this.data.allergies)
          console.log('是否包含 peanuts:', this.data.allergies.indexOf('peanuts') !== -1)
        }
      },
      fail: () => {
        // 测试数据
        this.setData({
          height: 175,
          weight: 70,
          age: 25,
          gender: 'male'
        })
      }
    })
  },

  loadDailyReport() {
    const token = wx.getStorageSync('token')
    if (!token) return
    wx.request({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/api/report/daily/',
      method: 'GET',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 0) {
          const d = res.data.data
          this.setData({
            reportData: {
              date: d.date,
              totalCalorie: d.total_calorie,
              totalProtein: d.total_protein,
              mealCount: d.meal_count,
              dishes: d.dishes.join('、'),
              content: d.report
            }
          })
        } else {
          this.setData({
            reportData: {
              date: '今天',
              totalCalorie: 0,
              totalProtein: 0,
              mealCount: 0,
              dishes: '',
              content: res.data.msg || '暂无数据'
            }
          })
        }
      }
    })
  },

  // 保存健康信息
  saveHealthInfo() {
    const token = wx.getStorageSync('token')
    const allAllergies = [...this.data.allergies, ...this.data.customAllergies]
    const allDiseases = [...this.data.diseases, ...this.data.customDiseases]
    
    wx.request({
      url: `${BASE_URL}/api/health-info/`,
      method: 'POST',
      header: { 'Authorization': token },
      data: {
        height: this.data.height,
        weight: this.data.weight,
        body_fat: this.data.bodyFat || null,
        age: this.data.age,
        gender: this.data.gender,
        activity_level: this.data.activityLevel,
        goal: this.data.goal,
        allergies: allAllergies,
        diseases: allDiseases
      },
      success: (res) => {
        if (res.data.code === 100) {
          wx.showToast({ title: '保存成功', icon: 'success' })
          setTimeout(() => {
            wx.navigateBack()
          }, 1500)
        } else {
          wx.showToast({ title: res.data.msg || '保存失败', icon: 'none' })
        }
      }
    })
  },

  // 输入事件
  onHeightInput(e) { 
    const val = parseFloat(e.detail.value) || 0
    this.setData({ height: val, heightSlider: val })
  },
  onHeightSlider(e) { 
    const val = e.detail.value
    this.setData({ height: val, heightSlider: val })
  },
  onWeightInput(e) { 
    const val = parseFloat(e.detail.value) || 0
    this.setData({ weight: val, weightSlider: val })
  },
  onWeightSlider(e) { 
    const val = e.detail.value
    this.setData({ weight: val, weightSlider: val })
  },
  onBodyFatInput(e) { 
    const val = parseFloat(e.detail.value) || 0
    this.setData({ bodyFat: val, bodyFatSlider: val })
  },
  onBodyFatSlider(e) { 
    const val = e.detail.value
    this.setData({ bodyFat: val, bodyFatSlider: val })
  },
  onAgeInput(e) { 
    const val = parseInt(e.detail.value) || 0
    this.setData({ age: val, ageSlider: val })
  },
  onAgeSlider(e) { 
    const val = e.detail.value
    this.setData({ age: val, ageSlider: val })
  },
  
  selectGender(e) { 
    this.setData({ gender: e.currentTarget.dataset.gender }) 
  },
  selectActivity(e) { 
    this.setData({ activityLevel: e.currentTarget.dataset.level }) 
  },
  selectGoal(e) { 
    this.setData({ goal: e.currentTarget.dataset.goal }) 
  },
  
  toggleAllergy(e) {
    const allergy = e.currentTarget.dataset.allergy
    let allergies = [...this.data.allergies]
    const idx = allergies.indexOf(allergy)
    idx === -1 ? allergies.push(allergy) : allergies.splice(idx, 1)
    this.setData({ allergies })
  },
  
  toggleDisease(e) {
    const disease = e.currentTarget.dataset.disease
    let diseases = [...this.data.diseases]
    const idx = diseases.indexOf(disease)
    idx === -1 ? diseases.push(disease) : diseases.splice(idx, 1)
    this.setData({ diseases })
  },
  
  onCustomAllergyInput(e) { 
    this.setData({ customAllergy: e.detail.value }) 
  },
  addCustomAllergy() {
    const val = this.data.customAllergy.trim()
    if (!val) return
    if (!this.data.customAllergies.includes(val)) {
      this.setData({ 
        customAllergies: [...this.data.customAllergies, val], 
        customAllergy: '' 
      })
    }
  },
  removeCustomAllergy(e) {
    const idx = e.currentTarget.dataset.index
    const arr = [...this.data.customAllergies]
    arr.splice(idx, 1)
    this.setData({ customAllergies: arr })
  },
  
  onCustomDiseaseInput(e) { 
    this.setData({ customDisease: e.detail.value }) 
  },
  addCustomDisease() {
    const val = this.data.customDisease.trim()
    if (!val) return
    if (!this.data.customDiseases.includes(val)) {
      this.setData({ 
        customDiseases: [...this.data.customDiseases, val], 
        customDisease: '' 
      })
    }
  },
  removeCustomDisease(e) {
    const idx = e.currentTarget.dataset.index
    const arr = [...this.data.customDiseases]
    arr.splice(idx, 1)
    this.setData({ customDiseases: arr })
  }
})