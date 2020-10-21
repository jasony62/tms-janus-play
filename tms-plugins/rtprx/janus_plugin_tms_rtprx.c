#include <errno.h>
#include <netdb.h>
#include <sys/poll.h>
#include <sys/socket.h>
#include <sys/time.h>

#include <jansson.h>

#include <apierror.h>
#include <config.h>
#include <debug.h>
#include <mutex.h>
#include <plugins/plugin.h>

#include <ip-utils.h>
#include <record.h>
#include <rtcp.h>
#include <rtp.h>
#include <utils.h>

#define TMS_JANUS_PLUGIN_RTPRX_VERSION 1
#define TMS_JANUS_PLUGIN_RTPRX_VERSION_STRING "0.0.1"
#define TMS_JANUS_PLUGIN_RTPRX_DESCRIPTION "receive rtp media stream"
#define TMS_JANUS_PLUGIN_RTPRX_NAME "RtpRX"
#define TMS_JANUS_PLUGIN_RTPRX_AUTHOR "Jason Young"
#define TMS_JANUS_PLUGIN_RTPRX_PACKAGE "janus.plugin.tms.rtprx"

janus_plugin *create(void);
int janus_plugin_init_tms_rtprx(janus_callbacks *callback, const char *config_path);
void janus_plugin_destroy_tms_rtprx(void);
int janus_plugin_get_api_compatibility_tms_rtprx(void);
int janus_plugin_get_version_tms_rtprx(void);
const char *janus_plugin_get_version_string_tms_rtprx(void);
const char *janus_plugin_get_description_tms_rtprx(void);
const char *janus_plugin_get_name_tms_rtprx(void);
const char *janus_plugin_get_author_tms_rtprx(void);
const char *janus_plugin_get_package_tms_rtprx(void);
void janus_plugin_create_session_tms_rtprx(janus_plugin_session *handle, int *error);
struct janus_plugin_result *janus_plugin_handle_message_tms_rtprx(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);
void janus_plugin_setup_media_tms_rtprx(janus_plugin_session *handle);
void janus_plugin_hangup_media_tms_rtprx(janus_plugin_session *handle);
void janus_plugin_destroy_session_tms_rtprx(janus_plugin_session *handle, int *error);
json_t *janus_plugin_query_session_tms_rtprx(janus_plugin_session *handle);

static janus_plugin janus_plugin_tms_rtprx =
    JANUS_PLUGIN_INIT(
            .init = janus_plugin_init_tms_rtprx,
            .destroy = janus_plugin_destroy_tms_rtprx,

            .get_api_compatibility = janus_plugin_get_api_compatibility_tms_rtprx,
            .get_version = janus_plugin_get_version_tms_rtprx,
            .get_version_string = janus_plugin_get_version_string_tms_rtprx,
            .get_description = janus_plugin_get_description_tms_rtprx,
            .get_name = janus_plugin_get_name_tms_rtprx,
            .get_author = janus_plugin_get_author_tms_rtprx,
            .get_package = janus_plugin_get_package_tms_rtprx,

            .create_session = janus_plugin_create_session_tms_rtprx,
            .handle_message = janus_plugin_handle_message_tms_rtprx,
            .setup_media = janus_plugin_setup_media_tms_rtprx,
            .hangup_media = janus_plugin_hangup_media_tms_rtprx,
            .destroy_session = janus_plugin_destroy_session_tms_rtprx,
            .query_session = janus_plugin_query_session_tms_rtprx, );

static void janus_streaming_hangup_media_internal(janus_plugin_session *handle);

static janus_callbacks *gateway = NULL;
/**
 * 媒体信息
 */
/* RTP range to use for random ports */
#define DEFAULT_RTP_RANGE_MIN 10000
#define DEFAULT_RTP_RANGE_MAX 60000
static uint16_t rtp_range_min = DEFAULT_RTP_RANGE_MIN;
static uint16_t rtp_range_max = DEFAULT_RTP_RANGE_MAX;
static uint16_t rtp_range_slider = DEFAULT_RTP_RANGE_MIN;
static uint8_t acodec = 0;
static char *artpmap = NULL, *afmtp = NULL;
static uint8_t vcodec = 0;
static char *vrtpmap = NULL, *vfmtp = NULL;

static janus_mutex fd_mutex = JANUS_MUTEX_INITIALIZER;

/* used for audio/video fd and RTCP fd */
typedef struct multiple_fds
{
  int fd;
  int rtcp_fd;
} multiple_fds;

typedef struct tms_rtprx_rtp_keyframe
{
  gboolean enabled;
  /* If enabled, we store the packets of the last keyframe, to immediately send them for new viewers */
  GList *latest_keyframe;
  /* This is where we store packets while we're still collecting the whole keyframe */
  GList *temp_keyframe;
  guint32 temp_ts;
  janus_mutex mutex;
} tms_rtprx_rtp_keyframe;
typedef struct tms_rtprx_rtp_source
{
  gint audio_port, remote_audio_port;
  gint audio_rtcp_port, remote_audio_rtcp_port;
  in_addr_t audio_mcast;
  gint video_port[3], remote_video_port;
  gint video_rtcp_port, remote_video_rtcp_port;
  in_addr_t video_mcast;
  gint data_port;
  janus_recorder *arc;   /* The Janus recorder instance for this streams's audio, if enabled */
  janus_recorder *vrc;   /* The Janus recorder instance for this streams's video, if enabled */
  janus_recorder *drc;   /* The Janus recorder instance for this streams's data, if enabled */
  janus_mutex rec_mutex; /* Mutex to protect the recorders from race conditions */
  janus_rtp_switching_context context[3];
  int audio_fd;
  int video_fd[3];
  int data_fd;
  int pipefd[2]; /* Just needed to quickly interrupt the poll when it's time to wrap up */
  int audio_rtcp_fd;
  int video_rtcp_fd;
  gboolean simulcast;
  gboolean svc;
  gboolean askew, vskew;
  gint64 last_received_audio;
  gint64 last_received_video;
  gint64 last_received_data;
  uint32_t audio_ssrc;       /* Only needed for fixing outgoing RTCP packets */
  uint32_t video_ssrc;       /* Only needed for fixing outgoing RTCP packets */
  volatile gint need_pli;    /* Whether we need to send a PLI later */
  volatile gint sending_pli; /* Whether we're currently sending a PLI */
  gint64 pli_latest;         /* Time of latest sent PLI (to avoid flooding) */
  uint32_t lowest_bitrate;   /* Lowest bitrate received by viewers via REMB since last update */
  gint64 remb_latest;        /* Time of latest sent REMB (to avoid flooding) */
  struct sockaddr_storage audio_rtcp_addr, video_rtcp_addr;
  // #ifdef HAVE_LIBCURL
  // 	gboolean rtsp;
  // 	CURL *curl;
  // 	janus_streaming_buffer *curldata;
  // 	char *rtsp_url;
  // 	char *rtsp_username, *rtsp_password;
  // 	int ka_timeout;
  // 	char *rtsp_ahost, *rtsp_vhost;
  // 	gboolean reconnecting;
  // 	gint64 reconnect_timer;
  // 	janus_mutex rtsp_mutex;
  // #endif
  tms_rtprx_rtp_keyframe keyframe;
  gboolean textdata;
  gboolean buffermsg;
  int rtp_collision;
  void *last_msg;
  janus_mutex buffermsg_mutex;
  janus_network_address audio_iface;
  janus_network_address video_iface;
  janus_network_address data_iface;
  // /* Only needed for SRTP forwarders */
  // gboolean is_srtp;
  // int srtpsuite;
  // char *srtpcrypto;
  // srtp_t srtp_ctx;
  // srtp_policy_t srtp_policy;
} tms_rtprx_rtp_source;
typedef struct tms_rtprx_codecs
{
  gint audio_pt;
  char *audio_rtpmap;
  char *audio_fmtp;
  janus_videocodec video_codec;
  gint video_pt;
  char *video_rtpmap;
  char *video_fmtp;
} tms_rtprx_codecs;
typedef struct tms_rtprx_mountpoint
{
  guint64 id;
  gboolean enabled;
  gboolean active;
  void *source; /* Can differ according to the source type */
  GDestroyNotify source_destroy;
  tms_rtprx_codecs codecs;
  gboolean audio, video, data;
  void *viewer; /* 关联的session */
  volatile gint destroyed;
  janus_mutex mutex;
  janus_refcount ref;
} tms_rtprx_mountpoint;

static tms_rtprx_mountpoint *tms_rtprx_create_rtp_mountpoint(gboolean doaudio, gboolean dovideo);

/* Static configuration instance */
static janus_config *config = NULL;
/**
 * 插件状态 
 */
static volatile gint initialized = 0;
/**
 * 插件会话 
 */
typedef struct tms_rtprx_session
{
  janus_plugin_session *handle;
  tms_rtprx_mountpoint *mountpoint;
  gint64 sdp_sessid;
  gint64 sdp_version;
  gboolean started;
  gboolean paused;
  gboolean audio, video, data; /* Whether audio, video and/or data must be sent to this listener */
  janus_rtp_switching_context context;
  janus_rtp_simulcasting_context sim_context;
  janus_vp8_simulcast_context vp8_context;
  /* The following are only relevant the mountpoint is VP9-SVC, and are not to be confused with VP8
	 * simulcast, which has similar info (substream/templayer) but in a completely different context */
  int spatial_layer, target_spatial_layer;
  gint64 last_spatial_layer[3];
  int temporal_layer, target_temporal_layer;
  gboolean stopping;
  volatile gint renegotiating;
  volatile gint hangingup;
  volatile gint destroyed;
  janus_refcount ref;
} tms_rtprx_session;
// 会话表
static GHashTable *sessions;
static janus_mutex sessions_mutex = JANUS_MUTEX_INITIALIZER;
/**
 * 消息
 */
static GThread *message_handle_thread;
typedef struct tms_rtprx_message
{
  janus_plugin_session *handle;
  char *transaction;
  json_t *message;
  json_t *jsep;
} tms_rtprx_message;
static GAsyncQueue *messages = NULL;
static tms_rtprx_message exit_message;

static void *tms_rtprx_rtp_relay_thread(void *data);

