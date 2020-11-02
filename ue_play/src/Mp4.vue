<template>
  <div id="dev">
    <div>
      <label>文件：<input type="text" v-model="file"></label>
    </div>
    <div>
      <button @click="openWebrtc" :disabled="play.state.webrtcUp">打开通道</button>
      <button @click="closeWebrtc" :disabled="!play.state.webrtcUp">关闭通道</button>
    </div>
    <div>
      <button @click="playFile">开始播放</button>
      <button @click="pauseFile">暂停播放</button>
      <button @click="resumeFile">恢复播放</button>
      <button @click="stopFile">停止播放</button>
    </div>
    <div>
      <video id="remotevideo" width="320" height="240" autoplay playsinline @playing="playing" />
    </div>
    <div>
      <img id="videoimage" width="320" height="240">
    </div>
  </div>
</template>

<script>
import { PlayMp4 } from './tms_play_mp4'

export default {
  name: 'Mp4',
  data() {
    return {
      file: '/home/janus/media/sine-8k-testsrc2-baseline31-gop10-10s.mp4',
      play: { state: { webrtcUp: false } },
    }
  },
  computed: {},
  methods: {
    openWebrtc() {
      this.play.open()
    },
    closeWebrtc() {
      this.play.close()
    },
    playing() {
      console.debug(' ::: Remote stream is playing :::')
    },
    playFile() {
      this.play.play(this.file)
    },
    pauseFile() {
      this.printScreen()
      this.play.pause()
    },
    resumeFile() {
      this.play.resume()
    },
    stopFile() {
      this.play.stop()
    },
    printScreen() {
      let elemVideo = document.querySelector('#remotevideo')
      let canvas = document.createElement('canvas')
      canvas.width = elemVideo.videoWidth
      canvas.height = elemVideo.videoHeight
      canvas
        .getContext('2d')
        .drawImage(elemVideo, 0, 0, canvas.width, canvas.height)
      let elemImg = document.querySelector('#videoimage')
      elemImg.src = canvas.toDataURL('image/png')
    },
  },
  mounted() {
    this.play = new PlayMp4({
      elemVideo: document.querySelector('#remotevideo'),
    })
  },
}
</script>