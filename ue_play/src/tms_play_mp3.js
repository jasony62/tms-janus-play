/**
 * 自定义Janus插件：播放mp4
 */

import { JANUS_SERVER as server, GlobalJanus } from './global'

const PLUGIN_NAME = 'janus.plugin.tms.mp3'

/* 建立会话 */
function createSession(myJanus) {
  let { state } = myJanus
  return new Promise((resolve, reject) => {
    let janus = new GlobalJanus({
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
    const opaqueId = 'dev-' + GlobalJanus.randomString(12)
    myJanus.janus.attach({
      plugin: PLUGIN_NAME,
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
  GlobalJanus.debug(' ::: Got a remote stream :::')
  GlobalJanus.debug(stream)
  GlobalJanus.attachMediaStream(this.elemAudio, stream)
}

function onPluginMessage(msg, remoteJsep) {
  GlobalJanus.debug(' ::: Got a message :::')
  GlobalJanus.debug(msg)
  if (remoteJsep !== undefined && remoteJsep !== null) {
    GlobalJanus.debug('Handling Remote SDP as well...')
    GlobalJanus.debug(remoteJsep)
    // Offer from the plugin, let's answer
    this.pluginHandle.createAnswer({
      jsep: remoteJsep,
      // We want recvonly audio/video and, if negotiated, datachannels
      media: { audioSend: false, videoSend: false, data: true },
      success: (localJsep) => {
        GlobalJanus.debug('Got Local SDP!')
        GlobalJanus.debug(localJsep)
        this.pluginHandle.send({
          message: { request: 'push.answer' },
          jsep: localJsep,
        })
      },
      error: (error) => {
        GlobalJanus.error('WebRTC error:', error)
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

export class PlayMp3 {
  constructor({ debug = 'all', elemAudio, onwebrtcstate = GlobalJanus.noop }) {
    this.janus = null
    this.pluginHandle = null
    this.onmessage = onPluginMessage.bind(this)
    this.elemAudio = elemAudio
    this.onremotestream = onRemoteStream.bind(this)
    this.onwebrtcstate = onwebrtcstate
    this.state = new MyJanusState()
    GlobalJanus.init({
      debug,
      callback: () => {
        this.state.initialized = true
      },
    })
  }
  /* 建立WebRTC媒体通道 */
  open() {
    return createSession(this)
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
