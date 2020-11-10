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
function hangupWebrtc(myJanus) {
  return new Promise((resolve) => {
    if (myJanus.pluginHandle) {
      myJanus.pluginHandle.hangup()
      resolve(true)
    } else resolve(false)
  })
}

function onRemoteStream(stream) {
  Janus.debug(' ::: Got a remote stream :::')
  Janus.debug(stream)
  Janus.attachMediaStream(this.elemMedia, stream)
  let [videoTrack] = stream.getVideoTracks()
  /* 通知视频状态发生变化 */
  if (!videoTrack || videoTrack.muted) {
    if (this.onVideoMute) this.onVideoMute()
  } else {
    if (this.onVideoUnmute) this.onVideoUnmute(videoTrack)
  }
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
  } else if (msg.tms_play_event === 'pause.play') {
    this.playState.state = 'paused'
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
    this.sending = false
  }
  reset() {
    this.connected = false
    this.attached = false
    this.webrtcUp = false
    this.sending = false
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
  constructor({
    debug = 'all',
    elemMedia,
    onVideoMute = Janus.noop,
    onVideoUnmute = Janus.noop,
    onwebrtcstate = Janus.noop,
  }) {
    this.plugin = PLUGIN_NAME
    this.janus = null
    this.pluginHandle = null
    this.onmessage = onPluginMessage.bind(this)
    this.elemMedia = elemMedia
    this.onremotestream = onRemoteStream.bind(this)
    this.onwebrtcstate = onwebrtcstate
    this.channelState = new ChannelState()
    this.playState = new PlayState()
    this.onVideoMute = onVideoMute.bind(this)
    this.onVideoUnmute = onVideoUnmute.bind(this)
    Janus.init({
      debug,
      callback: () => {
        this.channelState.initialized = true
      },
    })
  }
  get isConnEnable() {
    return !this.channelState.attached && !this.channelState.sending
  }
  get isProbeEnable() {
    return this.channelState.attached && !this.channelState.sending
  }
  get isCreateWebrtcEnable() {
    return this.channelState.attached && this.isWebrtcUp === false
  }
  get isWebrtcUp() {
    return this.channelState.webrtcUp
  }
  get isControlEnable() {
    return this.isWebrtcUp && this.playState.file
  }
  get isStopEnable() {
    return (this.isWebrtcUp && this.playState.state === 'going') || this.playState.state === 'paused'
  }
  reset() {
    this.channelState.reset()
    this.playState.reset()
  }
  /* 建立WebRTC媒体通道 */
  connect(server) {
    this.channelState.sending = true
    return createSession(server, this)
      .then(() => attach(this))
      .then(() => (this.channelState.sending = false))
  }
  closeConn() {
    // 不需要按建立的顺序逐步销毁会话
    return destroySession(this)
  }
  createWebrtc() {
    this.channelState.sending = true
    this.channelState.webrtcUp = undefined
    return createWebrtc(this).then(() => (this.channelState.sending = false))
  }
  hangupWebrtc() {
    this.channelState.sending = true
    return hangupWebrtc(this).then(() => (this.channelState.sending = false))
  }
  /*获取文件信息*/
  probe(file) {
    this.channelState.sending = true
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
        this.channelState.sending = false
      },
    })
  }
  /*控制播放，开始，暂停，恢复*/
  control() {
    let { file, state } = this.playState
    this.pluginHandle.send({
      message: {
        request: 'ctrl.play',
        file,
      },
      success: () => {
        if (state === 'going') this.playState.state = 'paused'
      },
    })
  }
  /*停止播放*/
  stop() {
    this.pluginHandle.send({
      message: { request: 'stop.play' },
      success: () => (this.playState.state = 'ready'),
    })
  }
}
