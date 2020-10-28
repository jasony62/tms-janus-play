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
    <div v-if="format">{{format}}</div>
    <div>{{status}}</div>
  </div>
</template>

<script>
import { ffmpeg } from '../apis'
import baseMixin from './base.js'

// eslint-disable-next-line
const GlobalJanus = Janus

export default {
  name: 'Playfile',
  mixins: [baseMixin],
  props: {
    file: { type: String, default: '' },
    seek: { type: String, default: '' }
  },
  data() {
    return { format: null }
  },
  methods: {
    PluginOnRemoteStream(stream) {
      GlobalJanus.debug(' ::: Got a remote stream :::')
      GlobalJanus.debug(stream)
      GlobalJanus.attachMediaStream(
        document.querySelector('#remotevideo'),
        stream
      )
    },
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
          ffmpeg.file
            .play(this.socket.id, this.file, audioport, videoport, this.seek)
            .then(({ cid, format }) => {
              this.ffmpegId = cid
              this.format = format
              this.$once('onStopped', this.destroySession)
            })
        })
    },
    stopSource() {
      ffmpeg.file.stop(this.ffmpegId)
    },
    pauseSource() {
      ffmpeg.file.pause(this.ffmpegId)
    },
    resumeSource() {
      ffmpeg.file.resume(this.ffmpegId)
    }
  }
}
</script>
<style scoped>
#remotevideo {
  border: 1px solid gray;
}
</style>