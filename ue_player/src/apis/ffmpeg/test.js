import baseApi from './base'

const api = {
  base: process.env.VUE_APP_FFMPEG_API_BASE + '/rtp/test',
  play(socketid, aport, vport, duration) {
    const params = { socketid, address: this.janusAddress, aport, vport }
    if (parseInt(duration)) params.duration = duration
    return this.tmsAxios()
      .get(`${this.base}/play`, { params })
      .then((rst) => rst.data.result)
  },
}

Object.assign(api, baseApi)

export default api
