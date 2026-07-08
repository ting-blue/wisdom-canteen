// pages/welcome/welcome.js
import api from '../../config/setting'

Page({
  data: {
    second: 3,
    img: '/images/bg/splash.jpg'
  },

  onLoad(options) {
    // 获取欢迎图片
    wx.request({
      url: 'https://wisplike-scoop-elk.ngrok-free.dev/smart/get_welcome_image/',
      method: 'GET',
      success: (res) => {
        console.log('后端返回数据：', res.data)
        
        if (res.data.code === 100) {
          const fullImageUrl = 'https://wisplike-scoop-elk.ngrok-free.dev' + res.data.result.img
          console.log('完整图片地址：', fullImageUrl)
          this.setData({
            img: fullImageUrl
          })
        } else {
          wx.showToast({
            title: res.data.msg || '网络请求异常',
            icon: 'none'
          })
        }
      },
      fail: (err) => {
        console.error('请求失败：', err)
      }
    })

    // 倒计时跳转
    const timer = setInterval(() => {
      if (this.data.second <= 0) {
        clearInterval(timer);
        wx.redirectTo({  
          url: '/pages/login/login'
        });
      } else {
        this.setData({
          second: this.data.second - 1
        });
      }
    }, 1000);
  },

  // 手动点击跳过
  doJump() {
    wx.redirectTo({
      url: '/pages/login/login'
    });
  }
})