static void tms_rtprx_session_destroy(tms_rtprx_session *session)
{
  if (session && g_atomic_int_compare_and_exchange(&session->destroyed, 0, 1))
    janus_refcount_decrease(&session->ref);
  JANUS_LOG(LOG_VERB, "[Rtprx] 完成销毁会话\n");
}
static void tms_rtprx_session_free(const janus_refcount *session_ref)
{
  tms_rtprx_session *session = janus_refcount_containerof(session_ref, tms_rtprx_session, ref);
  JANUS_LOG(LOG_VERB, "[Rtprx] 开始释放会话[%p][%p]\n", session, session->mountpoint);
  /* Remove the reference to the core plugin session */
  janus_refcount_decrease(&session->handle->ref);
  /* This session can be desftroyed, free all the resources */
  g_free(session);
  JANUS_LOG(LOG_VERB, "[Rtprx] 完成释放会话\n");
}
/**
 * 在插件会话表中查找会话 
 */
static tms_rtprx_session *tms_rtprx_lookup_session(janus_plugin_session *handle)
{
  tms_rtprx_session *session = NULL;
  if (g_hash_table_contains(sessions, handle))
  {
    session = (tms_rtprx_session *)handle->plugin_handle;
  }
  return session;
}
/**
 * 释放异步消息 
 */
static void tms_rtprx_message_free(tms_rtprx_message *msg)
{
  JANUS_LOG(LOG_VERB, "[Rtprx] 开始释放异步消息\n");
  if (!msg || msg == &exit_message)
    return;

  if (msg->handle && msg->handle->plugin_handle)
  {
    tms_rtprx_session *session = (tms_rtprx_session *)msg->handle->plugin_handle;
    janus_refcount_decrease(&session->ref);
  }
  msg->handle = NULL;

  g_free(msg->transaction);
  msg->transaction = NULL;
  if (msg->message)
    json_decref(msg->message);
  msg->message = NULL;
  if (msg->jsep)
    json_decref(msg->jsep);
  msg->jsep = NULL;

  g_free(msg);

  JANUS_LOG(LOG_VERB, "[Rtprx] 完成释放异步消息\n");
}
/**
 * 销毁播放源
 */
static void tms_rtprx_mountpoint_destroy(tms_rtprx_mountpoint *mp)
{
  JANUS_LOG(LOG_VERB, "[Rtprx] 开始销毁挂载点 %p\n", mp);
  if (!mp)
    return;
  if (!g_atomic_int_compare_and_exchange(&mp->destroyed, 0, 1))
    return;

  /* If this is an RTP source, interrupt the poll */
  // tms_rtprx_rtp_source *source = mountpoint->source;
  // if (source != NULL && source->pipefd[1] > 0)
  // {
  //     int code = 1;
  //     ssize_t res = 0;
  //     do
  //     {
  //         res = write(source->pipefd[1], &code, sizeof(int));
  //     } while (res == -1 && errno == EINTR);
  // }

  JANUS_LOG(LOG_VERB, "[Rtprx] 完成销毁挂载点 %p\n", mp);
}
/**
 * 释放播放源资源
 */
static void tms_rtprx_mountpoint_free(const janus_refcount *mp_ref)
{
  tms_rtprx_mountpoint *mp = janus_refcount_containerof(mp_ref, tms_rtprx_mountpoint, ref);
  JANUS_LOG(LOG_VERB, "[Rtprx] 开始释放挂载点 %p\n", mp);

  if (mp->source != NULL && mp->source_destroy != NULL)
  {
    mp->source_destroy(mp->source);
  }

  g_free(mp->codecs.audio_rtpmap);
  g_free(mp->codecs.audio_fmtp);
  g_free(mp->codecs.video_rtpmap);
  g_free(mp->codecs.video_fmtp);

  g_free(mp);

  JANUS_LOG(LOG_VERB, "[Rtprx] 完成释放挂载点\n");
}
/**
 * 异步消息处理 
 */
static void *tms_rtprx_async_message_thread(void *data)
{
  JANUS_LOG(LOG_VERB, "[Rtprx] 启动异步消息处理线程\n");
  tms_rtprx_message *msg = NULL;
  json_t *root = NULL;
  while (g_atomic_int_get(&initialized))
  {
    msg = g_async_queue_pop(messages);
    if (msg == &exit_message)
      break;
    if (msg->handle == NULL)
    {
      tms_rtprx_message_free(msg);
      continue;
    }
    janus_mutex_lock(&sessions_mutex);
    tms_rtprx_session *session = tms_rtprx_lookup_session(msg->handle);
    janus_refcount_increase(&session->ref);
    janus_mutex_unlock(&sessions_mutex);

    root = msg->message;
    const char *sdp_type = NULL;
    char *sdp = NULL;

    json_t *request = json_object_get(root, "request");
    const char *request_text = json_string_value(request);
    json_t *audio = json_object_get(root, "audio");
    gboolean doaudio = audio ? json_is_true(audio) : TRUE;
    json_t *video = json_object_get(root, "video");
    gboolean dovideo = video ? json_is_true(video) : TRUE;
    json_t *result = NULL; // 返回结果

    if (!strcasecmp(request_text, "create.webrtc"))
    {
      session->stopping = FALSE;
      session->audio = doaudio; /* True by default */
      session->video = dovideo; /* True by default */
      session->sdp_version = 1; /* This needs to be increased when it changes */
      session->sdp_sessid = janus_get_real_time();
      sdp_type = "offer"; /* We're always going to do the offer ourselves, never answer */
      char sdptemp[2048];
      memset(sdptemp, 0, 2048);
      gchar buffer[512];
      memset(buffer, 0, 512);
      g_snprintf(buffer, 512,
                 "v=0\r\no=%s %" SCNu64 " %" SCNu64 " IN IP4 127.0.0.1\r\n",
                 "-", session->sdp_sessid, session->sdp_version);
      g_strlcat(sdptemp, buffer, 2048);
      g_snprintf(buffer, 512,
                 "s=Mountpoint %" SCNu64 "\r\n", 1);
      g_strlcat(sdptemp, buffer, 2048);
      g_strlcat(sdptemp, "t=0 0\r\n", 2048);
      /* Add audio line */
      if (doaudio)
      {

        g_snprintf(buffer, 512, "m=audio 1 RTP/SAVPF %d\r\n"
                                "c=IN IP4 1.1.1.1\r\n",
                   acodec);
        g_strlcat(sdptemp, buffer, 2048);
        g_snprintf(buffer, 512, "a=rtpmap:%d %s\r\n", acodec, artpmap);
        g_strlcat(sdptemp, buffer, 2048);
        g_strlcat(sdptemp, "a=sendonly\r\n", 2048);
        g_snprintf(buffer, 512, "a=extmap:%d %s\r\n", 1, JANUS_RTP_EXTMAP_MID);
        g_strlcat(sdptemp, buffer, 2048);
      }
      /* Add video line */
      if (dovideo)
      {

        g_snprintf(buffer, 512, "m=video 1 RTP/SAVPF %d\r\n"
                                "c=IN IP4 1.1.1.1\r\n",
                   vcodec);
        g_strlcat(sdptemp, buffer, 2048);
        g_snprintf(buffer, 512, "a=rtpmap:%d %s\r\n", vcodec, vrtpmap);
        g_strlcat(sdptemp, buffer, 2048);
        g_snprintf(buffer, 512, "a=rtcp-fb:%d nack\r\n", vcodec);
        g_strlcat(sdptemp, buffer, 2048);
        g_snprintf(buffer, 512, "a=rtcp-fb:%d nack pli\r\n", vcodec);
        g_strlcat(sdptemp, buffer, 2048);
        g_snprintf(buffer, 512, "a=rtcp-fb:%d goog-remb\r\n", vcodec);
        g_strlcat(sdptemp, buffer, 2048);
        g_strlcat(sdptemp, "a=sendonly\r\n", 2048);
        g_snprintf(buffer, 512, "a=extmap:%d %s\r\n", 1, JANUS_RTP_EXTMAP_MID);
        g_strlcat(sdptemp, buffer, 2048);
      }

      sdp = g_strdup(sdptemp);
      JANUS_LOG(LOG_VERB, "[Rtprx] 返回[ %s ]SDP:\n%s\n", sdp_type, sdp);
    }
    else if (!strcasecmp(request_text, "prepare.source"))
    {
      JANUS_LOG(LOG_VERB, "[Rtprx] 请求创建RTP媒体数据源命令\n");
      session->paused = FALSE;
      tms_rtprx_mountpoint *mp = tms_rtprx_create_rtp_mountpoint(doaudio, dovideo);
      if (mp)
      {
        /* 建立会话和播放源的双向引用 */
        janus_refcount_increase(&mp->ref); // 会话引用，引用+1
        session->mountpoint = mp;
        janus_mutex_lock(&mp->mutex);
        mp->viewer = session;
        janus_mutex_unlock(&mp->mutex);
        janus_refcount_increase(&session->ref);

        /* 返回挂载点信息 */
        tms_rtprx_rtp_source *source = mp->source;
        json_t *ports = json_object();
        if (doaudio)
        {
          json_object_set_new(ports, "audioport", json_integer(source->audio_port));
          json_object_set_new(ports, "audiortcpport", json_integer(source->audio_rtcp_port));
        }
        if (dovideo)
        {
          json_object_set_new(ports, "videoport", json_integer(source->video_port[0]));
          json_object_set_new(ports, "videortcpport", json_integer(source->video_rtcp_port));
        }
        result = json_object();
        json_object_set_new(result, "ports", ports);
        json_object_set_new(result, "status", json_string("mounted"));

        JANUS_LOG(LOG_VERB, "[Rtprx] 完成创建RTP媒体数据源命令[ %p ]\n", mp);
      }
      else
      {
        // 创建挂载点失败
      }
    }
    else if (!strcasecmp(request_text, "destroy.source"))
    {
      tms_rtprx_mountpoint *mp = session->mountpoint;
      if (mp)
      {
        tms_rtprx_mountpoint_destroy(mp);
        JANUS_LOG(LOG_VERB, "[Rtprx] 完成销毁RTP媒体数据源命令\n");
      }
    }

    g_atomic_int_set(&session->renegotiating, 0);

    /* Prepare JSON event */
    json_t *jsep = json_pack("{ssss}", "type", sdp_type, "sdp", sdp);
    json_t *event = json_object();
    json_object_set_new(event, "rtprx", json_string("event"));
    if (result != NULL)
      json_object_set_new(event, "result", result);
    int ret = gateway->push_event(msg->handle, &janus_plugin_tms_rtprx, msg->transaction, event, jsep);
    JANUS_LOG(LOG_VERB, "[Rtprx] >> 推送事件: %d (%s)\n", ret, janus_get_api_error(ret));

    g_free(sdp);
    json_decref(event);
    json_decref(jsep);
    tms_rtprx_message_free(msg);

    JANUS_LOG(LOG_VERB, "[Rtprx] 完成消息处理\n");
  }

  JANUS_LOG(LOG_VERB, "[Rtprx] 离开插件消息处理线程\n");
}
/**
 * 接收外部媒体
*/
typedef struct tms_rtprx_rtp_relay_packet
{
  janus_rtp_header *data;
  gint length;
  gboolean is_rtp; /* This may be a data packet and not RTP */
  gboolean is_video;
  gboolean is_keyframe;
  gboolean simulcast;
  janus_videocodec codec;
  int substream;
  uint32_t timestamp;
  uint16_t seq_number;
  /* The following are only relevant for VP9 SVC*/
  gboolean svc;
  int spatial_layer;
  int temporal_layer;
  uint8_t pbit, dbit, ubit, bbit, ebit;
  /* The following is only relevant for datachannels */
  gboolean textdata;
} tms_rtprx_rtp_relay_packet;
static tms_rtprx_rtp_relay_packet exit_packet;

