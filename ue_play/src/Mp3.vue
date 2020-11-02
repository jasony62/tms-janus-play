<template>
  <div id="dev">
    <div>
      <label>文件：<input type="text" v-model="file"></label>
    </div>
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
import { PlayMp3 } from './tms_play_mp3'

export default {
  name: 'Mp3',
  data() {
    return {
      file: '/home/janus/media/sine-8k-10s.mp3',
      play: { state: { webrtcUp: false } },
    }
  },
  computed: {},
  methods: {
    openWebrtc() {
      this.play.open()
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
    this.play = new PlayMp3({
      elemAudio: document.querySelector('#remoteaudio'),
    })
  },
}
</script>