#define H264_PAYLOAD_TYPE 96

#ifndef TMS_FFMPEG_MP4_H
#define TMS_FFMPEG_MP4_H

#include <rtp.h>

/* 记录单次Webrtc连接播放的过程和状态 */
typedef struct tms_mp4_ffmpeg
{
  char *filename; // 要播放的文件
  janus_plugin_session *handle;
  janus_refcount ref;
  janus_mutex mutex;
  volatile gint webrtcup;  // Webrtc连接是否可用，只有可用时才可以播放，0：不可用，1：可用
  volatile gint playing;   // 播放状态，0：停止，1：播放，2：暂停
  volatile gint destroyed; // 如果session已不可用，ffmpeg应处于销毁状态
  /* 保留播放状态 */
  int nb_video_rtps; // 视频rtp包累计发送数量，解决多次播放，生成seq的问题
  int nb_audio_rtps; // 音频rtp包累计发送数量，解决多次播放，生成seq的问题
} tms_mp4_ffmpeg;

/* 记录单次播放的过程和状态 */
typedef struct TmsPlayContext
{
  /* 时间 */
  int64_t start_time_us;     // 播放开始时间，微秒
  int64_t end_time_us;       // 播放结束时间，微秒
  int64_t pause_duration_us; // 暂停状态持续的时间，微秒
  /* 计数器 */
  int nb_packets;       // 累计读取的包数量
  int nb_video_packets; // 累计读取的视频包数量
  int nb_audio_packets; // 累计读取的音频包数量
  int nb_audio_frames;  // 累计读取的音频帧数量（mp4文件中的原始编码）
  int nb_pcma_frames;   // 累计转码的音频帧数量（转换为pcma）
  /* rtp */
  int nb_video_rtps;        // 本次播放累计发送的视频rtp包数量
  int nb_before_video_rtps; // 已经发送的视频rtp包数量，解决seq问题
  int nb_audio_rtps;        // 本次播放累计发送的音频rtp包数量
  int nb_before_audio_rtps; // 已经发送的音频rtp包数量，解决seq问题
  /* janus */
  janus_callbacks *gateway;
  janus_plugin_session *handle;
} TmsPlayContext;

int tms_ffmpeg_mp4_main(janus_callbacks *gateway, janus_plugin_session *handle, tms_mp4_ffmpeg *ffmpeg);

#endif