static void tms_rtprx_rtp_source_free(tms_rtprx_rtp_source *source);

static void tms_rtprx_rtp_relay_packet_free(tms_rtprx_rtp_relay_packet *pkt)
{
  if (pkt == NULL || pkt == &exit_packet)
    return;
  g_free(pkt->data);
  g_free(pkt);
}

/* Helpers to create a listener filedescriptor */
static int tms_rtprx_create_fd(int port, const char *listenername, const char *medianame, gboolean quiet)
{
  janus_mutex_lock(&fd_mutex);
  struct sockaddr_in address = {0};
  struct sockaddr_in6 address6 = {0};
  janus_network_address_string_buffer address_representation;

  uint16_t rtp_port_next = rtp_range_slider; /* Read global slider */
  uint16_t rtp_port_start = rtp_port_next;
  gboolean use_range = (port == 0), rtp_port_wrap = FALSE;

  int fd = -1, family = 0;
  while (1)
  {
    if (use_range && rtp_port_wrap && rtp_port_next >= rtp_port_start)
    {
      /* Full range scanned */
      JANUS_LOG(LOG_ERR, "No ports available for RTP/RTCP in range: %u -- %u\n",
                rtp_range_min, rtp_range_max);
      break;
    }
    if (use_range)
    {
      /* Pick a port in the configured range */
      port = rtp_port_next;
      if ((uint32_t)(rtp_port_next) < rtp_range_max)
      {
        rtp_port_next++;
      }
      else
      {
        rtp_port_next = rtp_range_min;
        rtp_port_wrap = TRUE;
      }
    }
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr.s_addr = INADDR_ANY;
    address6.sin6_family = AF_INET6;
    address6.sin6_port = htons(port);
    address6.sin6_addr = in6addr_any;
    /* Bind to the specified port */
    if (fd == -1)
    {
      fd = socket(family == AF_INET ? AF_INET : AF_INET6, SOCK_DGRAM, IPPROTO_UDP);
      int v6only = 0;
      if (fd < 0)
      {
        JANUS_LOG(LOG_ERR, "Cannot create socket for %s... %d (%s)\n",
                  medianame, errno, strerror(errno));
        break;
      }
      if (family != AF_INET && setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) != 0)
      {
        JANUS_LOG(LOG_ERR, "setsockopt on socket failed for %s... %d (%s)\n",
                  medianame, errno, strerror(errno));
        break;
      }
    }
    size_t addrlen = (family == AF_INET ? sizeof(address) : sizeof(address6));
    if (bind(fd, (family == AF_INET ? (struct sockaddr *)&address : (struct sockaddr *)&address6), addrlen) < 0)
    {
      close(fd);
      fd = -1;
      if (!quiet)
      {
        JANUS_LOG(LOG_ERR, "Bind failed for %s (port %d)... %d (%s)\n",
                  medianame, port, errno, strerror(errno));
      }
      if (!use_range) /* Asked for a specific port but it's not available, give up */
        break;
    }
    else
    {
      if (use_range)
        rtp_range_slider = port; /* Update global slider */
      break;
    }
  }

  // struct sockaddr_in address;
  // janus_network_address_string_buffer address_representation;
  // int fd = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
  // if (fd < 0)
  // {
  //     JANUS_LOG(LOG_ERR, "Cannot create socket for %s...\n", medianame);
  //     return -1;
  // }

  // address.sin_family = AF_INET;
  // address.sin_port = htons(port);
  // address.sin_addr.s_addr = INADDR_ANY;

  // /* Bind to the specified port */
  // if (bind(fd, (struct sockaddr *)(&address), sizeof(struct sockaddr)) < 0)
  // {
  //     JANUS_LOG(LOG_ERR, "Bind failed for %s (port %d)...\n", medianame, port);
  //     close(fd);
  //     return -1;
  // }

  janus_mutex_unlock(&fd_mutex);
  return fd;
}
static int tms_rtprx_get_fd_port(int fd)
{
  struct sockaddr_in server;
  socklen_t len = sizeof(server);
  if (getsockname(fd, &server, &len) == -1)
  {
    return -1;
  }

  return ntohs(server.sin_port);
}
/* Helper to bind RTP/RTCP port pair */
static int tms_rtprx_allocate_port_pair(const char *name, const char *media,
                                        multiple_fds *fds, int ports[2])
{
  /* Start from the global slider */
  uint16_t rtp_port_next = rtp_range_slider;
  if (rtp_port_next % 2 != 0) /* We want an even port for RTP */
    rtp_port_next++;
  uint16_t rtp_port_start = rtp_port_next;
  gboolean rtp_port_wrap = FALSE;

  int rtp_fd = -1, rtcp_fd = -1;
  while (1)
  {
    if (rtp_port_wrap && rtp_port_next >= rtp_port_start)
    {
      /* Full range scanned */
      JANUS_LOG(LOG_ERR, "No ports available for audio/video channel in range: %u -- %u\n",
                rtp_range_min, rtp_range_max);
      break;
    }
    int rtp_port = rtp_port_next;
    int rtcp_port = rtp_port + 1;
    if ((uint32_t)(rtp_port_next + 2UL) < rtp_range_max)
    {
      /* Advance to next pair */
      rtp_port_next += 2;
    }
    else
    {
      rtp_port_next = rtp_range_min;
      rtp_port_wrap = TRUE;
    }
    rtp_fd = tms_rtprx_create_fd(rtp_port, media, name, TRUE);
    if (rtp_fd != -1)
    {
      rtcp_fd = tms_rtprx_create_fd(rtcp_port, media, name, TRUE);
      if (rtcp_fd != -1)
      {
        /* Done */
        fds->fd = rtp_fd;
        fds->rtcp_fd = rtcp_fd;
        ports[0] = rtp_port;
        ports[1] = rtcp_port;
        /* Update global slider */
        rtp_range_slider = rtp_port_next;
        return 0;
      }
    }
    /* If we got here, something failed: try again */
    if (rtp_fd != -1)
      close(rtp_fd);
  }
  return -1;
}
static tms_rtprx_mountpoint *tms_rtprx_create_rtp_mountpoint(gboolean doaudio, gboolean dovideo)
{
  JANUS_LOG(LOG_VERB, "[Rtprx] 开始创建挂载点\n");

  uint16_t aport = 0, artcpport = 0;
  int audio_fd = -1;
  int audio_rtcp_fd = -1;
  if (doaudio)
  {
    int aports[2];
    multiple_fds audio_fds = {-1, -1};
    if (tms_rtprx_allocate_port_pair("Audio", "audio", &audio_fds, aports))
    {
      JANUS_LOG(LOG_ERR, "Bind failed for...\n");
      return NULL;
    }
    aport = aports[0];
    artcpport = aports[1];
    audio_fd = audio_fds.fd;
    audio_rtcp_fd = audio_fds.rtcp_fd;
  }
  uint16_t vport = 0, vrtcpport = 0;
  int video_fd[3] = {-1, -1, -1};
  int video_rtcp_fd = -1;
  if (dovideo)
  {
    int vports[2];
    multiple_fds video_fds = {-1, -1};
    if (tms_rtprx_allocate_port_pair("Video", "video", &video_fds, vports))
    {
      JANUS_LOG(LOG_ERR, "Bind failed for...\n");
      return NULL;
    }
    vport = vports[0];
    vrtcpport = vports[1];
    video_fd[0] = video_fds.fd;
    video_rtcp_fd = video_fds.rtcp_fd;
  }
  tms_rtprx_mountpoint *mp = g_malloc0(sizeof(tms_rtprx_mountpoint));
  mp->id = 1;
  mp->active = FALSE;
  mp->enabled = TRUE;
  mp->audio = doaudio;
  mp->video = dovideo;
  tms_rtprx_rtp_source *mp_rtp_source = g_malloc0(sizeof(tms_rtprx_rtp_source));

  mp_rtp_source->audio_port = doaudio ? aport : -1;
  mp_rtp_source->audio_rtcp_port = artcpport;
  mp_rtp_source->video_port[0] = dovideo ? vport : -1;
  mp_rtp_source->video_port[1] = -1;
  mp_rtp_source->video_port[2] = -1;
  mp_rtp_source->video_rtcp_port = vrtcpport;
  mp_rtp_source->video_fd[0] = video_fd[0];
  mp_rtp_source->video_fd[1] = video_fd[1];
  mp_rtp_source->video_fd[2] = video_fd[2];
  mp_rtp_source->audio_fd = audio_fd;
  mp_rtp_source->audio_rtcp_fd = audio_rtcp_fd;
  mp_rtp_source->video_rtcp_fd = video_rtcp_fd;
  mp_rtp_source->data_port = -1;
  mp_rtp_source->data_fd = -1;
  //mp_rtp_source->pipefd[0] = -1;
  //mp_rtp_source->pipefd[1] = -1;
  //pipe(mp_rtp_source->pipefd);
  mp->source = mp_rtp_source;

  mp->source_destroy = (GDestroyNotify)tms_rtprx_rtp_source_free;
  mp->codecs.audio_pt = doaudio ? acodec : -1;
  mp->codecs.audio_rtpmap = doaudio ? g_strdup(artpmap) : NULL;
  mp->codecs.audio_fmtp = NULL;
  mp->codecs.video_codec = JANUS_VIDEOCODEC_NONE;
  if (dovideo)
  {
    if (strstr(vrtpmap, "vp8") || strstr(vrtpmap, "VP8"))
      mp->codecs.video_codec = JANUS_VIDEOCODEC_VP8;
    else if (strstr(vrtpmap, "vp9") || strstr(vrtpmap, "VP9"))
      mp->codecs.video_codec = JANUS_VIDEOCODEC_VP9;
    else if (strstr(vrtpmap, "h264") || strstr(vrtpmap, "H264"))
      mp->codecs.video_codec = JANUS_VIDEOCODEC_H264;
  }
  mp->codecs.video_pt = dovideo ? vcodec : -1;
  mp->codecs.video_rtpmap = dovideo ? g_strdup(vrtpmap) : NULL;
  mp->codecs.video_fmtp = NULL;
  mp->viewer = NULL;

  janus_refcount_init(&mp->ref, tms_rtprx_mountpoint_free);
  g_atomic_int_set(&mp->destroyed, 0);
  janus_mutex_init(&mp->mutex);

  // 启动接收RTP Packet线程
  GError *error = NULL;
  janus_refcount_increase(&mp->ref); // 线程内使用，引用加1
  JANUS_LOG(LOG_VERB, "[Rtprx] 准备启动挂载点线程 [ %p ]\n", mp);
  g_thread_try_new("rtp thread-1", &tms_rtprx_rtp_relay_thread, mp, &error);
  if (error != NULL)
  {
    JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the RTP thread...\n", error->code, error->message ? error->message : "??");
    janus_refcount_decrease(&mp->ref); /* This is for the failed thread */
    tms_rtprx_mountpoint_destroy(mp);
    return NULL;
  }

  JANUS_LOG(LOG_VERB, "[Rtprx] 完成创建挂载点 [ %p ]\n", mp);

  return mp;
}
/* Helpers to destroy a streaming mountpoint. */
static void tms_rtprx_rtp_source_free(tms_rtprx_rtp_source *source)
{
  JANUS_LOG(LOG_VERB, "[Rtprx] 开始释放挂载点RTP源\n");
  if (source->audio_fd > -1)
  {
    close(source->audio_fd);
  }
  if (source->video_fd[0] > -1)
  {
    close(source->video_fd[0]);
  }
  if (source->video_fd[1] > -1)
  {
    close(source->video_fd[1]);
  }
  if (source->video_fd[2] > -1)
  {
    close(source->video_fd[2]);
  }
  if (source->data_fd > -1)
  {
    close(source->data_fd);
  }
  if (source->audio_rtcp_fd > -1)
  {
    close(source->audio_rtcp_fd);
  }
  if (source->video_rtcp_fd > -1)
  {
    close(source->video_rtcp_fd);
  }
  // if (source->pipefd[0] > -1)
  // {
  //     close(source->pipefd[0]);
  // }
  // if (source->pipefd[1] > -1)
  // {
  //     close(source->pipefd[1]);
  // }
  janus_mutex_lock(&source->keyframe.mutex);
  if (source->keyframe.latest_keyframe != NULL)
    g_list_free_full(source->keyframe.latest_keyframe, (GDestroyNotify)tms_rtprx_rtp_relay_packet_free);
  source->keyframe.latest_keyframe = NULL;
  janus_mutex_unlock(&source->keyframe.mutex);
  janus_mutex_lock(&source->buffermsg_mutex);
  if (source->last_msg != NULL)
    tms_rtprx_rtp_relay_packet_free((tms_rtprx_rtp_relay_packet *)source->last_msg);
  source->last_msg = NULL;
  janus_mutex_unlock(&source->buffermsg_mutex);
  g_free(source);

  JANUS_LOG(LOG_VERB, "[Rtprx] 完成释放挂载点RTP源\n");
}
/**
 * 转发RTP包
 */
