module.exports = {
  appenders: {
    consoleout: { type: 'console' },
  },
  categories: {
    default: { appenders: ['consoleout'], level: 'debug' },
  },
  pm2: true,
}
