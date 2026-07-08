// pages/balance/balance.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    balance: '0.00',
    totalIncome: '0.00',
    totalExpense: '0.00',
    billList: [],
    groupedBills: [],
    selectedTab: 'all',
    isLoading: false,
    dateRange: ['本月', '近3个月', '近6个月', '全部'],
    selectedMonth: 0,
    monthValues: [1, 3, 6, 999],
    cardUid: ''      // 绑定的卡号
  },

  onLoad() {
    this.token = wx.getStorageSync('token')
    this.loadCardInfo()
  },

  onShow() {
    this.loadCardInfo()
  },

  // ===== 先获取绑定的卡号，再查余额 =====
  loadCardInfo() {
    const token = wx.getStorageSync('token')
    if (!token) {
      this.setData({ balance: '0.00' })
      return
    }

    wx.request({
      url: `${BASE_URL}/card/binding/status`,
      method: 'GET',
      header: { 'Authorization': token },
      success: (res) => {
        if (res.data.code === 0 && res.data.data.is_bound) {
          const cardUid = res.data.data.card_uid
          this.setData({ cardUid: cardUid })
          this.getBalance(cardUid)
          this.getBillList(cardUid)
        } else {
          wx.showToast({ title: '请先绑定饭卡', icon: 'none' })
          this.setData({ balance: '0.00', billList: [], groupedBills: [] })
        }
      }
    })
  },

  // ===== 查卡余额（从 RfidCard 表） =====
  getBalance(cardUid) {
    wx.request({
      url: `${BASE_URL}/api/card/query/?uid=${cardUid}`,
      method: 'GET',
      success: (res) => {
        if (res.data.code === 0 && res.data.data) {
          this.setData({
            balance: parseFloat(res.data.data.balance).toFixed(2)
          })
        }
      },
      fail: () => this.setData({ balance: '0.00' })
    })
  },

  // ===== 查账单（最近刷卡记录） =====
  getBillList(cardUid) {
    this.setData({ isLoading: true })
    wx.request({
      url: `${BASE_URL}/api/card/latest/`,
      method: 'GET',
      success: (res) => {
        this.setData({ isLoading: false })
        if (res.data.code === 0) {
          const bills = (res.data.data || []).map((r, index) => ({
            id: r.card_uid + '_' + r.create_time + '_' + index,
            type: r.user_name === '充值' ? 'income' : 'expense',
            type_text: r.user_name === '充值' ? '充值' : (r.user_name === '扫码支付' ? '扫码消费' : '消费'),
            amount: Number(r.total_price).toFixed(2),
            balance_after: Number(r.balance).toFixed(2),
            description: `${r.user_name}(${r.card_uid})`,
            create_time: r.create_time
          }))

          const grouped = this.groupBillsByDate(bills)
          const stats = this.calculateStats(bills)

          this.setData({
            billList: bills,
            groupedBills: grouped,
            totalIncome: stats.totalIncome.toFixed(2),
            totalExpense: stats.totalExpense.toFixed(2)
          })
        }
      },
      fail: () => {
        this.setData({ isLoading: false })
        wx.showToast({ title: '加载失败', icon: 'none' })
      }
    })
  },

  calculateStats(bills) {
    let totalIncome = 0, totalExpense = 0
    bills.forEach(bill => {
      const amt = parseFloat(bill.amount) || 0
      if (bill.type === 'income') totalIncome += amt
      else totalExpense += amt
    })
    return { totalIncome, totalExpense }
  },

  groupBillsByDate(bills) {
    const grouped = {}
    bills.forEach(bill => {
      const date = bill.create_time ? bill.create_time.split(' ')[0] : '未知日期'
      if (!grouped[date]) grouped[date] = []
      grouped[date].push(bill)
    })
    return Object.keys(grouped)
      .sort((a, b) => b.localeCompare(a))
      .map(date => ({
        date,
        bills: grouped[date],
        dayIncome: grouped[date].filter(b => b.type === 'income')
          .reduce((sum, b) => sum + (parseFloat(b.amount) || 0), 0),
        dayExpense: grouped[date].filter(b => b.type === 'expense')
          .reduce((sum, b) => sum + (parseFloat(b.amount) || 0), 0)
      }))
  },

  onTabChange(e) {
    const tab = e.currentTarget.dataset.tab
    this.setData({ selectedTab: tab })
    const bills = this.data.billList
    let filtered = bills
    if (tab === 'income') filtered = bills.filter(b => b.type === 'income')
    else if (tab === 'expense') filtered = bills.filter(b => b.type === 'expense')
    this.setData({
      groupedBills: this.groupBillsByDate(filtered),
      totalIncome: this.calculateStats(filtered).totalIncome.toFixed(2),
      totalExpense: this.calculateStats(filtered).totalExpense.toFixed(2)
    })
  },

  onDateChange(e) {
    this.setData({ selectedMonth: parseInt(e.detail.value) })
    this.loadCardInfo()
  },

  // ===== 充值：给卡充值 =====
  recharge() {
    if (!this.data.cardUid) {
      wx.showToast({ title: '请先绑定饭卡', icon: 'none' })
      return
    }
    wx.showModal({
      title: '饭卡充值',
      content: '请输入充值金额',
      editable: true,
      placeholderText: '输入金额',
      success: (res) => {
        if (res.confirm && res.content) {
          const amount = parseFloat(res.content)
          if (isNaN(amount) || amount <= 0) {
            wx.showToast({ title: '请输入有效金额', icon: 'none' })
            return
          }
          wx.showModal({
            title: '确认充值',
            content: `为卡号 ${this.data.cardUid} 充值 ¥${amount.toFixed(2)}？`,
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
          wx.showToast({ title: '充值指令已下发', icon: 'success' })
          setTimeout(() => this.loadCardInfo(), 3000)
        } else {
          wx.showToast({ title: res.data.msg || '充值失败', icon: 'none' })
        }
      },
      fail: () => {
        wx.hideLoading()
        wx.showToast({ title: '网络请求失败', icon: 'none' })
      }
    })
  },

  withdraw() {
    wx.showToast({ title: '请在小程序端解绑后重新绑定', icon: 'none' })
  },

  onPullDownRefresh() {
    this.loadCardInfo()
    wx.stopPullDownRefresh()
  }
})