static void tms_rtprx_relay_rtp_packet(gpointer data, gpointer user_data)
{
  JANUS_LOG(LOG_VERB, "[Rtprx] 执行转发RTP包\n");
  tms_rtprx_rtp_relay_packet *packet = (tms_rtprx_rtp_relay_packet *)user_data;
  if (!packet || !packet->data || packet->length < 1)
  {
    JANUS_LOG(LOG_ERR, "Invalid packet...\n");
    return;
  }
  tms_rtprx_session *session = (tms_rtprx_session *)data;
  if (!session)
  {
    JANUS_LOG(LOG_ERR, "[Rtprx] 没有传递会话\n");
  }
  if (!session->handle)
  {
    JANUS_LOG(LOG_ERR, "[Rtprx] 会话数据不完整\n");
    return;
  }
  if (!packet->is_keyframe && (!session->started || session->paused))
  {
    JANUS_LOG(LOG_ERR, "[Rtprx] 会话不是启用状态，无法转发\n");
    return;
  }

  if (packet->is_rtp)
  {
    /* Make sure there hasn't been a video source switch by checking the SSRC */
    if (packet->is_video)
    {
      if (!session->video)
        return;
      /* Check if there's any SVC info to take into account */
      // if (packet->svc)
      // {
      //     /* There is: check if this is a layer that can be dropped for this viewer
      // 	 * Note: Following core inspired by the excellent job done by Sergio Garcia Murillo here:
      // 	 * https://github.com/medooze/media-server/blob/master/src/vp9/VP9LayerSelector.cpp */
      //     int plen = 0;
      //     char *payload = janus_rtp_payload((char *)packet->data, packet->length, &plen);
      //     gboolean keyframe = janus_vp9_is_keyframe((const char *)payload, plen);
      //     gboolean override_mark_bit = FALSE, has_marker_bit = packet->data->markerbit;
      //     int spatial_layer = session->spatial_layer;
      //     gint64 now = janus_get_monotonic_time();
      //     if (packet->svc_info.spatial_layer >= 0 && packet->svc_info.spatial_layer <= 2)
      //         session->last_spatial_layer[packet->svc_info.spatial_layer] = now;
      //     if (session->target_spatial_layer > session->spatial_layer)
      //     {
      //         JANUS_LOG(LOG_HUGE, "We need to upscale spatially: (%d < %d)\n",
      //                   session->spatial_layer, session->target_spatial_layer);
      //         /* We need to upscale: wait for a keyframe */
      //         if (keyframe)
      //         {
      //             int new_spatial_layer = session->target_spatial_layer;
      //             while (new_spatial_layer > session->spatial_layer && new_spatial_layer > 0)
      //             {
      //                 if (now - session->last_spatial_layer[new_spatial_layer] >= 250000)
      //                 {
      //                     /* We haven't received packets from this layer for a while, try a lower layer */
      //                     JANUS_LOG(LOG_HUGE, "Haven't received packets from layer %d for a while, trying %d instead...\n",
      //                               new_spatial_layer, new_spatial_layer - 1);
      //                     new_spatial_layer--;
      //                 }
      //                 else
      //                 {
      //                     break;
      //                 }
      //             }
      //             if (new_spatial_layer > session->spatial_layer)
      //             {
      //                 JANUS_LOG(LOG_HUGE, "  -- Upscaling spatial layer: %d --> %d (need %d)\n",
      //                           session->spatial_layer, new_spatial_layer, session->target_spatial_layer);
      //                 session->spatial_layer = new_spatial_layer;
      //                 spatial_layer = session->spatial_layer;
      //                 /* Notify the viewer */
      //                 json_t *event = json_object();
      //                 json_object_set_new(event, "streaming", json_string("event"));
      //                 json_t *result = json_object();
      //                 json_object_set_new(result, "spatial_layer", json_integer(session->spatial_layer));
      //                 if (session->temporal_layer == -1)
      //                 {
      //                     /* We just started: initialize the temporal layer and notify that too */
      //                     session->temporal_layer = 0;
      //                     json_object_set_new(result, "temporal_layer", json_integer(session->temporal_layer));
      //                 }
      //                 json_object_set_new(event, "result", result);
      //                 gateway->push_event(session->handle, &janus_streaming_plugin, NULL, event, NULL);
      //                 json_decref(event);
      //             }
      //         }
      //     }
      //     else if (session->target_spatial_layer < session->spatial_layer)
      //     {
      //         /* We need to downscale */
      //         JANUS_LOG(LOG_HUGE, "We need to downscale spatially: (%d > %d)\n",
      //                   session->spatial_layer, session->target_spatial_layer);
      //         gboolean downscaled = FALSE;
      //         if (!packet->svc_info.fbit && keyframe)
      //         {
      //             /* Non-flexible mode: wait for a keyframe */
      //             downscaled = TRUE;
      //         }
      //         else if (packet->svc_info.fbit && packet->svc_info.ebit)
      //         {
      //             /* Flexible mode: check the E bit */
      //             downscaled = TRUE;
      //         }
      //         if (downscaled)
      //         {
      //             JANUS_LOG(LOG_HUGE, "  -- Downscaling spatial layer: %d --> %d\n",
      //                       session->spatial_layer, session->target_spatial_layer);
      //             session->spatial_layer = session->target_spatial_layer;
      //             /* Notify the viewer */
      //             json_t *event = json_object();
      //             json_object_set_new(event, "streaming", json_string("event"));
      //             json_t *result = json_object();
      //             json_object_set_new(result, "spatial_layer", json_integer(session->spatial_layer));
      //             json_object_set_new(event, "result", result);
      //             gateway->push_event(session->handle, &janus_streaming_plugin, NULL, event, NULL);
      //             json_decref(event);
      //         }
      //     }
      //     if (spatial_layer < packet->svc_info.spatial_layer)
      //     {
      //         /* Drop the packet: update the context to make sure sequence number is increased normally later */
      //         JANUS_LOG(LOG_HUGE, "Dropping packet (spatial layer %d < %d)\n", spatial_layer, packet->svc_info.spatial_layer);
      //         session->context.v_base_seq++;
      //         return;
      //     }
      //     else if (packet->svc_info.ebit && spatial_layer == packet->svc_info.spatial_layer)
      //     {
      //         /* If we stop at layer 0, we need a marker bit now, as the one from layer 1 will not be received */
      //         override_mark_bit = TRUE;
      //     }
      //     int temporal_layer = session->temporal_layer;
      //     if (session->target_temporal_layer > session->temporal_layer)
      //     {
      //         /* We need to upscale */
      //         JANUS_LOG(LOG_HUGE, "We need to upscale temporally: (%d < %d)\n",
      //                   session->temporal_layer, session->target_temporal_layer);
      //         if (packet->svc_info.ubit && packet->svc_info.bbit &&
      //             packet->svc_info.temporal_layer > session->temporal_layer &&
      //             packet->svc_info.temporal_layer <= session->target_temporal_layer)
      //         {
      //             JANUS_LOG(LOG_HUGE, "  -- Upscaling temporal layer: %d --> %d (want %d)\n",
      //                       session->temporal_layer, packet->svc_info.temporal_layer, session->target_temporal_layer);
      //             session->temporal_layer = packet->svc_info.temporal_layer;
      //             temporal_layer = session->temporal_layer;
      //             /* Notify the viewer */
      //             json_t *event = json_object();
      //             json_object_set_new(event, "streaming", json_string("event"));
      //             json_t *result = json_object();
      //             json_object_set_new(result, "temporal_layer", json_integer(session->temporal_layer));
      //             json_object_set_new(event, "result", result);
      //             gateway->push_event(session->handle, &janus_streaming_plugin, NULL, event, NULL);
      //             json_decref(event);
      //         }
      //     }
      //     else if (session->target_temporal_layer < session->temporal_layer)
      //     {
      //         /* We need to downscale */
      //         JANUS_LOG(LOG_HUGE, "We need to downscale temporally: (%d > %d)\n",
      //                   session->temporal_layer, session->target_temporal_layer);
      //         if (packet->svc_info.ebit && packet->svc_info.temporal_layer == session->target_temporal_layer)
      //         {
      //             JANUS_LOG(LOG_HUGE, "  -- Downscaling temporal layer: %d --> %d\n",
      //                       session->temporal_layer, session->target_temporal_layer);
      //             session->temporal_layer = session->target_temporal_layer;
      //             /* Notify the viewer */
      //             json_t *event = json_object();
      //             json_object_set_new(event, "streaming", json_string("event"));
      //             json_t *result = json_object();
      //             json_object_set_new(result, "temporal_layer", json_integer(session->temporal_layer));
      //             json_object_set_new(event, "result", result);
      //             gateway->push_event(session->handle, &janus_streaming_plugin, NULL, event, NULL);
      //             json_decref(event);
      //         }
      //     }
      //     if (temporal_layer < packet->svc_info.temporal_layer)
      //     {
      //         /* Drop the packet: update the context to make sure sequence number is increased normally later */
      //         JANUS_LOG(LOG_HUGE, "Dropping packet (temporal layer %d < %d)\n", temporal_layer, packet->svc_info.temporal_layer);
      //         session->context.v_base_seq++;
      //         return;
      //     }
      //     /* If we got here, we can send the frame: this doesn't necessarily mean it's
      // 	 * one of the layers the user wants, as there may be dependencies involved */
      //     JANUS_LOG(LOG_HUGE, "Sending packet (spatial=%d, temporal=%d)\n",
      //               packet->svc_info.spatial_layer, packet->svc_info.temporal_layer);
      //     /* Fix sequence number and timestamp (publisher switching may be involved) */
      //     janus_rtp_header_update(packet->data, &session->context, TRUE, 0);
      //     if (override_mark_bit && !has_marker_bit)
      //     {
      //         packet->data->markerbit = 1;
      //     }
      //     janus_plugin_rtp rtp = {.video = packet->is_video, .buffer = (char *)packet->data, .length = packet->length};
      //     janus_plugin_rtp_extensions_reset(&rtp.extensions);
      //     if (gateway != NULL)
      //         gateway->relay_rtp(session->handle, &rtp);
      //     if (override_mark_bit && !has_marker_bit)
      //     {
      //         packet->data->markerbit = 0;
      //     }
      //     /* Restore the timestamp and sequence number to what the publisher set them to */
      //     packet->data->timestamp = htonl(packet->timestamp);
      //     packet->data->seq_number = htons(packet->seq_number);
      // }
      // else if (packet->simulcast)
      // {
      //     /* Handle simulcast: don't relay if it's not the substream we wanted to handle */
      //     int plen = 0;
      //     char *payload = janus_rtp_payload((char *)packet->data, packet->length, &plen);
      //     if (payload == NULL)
      //         return;
      //     /* Process this packet: don't relay if it's not the SSRC/layer we wanted to handle */
      //     gboolean relay = janus_rtp_simulcasting_context_process_rtp(&session->sim_context,
      //                                                                 (char *)packet->data, packet->length, packet->ssrc, NULL, packet->codec, &session->context);
      //     /* Do we need to drop this? */
      //     if (!relay)
      //         return;
      //     /* Any event we should notify? */
      //     if (session->sim_context.changed_substream)
      //     {
      //         /* Notify the user about the substream change */
      //         json_t *event = json_object();
      //         json_object_set_new(event, "streaming", json_string("event"));
      //         json_t *result = json_object();
      //         json_object_set_new(result, "substream", json_integer(session->sim_context.substream));
      //         json_object_set_new(event, "result", result);
      //         gateway->push_event(session->handle, &janus_streaming_plugin, NULL, event, NULL);
      //         json_decref(event);
      //     }
      //     if (session->sim_context.need_pli)
      //     {
      //         /* Schedule a PLI */
      //         JANUS_LOG(LOG_VERB, "We need a PLI for the simulcast context\n");
      //         if (session->mountpoint != NULL)
      //         {
      //             *source = session->mountpoint->source;
      //             if (source != NULL)
      //                 g_atomic_int_set(&source->need_pli, 1);
      //         }
      //     }
      //     if (session->sim_context.changed_temporal)
      //     {
      //         /* Notify the user about the temporal layer change */
      //         json_t *event = json_object();
      //         json_object_set_new(event, "streaming", json_string("event"));
      //         json_t *result = json_object();
      //         json_object_set_new(result, "temporal", json_integer(session->sim_context.templayer));
      //         json_object_set_new(event, "result", result);
      //         gateway->push_event(session->handle, &janus_streaming_plugin, NULL, event, NULL);
      //         json_decref(event);
      //     }
      //     /* If we got here, update the RTP header and send the packet */
      //     janus_rtp_header_update(packet->data, &session->context, TRUE, 0);
      //     char vp8pd[6];
      //     if (packet->codec == JANUS_VIDEOCODEC_VP8)
      //     {
      //         /* For VP8, we save the original payload descriptor, to restore it after */
      //         memcpy(vp8pd, payload, sizeof(vp8pd));
      //         janus_vp8_simulcast_descriptor_update(payload, plen, &session->vp8_context,
      //                                               session->sim_context.changed_substream);
      //     }
      //     /* Send the packet */
      //     janus_plugin_rtp rtp = {.video = packet->is_video, .buffer = (char *)packet->data, .length = packet->length};
      //     janus_plugin_rtp_extensions_reset(&rtp.extensions);
      //     if (gateway != NULL)
      //         gateway->relay_rtp(session->handle, &rtp);
      //     /* Restore the timestamp and sequence number to what the publisher set them to */
      //     packet->data->timestamp = htonl(packet->timestamp);
      //     packet->data->seq_number = htons(packet->seq_number);
      //     if (packet->codec == JANUS_VIDEOCODEC_VP8)
      //     {
      //         /* Restore the original payload descriptor as well, as it will be needed by the next viewer */
      //         memcpy(payload, vp8pd, sizeof(vp8pd));
      //     }
      // }
      // else
      // {
      /* Fix sequence number and timestamp (switching may be involved) */
      janus_rtp_header_update(packet->data, &session->context, TRUE, 0);
      janus_plugin_rtp rtp = {.video = packet->is_video, .buffer = (char *)packet->data, .length = packet->length};
      janus_plugin_rtp_extensions_reset(&rtp.extensions);
      if (gateway != NULL)
        gateway->relay_rtp(session->handle, &rtp);
      /* Restore the timestamp and sequence number to what the video source set them to */
      packet->data->timestamp = htonl(packet->timestamp);
      packet->data->seq_number = htons(packet->seq_number);
      // }
    }
    else
    {
      if (!session->audio)
        return;
      /* Fix sequence number and timestamp (switching may be involved) */
      janus_rtp_header_update(packet->data, &session->context, FALSE, 0);
      janus_plugin_rtp rtp = {.video = packet->is_video, .buffer = (char *)packet->data, .length = packet->length};
      janus_plugin_rtp_extensions_reset(&rtp.extensions);
      if (gateway != NULL)
        gateway->relay_rtp(session->handle, &rtp);
      /* Restore the timestamp and sequence number to what the video source set them to */
      packet->data->timestamp = htonl(packet->timestamp);
      packet->data->seq_number = htons(packet->seq_number);
    }
  }
  else
  {
    /* We're broadcasting a data channel message */
    if (!session->data)
      return;
    if (gateway != NULL && packet->data != NULL)
    {
      janus_plugin_data data = {.label = NULL, .binary = !packet->textdata, .buffer = (char *)packet->data, .length = packet->length};
      gateway->relay_data(session->handle, &data);
    }
  }

  return;
}

