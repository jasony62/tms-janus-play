<template>
  <div id="player">
    <div v-if="source==='test'">
      <playtest :duration="duration" />
    </div>
    <div v-else-if="source==='video'">
      <playvideo :file="file" />
    </div>
    <div v-else>
      <div>无法识别的类型</div>
    </div>
  </div>
</template>

<script>
import queryString from 'query-string'
import Playvideo from './components/Playvideo.vue'
import Playtest from './components/Playtest.vue'

function setPlayerParameters(vm, params) {
  switch (params.source) {
    case 'test':
      vm.duration = parseInt(params.duration) || 10
      break
    case 'video':
      vm.file = params.file
      break
  }
  vm.source = params.source
}

export default {
  name: 'Player',
  components: {
    Playtest,
    Playvideo
  },
  data() {
    return {
      source: 'test',
      duration: 10,
      file: ''
    }
  },
  created() {
    const query = queryString.parse(location.search)
    if (query.source) {
      setPlayerParameters(this, query)
    }
  }
}
</script>
