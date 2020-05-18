<template>
  <div class="janus-player">
    <div>
      <label>
        <input type='radio' name='sourceType' value='test' v-model="sourceType">模拟媒体</label>
      <label>
        <input type='radio' name='sourceType' value='file' v-model="sourceType">媒体文件</label>
    </div>
    <div>
      <span v-if="sourceType==='test'">
        <label>播放时长（秒）：<input type="number" v-model="duration"></label>
      </span>
      <span v-if="sourceType==='file'">
        <input type="text" v-model="playfile" placeholder="要播放的文件">
        <button disabled>跳到指定位置播放</button>
      </span>
      <button @click="refresh">刷新</button>
    </div>
    <div>
      <button @click="createSession" :disabled="!canCreateSession">创建会话</button>
      <button @click="destroySession" :disabled="!canDestroySession">销毁会话</button>
      <button @click="attach" :disabled="!canAttach">连接插件</button>
      <button @click="detach" :disabled="!canDetach">销毁插件</button>
      <button @click="createWebrtc" :disabled="!canCreateWebrtc">建立WEBRTC连接</button>
      <button @click="hangupWebrtc" :disabled="!canHangupWebrtc">断开WEBRTC连接</button>
      <button @click="prepareSource" :disabled="!canPrepareSource">准备播放源</button>
      <button @click="destroySource" :disabled="!canDestroySource">销毁播放源</button>
    </div>
    <div>
      <div>
        <video id="remotevideo" width="320" height="240" autoplay playsinline @playing="playing" />
      </div>
      <div>
        <button @click="playSource">播放视频</button>
        <button @click="pauseSource" :disabled="!canStopSource">暂停播放</button>
        <button @click="resumeSource" :disabled="!canStopSource">恢复播放</button>
        <button @click="stopSource" :disabled="!canStopSource">停止播放</button>
        <span v-if="sourceType==='file'">
          <button disabled>跳到指定位置播放</button>
        </span>
      </div>
      <div>{{status}}</div>
    </div>
  </div>
</template>

<script>
import { ffmpeg } from '../apis'
import baseMixin from './base.js'

export default {
  name: 'JanusPlayfile',
  mixins: [baseMixin],
  data() {
    return {
      sourceType: 'test',
      duration: 10,
      playfile: ''
    }
  },
  computed: {
    canCreateSession() {
      return this.initialized && !this.connected
    },
    canDestroySession() {
      return this.initialized && this.connected
    },
    canAttach() {
      return this.connected && !this.attached
    },
    canDetach() {
      return this.connected && this.attached
    },
    canCreateWebrtc() {
      return this.connected && this.attached && !this.isWebrtcUp
    },
    canHangupWebrtc() {
      return this.connected && this.attached && this.isWebrtcUp
    },
    canPrepareSource() {
      return this.connected && this.isWebrtcUp && !this.sourcePorts.videoport
    },
    canDestroySource() {
      return this.connected && this.isWebrtcUp && this.sourcePorts.videoport
    },
    canPlaySource() {
      return (
        this.connected && parseInt(this.sourcePorts.videoport) && !this.ffmpegId
      )
    },
    canStopSource() {
      return this.connected && this.ffmpegId
    }
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
          switch (this.sourceType) {
            case 'test':
              ffmpeg.test
                .play(this.socket.id, audioport, videoport, this.duration)
                .then(result => {
                  this.ffmpegId = result.cid
                })
              break
            case 'file':
              ffmpeg.file
                .play(this.socket.id, this.playfile, audioport, videoport)
                .then(result => {
                  this.ffmpegId = result.cid
                })
              break
          }
        })
    },
    stopSource() {
      const api = ffmpeg[this.sourceType]
      if (api) api.stop(this.ffmpegId)
    },
    pauseSource() {
      const api = ffmpeg[this.sourceType]
      if (api) api.pause(this.ffmpegId)
    },
    resumeSource() {
      const api = ffmpeg[this.sourceType]
      if (api) api.resume(this.ffmpegId)
    }
  }
}
</script>
<style scoped>
#remotevideo {
  border: 1px solid gray;
}
</style>