export const PlayVueMixin = {
  props: { server: { type: String }, file: { type: String } },
  data() {
    return {
      play: { channelState: { webrtcUp: false }, playState: { duration: -1 } },
    }
  },
  methods: {
    openWebrtc() {
      this.play.open(this.server)
    },
    closeWebrtc() {
      this.play.close()
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
        this.closeWebrtc()
      }
    }
  },
}
