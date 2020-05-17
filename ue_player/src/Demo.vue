<template>
  <div id="demo">
    <div>
      <div>
        <label>播放时长（秒）：<input type="number" v-model="duration"></label>
      </div>
      <div>
        <div>{{testSrc}}</div>
      </div>
      <div>
        <button @click="renderTest" :disabled="duration<=0">生成播放器</button>
      </div>
      <div>
        <iframe id="playTest"></iframe>
      </div>
    </div>
    <div>
      <div>
        <label>要播放的文件：<input type="text" v-model="file"></label>
      </div>
      <div>
        <div>{{videoSrc}}</div>
      </div>
      <div>
        <button @click="renderVideo" :disabled="!file">生成播放器</button>
      </div>
      <div>
        <iframe id="playVideo"></iframe>
      </div>
    </div>
  </div>
</template>

<script>
export default {
  name: 'Demo',
  data() {
    return {
      duration: 10,
      file: ''
    }
  },
  computed: {
    testSrc() {
      return `index.html?source=test&duration=${this.duration}`
    },
    videoSrc() {
      return `index.html?source=video&file=${this.file}`
    }
  },
  methods: {
    renderTest() {
      document.querySelector('#playTest').src = this.testSrc
    },
    renderVideo() {
      document.querySelector('#playVideo').src = this.videoSrc
    }
  }
}
</script>
<style scoped>
#playTest,
#playVideo {
  width: 640px;
  height: 320px;
}
</style>