/**
 * 自定义Janus插件：播放mp4
 */
import Janus from './janus.es'

const PLUGIN_NAME = 'janus.plugin.tms.play'

/* 建立会话 */
function createSession(server, myJanus) {
  let { channelState } = myJanus
  return new Promise((resolve, reject) => {
    let janus = new Janus({
      server,
      success: () => {
        channelState.connected = true
        myJanus.janus = janus
        resolve(janus)
      },
      error: () => {
        reject()
      },
      destroyed: () => {
        /*会话终端时，需要重置状态*/
        myJanus.reset()
      },
    })
  })
}
/* 销毁会话 */
function destroySession(myJanus) {
  return new Promise((resolve) => {
    if (myJanus.janus) {
      myJanus.janus.destroy()
      resolve(true)
    } else resolve(false)
  })
}
/* 关联插件 */
function attach(myJanus) {
  if (!myJanus.janus) return Promise.reject()
  let { channelState } = myJanus
  return new Promise((resolve) => {
    const opaqueId = 'dev-' + Janus.randomString(12)
    myJanus.janus.attach({
      plugin: myJanus.plugin,
      opaqueId,
      success: (pluginHandle) => {
        myJanus.pluginHandle = pluginHandle
        channelState.attached = true
        resolve(pluginHandle)
      },
      webrtcState: (isWebrtcUp) => {
        channelState.webrtcUp = isWebrtcUp
        myJanus.onwebrtcstate(isWebrtcUp ? 'onWebrtcUp' : 'onHangup')
      },
      oncleanup: () => (channelState.webrtcUp = false),
      error: () => {},
      onmessage: myJanus.onmessage,
      onremotestream: myJanus.onremotestream,
    })
  })
}
/* 断开插件 */
// function detach(myJanus) {
//   return new Promise((resolve) => {
//     let { pluginHandle, channelState } = myJanus
//     if (!pluginHandle) resolve(false)
//     else {
//       pluginHandle.detach({
//         success: () => {
//           channelState.attached = false
//           myJanus.pluginHandle = null
//           resolve(true)
//         },
//       })
//     }
//   })
// }
/* 请求服务端建立WebRTC连接 */
function createWebrtc(myJanus) {
  if (!myJanus.pluginHandle) return Promise.reject()
  // 请求获得服务端sdp。WebRTC通道异步建立。
  return new Promise((resolve) => {
    const body = { request: 'request.offer' }
    myJanus.pluginHandle.send({
      message: body,
      success: resolve,
    })
  })
}
/* 关闭WebRTC */
// function hangupWebrtc(myJanus) {
//   return new Promise((resolve) => {
//     if (myJanus.pluginHandle) {
//       myJanus.pluginHandle.hangup()
//       resolve(true)
//     } else resolve(false)
//   })
// }

function onRemoteStream(stream) {
  Janus.debug(' ::: Got a remote stream :::')
  Janus.debug(stream)
  Janus.attachMediaStream(this.elemMedia, stream)
}

function onPluginMessage(msg, remoteJsep) {
  Janus.debug(' ::: Got a message :::')
  Janus.debug(msg)
  if (remoteJsep !== undefined && remoteJsep !== null) {
    Janus.debug('Handling Remote SDP as well...')
    Janus.debug(remoteJsep)
    // Offer from the plugin, let's answer
    this.pluginHandle.createAnswer({
      jsep: remoteJsep,
      // We want recvonly audio/video and, if negotiated, datachannels
      media: { audioSend: false, videoSend: false, data: true },
      success: (localJsep) => {
        Janus.debug('Got Local SDP!')
        Janus.debug(localJsep)
        this.pluginHandle.send({
          message: { request: 'push.answer' },
          jsep: localJsep,
        })
      },
      error: (error) => {
        Janus.error('WebRTC error:', error)
      },
    })
  } else if (msg.tms_play_event === 'launch.play') {
    this.playState.state = 'going'
  } else if (msg.tms_play_event === 'resume.play') {
    this.playState.state = 'going'
  } else if (msg.tms_play_event === 'exit.play') {
    this.playState.state = 'ready'
  }
}
/**
 * 通道状态
 */
class ChannelState {
  constructor() {
    this.initialized = false
    this.connected = false
    this.attached = false
    this.webrtcUp = false
  }
  reset() {
    this.connected = false
    this.attached = false
    this.webrtcUp = false
  }
}
/**
 * 播放状态
 * 记录播放文件的基本信息；记录播放状态。
 */
class PlayState {
  constructor(file) {
    this.file = file // 要播放的文件名
    this.duration = -1 // 播放时长（秒）
    this.position = -1 // 播放位置（秒）
    this.state = undefined // 播放状态
  }
  reset() {
    this.duration = -1
    this.position = -1
    this.state = undefined
  }
}

export class TmsJanusPlay {
  constructor({ debug = 'all', elemMedia, onwebrtcstate = Janus.noop }) {
    this.plugin = PLUGIN_NAME
    this.janus = null
    this.pluginHandle = null
    this.onmessage = onPluginMessage.bind(this)
    this.elemMedia = elemMedia
    this.onremotestream = onRemoteStream.bind(this)
    this.onwebrtcstate = onwebrtcstate
    this.channelState = new ChannelState()
    this.playState = new PlayState()
    Janus.init({
      debug,
      callback: () => {
        this.channelState.initialized = true
      },
    })
  }
  get isWebrtcUp() {
    return this.channelState.webrtcUp
  }
  get isPlayEnabled() {
    return this.playState.state === 'ready'
  }
  get isPauseEnabled() {
    return this.playState.state === 'going'
  }
  get isResumeEnabled() {
    return this.playState.state === 'paused'
  }
  get isStopEnabled() {
    return this.playState.state === 'going' || this.playState.state === 'paused'
  }
  reset() {
    this.channelState.reset()
    this.playState.reset()
  }
  /* 建立WebRTC媒体通道 */
  open(server) {
    return createSession(server, this)
      .then(() => attach(this))
      .then(() => createWebrtc(this))
  }
  close() {
    // return hangupWebrtc(this).then(() => {
    //   // hangup没有回调函数，只能等待oncleanup事件
    //   return detach(this).then(() => destroySession(this))
    // })
    // 不需要按建立的顺序逐步销毁会话
    return destroySession(this)
  }
  probe(file) {
    this.playState.file = file
    this.pluginHandle.send({
      message: {
        request: 'probe.file',
        file,
      },
      success: (rsp) => {
        if (rsp.code === 0) {
          console.log('获得播放文件信息', rsp)
          this.playState.state = 'ready'
          this.playState.duration = rsp.duration
        } else if (rsp.code == 404) console.log('播放文件文件不存在', rsp)
        else console.log('无法处理播放文件', rsp)
      },
    })
  }
  play() {
    this.pluginHandle.send({
      message: {
        request: 'play.file',
        file: this.playState.file,
      },
    })
  }
  pause() {
    this.pluginHandle.send({
      message: { request: 'pause.file' },
      success: () => (this.playState.state = 'paused'),
    })
  }
  resume() {
    this.pluginHandle.send({
      message: { request: 'resume.file' },
    })
  }
  stop() {
    this.pluginHandle.send({
      message: { request: 'stop.file' },
      success: () => (this.playState.state = 'ready'),
    })
  }
}
