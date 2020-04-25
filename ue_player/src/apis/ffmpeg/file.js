import baseApi from './base'

const api = {
  base: process.env.VUE_APP_FFMPEG_API_BASE + '/rtp/file',
  play(path, aport, vport) {
    return this.tmsAxios()
      .get(`${base}/play`, { params: { path, address: this.janusAddress, aport, vport } })
      .then((rst) => rst.data.result)
  },
}

Object.assign(api, baseApi)

export default api
