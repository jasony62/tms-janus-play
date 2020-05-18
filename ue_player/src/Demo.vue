<template>
  <div id="demo">
    <div>
      <div>
        <label>播放测试流时长（秒）：<input type="number" v-model="duration"></label>
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
        <label>要播放的视频文件：<input type="text" v-model="videoFile"></label>
      </div>
      <div>
        <label>开始播放位置：<input type="text" v-model="videoSeek" placeholder="秒数或时间点"></label>
      </div>
      <div>
        <div>{{videoSrc}}</div>
      </div>
      <div>
        <button @click="renderVideo" :disabled="!videoFile">生成播放器</button>
      </div>
      <div>
        <iframe id="playVideo"></iframe>
      </div>
    </div>
    <div>
      <div>
        <label>要播放的音频文件：<input type="text" v-model="audioFile"></label>
      </div>
      <div>
        <label>开始播放位置：<input type="text" v-model="audioSeek" placeholder="秒数或时间点"></label>
      </div>
      <div>
        <div>{{audioSrc}}</div>
      </div>
      <div>
        <button @click="renderAudio" :disabled="!audioFile">生成播放器</button>
      </div>
      <div>
        <iframe id="playAudio"></iframe>
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
      videoFile: '',
      videoSeek: '',
      audioFile: '',
      audioSeek: ''
    }
  },
  computed: {
    testSrc() {
      return `index.html?source=test&duration=${this.duration}`
    },
    videoSrc() {
      return `index.html?source=video&file=${this.videoFile}&seek=${this.videoSeek}`
    },
    audioSrc() {
      return `index.html?source=audio&file=${this.audioFile}&seek=${this.audioSeek}`
    }
  },
  methods: {
    renderTest() {
      document.querySelector('#playTest').src = this.testSrc
    },
    renderVideo() {
      document.querySelector('#playVideo').src = this.videoSrc
    },
    renderAudio() {
      document.querySelector('#playAudio').src = this.audioSrc
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
#playAudio {
  width: 640px;
  height: 160px;
}
</style>