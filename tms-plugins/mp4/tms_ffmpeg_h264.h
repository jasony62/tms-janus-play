#ifndef TMS_H264_H
#define TMS_H264_H

#include "tms_ffmpeg_mp4.h"
#include "tms_ffmpeg_stream.h"

/**/
typedef struct TmsVideoRtpContext
{
  uint32_t timestamp;
  uint32_t base_timestamp;
  uint32_t cur_timestamp;
  int8_t payload_type;
  int max_payload_size;
  /* buffer for output */
  uint8_t *buf;
  uint8_t *buf_ptr;
  // int nal_length_size;
  int buffered_nals;
  int flags;
} TmsVideoRtpContext;

int tms_init_video_rtp_context(TmsVideoRtpContext *rtp_ctx, uint8_t *video_buf, uint32_t base_timestamp);
int tms_handle_video_packet(TmsPlayContext *play, TmsInputStream *ist, AVPacket *pkt, AVBSFContext *h264bsfc, TmsVideoRtpContext *rtp_ctx);

/* 初始化视频rtp发送上下文 */
int tms_init_video_rtp_context(TmsVideoRtpContext *rtp_ctx, uint8_t *video_buf, uint32_t base_timestamp)
{
  rtp_ctx->buf = video_buf;
  rtp_ctx->max_payload_size = 1400;
  rtp_ctx->buffered_nals = 0;
  rtp_ctx->flags = 0;

  rtp_ctx->cur_timestamp = 0;
  rtp_ctx->base_timestamp = base_timestamp;

  rtp_ctx->payload_type = H264_PAYLOAD_TYPE;

  return 0;
}
/* 发送1帧RTP */
static void tms_rtp_send_video_frame(TmsVideoRtpContext *rtp_ctx, const uint8_t *buf1, int len, int m, TmsPlayContext *play)
{
  janus_callbacks *gateway = play->gateway;
  janus_plugin_session *handle = play->handle;

  rtp_ctx->timestamp = rtp_ctx->cur_timestamp;
  int16_t seq = play->nb_before_video_rtps + play->nb_video_rtps + 1;

  char *buffer = g_malloc0(1500); // 1个包最大的采样数是多少？

  /* 设置RTP头 */
  janus_rtp_header *header = (janus_rtp_header *)buffer;
  header->version = 2;
  header->markerbit = m;
  header->type = rtp_ctx->payload_type;
  header->seq_number = htons(seq);
  header->timestamp = htonl(rtp_ctx->timestamp);
  header->ssrc = htonl(1); /* The gateway will fix this anyway */

  memcpy(buffer + RTP_HEADER_SIZE, buf1, len);

  uint16_t length = RTP_HEADER_SIZE + len;

  janus_plugin_rtp janus_rtp = {.video = TRUE, .buffer = (char *)buffer, .length = length};
  gateway->relay_rtp(handle, &janus_rtp);

  play->nb_video_rtps++;

  g_free(buffer);

  JANUS_LOG(LOG_VERB, "完成第 %d 个视频RTP帧发送 seq=%d timestamp=%d\n", play->nb_video_rtps, seq, rtp_ctx->timestamp);
}
/* 将多个nal缓存起来一起发送 */
static void tms_flush_nal_buffered(TmsVideoRtpContext *rtp_ctx, int last, TmsPlayContext *play)
{
  if (rtp_ctx->buf_ptr != rtp_ctx->buf)
  {
    // If we're only sending one single NAL unit, send it as such, skip
    // the STAP-A/AP framing
    if (rtp_ctx->buffered_nals == 1)
    {
      tms_rtp_send_video_frame(rtp_ctx, rtp_ctx->buf + 3, rtp_ctx->buf_ptr - rtp_ctx->buf - 3, last, play);
    }
    else
    {
      tms_rtp_send_video_frame(rtp_ctx, rtp_ctx->buf, rtp_ctx->buf_ptr - rtp_ctx->buf, last, play);
    }
  }
  rtp_ctx->buf_ptr = rtp_ctx->buf;
  rtp_ctx->buffered_nals = 0;
}
/* 发送nal */
static void tms_send_h264_nal(TmsVideoRtpContext *rtp_ctx, const uint8_t *buf, int size, int last, TmsPlayContext *play)
{
  int nalu_type = buf[0] & 0x1F;
  JANUS_LOG(LOG_VERB, "Sending NAL %x of len %d M=%d\n", nalu_type, size, last);

  if (size <= rtp_ctx->max_payload_size)
  {
    int buffered_size = rtp_ctx->buf_ptr - rtp_ctx->buf;
    int header_size;
    int skip_aggregate = 1;

    header_size = 1;
    //skip_aggregate = rtp_ctx->flags & FF_RTP_FLAG_H264_MODE0;

    // Flush buffered NAL units if the current unit doesn't fit
    if (buffered_size + 2 + size > rtp_ctx->max_payload_size)
    {
      tms_flush_nal_buffered(rtp_ctx, 0, play);
      buffered_size = 0;
    }
    // If we aren't using mode 0, and the NAL unit fits including the
    // framing (2 bytes length, plus 1/2 bytes for the STAP-A/AP marker),
    // write the unit to the buffer as a STAP-A/AP packet, otherwise flush
    // and send as single NAL.
    if (buffered_size + 2 + header_size + size <= rtp_ctx->max_payload_size &&
        !skip_aggregate)
    {
      if (buffered_size == 0)
      {
        *rtp_ctx->buf_ptr++ = 24;
      }
      AV_WB16(rtp_ctx->buf_ptr, size);
      rtp_ctx->buf_ptr += 2;
      memcpy(rtp_ctx->buf_ptr, buf, size);
      rtp_ctx->buf_ptr += size;
      rtp_ctx->buffered_nals++;
    }
    else
    {
      tms_flush_nal_buffered(rtp_ctx, 0, play);
      tms_rtp_send_video_frame(rtp_ctx, buf, size, last, play);
    }
  }
  else
  {
    int flag_byte, header_size;
    tms_flush_nal_buffered(rtp_ctx, 0, play);
    // if (rtp_ctx->flags & FF_RTP_FLAG_H264_MODE0)
    // {
    //   JANUS_LOG(LOG_VERB, "NAL size %d > %d, try -slice-max-size %d\n", size, rtp_ctx->max_payload_size, rtp_ctx->max_payload_size);
    //   return;
    // }
    JANUS_LOG(LOG_VERB, "NAL size %d > %d\n", size, rtp_ctx->max_payload_size);

    uint8_t type = buf[0] & 0x1F;
    uint8_t nri = buf[0] & 0x60;

    rtp_ctx->buf[0] = 28; /* FU Indicator; Type = 28 ---> FU-A */
    rtp_ctx->buf[0] |= nri;
    rtp_ctx->buf[1] = type;
    rtp_ctx->buf[1] |= 1 << 7;
    buf += 1;
    size -= 1;

    flag_byte = 1;
    header_size = 2;

    while (size + header_size > rtp_ctx->max_payload_size)
    {
      memcpy(&rtp_ctx->buf[header_size], buf, rtp_ctx->max_payload_size - header_size);
      tms_rtp_send_video_frame(rtp_ctx, rtp_ctx->buf, rtp_ctx->max_payload_size, 0, play);
      buf += rtp_ctx->max_payload_size - header_size;
      size -= rtp_ctx->max_payload_size - header_size;
      rtp_ctx->buf[flag_byte] &= ~(1 << 7);
    }
    rtp_ctx->buf[flag_byte] |= 1 << 6;
    memcpy(&rtp_ctx->buf[header_size], buf, size);
    tms_rtp_send_video_frame(rtp_ctx, rtp_ctx->buf, size + header_size, last, play);
  }
}

