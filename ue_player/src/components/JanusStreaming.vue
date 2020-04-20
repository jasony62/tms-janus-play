<template>
  <div class="janus-player">
    <div>
      <button @click="start">开始</button>
      <button @click="end">结束</button>
      <button @click="play">播放</button>
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
const opaqueId = 'streamingtest-' + GlobalJanus.randomString(12)
let streaming
let selectedStream = 1

export default {
  name: 'JanusPlayer',
  data() {
    return { initialized: false, status: '' }
  },
  methods: {
    //001
    PluginSuccess(pluginHandle) {
      streaming = pluginHandle
      GlobalJanus.log(
        'Plugin attached! (' +
          streaming.getPlugin() +
          ', id=' +
          streaming.getId() +
          ')'
      )
    },
    //002
    PluginOnmessage(msg, jsep) {
      GlobalJanus.debug(' ::: Got a message :::')
      GlobalJanus.debug(msg)
      var result = msg['result']
      if (result !== null && result !== undefined) {
        if (result['status'] !== undefined && result['status'] !== null) {
          var status = result['status']
          if (status === 'starting') this.status = 'Starting, please wait...'
          else if (status === 'started') this.status = 'Started'
          else if (status === 'stopped') this.stopStream()
        } else if (msg['streaming'] === 'event') {
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
        // Offer from the plugin, let's answer
        streaming.createAnswer({
          jsep,
          // We want recvonly audio/video and, if negotiated, datachannels
          media: { audioSend: false, videoSend: false, data: true },
          success: function(jsep) {
            GlobalJanus.debug('Got SDP!')
            GlobalJanus.debug(jsep)
            var body = { request: 'start' }
            streaming.send({ message: body, jsep: jsep })
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
    playStream() {
      GlobalJanus.log('Selected video id #' + selectedStream)
      if (selectedStream === undefined || selectedStream === null) {
        console.log('Select a stream from the list')
        return
      }
      var body = { request: 'watch', id: parseInt(selectedStream) }
      streaming.send({ message: body })
    },
    stopStream() {
      let body = { request: 'stop' }
      streaming.send({ message: body })
      streaming.hangup()
    },
    start() {
      // Create session
      janus = new GlobalJanus({
        server: server,
        success: () => {
          janus.attach({
            plugin: 'janus.plugin.streaming',
            opaqueId: opaqueId,
            success: this.PluginSuccess,
            error: function() {},
            onmessage: this.PluginOnmessage,
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
    end() {
      janus.destroy()
    },
    play() {
      this.playStream()
    },
    playing() {
      console.log('playing...')
    }
  },
  mounted() {
    // eslint-disable-next-line
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