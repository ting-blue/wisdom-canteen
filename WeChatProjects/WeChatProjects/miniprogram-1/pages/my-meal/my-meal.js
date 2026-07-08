// pages/my-meal/my-meal.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    userId: '',
    // 原有的用餐记录数据
    records: [],
    groupedRecords: [],
    totalCalorie: 0,
    totalCount: 0,
    selectedDays: 7,
    dateRange: ['3天', '7天', '14天', '30天'],
    dateValues: [3, 7, 14, 30],
    
    // ===== 新增：从 OneNET 获取的最新菜品数据 =====
    currentDish: {
      dishName: '--',      // 菜品名称
      calorie: 0,          // 热量
      protein: 0,          // 蛋白质
      weight: 0,           // 重量
      position: '',        // 餐盘位置
      confidence: 0,       // 置信度
      confidencePercent: 0
    },
    isDishLoaded: false,   // 是否已加载数据
    lastUpdateTime: ''     // 最后更新时间
  },

  // 位置文本映射
  getPositionText(position) {
    const map = {
      'top_left': '左上',
      'top_right': '右上',
      'bottom_left': '左下',
      'bottom_right': '右下'
    }
    return map[position] || position
  },
  
  onLoad() {
    const token = wx.getStorageSync('token')
    if (!token) {
      this.setData({ groupedRecords: [] })
      return
    }
    wx.request({
      url: `${BASE_URL}/card/binding/status`,
      method: 'GET',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 0 && res.data.data.is_bound) {
          this.userId = res.data.data.card_uid
        } else {
          this.userId = wx.getStorageSync('userId') ||
'test_user'
        }
        this.loadRecords()
      },
      fail: () => {
        this.userId = wx.getStorageSync('userId') ||
'test_user'
        this.loadRecords()
      }
    })
  },

  onShow() {
    this.loadCurrentDish()  // 刷新最新数据
  },

  // ===== 新增：从 OneNET 获取最新菜品数据 =====
  loadCurrentDish() {
    wx.request({
      url: `${BASE_URL}/api/get_dish_data/`,
      method: 'GET',
      success: (res) => {
        console.log('获取最新菜品数据:', res.data)
        
        if (res.data.code === 100) {
          const data = res.data.data
          
          // 格式化时间
          const now = new Date()
          const timeStr = now.toLocaleString('zh-CN', {
            hour: '2-digit',
            minute: '2-digit',
            second: '2-digit'
          })
          const confidencePercent = (data.confidence * 100).toFixed(0);
          this.setData({
            currentDish: {
              dishName: data.dish_name || '--',
              calorie: data.calorie || 0,
              protein: data.protein || 0,
              weight: data.weight || 0,
              position: data.position || '',
              confidence: data.confidence || 0,
              confidencePercent: confidencePercent
            },
            isDishLoaded: true,
            lastUpdateTime: timeStr
          })
        } else {
          console.warn('获取菜品数据失败:', res.data.msg)
          this.setData({
            isDishLoaded: false
          })
        }
      },
      fail: (err) => {
        console.error('请求最新菜品数据失败:', err)
        this.setData({
          isDishLoaded: false
        })
      }
    })
  },

  // 加载用餐记录（原有方法，保持不变）
  loadRecords() {
    wx.showLoading({ title: '加载中...' })
    
    wx.request({
      url: `${BASE_URL}/api/meal/records/`,
      method: 'GET',
      data: {
        user_id: this.userId,
        days: this.data.selectedDays
      },
      success: (res) => {
        wx.hideLoading()
        
        if (res.data.code === 100) {
          const data = res.data.data
          // 补全营养数据
          const fixedRecords = this.fixNutrition(data.records || [])
          // ✅ 确保记录按时间排序（从新到旧）
          const sortedRecords = this.sortRecordsByTime(fixedRecords)
          // ✅ 按日期分组（并确保分组内也按时间排序）
          const grouped = this.groupRecordsByDate(sortedRecords)
          // ✅ 按日期从新到旧排序
          const sortedGrouped = grouped.sort((a, b) => {
            return b.date.localeCompare(a.date)
          })
          
          this.setData({
            records: data.records,
            groupedRecords: grouped,
            totalCalorie: data.total_calorie,
            totalCount: data.total_count
          })
        } else {
          wx.showToast({ title: res.data.msg || '加载失败', icon: 'none' })
        }
      },
      fail: (err) => {
        wx.hideLoading()
        console.error('加载失败：', err)
        wx.showToast({ title: '网络请求失败', icon: 'none' })
      }
    })
  },

  // ===== 补全营养数据 =====
  fixNutrition(records) {
    const db = {
      '西红柿炒鸡蛋': { calorie: 190, protein: 12 },
      '酸辣土豆丝': { calorie: 176, protein: 6 },
      '馒头': { calorie: 446, protein: 14 },
      '烧麦': { calorie: 440, protein: 16 },
      '纯牛奶': { calorie: 132, protein: 6 },
      '鸡蛋': { calorie: 288, protein: 26 },
      '红烧肉': { calorie: 700, protein: 30 },
      '白米饭': { calorie: 232, protein: 4 },
    }
    return records.map(r => {
      if (r.calorie === 0 && db[r.food_name]) {
        return { ...r, calorie: db[r.food_name].calorie, protein: db[r.food_name].protein }
      }
      return r
    })
  },

  // ===== 按时间排序记录（新增） =====
  sortRecordsByTime(records) {
    return records.sort((a, b) => {
      // 先按日期排序，再按时间排序
      const dateA = a.eat_date || a.create_time || ''
      const dateB = b.eat_date || b.create_time || ''
      if (dateA !== dateB) {
        return dateB.localeCompare(dateA)  // 日期从新到旧
      }
      const timeA = a.eat_time || a.create_time || ''
      const timeB = b.eat_time || b.create_time || ''
      return timeB.localeCompare(timeA)    // 时间从新到旧
    })
  },

  // ===== 按日期分组记录（新增） =====
  groupRecordsByDate(records) {
    const groupedMap = {}
      
    records.forEach(record => {
      const date = record.eat_date || record.create_time?.split(' ')[0] || '未知日期'
      if (!groupedMap[date]) {
        groupedMap[date] = []
      }
      groupedMap[date].push(record)
    })  
    // 将分组转换为数组，并确保每组内按时间排序
    const result = []
    for (const [date, items] of Object.entries(groupedMap)) {
      // 组内按时间从新到旧排序
      const sortedItems = items.sort((a, b) => {
        const timeA = a.eat_time || a.create_time || ''
        const timeB = b.eat_time || b.create_time || ''
        return timeB.localeCompare(timeA)
      })
        
      result.push({
        date: date,
        records: sortedItems,
        totalCalorie: sortedItems.reduce((sum, r) => sum + (r.calorie || 0), 0)
      })
    }  
    return result
  },

  // 日期范围切换（原有方法，保持不变）
  onDateChange(e) {
    const index = e.detail.value
    const days = this.data.dateValues[index]
    this.setData({ selectedDays: days })
    this.loadRecords()
  },

  // 删除记录（原有方法，保持不变）
  deleteRecord(e) {
    const recordId = e.currentTarget.dataset.id
    
    wx.showModal({
      title: '提示',
      content: '确定要删除这条记录吗？',
      success: (res) => {
        if (res.confirm) {
          wx.request({
            url: `${BASE_URL}/api/meal/delete/`,
            method: 'POST',
            data: {
              user_id: this.userId,
              record_id: recordId
            },
            success: (res) => {
              if (res.data.code === 100) {
                wx.showToast({ title: '删除成功', icon: 'success' })
                this.loadRecords()
              } else {
                wx.showToast({ title: res.data.msg || '删除失败', icon: 'none' })
              }
            },
            fail: () => {
              wx.showToast({ title: '网络请求失败', icon: 'none' })
            }
          })
        }
      }
    })
  },

  // ===== 新增：手动刷新最新菜品数据 =====
  refreshDish() {
    wx.showLoading({ title: '刷新中...' })
    this.loadCurrentDish()
    wx.hideLoading()
    wx.showToast({ title: '已刷新', icon: 'success' })
  }
})