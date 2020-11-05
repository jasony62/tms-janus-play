#ifndef TMS_PLAY_STREAM_H
#define TMS_PLAY_STREAM_H

/* 记录输入媒体流相关数据 */
typedef struct TmsInputStream
{
  int stream_index;
  AVStream *st;
  AVCodecContext *dec_ctx;
  AVCodec *codec;
  int bytes_per_sample;

  int saw_first_ts;

  int64_t start; /* time when read started */
  /* predicted dts of the next packet read for this stream or (when there are several frames in a packet) of the next frame in current packet (in AV_TIME_BASE units) */
  int64_t next_dts;
  int64_t dts; ///< dts of the last packet read for this stream (in AV_TIME_BASE units)
} TmsInputStream;

int tms_init_input_stream(AVFormatContext *fctx, int index, TmsInputStream *ist);

void tms_dump_stream_format(TmsInputStream *ist);

/* 生成自己使用的输入媒体流对象。只支持音频流和视频流。 */
int tms_init_input_stream(AVFormatContext *fctx, int index, TmsInputStream *ist)
{
  int ret;
  AVStream *st;
  AVCodec *codec;
  AVCodecContext *cctx;

  if (!ist)
  {
    JANUS_LOG(LOG_VERB, "没有传递有效参数");
    return -1;
  }

  st = fctx->streams[index];
  codec = avcodec_find_decoder(st->codecpar->codec_id);
  if (!codec)
  {
    JANUS_LOG(LOG_VERB, "stream #%d codec_id = %d 没有找到解码器\n", index, st->codecpar->codec_id);
    return -1;
  }

  if (codec->type != AVMEDIA_TYPE_VIDEO && codec->type != AVMEDIA_TYPE_AUDIO)
  {
    JANUS_LOG(LOG_VERB, "stream #%d codec.type = %d 不支持处理，只支持视频流或音频流\n", index, codec->type);
    return -1;
  }
  if (codec->type == AVMEDIA_TYPE_VIDEO && 0 != strcmp(codec->name, "h264"))
  {
    JANUS_LOG(LOG_VERB, "stream #%d 视频流不是h264格式\n", index);
    return -1;
  }
  // if (codec->type == AVMEDIA_TYPE_AUDIO && 0 != strcmp(codec->name, "aac"))
  // {
  //   JANUS_LOG(LOG_VERB, "stream #%d 音频流不是aac格式\n", index);
  //   return -1;
  // }

  cctx = avcodec_alloc_context3(codec);
  avcodec_parameters_to_context(cctx, st->codecpar);
  if ((ret = avcodec_open2(cctx, codec, NULL)) < 0)
  {
    JANUS_LOG(LOG_VERB, "stream #%d 读取媒体流基本信息势失败 %s\n", index, av_err2str(ret));
    return -1;
  }

  memset(ist, 0, sizeof(TmsInputStream));
  ist->stream_index = index;
  ist->st = st;
  ist->dec_ctx = cctx;
  ist->codec = codec;
  ist->bytes_per_sample = av_get_bytes_per_sample(cctx->sample_fmt);
  ist->start = AV_NOPTS_VALUE;
  ist->next_dts = AV_NOPTS_VALUE;
  ist->dts = AV_NOPTS_VALUE;

  return 0;
}

/* 输出媒体流格式信息 */
void tms_dump_stream_format(TmsInputStream *ist)
{
  AVStream *st = ist->st;
  AVCodecContext *cctx = ist->dec_ctx;
  AVCodec *codec = ist->codec;

  JANUS_LOG(LOG_VERB, "媒体流 #%d is %s\n", st->index, codec->type == AVMEDIA_TYPE_VIDEO ? "video" : "audio");
  JANUS_LOG(LOG_VERB, "-- codec.name %s\n", codec->name);

  /* 音频采样信息 */
  if (codec->type == AVMEDIA_TYPE_AUDIO)
  {
    JANUS_LOG(LOG_VERB, "-- ccxt.sample_fmt = %s\n", av_get_sample_fmt_name(cctx->sample_fmt));
    JANUS_LOG(LOG_VERB, "-- ccxt.sample_rate = %d\n", cctx->sample_rate);
    JANUS_LOG(LOG_VERB, "-- ccxt.bytes_per_sample = %d\n", av_get_bytes_per_sample(cctx->sample_fmt));
    JANUS_LOG(LOG_VERB, "-- ccxt.channels = %d\n", cctx->channels);
    char buf[64];
    av_get_channel_layout_string(buf, sizeof(buf), cctx->channels, cctx->channel_layout);
    JANUS_LOG(LOG_VERB, "-- ccxt.channel_layout = %s\n", buf);
  }

  JANUS_LOG(LOG_VERB, "-- stream.avg_frame_rate(%d, %d)\n", st->avg_frame_rate.num, st->avg_frame_rate.den);
  JANUS_LOG(LOG_VERB, "-- stream.tbn(%d, %d)\n", st->time_base.num, st->time_base.den);
  JANUS_LOG(LOG_VERB, "-- stream.duration = %s\n", av_ts2str(st->duration));
}

#endif