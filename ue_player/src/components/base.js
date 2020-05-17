import { JANUS_SERVER as server, FFMPEG_PUSH_ADDRESS } from '../global'
import io from 'socket.io-client'

// eslint-disable-next-line
const GlobalJanus = Janus

export default {
  data() {
    return {
      janus: null,
      rtprxHandle: null,
      status: '',
      initialized: false,
      connected: false,
      attached: false,
      isWebrtcUp: false,
      sourcePorts: {},
      ffmpegId: null,
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
      return this.connected && parseInt(this.sourcePorts.videoport) && !this.ffmpegId
    },
    canStopSource() {
      return this.connected && this.ffmpegId
    },
  },
  methods: {
    onPluginMessage(msg, remoteJsep) {
      GlobalJanus.debug(' ::: Got a message :::')
      GlobalJanus.debug(msg)
      const result = msg['result']
      if (result !== null && result !== undefined) {
        if (result['status'] !== undefined && result['status'] !== null) {
          const status = result['status']
          switch (status) {
            case 'mounted':
              this.sourcePorts = result.ports
              this.$emit('onMounted', result.ports)
              break
            case 'unmounted':
              this.sourcePorts = {}
              break
          }
        }
      } else if (msg['error'] !== undefined && msg['error'] !== null) {
        console.error(msg['error'])
        return
      }
      if (remoteJsep !== undefined && remoteJsep !== null) {
        GlobalJanus.debug('Handling Remote SDP as well...')
        GlobalJanus.debug(remoteJsep)
        // Offer from the plugin, let's answer
        this.rtprxHandle.createAnswer({
          jsep: remoteJsep,
          // We want recvonly audio/video and, if negotiated, datachannels
          media: { audioSend: false, videoSend: false, data: true },
          success: (localJsep) => {
            GlobalJanus.debug('Got Local SDP!')
            GlobalJanus.debug(localJsep)
            this.rtprxHandle.send({
              message: { request: 'answer.webrtc' },
              jsep: localJsep,
            })
          },
          error: (error) => {
            GlobalJanus.error('WebRTC error:', error)
          },
        })
      }
    },
    PluginOnRemoteStream(stream) {
      GlobalJanus.debug(' ::: Got a remote stream :::')
      GlobalJanus.debug(stream)
      GlobalJanus.attachMediaStream(document.querySelector('#remotevideo'), stream)
    },
    createSession() {
      return new Promise((resolve, reject) => {
        this.janus = new GlobalJanus({
          server: server,
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
          success: this.reset,
          error: this.reset,
        })
    },
    attach() {
      if (!this.janus) return Promise.reject()
      return new Promise((resolve) => {
        const opaqueId = 'rtprx-' + GlobalJanus.randomString(12)
        this.janus.attach({
          plugin: 'janus.plugin.tms.rtprx',
          opaqueId,
          success: (pluginHandle) => {
            this.rtprxHandle = pluginHandle
            this.attached = true
            resolve()
          },
          webrtcState: (isWebrtcUp) => {
            this.isWebrtcUp = isWebrtcUp
            this.$emit(isWebrtcUp ? 'onWebrtcUp' : 'onHangup')
          },
          oncleanup: () => (this.isWebrtcUp = false),
          error: () => {},
          onmessage: this.onPluginMessage,
          onremotestream: this.PluginOnRemoteStream,
        })
      })
    },
    detach() {
      if (!this.rtprxHandle) return Promise.reject()
      return new Promise((resolve) => {
        this.rtprxHandle.detach({
          success: () => {
            this.attached = false
            this.rtprxHandle = null
            resolve()
          },
        })
      })
    },
    createWebrtc() {
      if (!this.rtprxHandle) return Promise.reject()
      // 获得服务端sdp
      return new Promise((resolve) => {
        const body = { request: 'create.webrtc' }
        this.rtprxHandle.send({
          message: body,
          success: () => {
            resolve()
          },
        })
      })
    },
    hangupWebrtc() {
      if (this.rtprxHandle) this.rtprxHandle.hangup()
    },
    prepareSource() {
      if (!this.rtprxHandle) return Promise.reject()
      return new Promise((resolve) => {
        const body = { request: 'prepare.source' }
        this.rtprxHandle.send({
          message: body,
          success: () => {
            resolve()
          },
        })
      })
    },
    destroySource() {
      if (this.rtprxHandle) {
        const body = { request: 'destroy.source' }
        this.rtprxHandle.send({ message: body })
      }
    },
    playing() {
      this.status = 'playing'
      this.$emit('onPlaying')
    },
    preparePlay() {
      return new Promise((resolve) => {
        this.createSession().then(() => {
          this.attach().then(() => {
            let p1, p2
            p1 = new Promise((resolve) => {
              this.$once('onWebrtcUp', () => {
                this.prepareSource().then(() => {
                  this.$once('onMounted', (sourcePorts) => {
                    resolve(sourcePorts)
                  })
                })
              })
            })
            p2 = new Promise((resolve) => {
              this.$once('onPlaying', () => {
                resolve()
              })
            })
            this.createWebrtc().then(() => {
              Promise.all([p1, p2]).then((values) => {
                const [sourcePorts] = values
                resolve(sourcePorts)
              })
            })
          })
        })
      })
    },
    reset() {
      this.sourcePorts = {}
      this.isWebrtcUp = false
      this.attached = false
      this.connected = false
    },
  },
  mounted() {
    GlobalJanus.init({
      debug: 'all',
      callback: () => {
        this.initialized = true
      },
    })
    // 接收推送事件
    const socket = io(FFMPEG_PUSH_ADDRESS, { reconnectionAttempts: 10 })
    socket.on('tms-koa-push', (data) => {
      this.status = data.status
    })
    socket.on('tms-koa-ffmpeg', (data) => {
      this.status = data.status
      if (data.status === 'ended' || data.status === 'killed') {
        this.ffmpegId = null
        this.$emit('onStopped')
      }
    })
    this.socket = socket
  },
  beforeDestroy() {
    this.destroySession()
  },
}
