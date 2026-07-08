// pages/register/register.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    phone: '',
    smsCode: '',
    password: '',
    confirmPassword: '',
    nickname: '',
    showPassword: false,
    showConfirmPwd: false,
    isLoading: false,
    agreed: false,
    countdown: 0
  },

  // 返回上一页
  goBack() {
    wx.navigateBack()
  },

  // 手机号输入
  onPhoneInput(e) {
    this.setData({ phone: e.detail.value })
  },

  // 密码输入
  onPasswordInput(e) {
    this.setData({ password: e.detail.value })
  },

  // 确认密码输入
  onConfirmPwdInput(e) {
    this.setData({ confirmPassword: e.detail.value })
  },

  // 昵称输入
  onNicknameInput(e) {
    this.setData({ nickname: e.detail.value })
  },

  // 切换密码显示
  togglePassword() {
    this.setData({ showPassword: !this.data.showPassword })
  },

  // 切换确认密码显示
  toggleConfirmPwd() {
    this.setData({ showConfirmPwd: !this.data.showConfirmPwd })
  },

  // 协议勾选
  onAgreementChange(e) {
    this.setData({ agreed: e.detail.value.length > 0 })
  },

  // 发送验证码
  sendSmsCode() {
    const { phone, countdown } = this.data
    
    if (countdown > 0) return
    
    if (!phone) {
      wx.showToast({ title: '请输入手机号', icon: 'none' })
      return
    }
    if (!/^1[3-9]\d{9}$/.test(phone)) {
      wx.showToast({ title: '手机号格式不正确', icon: 'none' })
      return
    }
    
    wx.showLoading({ title: '发送中...' })
    
    wx.request({
      url: `${BASE_URL}/api/send_sms/`,
      method: 'POST',
      data: { phone_number: phone },
      success: (res) => {
        wx.hideLoading()
        if (res.data.code === 100) {
          wx.showToast({ title: '验证码已发送', icon: 'success' })
          // 开始倒计时
          this.startCountdown()
        } else {
          wx.showToast({ title: res.data.msg || '发送失败', icon: 'none' })
        }
      },
      fail: () => {
        wx.hideLoading()
        // 模拟发送成功（开发测试用）
        wx.showToast({ title: '验证码：123456（演示）', icon: 'none', duration: 3000 })
        this.startCountdown()
      }
    })
  },

  // 倒计时
  startCountdown() {
    this.setData({ countdown: 60 })
    const timer = setInterval(() => {
      if (this.data.countdown <= 1) {
        clearInterval(timer)
        this.setData({ countdown: 0 })
      } else {
        this.setData({ countdown: this.data.countdown - 1 })
      }
    }, 1000)
  },

  // 注册
  doRegister() {
    const { phone, smsCode, password, confirmPassword, agreed, isLoading } = this.data
    
    if (isLoading) return
    
    // 验证手机号
    if (!phone) {
      wx.showToast({ title: '请输入手机号', icon: 'none' })
      return
    }
    if (!/^1[3-9]\d{9}$/.test(phone)) {
      wx.showToast({ title: '手机号格式不正确', icon: 'none' })
      return
    }
    
    // 验证验证码
    if (!smsCode) {
      wx.showToast({ title: '请输入验证码', icon: 'none' })
      return
    }
    
    // 验证密码
    if (!password) {
      wx.showToast({ title: '请输入密码', icon: 'none' })
      return
    }
    if (password.length < 6) {
      wx.showToast({ title: '密码至少6位', icon: 'none' })
      return
    }
    if (password !== confirmPassword) {
      wx.showToast({ title: '两次密码不一致', icon: 'none' })
      return
    }
    
    // 验证协议
    if (!agreed) {
      wx.showToast({ title: '请同意用户协议', icon: 'none' })
      return
    }
    
    this.setData({ isLoading: true })
    
    wx.request({
      url: `${BASE_URL}/api/register/`,
      method: 'POST',
      data: {
        phone: phone,
        code: smsCode,
        password: password,
        nickname: this.data.nickname
      },
      success: (res) => {
        this.setData({ isLoading: false })
        
        if (res.data.code === 100) {
          wx.showToast({ title: '注册成功', icon: 'success' })
          
          // 自动登录
          wx.setStorageSync('token', res.data.token)
          wx.setStorageSync('userInfo', res.data.userInfo)
          
          setTimeout(() => {
            // 跳转到健康信息填写页面
            wx.navigateTo({
              url: '/pages/health-info/health-info'
            })
          }, 1500)
        } else {
          wx.showToast({ title: res.data.msg || '注册失败', icon: 'none' })
        }
      },
      fail: () => {
        this.setData({ isLoading: false })
        // 演示模式：模拟注册成功
        wx.showToast({ title: '注册成功（演示）', icon: 'success' })
        wx.setStorageSync('token', 'demo_token')
        wx.setStorageSync('userInfo', { phone: phone, name: this.data.nickname || '新用户' })
        setTimeout(() => {
          wx.navigateTo({
            url: '/pages/health-info/health-info'
          })
        }, 1500)
      }
    })
  },

  // 跳转到登录页
  goToLogin() {
    wx.navigateBack()
  },

  // 用户协议
  showUserAgreement() {
    wx.showModal({
      title: '用户协议',
      content: '欢迎使用智慧食堂小程序...',
      showCancel: false
    })
  },

  // 隐私政策
  showPrivacyPolicy() {
    wx.showModal({
      title: '隐私政策',
      content: '我们重视您的隐私保护...',
      showCancel: false
    })
  }
})