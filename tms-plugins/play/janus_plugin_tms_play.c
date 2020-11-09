#include <jansson.h>

#include <config.h>
#include <plugins/plugin.h>

#include <libavformat/avformat.h>

#include "tms_play.h"

#define TMS_JANUS_PLUGIN_PLAY_VERSION 1
#define TMS_JANUS_PLUGIN_PLAY_VERSION_STRING "0.0.1"
#define TMS_JANUS_PLUGIN_PLAY_DESCRIPTION "播放mp4 mp3 wav等格式文件"
#define TMS_JANUS_PLUGIN_PLAY_NAME "TmsPlay"
#define TMS_JANUS_PLUGIN_PLAY_AUTHOR "Jasony62"
#define TMS_JANUS_PLUGIN_PLAY_PACKAGE "janus.plugin.tms.play"

janus_plugin *create(void);
int janus_plugin_init_tms_play(janus_callbacks *callback, const char *config_path);
void janus_plugin_destroy_tms_play(void);
int janus_plugin_get_api_compatibility_tms_play(void);
int janus_plugin_get_version_tms_play(void);
const char *janus_plugin_get_version_string_tms_play(void);
const char *janus_plugin_get_description_tms_play(void);
const char *janus_plugin_get_package_tms_play(void);
const char *janus_plugin_get_name_tms_play(void);
const char *janus_plugin_get_author_tms_play(void);
void janus_plugin_create_session_tms_play(janus_plugin_session *handle, int *error);
json_t *janus_plugin_query_session_tms_play(janus_plugin_session *handle);
void janus_plugin_destroy_session_tms_play(janus_plugin_session *handle, int *error);
void janus_plugin_setup_media_tms_play(janus_plugin_session *handle);
void janus_plugin_hangup_media_tms_play(janus_plugin_session *handle);
struct janus_plugin_result *janus_plugin_handle_message_tms_play(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);

/* 指定实现插件接口的方法 */
static janus_plugin janus_plugin_tms_play =
    JANUS_PLUGIN_INIT(
            .init = janus_plugin_init_tms_play,
            .destroy = janus_plugin_destroy_tms_play,

            .get_api_compatibility = janus_plugin_get_api_compatibility_tms_play,
            .get_version = janus_plugin_get_version_tms_play,
            .get_version_string = janus_plugin_get_version_string_tms_play,
            .get_description = janus_plugin_get_description_tms_play,
            .get_package = janus_plugin_get_package_tms_play,
            .get_name = janus_plugin_get_name_tms_play,
            .get_author = janus_plugin_get_author_tms_play,

            .create_session = janus_plugin_create_session_tms_play,
            .query_session = janus_plugin_query_session_tms_play,
            .destroy_session = janus_plugin_destroy_session_tms_play,

            .setup_media = janus_plugin_setup_media_tms_play,
            .hangup_media = janus_plugin_hangup_media_tms_play,

            .handle_message = janus_plugin_handle_message_tms_play, );

/* Static configuration instance */
static janus_config *config = NULL;
static char *media_root = NULL; // 媒体文件存储位置