static void tms_rtprx_relay_rtcp_packet(gpointer data, gpointer user_data)
{
  tms_rtprx_rtp_relay_packet *packet = (tms_rtprx_rtp_relay_packet *)user_data;
  if (!packet || !packet->data || packet->length < 1)
  {
    JANUS_LOG(LOG_ERR, "Invalid packet...\n");
    return;
  }
  tms_rtprx_session *session = (tms_rtprx_session *)data;
  if (!session || !session->handle)
  {
    //~ JANUS_LOG(LOG_ERR, "Invalid session...\n");
    return;
  }
  if (!session->started || session->paused)
  {
    //~ JANUS_LOG(LOG_ERR, "Streaming not started yet for this session...\n");
    return;
  }

  janus_plugin_rtcp rtcp = {.video = packet->is_video, .buffer = (char *)packet->data, .length = packet->length};
  if (gateway != NULL)
    gateway->relay_rtcp(session->handle, &rtcp);

  return;
}
/** 
 * 接收外部媒体文件RTP帧的线程 
 */
static void *tms_rtprx_rtp_relay_thread(void *data)
{
  JANUS_LOG(LOG_INFO, "[Rtprx] 启动数据源RTP包接收转发线程\n");

  tms_rtprx_mountpoint *mountpoint = (tms_rtprx_mountpoint *)data;
  if (!mountpoint)
  {
    JANUS_LOG(LOG_ERR, "Invalid mountpoint!\n");
    return NULL;
  }
  tms_rtprx_rtp_source *source = mountpoint->source;
  if (source == NULL)
  {
    JANUS_LOG(LOG_ERR, "[%p] Invalid RTP source mountpoint!\n", mountpoint);
    janus_refcount_decrease(&mountpoint->ref);
    return NULL;
  }
  int audio_fd = source->audio_fd;
  int video_fd[3] = {source->video_fd[0], source->video_fd[1], source->video_fd[2]};
  int data_fd = source->data_fd;
  //int pipe_fd = source->pipefd[0];
  int audio_rtcp_fd = source->audio_rtcp_fd;
  int video_rtcp_fd = source->video_rtcp_fd;
  /* Needed to fix seq and ts */
  uint32_t ssrc = 0, a_last_ssrc = 0, v_last_ssrc[3] = {0, 0, 0};
  /* File descriptors */
  socklen_t addrlen;
  struct sockaddr remote;
  int resfd = 0, bytes = 0;
  struct pollfd fds[8];
  char buffer[1500];
  memset(buffer, 0, 1500);
  /* Loop */
  int num = 0;
  tms_rtprx_rtp_relay_packet packet;
  while (!g_atomic_int_get(&mountpoint->destroyed))
  {
    /* Prepare poll */
    num = 0;
    if (audio_fd != -1)
    {
      fds[num].fd = audio_fd;
      fds[num].events = POLLIN;
      fds[num].revents = 0;
      num++;
    }
    if (video_fd[0] != -1)
    {
      fds[num].fd = video_fd[0];
      fds[num].events = POLLIN;
      fds[num].revents = 0;
      num++;
    }
    if (video_fd[1] != -1)
    {
      fds[num].fd = video_fd[1];
      fds[num].events = POLLIN;
      fds[num].revents = 0;
      num++;
    }
    if (video_fd[2] != -1)
    {
      fds[num].fd = video_fd[2];
      fds[num].events = POLLIN;
      fds[num].revents = 0;
      num++;
    }
    // if (pipe_fd != -1)
    // {
    //     fds[num].fd = pipe_fd;
    //     fds[num].events = POLLIN;
    //     fds[num].revents = 0;
    //     num++;
    // }
    if (audio_rtcp_fd != -1)
    {
      fds[num].fd = audio_rtcp_fd;
      fds[num].events = POLLIN;
      fds[num].revents = 0;
      num++;
    }
    if (video_rtcp_fd != -1)
    {
      fds[num].fd = video_rtcp_fd;
      fds[num].events = POLLIN;
      fds[num].revents = 0;
      num++;
    }
    JANUS_LOG(LOG_VERB, "[Rtprx] 数据源RTP包接收线程，监听端口数[ %d ]\n", num);
    /* Wait for some data */
    resfd = poll(fds, num, 1000);
    if (resfd < 0)
    {
      if (errno == EINTR)
      {
        JANUS_LOG(LOG_HUGE, "[%p] Got an EINTR (%s), ignoring...\n", mountpoint, strerror(errno));
        continue;
      }
      JANUS_LOG(LOG_ERR, "[%p] Error polling... %d (%s)\n", mountpoint, errno, strerror(errno));
      mountpoint->enabled = FALSE;
      break;
    }
    else if (resfd == 0)
    {
      /* No data, keep going */
      continue;
    }
    int i = 0;
    for (i = 0; i < num; i++)
    {
      if (fds[i].revents & (POLLERR | POLLHUP))
      {
        /* Socket error? */
        JANUS_LOG(LOG_ERR, "[%p] Error polling: %s... %d (%s)\n", mountpoint,
                  fds[i].revents & POLLERR ? "POLLERR" : "POLLHUP", errno, strerror(errno));
        mountpoint->enabled = FALSE;
        break;
      }
      else if (fds[i].revents & POLLIN)
      {
        /* Got an RTP or data packet */
        // if (pipe_fd != -1 && fds[i].fd == pipe_fd)
        // {
        //     /* We're done here */
        //     int code = 0;
        //     bytes = read(pipe_fd, &code, sizeof(int));
        //     JANUS_LOG(LOG_VERB, "[%p] Interrupting mountpoint\n", mountpoint);
        //     break;
        // }
        // else
        if (audio_fd != -1 && fds[i].fd == audio_fd)
        {
          /* Got something audio (RTP) */
          if (mountpoint->active == FALSE)
            mountpoint->active = TRUE;
          gint64 now = janus_get_monotonic_time();
          addrlen = sizeof(remote);
          bytes = recvfrom(audio_fd, buffer, 1500, 0, &remote, &addrlen);
          if (!janus_is_rtp(buffer, bytes))
          {
            /* Failed to read or not an RTP packet? */
            continue;
          }
          janus_rtp_header *rtp = (janus_rtp_header *)buffer;
          ssrc = ntohl(rtp->ssrc);
          source->last_received_audio = now;
          //~ JANUS_LOG(LOG_VERB, "************************\nGot %d bytes on the audio channel...\n", bytes);
          /* If paused, ignore this packet */
          if (!mountpoint->enabled)
            continue;
          /* Is this SRTP? */
          //~ JANUS_LOG(LOG_VERB, " ... parsed RTP packet (ssrc=%u, pt=%u, seq=%u, ts=%u)...\n",
          //~ ntohl(rtp->ssrc), rtp->type, ntohs(rtp->seq_number), ntohl(rtp->timestamp));
          /* Relay on all sessions */
          packet.data = rtp;
          packet.length = bytes;
          packet.is_rtp = TRUE;
          packet.is_video = FALSE;
          packet.is_keyframe = FALSE;
          /* Do we have a new stream? */
          if (ssrc != a_last_ssrc)
          {
            source->audio_ssrc = a_last_ssrc = ssrc;
            JANUS_LOG(LOG_INFO, "[%p] New audio stream! (ssrc=%" SCNu32 ")\n", mountpoint, a_last_ssrc);
          }
          packet.data->type = mountpoint->codecs.audio_pt;
          /* Is there a recorder? */
          janus_rtp_header_update(packet.data, &source->context[0], FALSE, 0);
          packet.data->ssrc = ntohl((uint32_t)mountpoint->id);
          packet.data->ssrc = ssrc;
          /* Backup the actual timestamp and sequence number set by the restreamer, in case switching is involved */
          packet.timestamp = ntohl(packet.data->timestamp);
          packet.seq_number = ntohs(packet.data->seq_number);
          /* Go! */
          JANUS_LOG(LOG_VERB, "[Rtprx] 数据源RTP包接收线程，转发audio\n");
          janus_mutex_lock(&mountpoint->mutex);
          tms_rtprx_relay_rtp_packet(mountpoint->viewer, &packet);
          janus_mutex_unlock(&mountpoint->mutex);
          continue;
        }
        else if ((video_fd[0] != -1 && fds[i].fd == video_fd[0]) ||
                 (video_fd[1] != -1 && fds[i].fd == video_fd[1]) ||
                 (video_fd[2] != -1 && fds[i].fd == video_fd[2]))
        {
          /* Got something video (RTP) */
          int index = -1;
          if (fds[i].fd == video_fd[0])
            index = 0;
          else if (fds[i].fd == video_fd[1])
            index = 1;
          else if (fds[i].fd == video_fd[2])
            index = 2;
          if (mountpoint->active == FALSE)
            mountpoint->active = TRUE;
          gint64 now = janus_get_monotonic_time();
          addrlen = sizeof(remote);
          bytes = recvfrom(fds[i].fd, buffer, 1500, 0, &remote, &addrlen);
          if (!janus_is_rtp(buffer, bytes))
          {
            /* Failed to read or not an RTP packet? */
            continue;
          }
          janus_rtp_header *rtp = (janus_rtp_header *)buffer;
          ssrc = ntohl(rtp->ssrc);
          source->last_received_video = now;
          //~ JANUS_LOG(LOG_VERB, "************************\nGot %d bytes on the video channel...\n", bytes);
          /* Is this SRTP? */
          /* First of all, let's check if this is (part of) a keyframe that we may need to save it for future reference */
          if (source->keyframe.enabled)
          {
            if (source->keyframe.temp_ts > 0 && ntohl(rtp->timestamp) != source->keyframe.temp_ts)
            {
              /* We received the last part of the keyframe, get rid of the old one and use this from now on */
              JANUS_LOG(LOG_HUGE, "[%p] ... ... last part of keyframe received! ts=%" SCNu32 ", %d packets\n",
                        mountpoint, source->keyframe.temp_ts, g_list_length(source->keyframe.temp_keyframe));
              source->keyframe.temp_ts = 0;
              janus_mutex_lock(&source->keyframe.mutex);
              if (source->keyframe.latest_keyframe != NULL)
                g_list_free_full(source->keyframe.latest_keyframe, (GDestroyNotify)tms_rtprx_rtp_relay_packet_free);
              source->keyframe.latest_keyframe = source->keyframe.temp_keyframe;
              source->keyframe.temp_keyframe = NULL;
              janus_mutex_unlock(&source->keyframe.mutex);
            }
            else if (ntohl(rtp->timestamp) == source->keyframe.temp_ts)
            {
              /* Part of the keyframe we're currently saving, store */
              janus_mutex_lock(&source->keyframe.mutex);
              JANUS_LOG(LOG_HUGE, "[%p] ... other part of keyframe received! ts=%" SCNu32 "\n", mountpoint, source->keyframe.temp_ts);
              tms_rtprx_rtp_relay_packet *pkt = g_malloc0(sizeof(tms_rtprx_rtp_relay_packet));
              pkt->data = g_malloc(bytes);
              memcpy(pkt->data, buffer, bytes);
              pkt->data->ssrc = htons(1);
              pkt->data->type = mountpoint->codecs.video_pt;
              packet.is_rtp = TRUE;
              packet.is_video = TRUE;
              packet.is_keyframe = TRUE;
              pkt->length = bytes;
              pkt->timestamp = source->keyframe.temp_ts;
              pkt->seq_number = ntohs(rtp->seq_number);
              source->keyframe.temp_keyframe = g_list_append(source->keyframe.temp_keyframe, pkt);
              janus_mutex_unlock(&source->keyframe.mutex);
            }
            else
            {
              gboolean kf = FALSE;
              /* Parse RTP header first */
              janus_rtp_header *header = (janus_rtp_header *)buffer;
              guint32 timestamp = ntohl(header->timestamp);
              guint16 seq = ntohs(header->seq_number);
              JANUS_LOG(LOG_HUGE, "Checking if packet (size=%d, seq=%" SCNu16 ", ts=%" SCNu32 ") is a key frame...\n",
                        bytes, seq, timestamp);
              int plen = 0;
              char *payload = janus_rtp_payload(buffer, bytes, &plen);
              if (payload)
              {
                switch (mountpoint->codecs.video_codec)
                {
                case JANUS_VIDEOCODEC_VP8:
                  kf = janus_vp8_is_keyframe(payload, plen);
                  break;
                case JANUS_VIDEOCODEC_VP9:
                  kf = janus_vp9_is_keyframe(payload, plen);
                  break;
                case JANUS_VIDEOCODEC_H264:
                  kf = janus_h264_is_keyframe(payload, plen);
                  break;
                default:
                  break;
                }
                if (kf)
                {
                  /* New keyframe, start saving it */
                  source->keyframe.temp_ts = ntohl(rtp->timestamp);
                  JANUS_LOG(LOG_HUGE, "[%p] New keyframe received! ts=%" SCNu32 "\n", mountpoint, source->keyframe.temp_ts);
                  janus_mutex_lock(&source->keyframe.mutex);
                  tms_rtprx_rtp_relay_packet *pkt = g_malloc0(sizeof(tms_rtprx_rtp_relay_packet));
                  pkt->data = g_malloc(bytes);
                  memcpy(pkt->data, buffer, bytes);
                  pkt->data->ssrc = htons(1);
                  pkt->data->type = mountpoint->codecs.video_pt;
                  packet.is_rtp = TRUE;
                  packet.is_video = TRUE;
                  packet.is_keyframe = TRUE;
                  pkt->length = bytes;
                  pkt->timestamp = source->keyframe.temp_ts;
                  pkt->seq_number = ntohs(rtp->seq_number);
                  source->keyframe.temp_keyframe = g_list_append(source->keyframe.temp_keyframe, pkt);
                  janus_mutex_unlock(&source->keyframe.mutex);
                }
              }
            }
          }
          /* If paused, ignore this packet */
          if (!mountpoint->enabled)
            continue;
          //~ JANUS_LOG(LOG_VERB, " ... parsed RTP packet (ssrc=%u, pt=%u, seq=%u, ts=%u)...\n",
          //~ ntohl(rtp->ssrc), rtp->type, ntohs(rtp->seq_number), ntohl(rtp->timestamp));
          /* Relay on all sessions */
          packet.data = rtp;
          packet.length = bytes;
          packet.is_rtp = TRUE;
          packet.is_video = TRUE;
          packet.is_keyframe = FALSE;
          packet.simulcast = source->simulcast;
          packet.substream = index;
          packet.codec = mountpoint->codecs.video_codec;
          packet.svc = FALSE;
          if (source->svc)
          {
            /* We're doing SVC: let's parse this packet to see which layers are there */
            int plen = 0;
            char *payload = janus_rtp_payload(buffer, bytes, &plen);
            if (payload)
            {
              uint8_t pbit = 0, dbit = 0, ubit = 0, bbit = 0, ebit = 0;
              int found = 0, spatial_layer = 0, temporal_layer = 0;
              // if (janus_vp9_parse_svc(payload, plen, &found, &spatial_layer, &temporal_layer, &pbit, &dbit, &ubit, &bbit, &ebit) == 0)
              // {
              //     if (found)
              //     {
              //         packet.svc = TRUE;
              //         packet.spatial_layer = spatial_layer;
              //         packet.temporal_layer = temporal_layer;
              //         packet.pbit = pbit;
              //         packet.dbit = dbit;
              //         packet.ubit = ubit;
              //         packet.bbit = bbit;
              //         packet.ebit = ebit;
              //     }
              // }
            }
          }
          /* Do we have a new stream? */
          if (ssrc != v_last_ssrc[index])
          {
            v_last_ssrc[index] = ssrc;
            if (index == 0)
              source->video_ssrc = ssrc;
            JANUS_LOG(LOG_INFO, "[%p] New video stream! (ssrc=%" SCNu32 ", index %d)\n",
                      mountpoint, v_last_ssrc[index], index);
          }
          packet.data->type = mountpoint->codecs.video_pt;
          /* Is there a recorder? (FIXME notice we only record the first substream, if simulcasting) */
          janus_rtp_header_update(packet.data, &source->context[index], TRUE, 0);
          if (index == 0)
          {
            packet.data->ssrc = ntohl((uint32_t)mountpoint->id);
            packet.data->ssrc = ssrc;
          }
          /* Backup the actual timestamp and sequence number set by the restreamer, in case switching is involved */
          packet.timestamp = ntohl(packet.data->timestamp);
          packet.seq_number = ntohs(packet.data->seq_number);
          /* Go! */
          JANUS_LOG(LOG_VERB, "[Rtprx] 数据源RTP包接收线程，转发video\n");
          janus_mutex_lock(&mountpoint->mutex);
          tms_rtprx_relay_rtp_packet(mountpoint->viewer, &packet);
          janus_mutex_unlock(&mountpoint->mutex);
          continue;
        }
        else if (audio_rtcp_fd != -1 && fds[i].fd == audio_rtcp_fd)
        {
          JANUS_LOG(LOG_VERB, "[Rtprx] 转发线程 audio-rtcp");
          addrlen = sizeof(remote);
          bytes = recvfrom(audio_rtcp_fd, buffer, 1500, 0, &remote, &addrlen);
          if (!janus_is_rtcp(buffer, bytes))
          {
            /* Failed to read or not an RTCP packet? */
            continue;
          }
          memcpy(&source->audio_rtcp_addr, &remote, addrlen);
          JANUS_LOG(LOG_HUGE, "[%p] Got audio RTCP feedback: SSRC %" SCNu32 "\n",
                    mountpoint, janus_rtcp_get_sender_ssrc(buffer, bytes));
          /* Relay on all sessions */
          packet.is_video = FALSE;
          packet.data = (janus_rtp_header *)buffer;
          packet.length = bytes;
          /* Go! */
          janus_mutex_lock(&mountpoint->mutex);
          tms_rtprx_relay_rtcp_packet(mountpoint->viewer, &packet);
          janus_mutex_unlock(&mountpoint->mutex);
        }
        else if (video_rtcp_fd != -1 && fds[i].fd == video_rtcp_fd)
        {
          JANUS_LOG(LOG_VERB, "[Rtprx] 转发线程-2 video-rtcp");
          addrlen = sizeof(remote);
          bytes = recvfrom(video_rtcp_fd, buffer, 1500, 0, &remote, &addrlen);
          if (!janus_is_rtcp(buffer, bytes))
          {
            /* Failed to read or not an RTCP packet? */
            continue;
          }
          memcpy(&source->video_rtcp_addr, &remote, addrlen);
          JANUS_LOG(LOG_HUGE, "[%p] Got video RTCP feedback: SSRC %" SCNu32 "\n",
                    mountpoint, janus_rtcp_get_sender_ssrc(buffer, bytes));
          /* Relay on all sessions */
          packet.is_video = TRUE;
          packet.data = (janus_rtp_header *)buffer;
          packet.length = bytes;
          /* Go! */
          janus_mutex_lock(&mountpoint->mutex);
          tms_rtprx_relay_rtcp_packet(mountpoint->viewer, &packet);
          janus_mutex_unlock(&mountpoint->mutex);
        }
      }
    }
  }
  JANUS_LOG(LOG_VERB, "[Rtprx] 开始结束RTP转发线程\n");
  /* Notify users this mountpoint is done */

  tms_rtprx_session *session = (tms_rtprx_session *)mountpoint->viewer;
  if (session != NULL)
  {
    /* Prepare JSON event */
    JANUS_LOG(LOG_VERB, "[Rtprx] 推送卸载挂载点事件\n");
    json_t *event = json_object();
    json_object_set_new(event, "rtprx", json_string("event"));
    json_t *result = json_object();
    json_object_set_new(result, "status", json_string("unmounted"));
    json_object_set_new(event, "result", result);
    gateway->push_event(session->handle, &janus_plugin_tms_rtprx, NULL, event, NULL);
    json_decref(event);

    JANUS_LOG(LOG_VERB, "[Rtprx] 开始解除会话和挂载点间的相互引用\n");
    session->stopping = TRUE;
    session->started = FALSE;
    session->paused = FALSE;
    session->mountpoint = NULL;
    janus_refcount_decrease(&session->ref);

    janus_mutex_lock(&mountpoint->mutex);
    // 解除对会话的引用
    mountpoint->viewer = NULL;
    janus_mutex_unlock(&mountpoint->mutex);
    janus_refcount_decrease(&mountpoint->ref);
    JANUS_LOG(LOG_VERB, "[Rtprx] 完成解除会话和挂载点间的相互引用\n");
  }

  janus_refcount_decrease(&mountpoint->ref);

  JANUS_LOG(LOG_VERB, "[Rtprx] 完成结束RTP转发线程\n");

  return NULL;
}
/**************************************
 *  插件生命周期方法 
 **************************************/
