import Vue from 'vue'
import Demo from './Dev.vue'

Vue.config.productionTip = false

new Vue({
  render: (h) => h(Demo),
}).$mount('#app')