static const uint8_t *tms_avc_find_startcode_internal(const uint8_t *p, const uint8_t *end)
{
  const uint8_t *a = p + 4 - ((intptr_t)p & 3);

  for (end -= 3; p < a && p < end; p++)
  {
    if (p[0] == 0 && p[1] == 0 && p[2] == 1)
      return p;
  }

  for (end -= 3; p < end; p += 4)
  {
    uint32_t x = *(const uint32_t *)p;
    //      if ((x - 0x01000100) & (~x) & 0x80008000) // little endian
    //      if ((x - 0x00010001) & (~x) & 0x00800080) // big endian
    if ((x - 0x01010101) & (~x) & 0x80808080)
    { // generic
      if (p[1] == 0)
      {
        if (p[0] == 0 && p[2] == 1)
          return p;
        if (p[2] == 0 && p[3] == 1)
          return p + 1;
      }
      if (p[3] == 0)
      {
        if (p[2] == 0 && p[4] == 1)
          return p + 2;
        if (p[4] == 0 && p[5] == 1)
          return p + 3;
      }
    }
  }

  for (end += 3; p < end; p++)
  {
    if (p[0] == 0 && p[1] == 0 && p[2] == 1)
      return p;
  }

  return end + 3;
}

static const uint8_t *tms_avc_find_startcode(const uint8_t *p, const uint8_t *end)
{
  const uint8_t *out = tms_avc_find_startcode_internal(p, end);
  if (p < out && out < end && !out[-1])
    out--;
  return out;
}

static void tms_rtp_send_h264(TmsVideoRtpContext *rtp_ctx, const uint8_t *buf1, int size, TmsPlayContext *play)
{
  const uint8_t *r, *end = buf1 + size;

  rtp_ctx->buf_ptr = rtp_ctx->buf;

  r = tms_avc_find_startcode(buf1, end);
  while (r < end)
  {
    const uint8_t *r1;

    while (!*(r++))
      ;
    r1 = tms_avc_find_startcode(r, end);
    tms_send_h264_nal(rtp_ctx, r, r1 - r, r1 == end, play);
    r = r1;
  }

  tms_flush_nal_buffered(rtp_ctx, 1, play);
}
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
int tms_handle_video_packet(TmsPlayContext *play, TmsInputStream *ist, AVPacket *pkt, AVBSFContext *h264bsfc, TmsVideoRtpContext *rtp_ctx)
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
  int64_t dts_us = ist->dts;
  int64_t elapse_us = av_gettime_relative() - play->start_time_us - play->pause_duration_us;
  if (dts_us > elapse_us)
    usleep(dts_us - elapse_us);

  ist->next_dts += av_rescale_q(pkt->duration, ist->st->time_base, AV_TIME_BASE_Q);

  /* 计算时间戳 */
  int64_t video_ts = dts_us; // 微秒
  if (play->pause_duration_us > 0)
  {
    video_ts += play->pause_duration_us;
  }
  rtp_ctx->cur_timestamp = rtp_ctx->base_timestamp + (video_ts / 1000 * 90); // 每毫秒90个采样
  // if (!play->first_rtcp_video)
  // {
  //   tms_video_rtcp_first_sr(play, rtp_ctx);
  // }

  JANUS_LOG(LOG_VERB, "elapse = %ld dts = %ld base_timestamp = %d video_ts = %ld\n", elapse_us, dts_us, rtp_ctx->base_timestamp, video_ts);

  /* 发送RTP包 */
  tms_rtp_send_h264(rtp_ctx, pkt->data, pkt->size, play);

  return 0;
}

#endif