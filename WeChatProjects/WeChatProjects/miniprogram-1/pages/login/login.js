// pages/login/login.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    avatarUrl: '',                            // 默认头像
    nickName: '',                             // 用户填写的昵称
    agreed: false,                            // 协议勾选状态
    showPwdForm: false                        // 是否显示密码登录表单
  },

  // 用户选择了头像
  onChooseAvatar(e) {
    const { avatarUrl } = e.detail
    this.setData({ avatarUrl })
    console.log('用户选择了新头像:', avatarUrl)
  },

  // 用户输入昵称
  onNicknameInput(e) {
    this.setData({ nickName: e.detail.value })
  },

  // 协议勾选
  onAgreementChange(e) {
    this.setData({ agreed: e.detail.value.length > 0 })
  },

  // 用户点击"确认登录"
  handleAgreeAndLogin() {
    const { avatarUrl, nickName, agreed } = this.data

    // 1. 检查协议
    if (!agreed) {
      wx.showToast({ title: '请先同意用户协议', icon: 'none' })
      return
    }

    // 2. 检查昵称
    if (!nickName) {
      wx.showToast({ title: '请填写昵称', icon: 'none' })
      return
    }

    wx.showLoading({ title: '登录中...' })

    // 3. 获取微信 code
    wx.login({
      success: (loginRes) => {
        const code = loginRes.code

        // 4. 将头像、昵称、code 一起发送给后端
        wx.request({
          url: `${BASE_URL}/api/wx-login/`,
          method: 'POST',
          data: {
            code: code,
            userInfo: {
              nickName: nickName,
              avatarUrl: avatarUrl
            }
          },
          success: (res) => {
            wx.hideLoading()
            if (res.data.code === 100) {
              // 保存登录信息
              wx.setStorageSync('token', res.data.token)
              wx.setStorageSync('userInfo', res.data.userInfo)
              wx.showToast({ title: '登录成功', icon: 'success' })
              setTimeout(() => {
                if (res.data.userInfo.is_profile_completed) {
                  // 已经填过，直接进首页
                  wx.switchTab({ url: '/pages/index/index' })
                } else {
                  // 没填过，去健康信息页
                  wx.navigateTo({ url: '/pages/health/health' })
                }
              }, 1500)
            } else {
              wx.showToast({ title: res.data.msg || '登录失败', icon: 'none' })
            }
          },
          fail: () => {
            wx.hideLoading()
            wx.showToast({ title: '网络请求失败', icon: 'none' })
          }
        })
      },
      fail: () => {
        wx.hideLoading()
        wx.showToast({ title: '获取登录凭证失败', icon: 'none' })
      }
    })
  },

  // ===== 跳转到手机号登录/注册页面 =====
  goToPhoneLogin() {
   wx.navigateTo({
    url: '/pages/register/register'
   })
  },

  // 显示密码登录表单
  showPwdLogin() {
    // 直接跳转到手机号登录页
    this.goToPhoneLogin()
  },

  // 密码登录
  handlePwdLogin(e) {
    const { username, password } = e.detail.value
    
    if (!username || !password) {
      wx.showToast({ title: '请填写用户名和密码', icon: 'none' })
      return
    }
    
    wx.showLoading({ title: '登录中...' })
    
    wx.request({
      url: `${BASE_URL}/api/login/`,
      method: 'POST',
      data: {
        username: username,
        password: password
      },
      success: (res) => {
        wx.hideLoading()
        if (res.data.code === 100) {
          const token = res.data.data.token
          const userInfo = res.data.data.user
          
          // 保存 token
          wx.setStorageSync('token', token)
          
          // 保存用户信息
          wx.setStorageSync('userInfo', {
            avatar: userInfo.avatar || '/images/imge/b.jpg',
            nickname: userInfo.nickname || userInfo.username,
            name: userInfo.nickname || userInfo.username
          })
          
          wx.showToast({ title: '登录成功', icon: 'success' })
          setTimeout(() => {
            wx.switchTab({ url: '/pages/index/index' })
          }, 1500)
        } else {
          wx.showToast({ title: res.data.msg || '登录失败', icon: 'none' })
        }
      },
      fail: () => {
        wx.hideLoading()
        wx.showToast({ title: '网络请求失败', icon: 'none' })
      }
    })
  },

  // 显示用户协议
  showUserAgreement() {
    wx.showModal({ 
      title: '用户协议', 
      content: '这是用户协议内容...', 
      showCancel: false 
    })
  },

  // 显示隐私政策
  showPrivacyPolicy() {
    wx.showModal({ 
      title: '隐私政策', 
      content: '这是隐私政策内容...', 
      showCancel: false 
    })
  }
})