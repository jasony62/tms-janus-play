export const TmsJanusPlayVueMixin = {
  props: { server: { type: String }, file: { type: String } },
  data() {
    return {
      play: { channelState: { webrtcUp: false }, playState: { duration: -1 } },
    }
  },
  methods: {
    connect() {
      this.play.connect(this.server)
    },
    closeConn() {
      this.play.closeConn()
    },
    createWebrtc() {
      this.play.createWebrtc(this.server)
    },
    hangupWebrtc() {
      this.play.hangupWebrtc()
    },
    playing() {
      console.debug(' ::: Remote stream is playing :::')
    },
    probeFile() {
      this.play.probe(this.file)
    },
    playFile() {
      this.play.play(this.file)
    },
    pauseFile() {
      this.play.pause()
    },
    resumeFile() {
      this.play.resume()
    },
    stopFile() {
      this.play.stop()
    },
  },
  beforeDestroy() {
    if (this.play) {
      if (this.play.channelState.connected) {
        this.hangupWebrtc()
      }
    }
  },
}
