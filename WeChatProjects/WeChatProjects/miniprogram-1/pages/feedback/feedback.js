// pages/feedback/feedback.js

const BASE_URL = 'https://wisplike-scoop-elk.ngrok-free.dev'

Page({
  data: {
    feedbackTypes: ['功能建议', '体验问题', '菜品建议', '菜品问题', '其他'],
    selectedType: '',
    content: '',
    imageList: [],
    contact: ''
  },

  onTypeChange() {
    wx.showActionSheet({
      itemList: this.data.feedbackTypes,
      success: (res) => {
        this.setData({
          selectedType: this.data.feedbackTypes[res.tapIndex]
        })
      }
    })
  },

  onContentInput(e) {
    this.setData({ content: e.detail.value })
  },

  onContactInput(e) {
    this.setData({ contact: e.detail.value })
  },

  chooseImage() {
    wx.chooseImage({
      count: 3 - this.data.imageList.length,
      sizeType: ['compressed'],
      sourceType: ['album', 'camera'],
      success: (res) => {
        this.setData({
          imageList: [...this.data.imageList, ...res.tempFilePaths]
        })
      }
    })
  },

  deleteImage(e) {
    const index = e.currentTarget.dataset.index
    const newList = [...this.data.imageList]
    newList.splice(index, 1)
    this.setData({ imageList: newList })
  },

  previewImage(e) {
    const url = e.currentTarget.dataset.url
    wx.previewImage({
      current: url,
      urls: this.data.imageList
    })
  },

  submitFeedback() {
    if (!this.data.selectedType) {
      wx.showToast({ title: '请选择反馈类型', icon: 'none' })
      return
    }
    if (!this.data.content.trim()) {
      wx.showToast({ title: '请输入反馈内容', icon: 'none' })
      return
    }

    wx.showLoading({ title: '提交中...' })

    // TODO: 提交到后端
    wx.request({
      url: `${BASE_URL}/api/feedback/add/`,
      method: 'POST',
      data: {
        type: this.data.selectedType,
        content: this.data.content,
        images: this.data.imageList,
        contact: this.data.contact
      },
      success: (res) => {
        wx.hideLoading()
        if (res.data.code === 100) {
          wx.showToast({ title: '提交成功', icon: 'success' })
          setTimeout(() => {
            wx.navigateBack()
          }, 1500)
        } else {
          wx.showToast({ title: res.data.msg || '提交失败', icon: 'none' })
        }
      },
      fail: () => {
        wx.hideLoading()
        wx.showToast({ title: '网络请求失败', icon: 'none' })
      }
    })
  }
})