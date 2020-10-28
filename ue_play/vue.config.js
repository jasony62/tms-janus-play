module.exports = {
  publicPath: '/' + process.env.VUE_APP_BASE_URL,
  outputDir: 'dist' + (process.env.VUE_APP_BASE_URL ? `/${process.env.VUE_APP_BASE_URL}` : ''),
  filenameHashing: true,
  pages: {
    dev: {
      entry: 'src/dev.js',
      template: 'public/dev.html',
      filename: './dev.html',
      title: process.env.VUE_APP_TITLE,
      chunks: ['chunk-vendors', 'chunk-common', 'dev'],
    },
  },
  parallel: require('os').cpus().length > 1,
  runtimeCompiler: true,
  devServer: { https: true, port: 8011 },
}
