import { FFMPEG_RTP_TARGET, FFMPEG_API_ADDRESS, FFMPEG_VCODEC } from '../../global'
import baseApi from './base'

const base = `${FFMPEG_API_ADDRESS}/rtp/file`

const api = {
  base,
  play(socketid, path, aport, vport) {
    const params = { socketid, path, address: FFMPEG_RTP_TARGET }
    if (typeof FFMPEG_VCODEC === 'string') params.vcodec = FFMPEG_VCODEC
    if (parseInt(aport)) params.aport = aport
    if (parseInt(vport)) params.vport = vport

    return this.tmsAxios()
      .get(`${base}/play`, { params })
      .then((rst) => rst.data.result)
  },
}

Object.assign(api, baseApi)

export default api
