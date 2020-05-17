<template>
  <div class="janus-player">
    <div>
      <video id="remotevideo" width="320" height="240" autoplay playsinline @playing="playing" />
    </div>
    <div>
      <button @click="playSource">播放视频</button>
      <button @click="pauseSource" :disabled="!canStopSource">暂停播放</button>
      <button @click="resumeSource" :disabled="!canStopSource">恢复播放</button>
      <button @click="stopSource" :disabled="!canStopSource">停止播放</button>
    </div>
    <div>{{status}}</div>
  </div>
</template>

<script>
import { ffmpeg } from '../apis'
import baseMixin from './base.js'

export default {
  name: 'Playtest',
  mixins: [baseMixin],
  props: {
    duration: { type: Number, default: 10 }
  },
  methods: {
    playSource() {
      Promise.resolve(this.sourcePorts)
        .then(sourcePorts => {
          if (
            parseInt(sourcePorts.videoport) ||
            parseInt(sourcePorts.audioport)
          ) {
            return sourcePorts
          } else {
            return this.preparePlay()
          }
        })
        .then(sourcePorts => {
          const { audioport, videoport } = sourcePorts
          ffmpeg.test
            .play(this.socket.id, audioport, videoport, this.duration)
            .then(result => {
              this.ffmpegId = result.cid
              this.$once('onStopped', this.destroySession)
            })
        })
    },
    stopSource() {
      ffmpeg.test.stop(this.ffmpegId)
    },
    pauseSource() {
      ffmpeg.test.pause(this.ffmpegId)
    },
    resumeSource() {
      ffmpeg.test.resume(this.ffmpegId)
    }
  }
}
</script>
<style scoped>
#remotevideo {
  border: 1px solid gray;
}
</style>