janus_plugin *create(void)
{
  JANUS_LOG(LOG_VERB, "创建插件 %s\n", TMS_JANUS_PLUGIN_RTPRX_NAME);
  return &janus_plugin_tms_rtprx;
}
/** 
 * 初始化插件 
 */
int janus_plugin_init_tms_rtprx(janus_callbacks *callback, const char *config_path)
{
  if (callback == NULL || config_path == NULL)
  {
    return -1;
  }

  /* Read configuration */
  char filename[255];
  g_snprintf(filename, 255, "%s/%s.jcfg", config_path, TMS_JANUS_PLUGIN_RTPRX_PACKAGE);
  JANUS_LOG(LOG_VERB, "配置文件: %s\n", filename);
  config = janus_config_parse(filename);
  if (config == NULL)
  {
    config = janus_config_parse(filename);
  }
  if (config != NULL)
    janus_config_print(config);

  g_atomic_int_set(&initialized, 1);

  if (config != NULL)
  {
    janus_config_category *config_general = janus_config_get_create(config, NULL, janus_config_type_category, "general");
    /* 设置可用端口范围 */
    janus_config_item *range = janus_config_get(config, config_general, janus_config_type_item, "rtp_port_range");
    if (range && range->value)
    {
      /* Split in min and max port */
      char *maxport = strrchr(range->value, '-');
      if (maxport != NULL)
      {
        *maxport = '\0';
        maxport++;
        if (janus_string_to_uint16(range->value, &rtp_range_min) < 0)
          JANUS_LOG(LOG_WARN, "Invalid RTP min port value: %s (assuming 0)\n", range->value);
        if (janus_string_to_uint16(maxport, &rtp_range_max) < 0)
          JANUS_LOG(LOG_WARN, "Invalid RTP max port value: %s (assuming 0)\n", maxport);
        maxport--;
        *maxport = '-';
      }
      if (rtp_range_min > rtp_range_max)
      {
        uint16_t temp_port = rtp_range_min;
        rtp_range_min = rtp_range_max;
        rtp_range_max = temp_port;
      }
      if (rtp_range_min % 2)
        rtp_range_min++; /* Pick an even port for RTP */
      if (rtp_range_min > rtp_range_max)
      {
        JANUS_LOG(LOG_WARN, "Incorrect port range (%u -- %u), switching min and max\n", rtp_range_min, rtp_range_max);
        uint16_t range_temp = rtp_range_max;
        rtp_range_max = rtp_range_min;
        rtp_range_min = range_temp;
      }
      if (rtp_range_max == 0)
        rtp_range_max = 65535;
      rtp_range_slider = rtp_range_min;
      JANUS_LOG(LOG_VERB, "[RTPRX] RTP/RTCP port range: %u -- %u\n", rtp_range_min, rtp_range_max);
    }
    /* 音频格式 */
    janus_config_item *audiopt = janus_config_get(config, config_general, janus_config_type_item, "audiopt");
    acodec = (audiopt && audiopt->value) ? atoi(audiopt->value) : 0;
    janus_config_item *audiortpmap = janus_config_get(config, config_general, janus_config_type_item, "audiortpmap");
    artpmap = audiortpmap ? (char *)audiortpmap->value : NULL;
    janus_config_item *audiofmtp = janus_config_get(config, config_general, janus_config_type_item, "audiofmtp");
    afmtp = audiofmtp ? (char *)audiofmtp->value : NULL;
    /* 视频格式 */
    janus_config_item *videopt = janus_config_get(config, config_general, janus_config_type_item, "videopt");
    vcodec = (videopt && videopt->value) ? atoi(videopt->value) : 0;
    janus_config_item *videortpmap = janus_config_get(config, config_general, janus_config_type_item, "videortpmap");
    vrtpmap = videortpmap ? (char *)videortpmap->value : NULL;
    janus_config_item *videofmtp = janus_config_get(config, config_general, janus_config_type_item, "videofmtp");
    vfmtp = videofmtp ? (char *)videofmtp->value : NULL;
  }

  sessions = g_hash_table_new_full(NULL, NULL, NULL, (GDestroyNotify)tms_rtprx_session_destroy);
  messages = g_async_queue_new_full((GDestroyNotify)tms_rtprx_message_free);
  /* This is the callback we'll need to invoke to contact the Janus core */
  gateway = callback;

  /* Launch the thread that will handle incoming messages */
  GError *error = NULL;
  message_handle_thread = g_thread_try_new("Rtprx handler", tms_rtprx_async_message_thread, NULL, &error);
  if (error != NULL)
  {
    g_atomic_int_set(&initialized, 0);
    JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Rtprx handler thread...\n", error->code, error->message ? error->message : "??");
    return -1;
  }
  JANUS_LOG(LOG_INFO, "[%s] 完成初始化( %p )\n", TMS_JANUS_PLUGIN_RTPRX_NAME, gateway);
  return 0;
}
/** 
 * 销毁插件 
 */
