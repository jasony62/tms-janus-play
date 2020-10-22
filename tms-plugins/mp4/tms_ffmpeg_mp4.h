#ifndef TMS_FFMPEG_MP4_H
#define TMS_FFMPEG_MP4_H

/* 记录播放的过程和状态 */
typedef struct TmsPlayContext
{
  /* 时间 */
  int64_t start_time_us;     // 微秒
  int64_t end_time_us;       // 微秒
  int64_t pause_duration_us; // 微秒
  /* 计数器 */
  int nb_packets;
  int nb_video_packets;
  int nb_audio_packets;
  int nb_audio_frames;
  int nb_pcma_frames;
} TmsPlayContext;

int tms_ffmpeg_mp4_main(char *filename);

#endif