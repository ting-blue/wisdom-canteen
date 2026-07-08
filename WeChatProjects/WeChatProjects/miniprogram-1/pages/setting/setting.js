// pages/settings/settings.js

const app = getApp()

Page({
  data: {
    userInfo: {
      avatar: '/images/imge/b.jpg',
      name: 'wanteen',
      nickname: '',
      id: '10001'
    },
    cacheSize: '0 KB'
  },

  onLoad() {
    this.loadUserInfo()
    this.getCacheSize()
  },

  // 加载用户信息
  loadUserInfo() {
    const userInfo = wx.getStorageSync('userInfo')
    if (userInfo) {
      this.setData({
        userInfo: {
          ...this.data.userInfo,
          ...userInfo
        }
      })
    }
  },

  // 保存用户信息
  saveUserInfo(userInfo) {
    wx.setStorageSync('userInfo', userInfo)
    this.setData({ userInfo })
    // 更新全局数据
    if (app.globalData) {
      app.globalData.userInfo = userInfo
    }
  },

  // 获取缓存大小
  getCacheSize() {
    wx.getStorageInfo({
      success: (res) => {
        let size = res.currentSize
        if (size < 1024) {
          this.setData({ cacheSize: size + ' KB' })
        } else {
          this.setData({ cacheSize: (size / 1024).toFixed(2) + ' MB' })
        }
      }
    })
  },

  // 修改昵称
  editNickname() {
    const currentName = this.data.userInfo.nickname || this.data.userInfo.name
    wx.showModal({
      title: '修改昵称',
      content: '请输入新昵称',
      editable: true,
      placeholderText: currentName,
      success: (res) => {
        if (res.confirm && res.content) {
          const newNickname = res.content.trim()
          if (newNickname.length === 0) {
            wx.showToast({ title: '昵称不能为空', icon: 'none' })
            return
          }
          if (newNickname.length > 20) {
            wx.showToast({ title: '昵称不能超过20个字', icon: 'none' })
            return
          }
          
          // 更新本地数据
          const userInfo = { ...this.data.userInfo, nickname: newNickname, name: newNickname }
          this.saveUserInfo(userInfo)
          
          wx.showToast({ title: '修改成功', icon: 'success' })
          
          // 触发页面刷新（通知我的页面）
          const pages = getCurrentPages()
          const myPage = pages.find(page => page.route === 'pages/my/my')
          if (myPage) {
            myPage.setData({ 'userInfo.name': newNickname })
          }
        }
      }
    })
  },

  // 选择头像
  chooseAvatar() {
    wx.chooseMedia({
      count: 1,
      mediaType: ['image'],
      sourceType: ['album', 'camera'],
      success: (res) => {
        const tempFilePath = res.tempFiles[0].tempFilePath
        
        // 保存到本地
        const userInfo = { ...this.data.userInfo, avatar: tempFilePath }
        this.saveUserInfo(userInfo)
        
        wx.showToast({ title: '头像更新成功', icon: 'success' })
        
        // 可选：上传到服务器
        // this.uploadAvatar(tempFilePath)
      }
    })
  },

  // 上传头像到服务器
  uploadAvatar(filePath) {
    wx.uploadFile({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/api/upload/avatar/',
      filePath: filePath,
      name: 'avatar',
      success: (res) => {
        const data = JSON.parse(res.data)
        if (data.code === 100) {
          console.log('头像上传成功')
        }
      }
    })
  },

  // 切换账号
  switchAccount() {
    wx.showModal({
      title: '切换账号',
      content: '确定要切换账号吗？',
      success: (res) => {
        if (res.confirm) {
          // 清除当前用户信息
          wx.removeStorageSync('userInfo')
          wx.removeStorageSync('token')
          
          // 跳转到登录页
          wx.reLaunch({
            url: '/pages/login/login'
          })
        }
      }
    })
  },

  //修改密码
  changePassword() {
    wx.showModal({
      title: '修改密码',
      content: '请输入新密码',
      editable: true,
      placeholderText: '请输入新密码（6-20位）',
      success: (res) => {
        if (res.confirm && res.content) {
          const newPassword = res.content.trim()
          
          if (newPassword.length < 6) {
            wx.showToast({ title: '密码至少6位', icon: 'none' })
            return
          }
          
          // 再次确认
          wx.showModal({
            title: '确认密码',
            content: '请再次输入新密码',
            editable: true,
            placeholderText: '请再次输入新密码',
            success: (res2) => {
              if (res2.confirm && res2.content) {
                if (newPassword !== res2.content.trim()) {
                  wx.showToast({ title: '两次密码不一致', icon: 'none' })
                  return
                }
                
                // 保存新密码
                this.saveNewPassword(newPassword)
              }
            }
          })
        }
      }
    })
  },
  
  saveNewPassword(password) {
    // 本地模拟保存
    wx.setStorageSync('userPassword', password)
    wx.showToast({ title: '密码修改成功', icon: 'success' })
  },  

  // 清除缓存
  clearCache() {
    wx.showModal({
      title: '清除缓存',
      content: '确定要清除所有缓存吗？',
      success: (res) => {
        if (res.confirm) {
          wx.clearStorage({
            success: () => {
              wx.showToast({ title: '清除成功', icon: 'success' })
              this.setData({ cacheSize: '0 KB' })
              
              // 重新加载用户信息
              this.loadUserInfo()
            }
          })
        }
      }
    })
  },

  // 关于我们
  about() {
    wx.showModal({
      title: '智慧食堂SaaS',
      content: '版本：1.0.0\n\n智慧食堂SaaS，您的健康饮食助手！\n\n数据智能生态，丰富服务模式，增强管理效能。',
      showCancel: false
    })
  },

  // 隐私政策
  privacyPolicy() {
    wx.showModal({
      title: '隐私政策',
      content: '我们重视您的隐私保护。本应用仅收集必要的信息以提供更好的服务。',
      showCancel: false
    })
  },

  // 退出登录
  logout() {
    wx.showModal({
      title: '提示',
      content: '确定要退出登录吗？',
      success: (res) => {
        if (res.confirm) {
          // 清除用户信息
          wx.removeStorageSync('userInfo')
          wx.removeStorageSync('token')
          
          // 跳转到首页
          wx.reLaunch({
            url: '/pages/index/index'
          })
        }
      }
    })
  }
})