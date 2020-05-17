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
      </div>
      <div>{{status}}</div>
    </div>
  </div>
</template>

<script>
import { JANUS_SERVER as server, FFMPEG_PUSH_ADDRESS } from '../global'
import { ffmpeg } from '../apis'
import io from 'socket.io-client'

// eslint-disable-next-line
const GlobalJanus = Janus

let janus
const opaqueId = 'rtprx-' + GlobalJanus.randomString(12)
let rtprxHandle // 和Janus插件进行交互

export default {
  name: 'JanusPlayfile',
  data() {
    return {
      duration: 10,
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
        rtprxHandle.createAnswer({
          jsep: remoteJsep,
          // We want recvonly audio/video and, if negotiated, datachannels
          media: { audioSend: false, videoSend: false, data: true },
          success: localJsep => {
            GlobalJanus.debug('Got Local SDP!')
            GlobalJanus.debug(localJsep)
            rtprxHandle.send({
              message: { request: 'answer.webrtc' },
              jsep: localJsep
            })
          },
          error: error => {
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
      return new Promise((resolve, reject) => {
        janus = new GlobalJanus({
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
          }
        })
      })
    },
    destroySession() {
      return new Promise((resolve, reject) => {
        janus.destroy({
          success: () => {
            this.reset()
            resolve()
          },
          error: () => {
            this.reset()
            reject()
          }
        })
      })
    },
    attach() {
      return new Promise(resolve => {
        janus.attach({
          plugin: 'janus.plugin.tms.rtprx',
          opaqueId: opaqueId,
          success: pluginHandle => {
            rtprxHandle = pluginHandle
            this.attached = true
            resolve()
          },
          webrtcState: isWebrtcUp => {
            this.isWebrtcUp = isWebrtcUp
            this.$emit(isWebrtcUp ? 'onWebrtcUp' : 'onHangup')
          },
          oncleanup: () => (this.isWebrtcUp = false),
          error: () => {},
          onmessage: this.onPluginMessage,
          onremotestream: this.PluginOnRemoteStream
        })
      })
    },
    detach() {
      if (!rtprxHandle) return Promise.reject()
      return new Promise(resolve => {
        rtprxHandle.detach({
          success: () => {
            this.attached = false
            rtprxHandle = null
            resolve()
          }
        })
      })
    },
    createWebrtc() {
      // 获得服务端sdp
      return new Promise(resolve => {
        const body = { request: 'create.webrtc' }
        rtprxHandle.send({
          message: body,
          success: () => {
            resolve()
          }
        })
      })
    },
    hangupWebrtc() {
      rtprxHandle.hangup()
    },
    prepareSource() {
      return new Promise(resolve => {
        const body = { request: 'prepare.source' }
        rtprxHandle.send({
          message: body,
          success: () => {
            resolve()
          }
        })
      })
    },
    destroySource() {
      const body = { request: 'destroy.source' }
      rtprxHandle.send({ message: body })
    },
    playing() {
      this.status = 'playing'
      this.$emit('onPlaying')
    },
    preparePlay() {
      return new Promise(resolve => {
        this.createSession().then(() => {
          this.attach().then(() => {
            let p1, p2
            p1 = new Promise(resolve => {
              this.$once('onWebrtcUp', () => {
                this.prepareSource().then(() => {
                  this.$once('onMounted', sourcePorts => {
                    resolve(sourcePorts)
                  })
                })
              })
            })
            p2 = new Promise(resolve => {
              this.$once('onPlaying', () => {
                resolve()
              })
            })
            this.createWebrtc().then(() => {
              Promise.all([p1, p2]).then(values => {
                const [sourcePorts] = values
                resolve(sourcePorts)
              })
            })
          })
        })
      })
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
    },
    reset() {
      this.sourcePorts = {}
      this.isWebrtcUp = false
      this.attached = false
      this.connected = false
    }
  },
  mounted() {
    GlobalJanus.init({
      debug: 'all',
      callback: () => {
        this.initialized = true
      }
    })
    // 接收推送事件
    const socket = io(FFMPEG_PUSH_ADDRESS, { reconnectionAttempts: 10 })
    socket.on('tms-koa-push', data => {
      this.status = data.status
    })
    socket.on('tms-koa-ffmpeg', data => {
      this.status = data.status
      if (data.status === 'ended' || data.status === 'killed') {
        this.ffmpegId = null
      }
    })
    this.socket = socket
  }
}
</script>
<style scoped>
#remotevideo {
  border: 1px solid gray;
}
</style>