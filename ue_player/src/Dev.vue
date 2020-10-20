<template>
  <div id="dev">
    <div>
      dev
    </div>
    <div>
      <button @click="createSession">创建会话</button>
    </div>
    <div>
      <button @click="attach">关联插件</button>
    </div>
    <div>
      <button @click="sendMessage">发送消息</button>
    </div>
    <div>
      <button @click="detach">断开插件</button>
    </div>
    <div>
      <button @click="destroySession">销毁会话</button>
    </div>
  </div>
</template>

<script>
// eslint-disable-next-line
const GlobalJanus = Janus

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
          // onremotestream: this.PluginOnRemoteStream,
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
    onPluginMessage(msg, remoteJsep) {
      GlobalJanus.debug(' ::: Got a message :::')
      GlobalJanus.debug(msg)
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