/* 生成jsep offer sdp */
static void tms_play_create_offer_sdp(char **sdp, gboolean doaudio, gboolean dovideo)
{
  gint64 sdp_version = 1;
  gint64 sdp_sessid = janus_get_real_time();
  uint8_t aport = 1, acodec = 8;
  char *artpmap = "PCMA/8000";
  uint8_t vport = 1, vcodec = 96;
  char *vrtpmap = "H264/90000";

  char sdptemp[2048];
  memset(sdptemp, 0, 2048);
  gchar buffer[512];
  memset(buffer, 0, 512);
  g_snprintf(buffer, 512, "v=0\r\no=%s %" SCNu64 " %" SCNu64 " IN IP4 127.0.0.1\r\n", "-", sdp_sessid, sdp_version);
  g_strlcat(sdptemp, buffer, 2048);
  g_snprintf(buffer, 512, "s=TmsPlay\r\n");
  g_strlcat(sdptemp, buffer, 2048);
  g_strlcat(sdptemp, "t=0 0\r\n", 2048);
  /* Add audio line */
  if (doaudio)
  {
    g_snprintf(buffer, 512, "m=audio %d RTP/AVP %d\r\n"
                            "c=IN IP4 1.1.1.1\r\n",
               aport, acodec);
    g_strlcat(sdptemp, buffer, 2048);
    //g_snprintf(buffer, 512, "a=rtpmap:%d %s\r\n", acodec, artpmap);
    //g_strlcat(sdptemp, buffer, 2048);
    g_strlcat(sdptemp, "b=AS:64\r\n", 2048);
    g_strlcat(sdptemp, "a=sendonly\r\n", 2048);
  }
  /* Add video line */
  if (dovideo)
  {
    g_snprintf(buffer, 512, "m=video %d RTP/SAVPF %d\r\n"
                            "c=IN IP4 1.1.1.1\r\n",
               vport, vcodec);
    g_strlcat(sdptemp, buffer, 2048);
    g_snprintf(buffer, 512, "a=rtpmap:%d %s\r\n", vcodec, vrtpmap);
    g_strlcat(sdptemp, buffer, 2048);
    // g_snprintf(buffer, 512, "a=rtcp-fb:%d nack\r\n", vcodec);
    // g_strlcat(sdptemp, buffer, 2048);
    // g_snprintf(buffer, 512, "a=rtcp-fb:%d nack pli\r\n", vcodec);
    // g_strlcat(sdptemp, buffer, 2048);
    // g_snprintf(buffer, 512, "a=rtcp-fb:%d goog-remb\r\n", vcodec);
    // g_strlcat(sdptemp, buffer, 2048);
    g_strlcat(sdptemp, "a=sendonly\r\n", 2048);
  }

  *sdp = g_strdup(sdptemp);
}
/* 插件状态 */
static volatile gint initialized = 0;
/* 调用janus基础功能 */
static janus_callbacks *gateway = NULL;

/*************************************************
 * 插件ffmpeg媒体播放
 * ffmpeg运行在独立的线程中，获取数据状态时要加锁
 *************************************************/
/* 发送ffmpeg播放状态 */
static void tms_play_ffmpeg_send_event(tms_play_ffmpeg *ffmpeg, char *msg)
{
  janus_plugin_session *handle = ffmpeg->handle;
  json_t *event = json_object();
  json_object_set_new(event, "tms_play_event", json_string(msg));
  int ret = gateway->push_event(handle, &janus_plugin_tms_play, NULL, event, NULL);
  if (ret < 0)
    JANUS_LOG(LOG_VERB, "[TmsPlay] >> 推送事件: %d (%s)\n", ret, janus_get_api_error(ret));
}
/* 异步ffmpeg媒体播放 */
static void *tms_play_async_ffmpeg_thread(void *data)
{
  JANUS_LOG(LOG_VERB, "[TmsPlay] 启动异步媒体处理线程\n");

  tms_play_ffmpeg *ffmpeg = (tms_play_ffmpeg *)data;
  janus_plugin_session *handle = ffmpeg->handle;
  tms_play_main(gateway, handle, ffmpeg);

  /* 通知线程已经结束 */
  janus_mutex_lock(&ffmpeg->mutex);
  if (!g_atomic_int_get(&ffmpeg->destroyed))
  {
    tms_play_ffmpeg_send_event(ffmpeg, "exit.play");
  }
  janus_mutex_unlock(&ffmpeg->mutex);

  // 引用次数减1
  janus_refcount_decrease(&ffmpeg->ref);

  JANUS_LOG(LOG_VERB, "[TmsPlay] 结束媒体发送线程\n");
}
/* 销毁ffmpeg实例。可能播放线程仍然在执行，修改状态，需要加锁。 */
static void tms_play_ffmpeg_destroy(tms_play_ffmpeg *ffmpeg)
{
  JANUS_LOG(LOG_VERB, "[TmsPlay] 开始销毁ffmpeg\n");

  janus_mutex_lock(&ffmpeg->mutex);

  g_atomic_int_set(&ffmpeg->destroyed, 1);
  g_atomic_pointer_set(&ffmpeg->handle, NULL);
  g_free(ffmpeg->filename);

  janus_mutex_unlock(&ffmpeg->mutex);

  JANUS_LOG(LOG_VERB, "[TmsPlay] 完成销毁ffmpeg\n");
}
/**
 * 释放ffmpeg实例，通过引用计数调用
 */
static void tms_play_ffmpeg_ref_free(const janus_refcount *ffmpeg_ref)
{
  tms_play_ffmpeg *ffmpeg = janus_refcount_containerof(ffmpeg_ref, tms_play_ffmpeg, ref);
  JANUS_LOG(LOG_VERB, "[TmsPlay] 开始释放ffmpeg %p\n", ffmpeg);

  g_free(ffmpeg);

  JANUS_LOG(LOG_VERB, "[TmsPlay] 完成释放ffmpeg\n");
}

