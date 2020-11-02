export const JANUS_ADDRESS = process.env.VUE_APP_JANUS_ADDRESS

/* Janus REST 接口*/
export const JANUS_SERVER = (() => {
  const address = JANUS_ADDRESS ? JANUS_ADDRESS : window.location.hostname

  let server
  if (window.location.protocol === 'http:') server = `http://${address}:${process.env.VUE_APP_JANUS_HTTP_PORT}/janus`
  else server = `https://${address}:${process.env.VUE_APP_JANUS_HTTPS_PORT}/janus`

  return server
})()
