#include <jansson.h>

#include <plugins/plugin.h>

#define TMS_JANUS_PLUGIN_MP4_VERSION 1
#define TMS_JANUS_PLUGIN_MP4_VERSION_STRING "0.0.1"
#define TMS_JANUS_PLUGIN_MP4_DESCRIPTION "play mp4 file"
#define TMS_JANUS_PLUGIN_MP4_NAME "PlayMp4"
#define TMS_JANUS_PLUGIN_MP4_AUTHOR "Jason Young"
#define TMS_JANUS_PLUGIN_MP4_PACKAGE "janus.plugin.tms.mp4"

janus_plugin *create(void);
int janus_plugin_init_tms_mp4(janus_callbacks *callback, const char *config_path);
void janus_plugin_destroy_tms_mp4(void);
int janus_plugin_get_api_compatibility_tms_mp4(void);
int janus_plugin_get_version_tms_mp4(void);
const char *janus_plugin_get_version_string_tms_mp4(void);
const char *janus_plugin_get_description_tms_mp4(void);
const char *janus_plugin_get_package_tms_mp4(void);
const char *janus_plugin_get_name_tms_mp4(void);
const char *janus_plugin_get_author_tms_mp4(void);
void janus_plugin_create_session_tms_mp4(janus_plugin_session *handle, int *error);
json_t *janus_plugin_query_session_tms_mp4(janus_plugin_session *handle);
void janus_plugin_destroy_session_tms_mp4(janus_plugin_session *handle, int *error);
void janus_plugin_setup_media_tms_mp4(janus_plugin_session *handle);
void janus_plugin_hangup_media_tms_mp4(janus_plugin_session *handle);
struct janus_plugin_result *janus_plugin_handle_message_tms_mp4(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep);

/* 指定实现插件接口的方法 */
static janus_plugin janus_plugin_tms_mp4 =
    JANUS_PLUGIN_INIT(
            .init = janus_plugin_init_tms_mp4,
            .destroy = janus_plugin_destroy_tms_mp4,

            .get_api_compatibility = janus_plugin_get_api_compatibility_tms_mp4,
            .get_version = janus_plugin_get_version_tms_mp4,
            .get_version_string = janus_plugin_get_version_string_tms_mp4,
            .get_description = janus_plugin_get_description_tms_mp4,
            .get_package = janus_plugin_get_package_tms_mp4,
            .get_name = janus_plugin_get_name_tms_mp4,
            .get_author = janus_plugin_get_author_tms_mp4,

            .create_session = janus_plugin_create_session_tms_mp4,
            .query_session = janus_plugin_query_session_tms_mp4,
            .destroy_session = janus_plugin_destroy_session_tms_mp4,

            .setup_media = janus_plugin_setup_media_tms_mp4,
            .hangup_media = janus_plugin_hangup_media_tms_mp4,

            .handle_message = janus_plugin_handle_message_tms_mp4, );

/* */
static void tms_mp4_create_offer_sdp(char **sdp, gboolean doaudio, gboolean dovideo)
{
  gint64 sdp_version = 1;
  gint64 sdp_sessid = janus_get_real_time();
  uint8_t aport = 1, acodec = 111;
  char *artpmap = "opus/48000/2";
  uint8_t vport = 1, vcodec = 96;
  char *vrtpmap = "VP8/90000";

  char sdptemp[2048];
  memset(sdptemp, 0, 2048);
  gchar buffer[512];
  memset(buffer, 0, 512);
  g_snprintf(buffer, 512, "v=0\r\no=%s %" SCNu64 " %" SCNu64 " IN IP4 127.0.0.1\r\n", "-", sdp_sessid, sdp_version);
  g_strlcat(sdptemp, buffer, 2048);
  g_snprintf(buffer, 512, "s=PlayMp4\r\n");
  g_strlcat(sdptemp, buffer, 2048);
  g_strlcat(sdptemp, "t=0 0\r\n", 2048);
  /* Add audio line */
  if (doaudio)
  {
    g_snprintf(buffer, 512, "m=audio %d RTP/SAVPF %d\r\n"
                            "c=IN IP4 1.1.1.1\r\n",
               aport, acodec);
    g_strlcat(sdptemp, buffer, 2048);
    g_snprintf(buffer, 512, "a=rtpmap:%d %s\r\n", acodec, artpmap);
    g_strlcat(sdptemp, buffer, 2048);
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
    g_snprintf(buffer, 512, "a=rtcp-fb:%d nack\r\n", vcodec);
    g_strlcat(sdptemp, buffer, 2048);
    g_snprintf(buffer, 512, "a=rtcp-fb:%d nack pli\r\n", vcodec);
    g_strlcat(sdptemp, buffer, 2048);
    g_snprintf(buffer, 512, "a=rtcp-fb:%d goog-remb\r\n", vcodec);
    g_strlcat(sdptemp, buffer, 2048);
    g_strlcat(sdptemp, "a=sendonly\r\n", 2048);
  }

  *sdp = g_strdup(sdptemp);
}
/* 插件状态 */
static volatile gint initialized = 0;
/* 调用janus基础功能 */
static janus_callbacks *gateway = NULL;

