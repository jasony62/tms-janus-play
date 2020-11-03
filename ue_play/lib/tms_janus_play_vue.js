export const PlayVueMixin = {
  props: { server: { type: String }, file: { type: String } },
  data() {
    return {
      play: { state: { webrtcUp: false } },
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
      if (this.play.state.webrtcUp) {
        this.closeWebrtc()
      }
    }
  },
}
