<template>
  <div>
    <div>
      <button @click="connect" :disabled="!play.isConnEnable">打开通道</button>
      <button @click="closeConn" :disabled="play.isConnEnable">关闭通道</button>
    </div>
    <div>
      <button @click="probeFile" :disabled="!play.isProbeEnable">获取文件信息</button>
      <div>时长:{{play.playState.duration}}</div>
    </div>
    <div>
      <button @click="createWebrtc" :disabled="!play.isCreateWebrtcEnable">打开WebRTC</button>
      <button @click="hangupWebrtc" :disabled="play.isWebrtcUp!==true">关闭WebRTC</button>
    </div>
    <div>
      <button @click="playFile" :disabled="!play.isPlayEnable">播放</button>
      <button @click="pauseFile" :disabled="!play.isPauseEnable">暂停</button>
      <button @click="resumeFile" :disabled="!play.isResumeEnable">恢复</button>
      <button @click="stopFile" :disabled="!play.isStopEnable">停止</button>
    </div>
    <div class="video-box">
      <video width="320" height="240" autoplay playsinline />
    </div>
  </div>
</template>

<script>
import { TmsJanusPlay } from '../tms_janus_play'
import { TmsJanusPlayVueMixin } from '../tms_janus_play_vue'

export default {
  name: 'TmsJanusMp4',
  mixins: [TmsJanusPlayVueMixin],
  methods: {
    pauseFile() {
      this.printScreen()
      this.play.pause()
    },
    printScreen() {
      let elemVideo = document.querySelector('.video-box video')
      let canvas = document.createElement('canvas')
      canvas.width = elemVideo.videoWidth
      canvas.height = elemVideo.videoHeight
      canvas
        .getContext('2d')
        .drawImage(elemVideo, 0, 0, canvas.width, canvas.height)
      let elemImg = document.createElement('img')
      elemImg.src = canvas.toDataURL('image/png')
      document.querySelector('.video-box').appendChild(elemImg)
    },
  },
  mounted() {
    this.play = new TmsJanusPlay({
      elemMedia: document.querySelector('.video-box video'),
      onunmute: function () {
        let elemImg = document.querySelector('.video-box img')
        if (elemImg) document.querySelector('.video-box').removeChild(elemImg)
      },
    })
  },
}
</script>
<style>
.video-box {
  position: relative;
}
.video-box video {
  border: 1px solid #666;
}
.video-box img {
  position: absolute;
  top: 1px;
  bottom: 1px;
  left: 1px;
  right: 1px;
}
</style>