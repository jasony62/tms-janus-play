<template>
  <div>
    <div>
      <button @click="openWebrtc" :disabled="play.state.webrtcUp">打开通道</button>
      <button @click="closeWebrtc" :disabled="!play.state.webrtcUp">关闭通道</button>
    </div>
    <div>
      <button @click="playFile">开始播放</button>
      <button @click="pauseFile">暂停播放</button>
      <button @click="resumeFile">恢复播放</button>
      <button @click="stopFile">停止播放</button>
    </div>
    <div>
      <audio id="remoteaudio" autoplay playsinline @playing="playing" />
    </div>
  </div>
</template>

<script>
import { TmsJanusPlay } from '../tms_janus_play'
import { PlayVueMixin } from '../tms_janus_play_vue'

const PLUGIN_NAME = 'janus.plugin.tms.audio'

export default {
  name: 'TmsJanusAudio',
  mixins: [PlayVueMixin],
  mounted() {
    this.play = new TmsJanusPlay({
      plugin: PLUGIN_NAME,
      elemMedia: document.querySelector('#remoteaudio'),
    })
  },
}
</script>