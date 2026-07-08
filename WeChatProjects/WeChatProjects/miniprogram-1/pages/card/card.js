// pages/card/card.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    isBound: false,
    cardUid: '',
    balance: '0.00'
  },

  onShow() {
    this.checkBindStatus()
  },

  // ===== 1. 检查绑定状态 =====
  checkBindStatus() {
    const token = wx.getStorageSync('token')
    if (!token) {
      wx.showToast({ title: '请先登录', icon: 'none' })
      return
    }

    wx.request({
      url: `${BASE_URL}/card/binding/status`,
      method: 'GET',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 0 && res.data.data.is_bound) {
          this.setData({
            isBound: true,
            cardUid: res.data.data.card_uid,
            balance:
parseFloat(res.data.data.balance).toFixed(2)
          })
        } else {
          this.setData({ isBound: false })
        }
      }
    })
  },

  // ===== 2. 手动绑定卡 =====
  onBindManual() {
    wx.showModal({
      title: '绑定饭卡',
      content: '请输入您的饭卡卡号（印在卡片上）',
      editable: true,
      placeholderText: '输入卡号，如 5488CE06',
      success: (res) => {
        if (res.confirm && res.content) {
          const cardUid = res.content.trim().toUpperCase()
          if (cardUid.length < 6) {
            wx.showToast({ title: '卡号格式不正确', icon:
'none' })
            return
          }
          this.doBind(cardUid)
        }
      }
    })
  },

  doBind(cardUid) {
    const token = wx.getStorageSync('token')
    wx.request({
      url: `${BASE_URL}/card/bind/manual`,
      method: 'POST',
      header: { 'Authorization': token },
      data: { card_uid: cardUid },
      success: (res) => {
        if (res.data.code === 0) {
          wx.showToast({ title: '绑定成功', icon: 'success'
})
          this.checkBindStatus()
        } else {
          wx.showToast({ title: res.data.message ||
'绑定失败', icon: 'none' })
        }
      }
    })
  },

  // ===== 3. 充值 =====
  onRecharge() {
    wx.showModal({
      title: '饭卡充值',
      content: '请输入充值金额',
      editable: true,
      placeholderText: '输入金额',
      success: (res) => {
        if (res.confirm && res.content) {
          const amount = parseFloat(res.content)
          if (isNaN(amount) || amount <= 0) {
            wx.showToast({ title: '请输入有效金额', icon:
'none' })
            return
          }
          wx.showModal({
            title: '确认充值',
            content: `为卡号 ${this.data.cardUid} 充值
¥${amount.toFixed(2)}？`,
            success: (r) => {
              if (r.confirm) this.doRecharge(amount)
            }
          })
        }
      }
    })
  },

  doRecharge(amount) {
    wx.showLoading({ title: '充值中...' })
    wx.request({
      url: `${BASE_URL}/api/card/recharge/`,
      method: 'POST',
      data: {
        uid: this.data.cardUid,
        amount: String(amount),
        device_name: 'dish'
      },
      success: (res) => {
        wx.hideLoading()
        if (res.data.code === 0) {
          wx.showToast({ title: '充值指令已下发', icon:
'success' })
          // 3秒后刷新余额（等 OneNET 同步）
          setTimeout(() => {
            this.checkBindStatus()
          }, 3000)
        } else {
          wx.showToast({ title: res.data.msg || '充值失败',
icon: 'none' })
        }
      }
    })
  },

  // ===== 4. 从 Django数据库 同步最新余额 =====
  checkBindStatus() {
    const token = wx.getStorageSync('token')
    if (!token) {
      wx.showToast({ title: '请先登录', icon: 'none' })
      return
    }

    wx.request({
      url: `${BASE_URL}/card/binding/status`,
      method: 'GET',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 0 && res.data.data.is_bound) {
          const cardUid = res.data.data.card_uid
          // 直接从 Django 查卡余额，不依赖 OneNET
          wx.request({
            url: `${BASE_URL}/api/card/query/?uid=${cardUid}`,
            method: 'GET',
            success: (r) => {
              this.setData({
                isBound: true,
                cardUid: cardUid,
                balance: parseFloat(r.data.data.balance).toFixed(2)
              })
            }
          })
        } else {
          this.setData({ isBound: false })
        }
      }
    })
  },

  // ===== 5. 交易记录 =====
  onTransactions() {
    wx.showToast({ title: '功能开发中', icon: 'none' })
  },

  // ===== 6. 解绑 =====
  onUnbind() {
    wx.showModal({
      title: '确认解绑',
      content: `确定解绑卡号 ${this.data.cardUid} 吗？`,
      success: (res) => {
        if (res.confirm) {
          const token = wx.getStorageSync('token')
          wx.request({
            url: `${BASE_URL}/card/unbind`,
            method: 'POST',
            header: { 'Authorization': token },
            success: (res) => {
              if (res.data.code === 0) {
                wx.showToast({ title: '解绑成功', icon:
'success' })
                this.setData({ isBound: false, cardUid: '',
balance: '0.00' })
              } else {
                wx.showToast({ title: res.data.message ||
'解绑失败', icon: 'none' })
              }
            }
          })
        }
      }
    })
  }
})