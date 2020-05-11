import { FFMPEG_RTP_TARGET, FFMPEG_API_ADDRESS } from '../../global'
import baseApi from './base'

const base = `${FFMPEG_API_ADDRESS}/rtp/file`

const api = {
  base,
  play(socketid, path, aport, vport) {
    return this.tmsAxios()
      .get(`${base}/play`, { params: { socketid, path, address: FFMPEG_RTP_TARGET, aport, vport } })
      .then((rst) => rst.data.result)
  },
}

Object.assign(api, baseApi)

export default api