void janus_plugin_destroy_tms_rtprx(void)
{
  if (!g_atomic_int_get(&initialized))
    return;

  g_async_queue_push(messages, &exit_message);
  if (message_handle_thread != NULL)
  {
    g_thread_join(message_handle_thread);
    message_handle_thread = NULL;
  }

  janus_mutex_lock(&sessions_mutex);
  g_hash_table_destroy(sessions);
  sessions = NULL;
  janus_mutex_unlock(&sessions_mutex);
  g_async_queue_unref(messages);
  messages = NULL;

  g_atomic_int_set(&initialized, 0);
  JANUS_LOG(LOG_INFO, "销毁插件 %s\n", TMS_JANUS_PLUGIN_RTPRX_NAME);
}

int janus_plugin_get_api_compatibility_tms_rtprx(void)
{
  return JANUS_PLUGIN_API_VERSION;
}

int janus_plugin_get_version_tms_rtprx(void)
{
  return TMS_JANUS_PLUGIN_RTPRX_VERSION;
}

const char *janus_plugin_get_version_string_tms_rtprx(void)
{
  return TMS_JANUS_PLUGIN_RTPRX_VERSION_STRING;
}

const char *janus_plugin_get_description_tms_rtprx(void)
{
  return TMS_JANUS_PLUGIN_RTPRX_DESCRIPTION;
}

