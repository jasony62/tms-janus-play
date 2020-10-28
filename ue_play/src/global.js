export const JANUS_ADDRESS = process.env.VUE_APP_JANUS_ADDRESS

export const JANUS_SERVER = (() => {
  const address = JANUS_ADDRESS ? JANUS_ADDRESS : window.location.hostname

  let server
  if (window.location.protocol === 'http:') server = `http://${address}:${process.env.VUE_APP_JANUS_HTTP_PORT}/janus`
  else server = `https://${address}:${process.env.VUE_APP_JANUS_HTTPS_PORT}/janus`

  return server
})()

const FFMPEG_ADDRESS = `${process.env.VUE_APP_FFMPEG_PROTOCOL}://${process.env.VUE_APP_FFMPEG_HOSTNAME ||
  window.location.hostname}`
export const FFMPEG_API_ADDRESS = `${FFMPEG_ADDRESS}:${process.env.VUE_APP_FFMPEG_API_PORT}/${process.env.VUE_APP_FFMPEG_API_PATH}`

export const FFMPEG_PUSH_ADDRESS = `${FFMPEG_ADDRESS}:${process.env.VUE_APP_FFMPEG_PUSH_PORT}`

export const FFMPEG_RTP_TARGET = process.env.VUE_APP_FFMPEG_RTP_TARGET

export const FFMPEG_VCODEC = process.env.VUE_APP_FFMPEG_VCODEC
