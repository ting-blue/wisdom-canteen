// pages/index/index.js

Page({
  data: {
    banner_list: [{ img: '/images/banners/banner1.png' }],
    notice: '智慧选餐，健康每一天 均衡营养，从这一餐开始 建议每餐搭配：半盘蔬菜水果、四分之一盘优质蛋白（鱼、蛋、豆制品）、四分之一盘全谷物主食。少油少盐少糖，多蒸煮炖拌，少煎炸烤腌。每天饮水1500-2000ml，减少含糖饮料。细嚼慢咽，七分饱即可。养成良好饮食习惯，让身体更轻盈，精力更充沛',
    userInfo: null  // 添加 userInfo 初始值
  },

  onLoad() {
    // 获取本地缓存的用户信息
    const userInfo = wx.getStorageSync('userInfo')
    if (userInfo) {
      this.setData({ userInfo: userInfo })
    }
    
    // 请求轮播图数据（修正：放到 if 外面，确保一定会执行）
    this.getBanners()
  },

  // 获取轮播图数据
  getBanners() {
    wx.request({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/banners/',
      method: 'GET',
      header: { 'ngrok-skip-browser-warning': 'true' },
      success: (res) => {
        console.log('后端返回数据：', res.data)
        
        if (res.data.code === 100) {
          this.setData({
            banner_list: res.data.banners,
            notice: res.data.notice?.content || res.data.notice
          })
        } else {
          console.error('接口返回错误：', res.data.msg)
        }
      },
      fail: (err) => {
        console.error('请求失败：', err)
      }
    })
  },

  onShow() {
    // 每次显示页面时刷新用户信息（登录后更新头像昵称）
    const userInfo = wx.getStorageSync('userInfo')
    if (userInfo && JSON.stringify(userInfo) !== JSON.stringify(this.data.userInfo)) {
      this.setData({ userInfo: userInfo })
    }
  },

  gotohealth() {
    wx.navigateTo({
      url: '/pages/health/health',
    })
  },

  gotoMyMeal() {
    wx.navigateTo({
      url: '/pages/my-meal/my-meal'
    })
  },

  gotodocument() {
    wx.navigateTo({
      url: '/pages/document/document'
    })
  },
  
  gotoyunshipu() {
    wx.navigateTo({
      url: '/pages/yunshipu/yunshipu',
    })
  },
  
  gotofeedback() {
    wx.navigateTo({
      url: '/pages/feedback/feedback',
    })
  }
})