const char *janus_plugin_get_name_tms_rtprx(void)
{
  return TMS_JANUS_PLUGIN_RTPRX_NAME;
}

const char *janus_plugin_get_author_tms_rtprx(void)
{
  return TMS_JANUS_PLUGIN_RTPRX_AUTHOR;
}

const char *janus_plugin_get_package_tms_rtprx(void)
{
  return TMS_JANUS_PLUGIN_RTPRX_PACKAGE;
}
/* 建立新会话 */
void janus_plugin_create_session_tms_rtprx(janus_plugin_session *handle, int *error)
{
  if (!g_atomic_int_get(&initialized))
  {
    *error = -1;
    return;
  }
  tms_rtprx_session *session = g_malloc0(sizeof(tms_rtprx_session));
  session->handle = handle;
  session->mountpoint = NULL;
  session->started = FALSE; /* This will happen later */
  session->paused = FALSE;
  g_atomic_int_set(&session->destroyed, 0);
  g_atomic_int_set(&session->hangingup, 0);
  handle->plugin_handle = session;
  janus_refcount_init(&session->ref, tms_rtprx_session_free);

  janus_mutex_lock(&sessions_mutex);
  g_hash_table_insert(sessions, handle, session);
  janus_mutex_unlock(&sessions_mutex);

  JANUS_LOG(LOG_VERB, "[Janus][Rtprx] 完成创建会话[%p][%p]\n", session, session->mountpoint);

  return;
}
/* 销毁会话 */
void janus_plugin_destroy_session_tms_rtprx(janus_plugin_session *handle, int *error)
{
  JANUS_LOG(LOG_VERB, "[Janus][Rtprx] 开始销毁会话\n");
  if (!g_atomic_int_get(&initialized))
  {
    *error = -1;
    return;
  }
  janus_mutex_lock(&sessions_mutex);
  tms_rtprx_session *session = tms_rtprx_lookup_session(handle);
  if (!session)
  {
    janus_mutex_unlock(&sessions_mutex);
    JANUS_LOG(LOG_ERR, "[Rtprx] 没有找到和当前Janus会话关联的会话\n");
    *error = -2;
    return;
  }
  janus_streaming_hangup_media_internal(handle);
  g_hash_table_remove(sessions, handle);
  janus_mutex_unlock(&sessions_mutex);

  JANUS_LOG(LOG_VERB, "[Janus][Rtprx] 完成销毁会话\n");
}
/* 不知道有什么用？ */
json_t *janus_plugin_query_session_tms_rtprx(janus_plugin_session *handle)
{
  return NULL;
}

struct janus_plugin_result *janus_plugin_handle_message_tms_rtprx(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep)
{
  json_t *root = message;
  json_t *response = NULL;

  json_t *request = json_object_get(root, "request");
  const char *request_text = json_string_value(request);

  if (!strcasecmp(request_text, "create.webrtc") || !strcasecmp(request_text, "prepare.source") || !strcasecmp(request_text, "destroy.source"))
  {
    tms_rtprx_message *msg = g_malloc(sizeof(tms_rtprx_message));
    msg->handle = handle;
    msg->transaction = transaction;
    msg->message = root;
    msg->jsep = jsep;

    JANUS_LOG(LOG_VERB, "[Rtprx] 收到客户端请求[%s][%s]，进入队列等待处理\n", request_text, msg->transaction);
    g_async_queue_push(messages, msg);
  }

  return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}
/* a callback to notify you the peer PeerConnection is now ready to be used */
void janus_plugin_setup_media_tms_rtprx(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_INFO, "[%s-%p] WebRTC连接已可用\n", TMS_JANUS_PLUGIN_RTPRX_PACKAGE, handle);
  if (!g_atomic_int_get(&initialized))
    return;
  janus_mutex_lock(&sessions_mutex);
  tms_rtprx_session *session = tms_rtprx_lookup_session(handle);
  if (!session)
  {
    janus_mutex_unlock(&sessions_mutex);
    JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
    return;
  }
  if (g_atomic_int_get(&session->destroyed))
  {
    janus_mutex_unlock(&sessions_mutex);
    return;
  }
  janus_refcount_increase(&session->ref);
  janus_mutex_unlock(&sessions_mutex);

  g_atomic_int_set(&session->hangingup, 0);
  session->started = TRUE;

  /* Prepare JSON event */
  json_t *event = json_object();
  json_object_set_new(event, "rtprx", json_string("event"));
  json_t *result = json_object();
  json_object_set_new(result, "status", json_string("started"));
  json_object_set_new(event, "result", result);
  int ret = gateway->push_event(handle, &janus_plugin_tms_rtprx, NULL, event, NULL);
  JANUS_LOG(LOG_VERB, "  >> 推送事件: %d (%s)\n", ret, janus_get_api_error(ret));
  json_decref(event);
  janus_refcount_decrease(&session->ref);
}

static void janus_streaming_hangup_media_internal(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_VERB, "[%s-%p] 开始释放WebTRC挂断后的资源\n", TMS_JANUS_PLUGIN_RTPRX_NAME, handle);
  if (!g_atomic_int_get(&initialized))
    return;
  tms_rtprx_session *session = tms_rtprx_lookup_session(handle);
  if (!session)
  {
    JANUS_LOG(LOG_ERR, "No session associated with this handle...\n");
    return;
  }
  if (g_atomic_int_get(&session->destroyed))
    return;
  if (!g_atomic_int_compare_and_exchange(&session->hangingup, 0, 1))
    return;
  //janus_rtp_switching_context_reset(&session->context);
  // janus_rtp_simulcasting_context_reset(&session->sim_context);
  // janus_vp8_simulcast_context_reset(&session->vp8_context);
  // session->spatial_layer = -1;
  // session->target_spatial_layer = 2; /* FIXME Chrome sends 0, 1 and 2 (if using EnabledByFlag_3SL3TL) */
  // session->last_spatial_layer[0] = 0;
  // session->last_spatial_layer[1] = 0;
  // session->last_spatial_layer[2] = 0;
  // session->temporal_layer = -1;
  // session->target_temporal_layer = 2; /* FIXME Chrome sends 0, 1 and 2 */
  session->stopping = TRUE;
  session->started = FALSE;
  session->paused = FALSE;
  tms_rtprx_mountpoint *mp = session->mountpoint;
  if (mp != NULL)
  {
    JANUS_LOG(LOG_VERB, "[Rtprx] WebTRC挂断后，释放会话关联的挂载点[%p][%p]\n", session, mp);
    tms_rtprx_mountpoint_destroy(mp);
  }
  g_atomic_int_set(&session->hangingup, 0);

  JANUS_LOG(LOG_VERB, "[%s-%p] 完成释放WebTRC挂断后的资源\n", TMS_JANUS_PLUGIN_RTPRX_NAME, handle);
}

void janus_plugin_hangup_media_tms_rtprx(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_INFO, "[Janus][%s-%p] WebRTC连接已挂断\n", TMS_JANUS_PLUGIN_RTPRX_NAME, handle);
  janus_mutex_lock(&sessions_mutex);
  janus_streaming_hangup_media_internal(handle);
  janus_mutex_unlock(&sessions_mutex);
}