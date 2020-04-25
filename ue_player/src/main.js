import Vue from 'vue'
import App from './App.vue'
import { TmsAxiosPlugin } from 'tms-vue'

Vue.config.productionTip = false

Vue.use(TmsAxiosPlugin)

Vue.TmsAxios({ name: 'ffmpeg-api', rules: [] })

new Vue({
  render: (h) => h(App),
}).$mount('#app')
