<template>
  <div class="janus-player">
    <div>
      <button @click="createSession">创建会话</button>
      <button @click="destroySession">销毁会话</button>
      <button @click="createConnection">建立媒体连接</button>
      <button @click="hangupConnection">断开媒体连接</button>
      <button @click="prepareSource">准备播放源</button>
      <button @click="destroySource">销毁播放源</button>
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
// eslint-disable-next-line
const GlobalJanus = Janus

var server
if (window.location.protocol === 'http:')
  server = 'http://' + window.location.hostname + ':8088/janus'
else server = 'https://' + window.location.hostname + ':8089/janus'

let janus
const opaqueId = 'playfiletest-' + GlobalJanus.randomString(12)
let playfileHandle

export default {
  name: 'JanusPlayfile',
  data() {
    return { initialized: false, status: '', localJsep: null }
  },
  methods: {
    //001
    onAttachSuccess(pluginHandle) {
      playfileHandle = pluginHandle
      GlobalJanus.log(
        'Plugin attached! (' +
          playfileHandle.getPlugin() +
          ', id=' +
          playfileHandle.getId() +
          ')'
      )
    },
    //002
    onPluginMessage(msg, jsep) {
      GlobalJanus.debug(' ::: Got a message :::')
      GlobalJanus.debug(msg)
      var result = msg['result']
      if (result !== null && result !== undefined) {
        if (result['status'] !== undefined && result['status'] !== null) {
          var status = result['status']
          if (status === 'starting') this.status = 'Starting, please wait...'
          else if (status === 'started') this.status = 'Started'
          else if (status === 'stopped') this.stopStream()
        } else if (msg['playfileHandle'] === 'event') {
          //
        }
      } else if (msg['error'] !== undefined && msg['error'] !== null) {
        console.error(msg['error'])
        this.stopStream()
        return
      }
      if (jsep !== undefined && jsep !== null) {
        GlobalJanus.debug('Handling SDP as well...')
        GlobalJanus.debug(jsep)
        let _this = this
        // Offer from the plugin, let's answer
        playfileHandle.createAnswer({
          jsep,
          // We want recvonly audio/video and, if negotiated, datachannels
          media: { audioSend: false, videoSend: false, data: true },
          success: jsep => {
            GlobalJanus.debug('Got SDP!')
            GlobalJanus.debug(jsep)
            _this.localJsep = jsep
          },
          error: function(error) {
            GlobalJanus.error('WebRTC error:', error)
          }
        })
      }
    },
    // 003
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
          janus.attach({
            plugin: 'janus.plugin.tms.playfile',
            opaqueId: opaqueId,
            success: this.onAttachSuccess,
            error: function() {},
            onmessage: this.onPluginMessage,
            onremotestream: this.PluginOnRemoteStream,
            ondataopen: function() {},
            ondata: function() {},
            oncleanup: function() {}
          })
        },
        error: function() {},
        destroyed: function() {}
      })
    },
    destroySession() {
      janus.destroy()
    },
    createConnection() {
      const body = { request: 'connect' }
      playfileHandle.send({ message: body })
    },
    hangupConnection() {},
    prepareSource() {
      const body = { request: 'prepare.source' }
      playfileHandle.send({ message: body, jsep: this.localJsep })
    },
    destroySource() {
      const body = { request: 'destroy.source' }
      playfileHandle.send({ message: body })
    },
    playing() {
      console.log('playing...')
    },
    stopStream() {
      // const body = { request: 'stop' }
      // playfileHandle.send({ message: body })
      // playfileHandle.hangup()
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