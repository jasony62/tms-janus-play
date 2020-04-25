<template>
  <div class="janus-player">
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
      <label>
        <input type='radio' name='sourceType' value='test' v-model="sourceType">模拟媒体</label>
      <label>
        <input type='radio' name='sourceType' value='file' v-model="sourceType">媒体文件</label>
    </div>
    <div>
      <span v-if="sourceType==='test'">
        <button @click="playTestSource" :disabled="!canPlaySource">开始播放模拟视频</button>
      </span>
      <span v-if="sourceType==='file'">
        <input type="text" v-model="playfile" placeholder="要播放的文件">
        <button @click="playFileSource" :disabled="!canPlaySource">开始播放指定文件</button>
        <button disabled>跳到指定位置播放</button>
      </span>
      <button @click="pauseSource" :disabled="!canStopSource">暂停播放</button>
      <button @click="resumeSource" :disabled="!canStopSource">恢复播放</button>
      <button @click="stopSource" :disabled="!canStopSource">停止播放</button>
    </div>
    <div>
      <video id="remotevideo" width="320" height="240" autoplay playsinline @playing="playing" />
    </div>
    <div>
      <div>{{status}}</div>
    </div>
  </div>
</template>

<script>
import { ffmpeg } from '../apis'

// eslint-disable-next-line
const GlobalJanus = Janus

let server
if (window.location.protocol === 'http:')
  server = 'http://' + window.location.hostname + ':8088/janus'
else server = 'https://' + window.location.hostname + ':8089/janus'

let janus
const opaqueId = 'playfiletest-' + GlobalJanus.randomString(12)
let playfileHandle

export default {
  name: 'JanusPlayfile',
  data() {
    return {
      playfile: '',
      status: '',
      initialized: false,
      connected: false,
      attached: false,
      isWebrtcUp: false,
      sourcePorts: {},
      ffmpegId: null,
      sourceType: 'test'
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
      return this.attached && !this.isWebrtcUp
    },
    canHangupWebrtc() {
      return this.attached && this.isWebrtcUp
    },
    canPrepareSource() {
      return this.isWebrtcUp && !this.sourcePorts.videoport
    },
    canDestroySource() {
      return this.isWebrtcUp && this.sourcePorts.videoport
    },
    canPlaySource() {
      return parseInt(this.sourcePorts.videoport) && !this.ffmpegId
    },
    canStopSource() {
      return this.ffmpegId
    }
  },
  methods: {
    onAttachSuccess(pluginHandle) {
      playfileHandle = pluginHandle
      this.attached = true
    },
    onPluginMessage(msg, remoteJsep) {
      GlobalJanus.debug(' ::: Got a message :::')
      GlobalJanus.debug(msg)
      var result = msg['result']
      if (result !== null && result !== undefined) {
        if (result['status'] !== undefined && result['status'] !== null) {
          var status = result['status']
          if (status === 'starting') this.status = 'Starting, please wait...'
          else if (status === 'started') this.status = 'Started'
          else if (status === 'stopped') this.status = 'Stopped'
          else if (status === 'mounted') {
            this.sourcePorts = result.ports
          }
        } else if (msg['playfileHandle'] === 'event') {
          //
        }
      } else if (msg['error'] !== undefined && msg['error'] !== null) {
        console.error(msg['error'])
        return
      }
      if (remoteJsep !== undefined && remoteJsep !== null) {
        GlobalJanus.debug('Handling SDP as well...')
        GlobalJanus.debug(remoteJsep)
        // Offer from the plugin, let's answer
        playfileHandle.createAnswer({
          jsep: remoteJsep,
          // We want recvonly audio/video and, if negotiated, datachannels
          media: { audioSend: false, videoSend: false, data: true },
          success: localJsep => {
            GlobalJanus.debug('Got SDP!')
            GlobalJanus.debug(localJsep)
            playfileHandle.send({
              message: { request: 'answer.webrtc' },
              jsep: localJsep
            })
          },
          error: function(error) {
            GlobalJanus.error('WebRTC error:', error)
          }
        })
      }
    },
    PluginOnRemoteStream(stream) {
      GlobalJanus.debug(' ::: Got a remote stream :::')
      GlobalJanus.debug(stream)
      GlobalJanus.attachMediaStream(
        document.querySelector('#remotevideo'),
        stream
      )
    },
    createSession() {
      // Create session
      janus = new GlobalJanus({
        server: server,
        success: () => {
          this.connected = true
        },
        error: function() {},
        destroyed: function() {}
      })
    },
    destroySession() {
      janus.destroy({
        success: () => (this.connected = false),
        error: () => (this.connected = false)
      })
    },
    attach() {
      janus.attach({
        plugin: 'janus.plugin.tms.rtprx',
        opaqueId: opaqueId,
        success: this.onAttachSuccess,
        webrtcState: isWebrtcUp => {
          this.isWebrtcUp = isWebrtcUp
        },
        oncleanup: () => (this.isWebrtcUp = false),
        error: function() {},
        onmessage: this.onPluginMessage,
        onremotestream: this.PluginOnRemoteStream
      })
    },
    detach() {
      if (playfileHandle)
        playfileHandle.detach({
          success: () => {
            this.attached = false
            playfileHandle = null
          }
        })
    },
    createWebrtc() {
      // 获得服务端sdp
      const body = { request: 'create.webrtc' }
      playfileHandle.send({ message: body })
    },
    hangupWebrtc() {
      playfileHandle.hangup()
    },
    prepareSource() {
      const body = { request: 'prepare.source' }
      playfileHandle.send({ message: body })
    },
    destroySource() {
      const body = { request: 'destroy.source' }
      playfileHandle.send({ message: body })
    },
    playTestSource() {
      if (this.canPlaySource) {
        const { audioport, videoport } = this.sourcePorts
        ffmpeg.test.play(audioport, videoport).then(result => {
          this.ffmpegId = result.cid
        })
      }
    },
    playFileSource() {
      if (this.canPlaySource) {
        const { audioport, videoport } = this.sourcePorts
        ffmpeg.file.play(this.playfile, audioport, videoport).then(result => {
          this.ffmpegId = result.cid
        })
      }
    },
    stopSource() {
      const api = ffmpeg[this.sourceType]
      if (api) api.stop(this.ffmpegId).then(() => (this.ffmpegId = null))
    },
    pauseSource() {
      const api = ffmpeg[this.sourceType]
      if (api) api.pause(this.ffmpegId)
    },
    resumeSource() {
      const api = ffmpeg[this.sourceType]
      if (api) api.resume(this.ffmpegId)
    },
    playing() {
      console.log('playing...')
    }
  },
  mounted() {
    GlobalJanus.init({
      debug: 'all',
      callback: () => {
        this.initialized = true
      }
    })
  }
}
</script>
<style scoped>
#remotevideo {
  border: 1px solid gray;
}
</style>