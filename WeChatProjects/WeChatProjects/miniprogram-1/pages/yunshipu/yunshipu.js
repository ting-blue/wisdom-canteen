// pages/yunshipu/yunshipu.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'
const STORAGE_KEY = 'yunshipu_chat_history'  // 本地存储的 key

Page({
  data: {
    messages: [],
    inputText: '',
    isLoading: false,
    scrollToView: '',
    userHealthInfo: null,  // 用户健康信息
    menuItems: [],         // 食堂菜单
    statusBarHeight: 20,   // 状态栏高度（默认值）
    headerHeight: 80,      // 头部总高度（默认值）
    safeBottomHeight: 0    // 底部安全区域高度（默认值）
  },

  onLoad() {
    this.initSafeArea()           // 初始化安全区域
    this.loadChatHistory() 
    this.loadUserHealthInfo()  // 加载健康档案
    this.loadMenuItems()       // 加载菜单
  },

  // ===== 加载历史聊天记录（新增） =====
  loadChatHistory() {
    try {
      const history = wx.getStorageSync(STORAGE_KEY)
      if (history && Array.isArray(history) && history.length > 0) {
        this.setData({ messages: history })
        console.log('加载历史聊天记录:', history.length, '条')
        // 滚动到底部
        setTimeout(() => {
          this.setData({ scrollToView: 'bottom' })
        }, 300)
      } else {
        console.log('没有历史聊天记录')
      }
    } catch (e) {
      console.error('加载历史记录失败:', e)
    }
  },

  // ===== 保存聊天记录到本地（新增） =====
  saveChatHistory() {
    const token = wx.getStorageSync('token')
    // 本地存储
    try { wx.setStorageSync(STORAGE_KEY, this.data.messages) }
  catch (e) {}
    // 云端同步
    if (token) {
      wx.request({
        url: `${BASE_URL}/api/chat/save/`,
        method: 'POST',
        header: { 'Authorization': token },
        data: { messages: this.data.messages }
      })
    }
  },
  // ===== 清除历史记录（可选功能） =====
  clearHistory() {
    wx.showModal({
      title: '确认清除',
      content: '确定要清除所有聊天记录吗？',
      success: (res) => {
        if (res.confirm) {
          try {
            wx.removeStorageSync(STORAGE_KEY)
            this.setData({ messages: [] })
            wx.showToast({ title: '已清除', icon: 'success' })
          } catch (e) {
            wx.showToast({ title: '清除失败', icon: 'none' })
          }
        }
      }
    })
  },

  // 初始化安全区域（适配灵动岛和底部横条）- 新增方法
  initSafeArea() {
    // 获取系统信息
    const systemInfo = wx.getSystemInfoSync()
    const statusBarHeight = systemInfo.statusBarHeight || 20
    
    // 获取胶囊按钮位置（用于计算头部高度）
    const menuButtonInfo = wx.getMenuButtonBoundingClientRect()
    
    // 计算头部高度（状态栏 + 导航栏内容高度）
    let headerHeight = 0
    if (menuButtonInfo) {
      // 头部高度 = 胶囊底部位置 + 底部间距
      headerHeight = menuButtonInfo.bottom + 10
    } else {
      headerHeight = statusBarHeight + 44
    }
    
    // 获取底部安全区域高度
    let safeBottomHeight = 0
    if (systemInfo.platform === 'ios') {
      // iOS 底部安全区域（iPhone X 及以上机型）
      // 通过屏幕高度减去窗口高度和状态栏高度计算
      const safeBottom = systemInfo.screenHeight - systemInfo.windowHeight - statusBarHeight
      safeBottomHeight = safeBottom > 0 ? safeBottom : 0
    } else {
      // Android 平台，检查是否有虚拟按键
      safeBottomHeight = 0
    }
    
    console.log('安全区域配置:', { 
      platform: systemInfo.platform,
      statusBarHeight, 
      headerHeight, 
      safeBottomHeight,
      screenHeight: systemInfo.screenHeight,
      windowHeight: systemInfo.windowHeight
    })
    
    this.setData({
      statusBarHeight: statusBarHeight,
      headerHeight: headerHeight,
      safeBottomHeight: safeBottomHeight
    })
  },

  // 加载食堂菜单
  loadMenuItems() {
    wx.request({
      url: `${BASE_URL}/foods/`,
      method: 'GET',
      success: (res) => {
        if (res.data && res.data.code === 100) {
          this.setData({ menuItems: res.data.foods })
          console.log('菜单加载成功:', this.data.menuItems.length, '道菜品')
        } else if (res.data && res.data.foods) {
          this.setData({ menuItems: res.data.foods })
          console.log('菜单加载成功:', this.data.menuItems.length, '道菜品')
        }
      },
      fail: (err) => {
        console.error('获取菜单失败:', err)
      }
    })
  },

  // 加载用户的健康档案
  loadUserHealthInfo() {
    const token = wx.getStorageSync('token')
    
    if (!token) {
      console.log('未登录，使用默认健康信息')
      return
    }

    wx.request({
      url: `${BASE_URL}/api/health-info/`,
      method: 'GET',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 100) {
          this.setData({ userHealthInfo: res.data.data })
          console.log('健康档案加载成功:', res.data.data)
        }
      },
      fail: (err) => {
        console.error('获取健康档案失败:', err)
      }
    })
  },

  // 发送消息
  sendMessage() {
    const content = this.data.inputText.trim()
    if (!content || this.data.isLoading) return

    // 添加用户消息
    const userMessage = { role: 'user', content: content }
    this.setData({
      messages: [...this.data.messages, userMessage],
      inputText: '',
      isLoading: true
    })
    this.scrollToBottom()
    this.saveChatHistory()

    // 判断使用哪个接口
    if (this.isMenuRecommendRequest(content)) {
      this.callAIWithMenu(content)   // 基于食堂菜单推荐
    } else {
      this.callAIWithHealth(content) // 通用食谱推荐
    }
  },

  // 判断用户是否想从食堂菜单中推荐
  isMenuRecommendRequest(content) {
    const keywords = ['推荐', '食堂', '菜单', '吃什么', '今天', '现有', '选择', '餐单']
    return keywords.some(keyword => content.includes(keyword))
  },

  // 基于食堂菜单推荐（新增方法）
  callAIWithMenu(userMessage) {
    const token = wx.getStorageSync('token')
    const healthInfo = this.data.userHealthInfo
    const menuItems = this.data.menuItems

    // 如果没有菜单，降级使用通用推荐
    if (menuItems.length === 0) {
      console.log('没有菜单数据，使用通用推荐')
      this.callAIWithHealth(userMessage)
      return
    }

    // 构建菜单数据
    const menuData = menuItems.map(item => ({
      name: item.name,
      calorie_per_100g: item.calorie_per_100g || 0,
      price: item.price || 0,
      weight: 200
    }))

    // 构建偏好文本（包含健康信息）
    let preference = userMessage
    if (healthInfo) {
      const healthText = this.formatHealthInfoForAI(healthInfo)
      if (healthText) {
        preference = `${userMessage}，${healthText}`
      }
    }

    wx.request({
      url: `${BASE_URL}/api/ai/recommend-from-menu/`,
      method: 'POST',
      header: {
        'Authorization': token || '',
        'Content-Type': 'application/json'
      },
      data: {
        menu_items: menuData,
        preference: preference
      },
      success: (res) => {
        this.setData({ isLoading: false })
        
        if (res.data.code === 100) {
          const assistantMessage = { 
            role: 'assistant', 
            content: res.data.data.reply 
          }
          this.setData({
            messages: [...this.data.messages, assistantMessage]
          })
          this.saveChatHistory()
        } else {
          // 失败时降级使用通用推荐
          console.log('菜单推荐失败，降级使用通用推荐')
          this.callAIWithHealth(userMessage)
        }
        this.scrollToBottom()
      },
      fail: (err) => {
        this.setData({ isLoading: false })
        console.error('菜单推荐请求失败:', err)
        // 降级使用通用推荐
        this.callAIWithHealth(userMessage)
      }
    })
  },

  // 通用 AI 推荐（带健康档案）
  callAIWithHealth(userMessage) {
    const token = wx.getStorageSync('token')
    const healthInfo = this.data.userHealthInfo

    wx.request({
      url: `${BASE_URL}/api/ai/recommend/`,  
      method: 'POST',
      header: {
        'Authorization': token || '',
        'Content-Type': 'application/json'
      },
      data: {
        message: userMessage,
        health_info: healthInfo
      },
      success: (res) => {
        this.setData({ isLoading: false })
        
        if (res.data.code === 100) {
          const assistantMessage = { 
            role: 'assistant', 
            content: res.data.data.reply 
          }
          this.setData({
            messages: [...this.data.messages, assistantMessage]
          })
          this.saveChatHistory()
        } else {
          this.showError(res.data.msg || '请求失败')
          // 添加错误提示消息
          const errorMessage = { 
            role: 'assistant', 
            content: '抱歉，AI服务暂时不可用，请稍后再试。' 
          }
          this.setData({
            messages: [...this.data.messages, errorMessage]
          })
          this.saveChatHistory()
        }
        this.scrollToBottom()
      },
      fail: (err) => {
        this.setData({ isLoading: false })
        console.error('请求失败:', err)
        this.showError('网络连接失败，请稍后重试')
        // 添加错误提示消息
        const errorMessage = { 
          role: 'assistant', 
          content: '网络连接失败，请检查网络后重试。' 
        }
        this.setData({
          messages: [...this.data.messages, errorMessage]
        })
        this.saveChatHistory()
        this.scrollToBottom()
      }
    })
  },

  // 格式化健康信息给 AI（简短版）
  formatHealthInfoForAI(healthInfo) {
    const parts = []
    
    if (healthInfo.goal) {
      const goalMap = { 'lose': '减脂', 'maintain': '保持体重', 'gain': '增肌' }
      parts.push(`饮食目标：${goalMap[healthInfo.goal] || healthInfo.goal}`)
    }
    
    if (healthInfo.allergies && healthInfo.allergies.length > 0) {
      const allergyNames = {
        'peanuts': '花生', 'seafood': '海鲜', 'milk': '乳制品',
        'gluten': '麸质', 'soy': '大豆', 'egg': '鸡蛋'
      }
      const allergyText = healthInfo.allergies.map(a => allergyNames[a] || a).join('、')
      parts.push(`忌口：${allergyText}`)
    }
    
    if (healthInfo.diseases && healthInfo.diseases.length > 0) {
      parts.push(`健康状况：${healthInfo.diseases.join('、')}`)
    }
    
    return parts.join('，')
  },

  // 从菜单推荐按钮（基于健康档案）
  recommendFromMenu() {
    const message = '根据我的身体状况，推荐一份适合我的健康餐单'
    this.setData({ inputText: message })
    this.sendMessage()
  },

  // 分析餐品组合按钮
  analyzeMeal() {
    const message = '帮我分析一下我常吃的餐品组合是否健康，有什么改进建议？'
    this.setData({ inputText: message })
    this.sendMessage()
  },

  onInput(e) {
    this.setData({ inputText: e.detail.value })
  },

  scrollToBottom() {
    setTimeout(() => {
      this.setData({ scrollToView: 'bottom' })
    }, 100)
  },

  recordMeal(e) {
    const cardUid = wx.getStorageSync('cardUid') || 'test_user'
    wx.request({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/api/meal/add/',
      method: 'POST',
      data: {
        user_id: cardUid,
        food_name: 'AI推荐菜品',
        weight: 200,
        calorie: 0,
        meal_type: 'lunch'
      },
      complete: () => {
        wx.showToast({ title: '已记录', icon: 'success' })
      }
    })
  },

  showError(msg) {
    wx.showToast({ title: msg, icon: 'none' })
  },

  goBack() {
    wx.navigateBack()
  }
})