/***********************************
 * 插件会话 
 ***********************************/
typedef struct tms_play_session
{
  janus_plugin_session *handle;
  janus_refcount ref;
  tms_play_ffmpeg *ffmpeg;
  volatile gint webrtcup;
  int64_t create_time_us; // 会话创建时间，单位：微秒
} tms_play_session;
/**
 * 释放会话
 */
static void tms_play_session_free(tms_play_session *session)
{
  JANUS_LOG(LOG_VERB, "[TmsPlay] 开始释放会话\n");

  /* 去掉对其它资源的引用 */
  if (session->ffmpeg)
  {
    tms_play_ffmpeg_destroy(session->ffmpeg);
    janus_refcount_decrease(&session->ffmpeg->ref);
    session->ffmpeg = NULL;
  }
  // 为什么不用释放？janus的session会进行释放？
  // g_free(session);

  JANUS_LOG(LOG_VERB, "[TmsPlay] 完成释放会话\n");
}
/*************************************
 * 插件消息
 *************************************/
typedef struct tms_play_message
{
  janus_plugin_session *handle;
  char *transaction;
  json_t *message;
  json_t *jsep;
} tms_play_message;
static GAsyncQueue *messages = NULL;
static tms_play_message exit_message;
static GThread *message_handle_thread;
/**
 * 释放异步消息 
 */
