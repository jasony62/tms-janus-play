/**
 * 自定义Janus插件：播放mp4
 */
import Janus from './janus.es'

/* 建立会话 */
function createSession(server, myJanus) {
  let { state } = myJanus
  return new Promise((resolve, reject) => {
    let janus = new Janus({
      server,
      success: () => {
        state.connected = true
        myJanus.janus = janus
        resolve(janus)
      },
      error: () => {
        reject()
      },
      destroyed: () => {
        state.connected = false
      },
    })
  })
}
/* 销毁会话 */
function destroySession(myJanus) {
  return new Promise((resolve) => {
    if (myJanus.janus) {
      myJanus.janus.destroy()
      myJanus.state.connected = false
      resolve(true)
    } else resolve(false)
  })
}
/* 关联插件 */
function attach(myJanus) {
  if (!myJanus.janus) return Promise.reject()
  let { state } = myJanus
  return new Promise((resolve) => {
    const opaqueId = 'dev-' + Janus.randomString(12)
    myJanus.janus.attach({
      plugin: myJanus.plugin,
      opaqueId,
      success: (pluginHandle) => {
        myJanus.pluginHandle = pluginHandle
        state.attached = true
        resolve(pluginHandle)
      },
      webrtcState: (isWebrtcUp) => {
        state.webrtcUp = isWebrtcUp
        myJanus.onwebrtcstate(isWebrtcUp ? 'onWebrtcUp' : 'onHangup')
      },
      oncleanup: () => (state.webrtcUp = false),
      error: () => {},
      onmessage: myJanus.onmessage,
      onremotestream: myJanus.onremotestream,
    })
  })
}
/* 断开插件 */
function detach(myJanus) {
  return new Promise((resolve) => {
    let { pluginHandle, state } = myJanus
    if (!pluginHandle) resolve(false)
    else {
      pluginHandle.detach({
        success: () => {
          state.attached = false
          myJanus.pluginHandle = null
          resolve(true)
        },
      })
    }
  })
}
/* 请求服务端建立WebRTC连接 */
function createWebrtc(myJanus) {
  if (!myJanus.pluginHandle) return Promise.reject()
  // 获得服务端sdp
  return new Promise((resolve) => {
    const body = { request: 'request.offer' }
    myJanus.pluginHandle.send({
      message: body,
      success: () => {
        resolve()
      },
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
  }
}

class MyJanusState {
  constructor() {
    this.initialized = false
    this.connected = false
    this.attached = false
    this.webrtcUp = false
  }
}

export class TmsJanusPlay {
  constructor({ plugin, debug = 'all', elemMedia, onwebrtcstate = Janus.noop }) {
    this.plugin = plugin
    this.janus = null
    this.pluginHandle = null
    this.onmessage = onPluginMessage.bind(this)
    this.elemMedia = elemMedia
    this.onremotestream = onRemoteStream.bind(this)
    this.onwebrtcstate = onwebrtcstate
    this.state = new MyJanusState()
    Janus.init({
      debug,
      callback: () => {
        this.state.initialized = true
      },
    })
  }
  /* 建立WebRTC媒体通道 */
  open(server) {
    return createSession(server, this)
      .then(() => attach(this))
      .then(() => createWebrtc(this))
  }
  close() {
    return hangupWebrtc(this)
      .then(() => detach(this))
      .then(() => destroySession(this))
  }
  play(file) {
    this.file = file
    this.pluginHandle.send({
      message: {
        request: 'play.file',
        file,
      },
    })
  }
  pause() {
    this.pluginHandle.send({
      message: { request: 'pause.file' },
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
    })
  }
}