/***********************************
 * 插件会话 
 ***********************************/
typedef struct tms_mp4_session
{
  janus_plugin_session *handle;
  janus_refcount ref;
} tms_mp4_session;

/*************************************
 * 插件消息
 *************************************/

typedef struct tms_mp4_message
{
  janus_plugin_session *handle;
  char *transaction;
  json_t *message;
  json_t *jsep;
} tms_mp4_message;
static GAsyncQueue *messages = NULL;
static tms_mp4_message exit_message;
static GThread *message_handle_thread;
/**
 * 释放异步消息 
 */
static void tms_mp4_message_free(tms_mp4_message *msg)
{
  JANUS_LOG(LOG_VERB, "[PlayMp4] 开始释放异步消息\n");
  if (!msg || msg == &exit_message)
    return;

  if (msg->handle && msg->handle->plugin_handle)
  {
    tms_mp4_session *session = (tms_mp4_session *)msg->handle->plugin_handle;
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

  JANUS_LOG(LOG_VERB, "[PlayMp4] 完成释放异步消息\n");
}
/**
 * 异步消息处理 
 */
static void *tms_mp4_async_message_thread(void *data)
{
  JANUS_LOG(LOG_VERB, "[PlayMp4] 启动异步消息处理线程\n");

  tms_mp4_message *msg = NULL;
  json_t *root = NULL;
  while (g_atomic_int_get(&initialized))
  {
    msg = g_async_queue_pop(messages);
    if (msg == &exit_message)
      break;
    if (msg->handle == NULL)
    {
      tms_mp4_message_free(msg);
      continue;
    }

    root = msg->message;
    json_t *request = json_object_get(root, "request");
    const char *request_text = json_string_value(request);

    if (!strcasecmp(request_text, "request.offer"))
    {
      json_t *event = json_object();
      json_object_set_new(event, "playmp4", json_string("create.offer"));

      char *sdp = NULL;
      tms_mp4_create_offer_sdp(&sdp, TRUE, TRUE);
      JANUS_LOG(LOG_VERB, "[PlayMp4] 创建Offer SDP:\n%s\n", sdp);
      json_t *jsep = json_pack("{ssss}", "type", "offer", "sdp", sdp);

      int ret = gateway->push_event(msg->handle, &janus_plugin_tms_mp4, msg->transaction, event, jsep);
      JANUS_LOG(LOG_VERB, "[PlayMp4] >> 推送事件: %d (%s)\n", ret, janus_get_api_error(ret));

      g_free(sdp);
      json_decref(event);
    }

    tms_mp4_message_free(msg);
  }

  JANUS_LOG(LOG_VERB, "[PlayMp4] 离开插件消息处理线程\n");
}
/**************************************
 *  插件基本信息描述 
 **************************************/

int janus_plugin_get_api_compatibility_tms_mp4(void)
{
  return JANUS_PLUGIN_API_VERSION;
}

int janus_plugin_get_version_tms_mp4(void)
{
  return TMS_JANUS_PLUGIN_MP4_VERSION;
}

const char *janus_plugin_get_version_string_tms_mp4(void)
{
  return TMS_JANUS_PLUGIN_MP4_VERSION_STRING;
}

const char *janus_plugin_get_description_tms_mp4(void)
{
  return TMS_JANUS_PLUGIN_MP4_DESCRIPTION;
}

const char *janus_plugin_get_package_tms_mp4(void)
{
  return TMS_JANUS_PLUGIN_MP4_PACKAGE;
}

const char *janus_plugin_get_name_tms_mp4(void)
{
  return TMS_JANUS_PLUGIN_MP4_NAME;
}

const char *janus_plugin_get_author_tms_mp4(void)
{
  return TMS_JANUS_PLUGIN_MP4_AUTHOR;
}

/**************************************
 *  插件生命周期方法 
 **************************************/

/* 创建插件 */
janus_plugin *create(void)
{
  JANUS_LOG(LOG_VERB, "创建插件 %s\n", TMS_JANUS_PLUGIN_MP4_NAME);
  return &janus_plugin_tms_mp4;
}

/* 初始化插件 */
int janus_plugin_init_tms_mp4(janus_callbacks *callback, const char *config_path)
{
  g_atomic_int_set(&initialized, 1);

  /* 需要异步处理的消息 */
  messages = g_async_queue_new_full((GDestroyNotify)tms_mp4_message_free);
  /* This is the callback we'll need to invoke to contact the Janus core */
  gateway = callback;

  /* Launch the thread that will handle incoming messages */
  GError *error = NULL;
  message_handle_thread = g_thread_try_new("PlayMp4 message thread", tms_mp4_async_message_thread, NULL, &error);
  if (error != NULL)
  {
    g_atomic_int_set(&initialized, 0);
    JANUS_LOG(LOG_ERR, "Got error %d (%s) trying to launch the Rtprx handler thread...\n", error->code, error->message ? error->message : "??");
    return -1;
  }

  return 0;
}

/* 销毁插件，释放资源 */
void janus_plugin_destroy_tms_mp4(void)
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

  JANUS_LOG(LOG_INFO, "销毁插件 %s\n", TMS_JANUS_PLUGIN_MP4_NAME);
}

