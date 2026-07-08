Page({
  data: {
    amount: '',
    cardUid: '',
    balance: '0.00',
    dishes: ''
  },

  onLoad(options) {
    const amount = this.parseAmount(options || {})
    this.setData({ amount: amount })
    this.loadCardInfo()
  },

  parseAmount(options) {
    if (options.q) {
      const decoded = decodeURIComponent(options.q)
      if (decoded === 'smart://pay') {
        this.fetchPriceFromServer()
        return '加载中...'
      }
    }
    if (options.amount) return
parseFloat(options.amount).toFixed(2)
    return ''
  },

  fetchPriceFromServer() {
    wx.request({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/api/price/current/',
      success: (res) => {
        if (res.data.code === 0) {
          this.setData({
            amount: res.data.data.amount,
            dishes: res.data.data.dishes
          })
        }
      }
    })
  },

  loadCardInfo() {
    const token = wx.getStorageSync('token')
    wx.request({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/card/binding/status',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 0 && res.data.data.is_bound) {
          this.setData({
            cardUid: res.data.data.card_uid,
            balance:
parseFloat(res.data.data.balance).toFixed(2)
          })
        }
      }
    })
  },

  onAmountInput(e) {
    this.setData({ amount: e.detail.value })
  },

  confirmPay() {
    const amount = parseFloat(this.data.amount)
    if (!amount || amount <= 0) {
      wx.showToast({ title: '请输入金额', icon: 'none' })
      return
    }
    if (!this.data.cardUid) {
      wx.showToast({ title: '请先绑定饭卡', icon: 'none' })
      return
    }
    if (amount > parseFloat(this.data.balance)) {
      wx.showToast({ title: '余额不足', icon: 'none' })
      return
    }
    wx.showModal({
      title: '确认付款',
      content: '支付 ¥' + amount.toFixed(2) + '\n当前余额：¥'
 + this.data.balance,
      success: (r) => {
        if (r.confirm) {
          wx.request({
            url: 'https://wisplike-scoop-elk.ngrok-free.dev/api/card/deduct/',
            method: 'POST',
            data: { uid: this.data.cardUid, amount:
String(amount), device_name: 'dish' },
            complete: () => {
              wx.showToast({ title: '支付成功', icon:
'success' })
              this.setData({
                balance: (parseFloat(this.data.balance) -
amount).toFixed(2)
              })
            }
            
          })
        }
      }
    })
  }
})