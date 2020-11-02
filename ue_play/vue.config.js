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
    mp3: {
      entry: 'src/mp3.js',
      template: 'public/index.html',
      filename: './mp3.html',
      title: process.env.VUE_APP_TITLE,
      chunks: ['chunk-vendors', 'chunk-common', 'mp3'],
    },
  },
  parallel: require('os').cpus().length > 1,
  runtimeCompiler: true,
  devServer: { https: true, port: 8011 },
}
