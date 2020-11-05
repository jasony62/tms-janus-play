<template>
  <div>
    <div>
      <button @click="openWebrtc" :disabled="play.isWebrtcUp">打开通道</button>
      <button @click="closeWebrtc" :disabled="!play.isWebrtcUp">关闭通道</button>
    </div>
    <div>
      <button @click="probeFile" :disabled="!play.channelState.webrtcUp">获取文件信息</button>
      <div>时长:{{play.playState.duration}}</div>
    </div>
    <div>
      <button @click="playFile" :disabled="!play.isPlayEnabled">播放</button>
      <button @click="pauseFile" :disabled="!play.isPauseEnabled">暂停</button>
      <button @click="resumeFile" :disabled="!play.isResumeEnabled">恢复</button>
      <button @click="stopFile" :disabled="!play.isStopEnabled">停止</button>
    </div>
    <div>
      <video id="remotevideo" width="320" height="240" autoplay playsinline @playing="playing" />
    </div>
    <div>
      <img id="videoimage" width="320" height="240">
    </div>
  </div>
</template>

<script>
import { TmsJanusPlay } from '../tms_janus_play'
import { PlayVueMixin } from '../tms_janus_play_vue'

const PLUGIN_NAME = 'janus.plugin.tms.mp4'

export default {
  name: 'TmsJanusMp4',
  mixins: [PlayVueMixin],
  methods: {
    pauseFile() {
      this.printScreen()
      this.play.pause()
    },
    printScreen() {
      let elemVideo = document.querySelector('#remotevideo')
      let canvas = document.createElement('canvas')
      canvas.width = elemVideo.videoWidth
      canvas.height = elemVideo.videoHeight
      canvas
        .getContext('2d')
        .drawImage(elemVideo, 0, 0, canvas.width, canvas.height)
      let elemImg = document.querySelector('#videoimage')
      elemImg.src = canvas.toDataURL('image/png')
    },
  },
  mounted() {
    this.play = new TmsJanusPlay({
      plugin: PLUGIN_NAME,
      elemMedia: document.querySelector('#remotevideo'),
    })
  },
}
</script>