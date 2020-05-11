module.exports = {
  port: 3001,
  https: { port: 3444, key: process.env.ssl_certificate_key, cert: process.env.ssl_certificate },
}
