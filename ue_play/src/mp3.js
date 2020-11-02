import Vue from 'vue'
import Demo from './Mp3.vue'

Vue.config.productionTip = false

new Vue({
  render: (h) => h(Demo),
}).$mount('#app')
