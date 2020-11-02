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
import { PlayAudio } from './tms_play_audio'

export default {
  name: 'TmsJanusAudio',
  props: { server: { type: String }, file: { type: String } },
  data() {
    return {
      play: { state: { webrtcUp: false } },
    }
  },
  methods: {
    openWebrtc() {
      this.play.open(this.server)
    },
    closeWebrtc() {
      this.play.close()
    },
    playing() {
      console.debug(' ::: Remote stream is playing :::')
    },
    playFile() {
      this.play.play(this.file)
    },
    pauseFile() {
      this.play.pause()
    },
    resumeFile() {
      this.play.resume()
    },
    stopFile() {
      this.play.stop()
    },
  },
  mounted() {
    this.play = new PlayAudio({
      elemAudio: document.querySelector('#remoteaudio'),
    })
  },
}
</script>