static void tms_play_message_free(tms_play_message *msg)
{
  JANUS_LOG(LOG_VERB, "[TmsPlay] 开始释放异步消息\n");
  if (!msg || msg == &exit_message)
    return;

  if (msg->handle && msg->handle->plugin_handle)
  {
    tms_play_session *session = (tms_play_session *)msg->handle->plugin_handle;
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

  JANUS_LOG(LOG_VERB, "[TmsPlay] 完成释放异步消息\n");
}
/**
 * 异步消息处理 
 */
static void *tms_play_async_message_thread(void *data)
{
  JANUS_LOG(LOG_VERB, "[TmsPlay] 启动异步消息处理线程\n");

  tms_play_message *msg = NULL;
  json_t *root = NULL;
  while (g_atomic_int_get(&initialized))
  {
    msg = g_async_queue_pop(messages);
    if (msg == &exit_message)
      break;
    if (msg->handle == NULL || msg->handle->plugin_handle == NULL)
    {
      tms_play_message_free(msg);
      continue;
    }

    tms_play_session *session = (tms_play_session *)msg->handle->plugin_handle;

    root = msg->message;
    json_t *request = json_object_get(root, "request");
    const char *request_text = json_string_value(request);

    if (!strcasecmp(request_text, "request.offer"))
    {
      /* 要求服务端创建Offer，发起Webrtc连接 */
      json_t *event = json_object();
      json_object_set_new(event, "tms_play_event", json_string("create.offer"));

      char *sdp = NULL;
      tms_play_create_offer_sdp(&sdp, TRUE, TRUE);
      JANUS_LOG(LOG_VERB, "[TmsPlay] 创建Offer SDP:\n%s\n", sdp);
      json_t *jsep = json_pack("{ssss}", "type", "offer", "sdp", sdp);

      int ret = gateway->push_event(msg->handle, &janus_plugin_tms_play, msg->transaction, event, jsep);
      if (ret < 0)
        JANUS_LOG(LOG_VERB, "[TmsPlay] >> 推送事件: %d (%s)\n", ret, janus_get_api_error(ret));

      g_free(sdp);
      json_decref(event);
    }
    else if (g_atomic_int_get(&session->webrtcup))
    {
      /* Webrtc连接已经建立，可以控制媒体播放 */
      if (NULL != strstr(request_text, ".file"))
      {
        tms_play_ffmpeg *ffmpeg;
        if (!strcasecmp(request_text, "play.file"))
        {
          /* 指定要播放的文件 */
          char fullpath[512]; // 要播放文件的完整路径
          memset(fullpath, 9, 512);
          json_t *file = json_object_get(root, "file");
          const char *filename = json_string_value(file);
          g_snprintf(fullpath, 512, "%s/%s", media_root, filename);

          /* 应该检查文件是否存在！！！ */
          JANUS_LOG(LOG_VERB, "[TmsPlay] 准备播放文件%s\n", fullpath);

          /* 要求播放指定的文件 */
          if (!session->ffmpeg)
          {
            ffmpeg = g_malloc0(sizeof(tms_play_ffmpeg));
            ffmpeg->handle = msg->handle;
            ffmpeg->webrtcup = 1;
            ffmpeg->playing = 1;
            ffmpeg->destroyed = 0;
            ffmpeg->nb_audio_rtps = 0;
            ffmpeg->base_timestamp = session->create_time_us; // 在一次会话中，为了支持多次播放，采用会话的创建时间作为媒体RTP时间戳的基础时间
            session->ffmpeg = ffmpeg;

            janus_mutex_init(&ffmpeg->mutex);
            janus_refcount_init(&ffmpeg->ref, tms_play_ffmpeg_ref_free);
            janus_refcount_increase(&ffmpeg->ref); // 会话使用，引用加1
          }
          else
          {
            ffmpeg = session->ffmpeg;
            g_free(ffmpeg->filename);
          }
          ffmpeg->filename = g_strdup(fullpath);

          /* 启用ffmpeg媒体播放线程 */
          GError *error = NULL;
          janus_refcount_increase(&ffmpeg->ref); // 线程使用，引用加1
          g_thread_try_new("TmsPlay ffmpeg thread", tms_play_async_ffmpeg_thread, ffmpeg, &error);
          if (error != NULL)
          {
            JANUS_LOG(LOG_ERR, "启动ffmpeg媒体播放线程发生错误：%d (%s)\n", error->code, error->message ? error->message : "??");
            janus_refcount_decrease(&ffmpeg->ref);
          }
          else
          {
            /* 通知启用了媒体播放线程 */
            json_t *event = json_object();
            json_object_set_new(event, "tms_play_event", json_string("launch.play"));
            int ret = gateway->push_event(msg->handle, &janus_plugin_tms_play, msg->transaction, event, NULL);
            if (ret < 0)
              JANUS_LOG(LOG_VERB, "[TmsPlay] >> 推送事件: %d (%s)\n", ret, janus_get_api_error(ret));
          }
        }
        else if (!strcasecmp(request_text, "pause.file"))
        {
          g_atomic_int_set(&ffmpeg->playing, 2);
        }
        else if (!strcasecmp(request_text, "resume.file"))
        {
          g_atomic_int_set(&ffmpeg->playing, 1);
          /* 通知恢复了媒体播放线程 */
          json_t *event = json_object();
          json_object_set_new(event, "tms_play_event", json_string("resume.play"));
          int ret = gateway->push_event(msg->handle, &janus_plugin_tms_play, msg->transaction, event, NULL);
          if (ret < 0)
            JANUS_LOG(LOG_VERB, "[TmsPlay] >> 推送事件: %d (%s)\n", ret, janus_get_api_error(ret));
        }
        else if (!strcasecmp(request_text, "stop.file"))
        {
          g_atomic_int_set(&ffmpeg->playing, 0);
        }
      }
    }
    tms_play_message_free(msg);
  }

  JANUS_LOG(LOG_VERB, "[TmsPlay] 结束消息处理线程\n");
}

/**************************************
 * 插件基本信息描述 
 **************************************/

int janus_plugin_get_api_compatibility_tms_play(void)
{
  return JANUS_PLUGIN_API_VERSION;
}

int janus_plugin_get_version_tms_play(void)
{
  return TMS_JANUS_PLUGIN_PLAY_VERSION;
}

const char *janus_plugin_get_version_string_tms_play(void)
{
  return TMS_JANUS_PLUGIN_PLAY_VERSION_STRING;
}

const char *janus_plugin_get_description_tms_play(void)
{
  return TMS_JANUS_PLUGIN_PLAY_DESCRIPTION;
}

const char *janus_plugin_get_package_tms_play(void)
{
  return TMS_JANUS_PLUGIN_PLAY_PACKAGE;
}

const char *janus_plugin_get_name_tms_play(void)
{
  return TMS_JANUS_PLUGIN_PLAY_NAME;
}

const char *janus_plugin_get_author_tms_play(void)
{
  return TMS_JANUS_PLUGIN_PLAY_AUTHOR;
}

/**************************************
 *  插件生命周期方法 
 **************************************/

/* 创建插件 */
janus_plugin *create(void)
{
  JANUS_LOG(LOG_VERB, "创建插件 %s\n", TMS_JANUS_PLUGIN_PLAY_NAME);
  return &janus_plugin_tms_play;
}

/* 初始化插件 */
int janus_plugin_init_tms_play(janus_callbacks *callback, const char *config_path)
{
  /* Read configuration */
  char filename[255];
  g_snprintf(filename, 255, "%s/%s.jcfg", config_path, TMS_JANUS_PLUGIN_PLAY_PACKAGE);
  JANUS_LOG(LOG_VERB, "[TmsPlay] 配置文件：%s\n", filename);
  config = janus_config_parse(filename);

  /* Parse configuration */
  if (config != NULL)
  {
    janus_config_print(config);
    janus_config_category *config_general = janus_config_get_create(config, NULL, janus_config_type_category, "general");
    janus_config_item *item_media_root = janus_config_get(config, config_general, janus_config_type_item, "media_root");
    if (item_media_root != NULL && item_media_root->value != NULL)
      media_root = g_strdup(item_media_root->value);
    JANUS_LOG(LOG_VERB, "[TmsPlay] 媒体文件根目录：%s\n", media_root);
  }

  g_atomic_int_set(&initialized, 1);

  /* 需要异步处理的消息 */
  messages = g_async_queue_new_full((GDestroyNotify)tms_play_message_free);
  /* This is the callback we'll need to invoke to contact the Janus core */
  gateway = callback;

  /* Launch the thread that will handle incoming messages */
  GError *error = NULL;
  message_handle_thread = g_thread_try_new("TmsPlay message thread", tms_play_async_message_thread, NULL, &error);
  if (error != NULL)
  {
    g_atomic_int_set(&initialized, 0);
    JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Rtprx handler thread...\n", error->code, error->message ? error->message : "??");
    return -1;
  }

  return 0;
}

/* 销毁插件，释放资源 */
void janus_plugin_destroy_tms_play(void)
{
  if (!g_atomic_int_get(&initialized))
    return;

  g_async_queue_push(messages, &exit_message);
  if (message_handle_thread != NULL)
  {
    g_thread_join(message_handle_thread);
    message_handle_thread = NULL;
  }
  g_async_queue_unref(messages);
  messages = NULL;

  g_atomic_int_set(&initialized, 0);

  /* 释放配置文件数据 */
  janus_config_destroy(config);
  g_free(media_root);

  JANUS_LOG(LOG_INFO, "销毁插件 %s\n", TMS_JANUS_PLUGIN_PLAY_NAME);
}

/**************************************
 *  会话生命周期方法 
 **************************************/
/* 创建插件 */
void janus_plugin_create_session_tms_play(janus_plugin_session *handle, int *error)
{
  JANUS_LOG(LOG_VERB, "[TmsPlay][%p] 创建会话\n", handle);

  /* 创建本地会话，记录状态信息 */
  tms_play_session *session = g_malloc0(sizeof(tms_play_session));
  session->handle = handle;
  session->webrtcup = 0;
  handle->plugin_handle = session;
  session->create_time_us = av_gettime_relative();
}
/* 必须有，怎么用？返回json对象，记录和session关联的业务信息 */
json_t *janus_plugin_query_session_tms_play(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_VERB, "[%s][%p] 查找会话\n", TMS_JANUS_PLUGIN_PLAY_NAME, handle);

  return NULL;
}
/* 销毁插件 */
void janus_plugin_destroy_session_tms_play(janus_plugin_session *handle, int *error)
{
  /* 释放资源 */
  if (handle->plugin_handle)
  {
    tms_play_session *session = (tms_play_session *)handle->plugin_handle;
    tms_play_session_free(session);
  }

  JANUS_LOG(LOG_VERB, "[%s][%p] 完成销毁会话\n", TMS_JANUS_PLUGIN_PLAY_NAME, handle);
}

