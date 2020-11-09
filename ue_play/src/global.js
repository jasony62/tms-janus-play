export const JANUS_ADDRESS = process.env.VUE_APP_TMS_JANUS_ADDRESS

/* Janus REST 接口*/
export const JANUS_SERVER = (() => {
  const address = JANUS_ADDRESS ? JANUS_ADDRESS : window.location.hostname

  let server
  if (window.location.protocol === 'http:') {
    server = `http://${address}`
    if (parseInt(process.env.VUE_APP_TMS_JANUS_HTTP_PORT) > 0)
      server += `:${parseInt(process.env.VUE_APP_TMS_JANUS_HTTP_PORT)}`
  } else {
    server = `https://${address}`
    if (parseInt(process.env.VUE_APP_TMS_JANUS_HTTPS_PORT) > 0)
      server += `:${parseInt(process.env.VUE_APP_TMS_JANUS_HTTPS_PORT)}`
  }
  server += '/janus'

  return server
})()
