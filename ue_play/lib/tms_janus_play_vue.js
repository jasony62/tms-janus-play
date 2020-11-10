export const TmsJanusPlayVueMixin = {
  props: { server: { type: String }, file: { type: String } },
  data() {
    return {
      play: { channelState: { webrtcUp: false }, playState: { duration: -1 } },
    }
  },
  computed: {
    runActionLabel: function() {
      let { state } = this.play.playState
      return state === 'paused' ? '恢复' : state === 'going' ? '暂停' : '播放'
    },
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
    probe() {
      this.play.probe(this.file)
    },
    control() {
      this.play.control(this.file)
    },
    stop() {
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
