module.exports = {
  publicPath: '/' + process.env.VUE_APP_BASE_URL,
  outputDir: 'dist' + (process.env.VUE_APP_BASE_URL ? `/${process.env.VUE_APP_BASE_URL}` : ''),
  filenameHashing: true,
  pages: {
    mp4: {
      entry: 'src/mp4.js',
      template: 'public/index.html',
      filename: './mp4.html',
      title: process.env.VUE_APP_TITLE,
      chunks: ['chunk-vendors', 'chunk-common', 'mp4'],
    },
    audio: {
      entry: 'src/audio.js',
      template: 'public/index.html',
      filename: './audio.html',
      title: process.env.VUE_APP_TITLE,
      chunks: ['chunk-vendors', 'chunk-common', 'audio'],
    },
  },
  parallel: require('os').cpus().length > 1,
  runtimeCompiler: true,
  devServer: { https: true, port: 8011 },
}
