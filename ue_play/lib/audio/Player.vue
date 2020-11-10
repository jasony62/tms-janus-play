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
    <div class="tms-audio-box">
      <div class="tms-audio-screen">
        <audio autoplay playsinline />
      </div>
      <div class="tms-audio-bar">
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
  name: 'TmsJanusAudio',
  mixins: [TmsJanusPlayVueMixin],
  mounted() {
    this.play = new TmsJanusPlay({
      elemMedia: document.querySelector('.tms-audio-screen audio'),
    })
  },
}
</script>