/**************************************
 *  媒体生命周期方法 
 **************************************/
/* a callback to notify you the peer PeerConnection is now ready to be used */
void janus_plugin_setup_media_tms_play(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_VERB, "[%s][%p] Webrtc连接已建立\n", TMS_JANUS_PLUGIN_PLAY_NAME, handle);

  if (handle->plugin_handle)
  {
    tms_play_session *session = (tms_play_session *)handle->plugin_handle;
    g_atomic_int_set(&session->webrtcup, 1);
  }
}
void janus_plugin_hangup_media_tms_play(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_VERB, "[%s][%p] Webrtc连接已挂断\n", TMS_JANUS_PLUGIN_PLAY_NAME, handle);

  if (handle->plugin_handle)
  {
    tms_play_session *session = (tms_play_session *)handle->plugin_handle;
    g_atomic_int_set(&session->webrtcup, 0);
    if (session->ffmpeg)
    {
      g_atomic_int_set(&session->ffmpeg->webrtcup, 0);
    }
  }
}

/**************************************
 *  消息处理 
 **************************************/

struct janus_plugin_result *janus_plugin_handle_message_tms_play(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep)
{
  json_t *root = message;
  json_t *response = NULL;

  json_t *request = json_object_get(root, "request");
  const char *request_text = json_string_value(request);