/**************************************
 *  会话生命周期方法 
 **************************************/
/* 创建插件 */
void janus_plugin_create_session_tms_mp4(janus_plugin_session *handle, int *error)
{
  JANUS_LOG(LOG_VERB, "插件[PlayMp4] 创建会话 %p\n", handle);

  /* 创建本地会话，记录状态信息 */
  tms_mp4_session *session = g_malloc0(sizeof(tms_mp4_session));
  session->handle = handle;
  handle->plugin_handle = session;
}
/* 必须有，怎么用？返回json对象，记录和session关联的业务信息 */
json_t *janus_plugin_query_session_tms_mp4(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_VERB, "插件[%s] 查找会话 %p\n", TMS_JANUS_PLUGIN_MP4_NAME, handle);

  return NULL;
}
/* 销毁插件 */
void janus_plugin_destroy_session_tms_mp4(janus_plugin_session *handle, int *error)
{
  JANUS_LOG(LOG_VERB, "插件[%s] 销毁会话 %p\n", TMS_JANUS_PLUGIN_MP4_NAME, handle);
}

/**************************************
 *  媒体生命周期方法 
 **************************************/

void janus_plugin_setup_media_tms_mp4(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_VERB, "插件[%s] 设置媒体 %p\n", TMS_JANUS_PLUGIN_MP4_NAME, handle);
}
void janus_plugin_hangup_media_tms_mp4(janus_plugin_session *handle)
{
  JANUS_LOG(LOG_VERB, "插件[%s] 结束媒体 %p\n", TMS_JANUS_PLUGIN_MP4_NAME, handle);
}

/**************************************
 *  消息处理 
 **************************************/

struct janus_plugin_result *janus_plugin_handle_message_tms_mp4(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep)
{
  json_t *root = message;
  json_t *response = NULL;

  json_t *request = json_object_get(root, "request");
  const char *request_text = json_string_value(request);

  JANUS_LOG(LOG_VERB, "插件[%s] 收到客户端请求[%s][%s]\n", TMS_JANUS_PLUGIN_MP4_NAME, request_text, transaction);

  if (!strcasecmp(request_text, "ping"))
  {
    json_t *response = json_object();
    json_object_set_new(response, "msg", json_string("pong"));

    return janus_plugin_result_new(JANUS_PLUGIN_OK, NULL, response);
  }

  /* 放入队列异步处理的消息 */
  if (!strcasecmp(request_text, "request.offer"))
  {
    tms_mp4_message *msg = g_malloc(sizeof(tms_mp4_message));
    msg->handle = handle;
    msg->transaction = transaction;
    msg->message = root;
    msg->jsep = jsep;

    JANUS_LOG(LOG_VERB, "插件[%s] 收到客户端请求[%s][%s]，进入队列等待处理\n", TMS_JANUS_PLUGIN_MP4_NAME, request_text, msg->transaction);
    g_async_queue_push(messages, msg);
  }

  return janus_plugin_result_new(JANUS_PLUGIN_OK_WAIT, NULL, NULL);
}
