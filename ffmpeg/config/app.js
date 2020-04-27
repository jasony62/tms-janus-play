module.exports = {
  port: 3000,
  https: { port: 3443, key: process.env.ssl_certificate_key, cert: process.env.ssl_certificate },
  name: 'tms-koa-ffmpeg-demo',
  router: {
    controllers: {
      prefix: '', // 接口调用url的前缀
      plugins_npm: [{ id: 'tms-koa-ffmpeg', alias: 'ffmpeg' }],
    },
  },
}
