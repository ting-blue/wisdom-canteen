const _request = wx.request
Object.defineProperty(wx, 'request', {
  configurable: true,
  enumerable: true,
  writable: true,
  value: function(options) {
    options.header = options.header || {}
    options.header['ngrok-skip-browser-warning'] = 'true'
    return _request.call(wx, options)
  }
})

App({})
