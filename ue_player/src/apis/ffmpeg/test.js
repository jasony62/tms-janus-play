import baseApi from './base'

const api = {
  base: process.env.VUE_APP_FFMPEG_API_BASE + '/rtp/test',
  play(aport, vport) {
    return this.tmsAxios()
      .get(`${this.base}/play`, { params: { address: this.janusAddress, aport, vport } })
      .then((rst) => rst.data.result)
  },
}

Object.assign(api, baseApi)

export default api
