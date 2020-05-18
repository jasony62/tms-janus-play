<template>
  <div class="janus-player">
    <div>
      <audio id="remoteaudio" width="320" height="240" autoplay @playing="playing" />
    </div>
    <div>
      <button @click="playSource">播放音频</button>
      <button @click="pauseSource" :disabled="!canStopSource">暂停播放</button>
      <button @click="resumeSource" :disabled="!canStopSource">恢复播放</button>
      <button @click="stopSource" :disabled="!canStopSource">停止播放</button>
      <button disabled>跳到指定位置播放</button>
    </div>
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
    file: { type: String, default: '' }
  },
  methods: {
    PluginOnRemoteStream(stream) {
      GlobalJanus.debug(' ::: Got a remote stream :::')
      GlobalJanus.debug(stream)
      GlobalJanus.attachMediaStream(
        document.querySelector('#remoteaudio'),
        stream
      )
    },
    playSource() {
      Promise.resolve(this.sourcePorts)
        .then(sourcePorts => {
          if (parseInt(sourcePorts.audioport)) {
            return sourcePorts
          } else {
            return this.preparePlay()
          }
        })
        .then(sourcePorts => {
          const { audioport } = sourcePorts
          ffmpeg.file
            .play(this.socket.id, this.file, audioport)
            .then(result => {
              this.ffmpegId = result.cid
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
#remoteaudio {
  border: 1px solid gray;
}
</style>