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
  return 0;
}

/* 销毁插件 */
void janus_plugin_destroy_tms_mp4(void) {}

/**************************************
 *  会话生命周期方法 
 **************************************/

void janus_plugin_create_session_tms_mp4(janus_plugin_session *handle, int *error) {}
json_t *janus_plugin_query_session_tms_mp4(janus_plugin_session *handle) { return NULL; }
void janus_plugin_destroy_session_tms_mp4(janus_plugin_session *handle, int *error) {}

/**************************************
 *  媒体生命周期方法 
 **************************************/

void janus_plugin_setup_media_tms_mp4(janus_plugin_session *handle) {}
void janus_plugin_hangup_media_tms_mp4(janus_plugin_session *handle) {}

/**************************************
 *  消息处理 
 **************************************/

struct janus_plugin_result *janus_plugin_handle_message_tms_mp4(janus_plugin_session *handle, char *transaction, json_t *message, json_t *jsep) { return NULL; }
