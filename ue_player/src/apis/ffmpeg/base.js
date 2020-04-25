import { TmsAxios } from 'tms-vue'

export default {
  tmsAxios() {
    return TmsAxios.ins('ffmpeg-api')
  },
  janusAddress: process.env.VUE_APP_JANUS_ADDRESS,
  stop(cid) {
    return this.tmsAxios()
      .get(`${this.base}/stop`, { params: { cid } })
      .then((rst) => rst.data.result)
  },
  pause(cid) {
    return this.tmsAxios()
      .get(`${this.base}/pause`, { params: { cid } })
      .then((rst) => rst.data.result)
  },
  resume(cid) {
    return this.tmsAxios()
      .get(`${this.base}/resume`, { params: { cid } })
      .then((rst) => rst.data.result)
  },
}