  JANUS_LOG(LOG_VERB, "[%s][%p] 收到客户端请求[%s][%s]\n", TMS_JANUS_PLUGIN_PLAY_NAME, handle, request_text, transaction);

  /* 测试用 */
  if (!strcasecmp(request_text, "ping"))
  {
    json_t *response = json_object();
    json_object_set_new(response, "msg", json_string("pong"));

    return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
  }
  else if (!strcasecmp(request_text, "probe.file"))
  {
    /* 指定要播放的文件 */
    char fullpath[512]; // 要播放文件的完整路径
    memset(fullpath, 9, 512);
    json_t *file = json_object_get(root, "file");
    const char *filename = json_string_value(file);
    g_snprintf(fullpath, 512, "%s/%s", media_root, filename);

    /* 应该检查文件是否存在！！！ */
    int ret;
    AVFormatContext *ictx = NULL;
    /* 打开指定的媒体文件 */
    if ((ret = avformat_open_input(&ictx, fullpath, NULL, NULL)) < 0)
    {
      JANUS_LOG(LOG_VERB, "[TmsPlay] 无法打开媒体文件 %s\n", fullpath);

      response = json_object();
      json_object_set_new(response, "code", json_integer(404));
      json_object_set_new(response, "path", json_string(fullpath));

      return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
    }

    /* 获得指定的视频文件的信息 */
    if ((ret = avformat_find_stream_info(ictx, NULL)) < 0)
    {
      JANUS_LOG(LOG_VERB, "[TmsPlay] 无法获取文件媒体流信息 %s\n", fullpath);

      response = json_object();
      json_object_set_new(response, "code", json_integer(400));
      json_object_set_new(response, "path", json_string(fullpath));
      json_object_set_new(response, "reason", json_string("无法获取文件媒体流信息"));

      return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
    }

    JANUS_LOG(LOG_VERB, "[TmsPlay] 媒体文件 %s nb_streams = %d , duration = %ld\n", fullpath, ictx->nb_streams, ictx->duration);

    response = json_object();
    json_object_set_new(response, "code", json_integer(0));
    json_object_set_new(response, "streams", json_integer(ictx->nb_streams));
    json_object_set_new(response, "duration", json_integer(ictx->duration));

    return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
  }
  /* 如果没有关联session无法处理后续逻辑 */
  if (!handle->plugin_handle)
  {
    return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "未正确建立和插件的绑定，无法能执行指定操作", NULL);
  }

  /* 放入队列异步处理的消息 */
  if (!strcasecmp(request_text, "request.offer") || NULL != strstr(request_text, ".file"))
  {
    if (NULL != strstr(request_text, ".file"))
    {
      /* 检查指定的播放文件 */
      if (!strcasecmp(request_text, "play.file"))
      {
        json_t *file = json_object_get(root, "file");
        if (NULL == file)
        {
          response = json_object();
          json_object_set_new(response, "code", json_integer(400));
          json_object_set_new(response, "reason", json_string("没有指定要播放的文件"));
          return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
        }
      }
      /* 检查通道连接情况 */
      tms_play_session *session = (tms_play_session *)handle->plugin_handle;
      if (!session->webrtcup)
      {
        return janus_plugin_result_new(JANUS_PLUGIN_ERROR, "Webrtc连接未建立，不能执行指定操作", NULL);
      }
    }

    tms_play_message *msg = g_malloc(sizeof(tms_play_message));
    msg->handle = handle;
    msg->transaction = transaction;
    msg->message = root;
    msg->jsep = jsep;

    JANUS_LOG(LOG_VERB, "[%s][%p] 收到客户端请求[%s][%s]，进入队列等待处理\n", TMS_JANUS_PLUGIN_PLAY_NAME, handle, request_text, msg->transaction);
    g_async_queue_push(messages, msg);
  }

  return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}
