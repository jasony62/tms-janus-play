<template>
  <div>
    <div>
      <button @click="connect" :disabled="!play.isConnEnable">打开通道</button>
      <button @click="closeConn" :disabled="play.isConnEnable">关闭通道</button>
    </div>
    <div>
      <button @click="probe" :disabled="!play.isProbeEnable">获取文件信息</button>
      <span>时长:{{play.playState.duration}}</span>
    </div>
    <div>
      <button @click="createWebrtc" :disabled="!play.isCreateWebrtcEnable">打开WebRTC</button>
      <button @click="hangupWebrtc" :disabled="play.isWebrtcUp!==true">关闭WebRTC</button>
    </div>
    <div class='tms-video-box'>
      <div class="tms-video-screen">
        <video width="320" height="240" autoplay playsinline />
      </div>
      <div>
        <button @click="control" :disabled="!play.isControlEnable">{{runActionLabel}}</button>
        <button @click="stop" :disabled="!play.isStopEnable">停止</button>
      </div>
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
    control() {
      if (this.play.playState.state === 'going') this.printScreen()
      this.play.control()
    },
    printScreen() {
      let elemVideo = document.querySelector('.tms-video-screen video')
      let canvas = document.createElement('canvas')
      canvas.width = elemVideo.videoWidth
      canvas.height = elemVideo.videoHeight
      canvas
        .getContext('2d')
        .drawImage(elemVideo, 0, 0, canvas.width, canvas.height)
      let elemImg = document.createElement('img')
      elemImg.src = canvas.toDataURL('image/png')
      document.querySelector('.tms-video-screen').appendChild(elemImg)
    },
  },
  mounted() {
    this.play = new TmsJanusPlay({
      elemMedia: document.querySelector('.tms-video-screen video'),
      onVideoUnmute: function () {
        let elemImg = document.querySelector('.tms-video-screen img')
        if (elemImg)
          document.querySelector('.tms-video-screen').removeChild(elemImg)
      },
    })
  },
}
</script>
<style>
.tms-video-screen {
  position: relative;
}
.tms-video-screen video {
  border: 1px solid #666;
}
.tms-video-screen img {
  position: absolute;
  top: 1px;
  bottom: 1px;
  left: 1px;
  right: 1px;
}
</style>