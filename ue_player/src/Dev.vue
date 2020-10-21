<template>
  <div id="dev">
    <div>
      dev
    </div>
    <div>
      <button @click="createSession">创建会话</button>
      <button @click="attach">关联插件</button>
      <button @click="sendMessage">发送消息</button>
    </div>
    <div>
      <button @click="createWebrtc">建立Webrtc</button>
      <button @click="hangupWebrtc">断开Webrtc</button>
    </div>
    <div>
      <button @click="playFile">开始播放</button>
      <button @click="pauseFile">暂停播放</button>
      <button @click="resumeFile">恢复播放</button>
      <button @click="stopFile">停止播放</button>
    </div>
    <div>
      <button @click="detach">断开插件</button>
      <button @click="destroySession">销毁会话</button>
    </div>
    <div>
      <video id="remotevideo" width="320" height="240" autoplay playsinline @playing="playing" />
    </div>
  </div>
</template>

<script>
// eslint-disable-next-line
const GlobalJanus = Janus

// let server = `https://192.168.31.100:8089/janus`
let server = `https://192.168.43.165:8089/janus`

export default {
  name: 'Dev',
  data() {
    return {
      initialized: false,
      connected: false,
      attached: false,
      webrtcUp: false,
    }
  },
  computed: {},
  methods: {
    PluginOnRemoteStream(stream) {
      GlobalJanus.debug(' ::: Got a remote stream :::')
      GlobalJanus.debug(stream)
      GlobalJanus.attachMediaStream(
        document.querySelector('#remotevideo'),
        stream
      )
    },
    playing() {
      GlobalJanus.debug(' ::: Remote stream is playing :::')
    },
    createSession() {
      return new Promise((resolve, reject) => {
        this.janus = new GlobalJanus({
          server,
          success: () => {
            this.connected = true
            resolve()
          },
          error: () => {
            reject()
          },
          destroyed: () => {
            this.connected = false
          },
        })
      })
    },
    destroySession() {
      if (this.janus)
        this.janus.destroy({
          // success: this.reset,
          // error: this.reset,
        })
    },
    attach() {
      if (!this.janus) return Promise.reject()
      return new Promise((resolve) => {
        const opaqueId = 'dev-' + GlobalJanus.randomString(12)
        this.janus.attach({
          plugin: 'janus.plugin.tms.mp4',
          opaqueId,
          success: (pluginHandle) => {
            this.mp4Handle = pluginHandle
            this.attached = true
            resolve()
          },
          webrtcState: (isWebrtcUp) => {
            this.webrtcUp = isWebrtcUp
            this.$emit(isWebrtcUp ? 'onWebrtcUp' : 'onHangup')
          },
          oncleanup: () => (this.webrtcUp = false),
          error: () => {},
          onmessage: this.onPluginMessage,
          onremotestream: this.PluginOnRemoteStream,
        })
      })
    },
    detach() {
      if (!this.mp4Handle) return Promise.reject()
      return new Promise((resolve) => {
        this.mp4Handle.detach({
          success: () => {
            this.attached = false
            this.mp4Handle = null
            resolve()
          },
        })
      })
    },
    sendMessage() {
      if (!this.mp4Handle) return Promise.reject()
      return new Promise((resolve) => {
        const body = { request: 'ping' }
        this.mp4Handle.send({
          message: body,
          success: (msg) => {
            console.log('发送消息获得的同步响应', msg)
            resolve()
          },
        })
      })
    },
    createWebrtc() {
      if (!this.mp4Handle) return Promise.reject()
      // 获得服务端sdp
      return new Promise((resolve) => {
        const body = { request: 'request.offer' }
        this.mp4Handle.send({
          message: body,
          success: () => {
            resolve()
          },
        })
      })
    },
    hangupWebrtc() {
      if (this.mp4Handle) this.mp4Handle.hangup()
    },
    playFile() {
      this.mp4Handle.send({
        message: { request: 'play.file' },
      })
    },
    pauseFile() {
      this.mp4Handle.send({
        message: { request: 'pause.file' },
      })
    },
    resumeFile() {
      this.mp4Handle.send({
        message: { request: 'resume.file' },
      })
    },
    stopFile() {
      this.mp4Handle.send({
        message: { request: 'stop.file' },
      })
    },
    onPluginMessage(msg, remoteJsep) {
      GlobalJanus.debug(' ::: Got a message :::')
      GlobalJanus.debug(msg)
      if (remoteJsep !== undefined && remoteJsep !== null) {
        GlobalJanus.debug('Handling Remote SDP as well...')
        GlobalJanus.debug(remoteJsep)
        // Offer from the plugin, let's answer
        this.mp4Handle.createAnswer({
          jsep: remoteJsep,
          // We want recvonly audio/video and, if negotiated, datachannels
          media: { audioSend: false, videoSend: false, data: true },
          success: (localJsep) => {
            GlobalJanus.debug('Got Local SDP!')
            GlobalJanus.debug(localJsep)
            this.mp4Handle.send({
              message: { request: 'push.answer' },
              jsep: localJsep,
            })
          },
          error: (error) => {
            GlobalJanus.error('WebRTC error:', error)
          },
        })
      }
    },
  },
  mounted() {
    GlobalJanus.init({
      debug: 'all',
      callback: () => {
        this.initialized = true
      },
    })
  },
}
</script>