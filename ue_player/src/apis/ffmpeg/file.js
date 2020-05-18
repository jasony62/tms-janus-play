import { FFMPEG_RTP_TARGET, FFMPEG_API_ADDRESS, FFMPEG_VCODEC } from '../../global'
import baseApi from './base'

const base = `${FFMPEG_API_ADDRESS}/rtp/file`

const api = {
  base,
  play(socketid, path, aport, vport) {
    return this.tmsAxios()
      .get(`${base}/play`, {
        params: { socketid, path, address: FFMPEG_RTP_TARGET, aport, vport, vcodec: FFMPEG_VCODEC },
      })
      .then((rst) => rst.data.result)
  },
}

Object.assign(api, baseApi)

export default api
