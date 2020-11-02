<template>
  <div>
    <div>
      <label>音频文件(mp3或wav)：<input type="text" v-model="audioFile"></label>
    </div>
    <div>
      <tms-janus-audio :server="server" :file="audioFile"></tms-janus-audio>
    </div>
    <hr>
    <div>
      <label>MP4文件：<input type="text" v-model="mp4File"></label>
    </div>
    <div>
      <tms-janus-mp4 :server="server" :file="mp4File"></tms-janus-mp4>
    </div>
  </div>
</template>

<script>
import { JANUS_SERVER as server } from './global'
import { TmsJanusAudio, TmsJanusMp4 } from '../lib'

export default {
  name: 'Demo',
  data() {
    return {
      server,
      audioFile: '/home/janus/media/sine-8k-10s.mp3',
      mp4File: '/home/janus/media/sine-8k-testsrc2-baseline31-gop10-10s.mp4',
    }
  },
  components: { TmsJanusAudio, TmsJanusMp4 },
}
</script>