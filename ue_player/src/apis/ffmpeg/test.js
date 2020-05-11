import { FFMPEG_RTP_TARGET, FFMPEG_API_ADDRESS } from '../../global'
import baseApi from './base'

const base = `${FFMPEG_API_ADDRESS}/rtp/test`

const api = {
  base,
  play(socketid, aport, vport, duration) {
    const params = { socketid, address: FFMPEG_RTP_TARGET, aport, vport }
    if (parseInt(duration)) params.duration = duration
    return this.tmsAxios()
      .get(`${this.base}/play`, { params })
      .then((rst) => rst.data.result)
  },
}

Object.assign(api, baseApi)

export default api
