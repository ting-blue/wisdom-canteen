// pages/my/my.js

Page({
  data: {
    userInfo: {
      avatar: '/images/imge/b.jpg',
      name: 'wanteen',
      score: 100
    },
    showShare: false,
    options: [
      { name: '微信', icon: 'wechat', openType: 'share' },
      { name: '朋友圈', icon: '/images/imge/朋友圈.png', openType: 'shareTimeline' },
      { name: '复制链接', icon: 'link' },
      { name: '分享海报', icon: 'poster' }
    ]
  },

  onLoad() {
    this.loadUserInfo()
  },

  onShow() {
    this.loadUserInfo()
  },

  // 从后端加载用户信息
  loadUserInfo() {
    const token = wx.getStorageSync('token')
    
    if (!token) {
      console.log('未登录，使用本地缓存')
      this.loadFromLocal()
      return
    }

    wx.request({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/api/user/profile/',
      method: 'GET',
      header: {
        'Authorization': token  // 直接传 token，不需要 Bearer 前缀
      },
      success: (res) => {
        console.log('获取用户信息响应:', res.data)
        
        if (res.data.code === 100) {
          const userData = res.data.data
          this.setData({
            userInfo: {
              avatar: userData.avatar || this.data.userInfo.avatar,
              name: userData.nickname || this.data.userInfo.name,
              score: userData.score || this.data.userInfo.score
            }
          })
          // 更新本地缓存
          wx.setStorageSync('userInfo', this.data.userInfo)
        } else {
          console.error('获取失败:', res.data.msg)
          this.loadFromLocal()  // 失败时用本地缓存
        }
      },
      fail: (err) => {
        console.error('请求失败:', err)
        this.loadFromLocal()  // 网络错误时用本地缓存
      }
    })
  },

  // 从本地缓存加载
  loadFromLocal() {
    const cached = wx.getStorageSync('userInfo')
    if (cached) {
      this.setData({
        userInfo: {
          avatar: cached.avatar || this.data.userInfo.avatar,
          name: cached.nickname || cached.name || this.data.userInfo.name,
          score: cached.score || this.data.userInfo.score
        }
      })
    }
  },

  gotoReport() {
    wx.navigateTo({ url: '/pages/health/health?mode=report' })
  },

  scanPay() {
    wx.scanCode({
      onlyFromCamera: true,
      success: (res) => {
        wx.navigateTo({ url: '/pages/pay/pay?' + res.result})
      }
    })
  },

  // 其他方法保持不变...
  gotoCard() {
    wx.navigateTo({ url: '/pages/card/card' })
  },
  
  gotoBalance() {
    wx.navigateTo({ url: '/pages/balance/balance' })
  },

  gotoSettings() {
    wx.navigateTo({ url: '/pages/setting/setting' })
  },

  gotoMyMeal() {
    wx.navigateTo({ url: '/pages/my-meal/my-meal' })
  },

  onClick() {
    this.setData({ showShare: true })
  },

  onClose() {
    this.setData({ showShare: false })
  },

  onSelect(event) {
    const { name, openType } = event.detail
    this.setData({ showShare: false })
    
    if (openType === 'share') {
      // 触发微信分享给好友
    } else if (openType === 'shareTimeline') {
      // 触发分享到朋友圈
    } else if (name === '复制链接') {
      this.copyLink()
    } else if (name === '分享海报') {
      this.generatePoster()
    }
  },

  copyLink() {
    wx.setClipboardData({
      data: 'https://your-domain.com/pages/index/index',
      success: () => {
        wx.showToast({ title: '链接已复制', icon: 'success' })
      }
    })
  },

  generatePoster() {
    wx.showToast({ title: '功能开发中', icon: 'none' })
  },

  onShareAppMessage() {
    return {
      title: '智慧食堂SaaS，健康饮食新选择',
      path: '/pages/index/index',
      imageUrl: '/images/share.jpg'
    }
  },

  onShareTimeline() {
    return {
      title: '智慧食堂SaaS，健康饮食新选择',
      query: 'from=pyq',
      imageUrl: '/images/share-timeline.jpg'
    }
  },

  contactService() {
    wx.showModal({
      title: '联系客服',
      content: '客服微信号：health_canteen',
      confirmText: '复制微信号',
      cancelText: '取消',
      success: (res) => {
        if (res.confirm) {
          wx.setClipboardData({
            data: 'health_canteen',
            success: () => {
              wx.showToast({ title: '微信号已复制', icon: 'success' })
            }
          })
        }
      }
    })
  }
})