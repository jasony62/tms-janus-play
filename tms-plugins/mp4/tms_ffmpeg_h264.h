#ifndef TMS_H264_H
#define TMS_H264_H

#include "tms_ffmpeg_mp4.h"
#include "tms_ffmpeg_stream.h"

int tms_handle_video_packet(TmsPlayContext *play, TmsInputStream *ist, AVPacket *pkt, AVBSFContext *h264bsfc);

/* 输出调试信息 */
static void tms_dump_video_packet(AVPacket *pkt, TmsPlayContext *play)
{
  int64_t dts = pkt->dts == INT64_MIN ? -1 : pkt->dts;
  int64_t pts = pkt->pts == INT64_MIN ? -1 : pkt->pts;
  uint8_t *pkt_data = pkt->data;

  JANUS_LOG(LOG_VERB, "读取媒体包 #%d 所属媒体流 #%d size= %d dts = %" PRId64 " pts = %" PRId64 "\n", play->nb_video_packets, pkt->stream_index, pkt->size, dts, pts);
  if (play->nb_video_packets < 8)
  {
    JANUS_LOG(LOG_VERB, "av_read_frame.packet 前12个字节 %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n", pkt_data[0], pkt_data[1], pkt_data[2], pkt_data[3], pkt_data[4], pkt_data[5], pkt_data[6], pkt_data[7], pkt_data[8], pkt_data[9], pkt_data[10], pkt_data[11]);
  }

  int nal_unit_type = pkt_data[4] & 0x1f; // 5 bit
  JANUS_LOG(LOG_VERB, "媒体包 #%d 视频包 #%d nal_unit_type = %d\n", play->nb_packets, play->nb_video_packets, nal_unit_type);
}

/* 处理视频媒体包 */
int tms_handle_video_packet(TmsPlayContext *play, TmsInputStream *ist, AVPacket *pkt, AVBSFContext *h264bsfc)
{
  int ret = 0;

  play->nb_video_packets++;
  /*将avcc格式转为annexb格式*/
  if ((ret = av_bsf_send_packet(h264bsfc, pkt)) < 0)
  {
    JANUS_LOG(LOG_VERB, "av_bsf_send_packet error");
    return -1;
  }
  while ((ret = av_bsf_receive_packet(h264bsfc, pkt)) == 0)
    ;

  tms_dump_video_packet(pkt, play);

  /* 处理视频包 */
  if (!ist->saw_first_ts)
  {
    ist->dts = ist->st->avg_frame_rate.num ? -ist->dec_ctx->has_b_frames * AV_TIME_BASE / av_q2d(ist->st->avg_frame_rate) : 0;
    ist->next_dts = ist->dts;
    ist->saw_first_ts = 1;
  }
  else
  {
    ist->dts = ist->next_dts;
  }

  /* 添加发送间隔 */
  int64_t dts = ist->dts;
  int64_t elapse = av_gettime_relative() - play->start_time_us - play->pause_duration_us;
  if (dts > elapse)
    usleep(dts - elapse);

  // ist->next_dts += av_rescale_q(pkt->duration, ist->st->time_base, AV_TIME_BASE_Q);

  // /* 指定帧时间戳，单位是毫秒 */
  // int64_t video_ts = video_rtp_ctx->base_timestamp + dts; // 微秒
  // /**
  //  * int h264_sample_rate = (int)ast_format_get_sample_rate(ast_format_h264);
  //  * asterisk中，h264的sample_rate取到的值是1000，实际上应该是90000，需要在设置ts的时候修正
  //  */
  // video_rtp_ctx->cur_timestamp = video_ts / 1000 * (RTP_H264_TIME_BASE / 1000); // 毫秒
  // if (!play->first_rtcp_video)
  // {
  //   tms_video_rtcp_first_sr(play, video_rtp_ctx);
  // }

  // JANUS_LOG(LOG_VERB, "elapse = %ld dts = %ld base_timestamp = %d video_ts = %ld\n", elapse, dts, video_rtp_ctx->base_timestamp, video_ts);

  /* 发送RTP包 */
  // ff_rtp_send_h264(video_rtp_ctx, pkt->data, pkt->size, play);

  return 0;
}

#endif