import Vue from 'vue'
import App from './App.vue'
import { TmsAxiosPlugin } from 'tms-vue'

Vue.config.productionTip = false

// ffmpeg服务地址
window.FFMPEG_ADDRESS = `${process.env.FFMPEG_PROTOCOL}://${process.env.FFMPEG_HOSTNAME || window.location.hostname}`
window.FFMPEG_API_ADDRESS = `${window.FFMPEG_ADDRESS}:${process.env.FFMPEG_API_PORT}/${process.env.FFMPEG_API_PATH}`
window.FFMPEG_PUSH_ADDRESS = `${window.FFMPEG_ADDRESS}:${process.env.FFMPEG_PUSH_PORT}`

Vue.use(TmsAxiosPlugin)

Vue.TmsAxios({ name: 'ffmpeg-api', rules: [] })

new Vue({
  render: (h) => h(App),
}).$mount('#app')
