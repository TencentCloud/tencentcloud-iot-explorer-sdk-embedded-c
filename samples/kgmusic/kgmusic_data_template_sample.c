/*
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

 * Licensed under the MIT License (the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT

 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"
#include "utils_getopt.h"
#include "lite-utils.h"
#include "service_mqtt.h"
#include "json_parser.h"
#include "data_config.c"
#include "string.h"
#include "data_template_client.h"

#ifdef AUTH_MODE_CERT
static char sg_cert_file[PATH_MAX + 1];  // full path of device cert file
static char sg_key_file[PATH_MAX + 1];   // full path of device key file
#endif

static DeviceInfo    sg_devInfo;
static MQTTEventType sg_subscribe_event_result = MQTT_EVENT_UNDEF;
static bool          sg_control_msg_arrived    = false;
static char          sg_data_report_buffer[2048];
static size_t        sg_data_report_buffersize = sizeof(sg_data_report_buffer) / sizeof(sg_data_report_buffer[0]);

#ifdef ACTION_ENABLED
#include "action_config.c"

// action : regist action and set the action handle callback, add your aciton logic here
static void OnActionCallback(void *pClient, const char *pClientToken, DeviceAction *pAction)
{
    int        i;
    sReplyPara replyPara;

    // do something base on input, just print as an sample
    DeviceProperty *pActionInput = pAction->pInput;
    for (i = 0; i < pAction->input_num; i++) {
        if (JSTRING == pActionInput[i].type) {
            Log_d("Input:[%s], data:[%s]", pActionInput[i].key, pActionInput[i].data);
            HAL_Free(pActionInput[i].data);
        } else {
            if (JINT32 == pActionInput[i].type) {
                Log_d("Input:[%s], data:[%d]", pActionInput[i].key, *((int *)pActionInput[i].data));
            } else if (JFLOAT == pActionInput[i].type) {
                Log_d("Input:[%s], data:[%f]", pActionInput[i].key, *((float *)pActionInput[i].data));
            } else if (JUINT32 == pActionInput[i].type) {
                Log_d("Input:[%s], data:[%u]", pActionInput[i].key, *((uint32_t *)pActionInput[i].data));
            }
        }
    }

    // construct output
    memset((char *)&replyPara, 0, sizeof(sReplyPara));
    replyPara.code       = eDEAL_SUCCESS;
    replyPara.timeout_ms = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    strcpy(replyPara.status_msg, "action execute success!");  // add the message about the action resault

    DeviceProperty *pActionOutnput = pAction->pOutput;
    (void)pActionOutnput;  // elimate warning
    // TO DO: add your aciont logic here and set output properties which will be reported by action_reply

    IOT_Action_Reply(pClient, pClientToken, sg_data_report_buffer, sg_data_report_buffersize, pAction, &replyPara);
}

static int _register_data_template_action(void *pTemplate_client)
{
    int i, rc;

    for (i = 0; i < TOTAL_ACTION_COUNTS; i++) {
        rc = IOT_Template_Register_Action(pTemplate_client, &g_actions[i], OnActionCallback);
        if (rc != QCLOUD_RET_SUCCESS) {
            rc = IOT_Template_Destroy(pTemplate_client);
            Log_e("register device data template action failed, err: %d", rc);
            return rc;
        } else {
            Log_i("data template action=%s registered.", g_actions[i].pActionId);
        }
    }

    return QCLOUD_RET_SUCCESS;
}
#endif

static void event_handler(void *pclient, void *handle_context, MQTTEventMsg *msg)
{
    uintptr_t packet_id = (uintptr_t)msg->msg;

    switch (msg->event_type) {
        case MQTT_EVENT_UNDEF:
            Log_i("undefined event occur.");
            break;

        case MQTT_EVENT_DISCONNECT:
            Log_i("MQTT disconnect.");
            break;

        case MQTT_EVENT_RECONNECT:
            Log_i("MQTT reconnect.");
            break;

        case MQTT_EVENT_SUBCRIBE_SUCCESS:
            sg_subscribe_event_result = msg->event_type;
            Log_i("subscribe success, packet-id=%u", packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_TIMEOUT:
            sg_subscribe_event_result = msg->event_type;
            Log_i("subscribe wait ack timeout, packet-id=%u", packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_NACK:
            sg_subscribe_event_result = msg->event_type;
            Log_i("subscribe nack, packet-id=%u", packet_id);
            break;

        case MQTT_EVENT_PUBLISH_SUCCESS:
            Log_i("publish success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_PUBLISH_TIMEOUT:
            Log_i("publish timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_PUBLISH_NACK:
            Log_i("publish nack, packet-id=%u", (unsigned int)packet_id);
            break;
        default:
            Log_i("Should NOT arrive here.");
            break;
    }
}

/*add user init code, like sensor init*/
static void _usr_init(void)
{
    Log_d("add your init code here");
}

// Setup MQTT construct parameters
static int _setup_connect_init_params(TemplateInitParams *initParams)
{
    int ret;

    ret = HAL_GetDevInfo((void *)&sg_devInfo);
    if (QCLOUD_RET_SUCCESS != ret) {
        return ret;
    }

    initParams->device_name = sg_devInfo.device_name;
    initParams->product_id  = sg_devInfo.product_id;

#ifdef AUTH_MODE_CERT
    /* TLS with certs*/
    char  certs_dir[PATH_MAX + 1] = "certs";
    char  current_path[PATH_MAX + 1];
    char *cwd = getcwd(current_path, sizeof(current_path));
    if (cwd == NULL) {
        Log_e("getcwd return NULL");
        return QCLOUD_ERR_FAILURE;
    }
    sprintf(sg_cert_file, "%s/%s/%s", current_path, certs_dir, sg_devInfo.dev_cert_file_name);
    sprintf(sg_key_file, "%s/%s/%s", current_path, certs_dir, sg_devInfo.dev_key_file_name);

    initParams->cert_file = sg_cert_file;
    initParams->key_file  = sg_key_file;
#else
    initParams->device_secret = sg_devInfo.device_secret;
#endif

    initParams->command_timeout        = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    initParams->keep_alive_interval_ms = QCLOUD_IOT_MQTT_KEEP_ALIVE_INTERNAL;
    initParams->auto_connect_enable    = 1;
    initParams->event_handle.h_fp      = event_handler;
    initParams->usr_control_handle     = NULL;

    return QCLOUD_RET_SUCCESS;
}

static void OnReportReplyCallback(void *pClient, Method method, ReplyAck replyAck, const char *pJsonDocument,
                                  void *pUserdata)
{
    Log_i("recv report reply response, reply ack: %d", replyAck);
}

// register data template properties

// when control msg received, data_template's properties has been parsed in pData you should add your logic how to use
// pData
void deal_down_stream_user_logic(void *client, ProductDataDefine *pData)
{
    Log_d("someting about your own product logic wait to be done");

}

/*get local property data, like sensor data*/
static void _refresh_local_property(void)
{
    // add your local property refresh logic
}

/*find propery need report*/
static int find_wait_report_property(DeviceProperty *pReportDataList[])
{
    int i, j;

    for (i = 0, j = 0; i < TOTAL_PROPERTY_COUNT; i++) {
        if (eCHANGED == sg_DataTemplate[i].state) {
            if (0 != strcmp(sg_DataTemplate[i].data_property.key, "_sys_pre_next")) {
                pReportDataList[j++] = &(sg_DataTemplate[i].data_property);
            }

            if (sg_control_msg_arrived == false) {
                sg_DataTemplate[i].state = eNOCHANGE;
            }
        }
    }

    sg_control_msg_arrived = false;

    return j;
}

// demo for up-stream
// add changed properties to pReportDataList, then the changed properties would be reported
// you should add your own logic for how to get the changed properties
int deal_up_stream_user_logic(DeviceProperty *pReportDataList[], int *pCount)
{
    // refresh local property
    _refresh_local_property();

    /*find propery need report*/
    *pCount = find_wait_report_property(pReportDataList);

    return (*pCount > 0) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

/*You should get the real info for your device, here just for example*/
static int _get_sys_info(void *handle, char *pJsonDoc, size_t sizeOfBuffer)
{
    /*platform info has at least one of module_hardinfo/module_softinfo/fw_ver property*/
    DeviceProperty plat_info[] = {
        {.key = "module_hardinfo", .type = TYPE_TEMPLATE_STRING, .data = "ESP8266"},
        {.key = "module_softinfo", .type = TYPE_TEMPLATE_STRING, .data = "V1.0"},
        {.key = "fw_ver", .type = TYPE_TEMPLATE_STRING, .data = QCLOUD_IOT_DEVICE_SDK_VERSION},
        {.key = "imei", .type = TYPE_TEMPLATE_STRING, .data = "11-22-33-44"},
        {.key = "lat", .type = TYPE_TEMPLATE_STRING, .data = "22.546015"},
        {.key = "lon", .type = TYPE_TEMPLATE_STRING, .data = "113.941125"},
        {.key = NULL, .data = NULL}  // end
    };

    /*self define info*/
    DeviceProperty self_info[] = {
        {.key = "append_info", .type = TYPE_TEMPLATE_STRING, .data = "your self define info"},
        {.key = NULL, .data = NULL}  // end
    };

    return IOT_Template_JSON_ConstructSysInfo(handle, pJsonDoc, sizeOfBuffer, plat_info, self_info);
}

static int parse_arguments(int argc, char **argv)
{
    int c;
    while ((c = utils_getopt(argc, argv, "c:l:")) != EOF) switch (c) {
            case 'c':
                if (HAL_SetDevInfoFile(utils_optarg))
                    return -1;
                break;

            default:
                HAL_Printf(
                    "usage: %s [options]\n"
                    "  [-c <config file for DeviceInfo>] \n",
                    argv[0]);
                return -1;
        }
    return 0;
}

#define QCLOUD_IOT_KGMUSIC_PAGESIZE 5

typedef enum {
    KGMUSIC_SONG_CAN          = 0,
    KGMUSIC_SONG_CHINA_AREA   = 1,
    KGMUSIC_SONG_NO_COPYRIGHT = 2,
    KGMUSIC_SONG_ISVIP        = 3,
} KgmusicSongPlayAbleCode;

typedef enum {
    KGMUSIC_MODE_CYCLE  = 0,
    KGMUSIC_MODE_SINGLE = 1,
    KGMUSIC_MODE_RANDOM = 2,
} KgmusicPlayMode;

typedef struct {
    char *url;
    int   duration;
    int   quality;
    char *singer_name;
    char *song_name;
} PlayerSongInfo;

typedef struct {
    char *                  song_id;
    char *                  song_name;
    char *                  singer_name;
    char *                  song_url;
    int                     song_size;
    char *                  song_url_hq;
    int                     song_size_hq;
    char *                  song_url_sq;
    int                     song_size_sq;
    int                     duration;
    KgmusicSongPlayAbleCode playable_code;
    char *                  try_url;
    int                     try_size;
    int                     try_begin;
    int                     try_end;
    bool                    has_info;
} KgmusicSongInfo;

typedef struct {
    int             list_curr_count;
    int             list_total_count;
    int             curr_play_index;
    int             song_reply_index;
    int             curr_song_duration;
    int             curr_play_position;
    int             seek_play_position;
    int             set_play_quality;
    int             curr_play_volume;
    int             page_num;
    char *          list_id;
    char *          list_type;
    char *          list_img;
    char *          list_name;
    bool            passive_next;
    bool            user_next;
    bool            user_prev;
    bool            not_in_list;
    bool            start_play;
    bool            switch_quality;
    KgmusicSongInfo song_info_list[QCLOUD_IOT_KGMUSIC_PAGESIZE + 1];
} KgmusicSongList;

typedef struct {
    char *type_key;
    char *except_fields;
} KgmusicExceptFields;

static KgmusicExceptFields sg_kgmusic_except_fields[] = {
    {"album_info",
     "publish_time|album_translate_name|company|singer_name|intro|album_name|album_img|singer_id,songs.*.has_accompany|"
     "singer_img|mv_id|album_id|album_img_mini|singer_name|album_img_small|album_img_medium|album_img_large|album_name|"
     "album_img|singer_id"},
    {"playlist_song", "songs.*.|has_accompany|album_name|mv_id|album_id||singer_id|"},
    {"awesome_everyday",
     "songs.*.has_accompany|"
     "singer_img|mv_id|album_id|album_img_mini|singer_name|album_img_small|album_img_medium|album_img_large|album_name|"
     "album_img|singer_id"},
    {"awesome_recommend",
     "songs.*.has_accompany|"
     "singer_img|mv_id|album_id|album_img_mini|singer_name|album_img_small|album_img_medium|album_img_large|album_name|"
     "album_img|singer_id"},
    {"awesome_newsong",
     "songs.*.has_accompany|"
     "singer_img|mv_id|album_id|album_img_mini|singer_name|album_img_small|album_img_medium|album_img_large|album_name|"
     "album_img|singer_id"},
    {"top_song",
     "songs.*.has_accompany|"
     "singer_img|mv_id|album_id|album_img_mini|singer_name|album_img_small|album_img_medium|album_img_large|album_name|"
     "album_img|singer_id"},
};

#define QCLOUD_IOT_KGMUSIC_SYNC_SEND_FAILED  -1
#define QCLOUD_IOT_KGMUSIC_SYNC_RECV_TIMEOUT -2

static KgmusicSongList  sg_song_list   = {0};
static QcloudIotKgmusic sg_iot_kgmusic = {0};

static void _kgmusic_song_info_clear(KgmusicSongInfo *song_info)
{
    if (song_info == NULL) {
        return;
    }
    // clear old data
    HAL_Free(song_info->song_name);
    HAL_Free(song_info->singer_name);
    HAL_Free(song_info->song_url);
    HAL_Free(song_info->song_url_hq);
    HAL_Free(song_info->song_url_sq);
    HAL_Free(song_info->try_url);
    HAL_Free(song_info->song_id);
    memset(song_info, 0, sizeof(KgmusicSongInfo));
}

static int _kgmusic_query_songlist_callback(char *song_info_json, char *userdata)
{
    char *total_str    = LITE_json_value_of("total", song_info_json);
    char *songs        = LITE_json_value_of("songs", song_info_json);
    char *album_id_str = LITE_json_value_of("album_id", song_info_json);
    char *album_name   = LITE_json_value_of("album_name", song_info_json);

    // clear song_info_list;
    for (int i = 0; i < QCLOUD_IOT_KGMUSIC_PAGESIZE; i++) {
        _kgmusic_song_info_clear(&sg_song_list.song_info_list[i]);
    }

    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    // parser json array
    int count = 0;
    int index = 0;
    json_array_for_each_entry(songs, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);
        if (total_str == NULL && sg_song_list.page_num * QCLOUD_IOT_KGMUSIC_PAGESIZE <= count &&
            index < QCLOUD_IOT_KGMUSIC_PAGESIZE) {
            sg_song_list.song_info_list[index].song_id = LITE_json_value_of("song_id", entry);
            index++;
        } else if (total_str != NULL) {
            sg_song_list.song_info_list[index].song_id = LITE_json_value_of("song_id", entry);
            index++;
        }

        count++;

        restore_json_str_last_char(entry, entry_len, old_ch);
    }

    sg_song_list.list_curr_count = index;

    if (total_str == NULL) {
        sg_song_list.list_total_count = count;
    } else {
        sg_song_list.list_total_count = atoi(total_str);
    }

    if (sg_song_list.list_total_count == 0) {
        sg_song_list.page_num        = 0;
        sg_song_list.curr_play_index = 0;
    }

    Log_d("song list id:%s, total:%d, cunrr_count:%d, name:%s", album_id_str == NULL ? "" : album_id_str,
          sg_song_list.list_total_count, sg_song_list.list_curr_count, album_name == NULL ? "" : album_name);

    HAL_Free(album_id_str);
    HAL_Free(album_name);
    HAL_Free(songs);
    HAL_Free(total_str);

    return QCLOUD_RET_SUCCESS;
}

static int _kgmusic_query_songinfo_callback(char *song_info_json, char *userdata)
{
    int   ret   = QCLOUD_RET_SUCCESS;
    char *value = NULL;

    char *song_id = LITE_json_value_of("song_id", song_info_json);
    if (song_id == NULL) {
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    KgmusicSongInfo *song_info = NULL;

    // get song info index
    if (sg_song_list.song_info_list[sg_song_list.song_reply_index].song_id != NULL &&
        0 == strcmp(sg_song_list.song_info_list[sg_song_list.song_reply_index].song_id, song_id)) {
        song_info = &sg_song_list.song_info_list[sg_song_list.song_reply_index];
    } else {
        Log_e("reply song id is not query song id %s %s", song_id,
              sg_song_list.song_info_list[sg_song_list.song_reply_index].song_id);
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    _kgmusic_song_info_clear(song_info);

    song_info->song_id     = song_id;
    song_info->song_name   = LITE_json_value_of("song_name", song_info_json);
    song_info->singer_name = LITE_json_value_of("singer_name", song_info_json);

    song_info->song_url = LITE_json_value_of("song_url", song_info_json);

    value = LITE_json_value_of("song_size", song_info_json);
    if (value != NULL) {
        song_info->song_size = atoi(value);
        HAL_Free(value);
    }

    song_info->song_url_hq = LITE_json_value_of("song_url_hq", song_info_json);
    value                  = LITE_json_value_of("song_size_hq", song_info_json);
    if (value != NULL) {
        song_info->song_size_hq = atoi(value);
        HAL_Free(value);
    }

    song_info->song_url_sq = LITE_json_value_of("song_url_sq", song_info_json);
    value                  = LITE_json_value_of("song_size_sq", song_info_json);
    if (value != NULL) {
        song_info->song_size_sq = atoi(value);
        HAL_Free(value);
    }

    value = LITE_json_value_of("duration", song_info_json);
    if (value != NULL) {
        song_info->duration = atoi(value);
        HAL_Free(value);
    }

    value = LITE_json_value_of("playable_code", song_info_json);
    if (value != NULL) {
        song_info->playable_code = atoi(value);
        HAL_Free(value);
    }

    song_info->try_url = LITE_json_value_of("try_url", song_info_json);

    value = LITE_json_value_of("try_size", song_info_json);
    if (value != NULL) {
        song_info->try_size = atoi(value);
        HAL_Free(value);
    }
    value = LITE_json_value_of("try_begin", song_info_json);
    if (value != NULL) {
        song_info->try_begin = atoi(value);
        HAL_Free(value);
    }
    value = LITE_json_value_of("try_end", song_info_json);
    if (value != NULL) {
        song_info->try_end = atoi(value);
        HAL_Free(value);
        value = NULL;
    }

    song_info->has_info = true;

exit:
    HAL_Free(value);

    return ret;
}

static int _kgmusic_query_song_list(void *client)
{
    // get song list
    if (sg_song_list.list_type == NULL) {
        return QCLOUD_ERR_FAILURE;
    }
    QcloudIotKgmusicSongListParams params;
    params.list_id       = sg_song_list.list_id;
    params.list_type     = sg_song_list.list_type;
    params.page_size     = QCLOUD_IOT_KGMUSIC_PAGESIZE;
    params.page          = sg_song_list.curr_play_index / params.page_size + 1;
    params.except_fields = "";

    int i = 0;
    for (i = 0; i < sizeof(sg_kgmusic_except_fields) / sizeof(KgmusicExceptFields); i++) {
        if (NULL != strstr(sg_song_list.list_type, sg_kgmusic_except_fields[i].type_key)) {
            params.except_fields = sg_kgmusic_except_fields[i].except_fields;
            break;
        }
    }

    int ret = IOT_KGMUSIC_syncquery_songlist(client, &params, &sg_iot_kgmusic, 5000);
    if (QCLOUD_RET_SUCCESS == ret) {
        sg_song_list.page_num = params.page;
    }

    return ret;
}

static int _kgmusic_get_song_info(void *client, bool get_next)
{
    int              ret           = QCLOUD_RET_SUCCESS;
    int              index         = 0;
    bool             get_song_list = false;
    KgmusicSongInfo *song_info     = NULL;

    int song_index = 0;

    if (get_next == false) {
        if (sg_song_list.not_in_list == true) {
            song_index = QCLOUD_IOT_KGMUSIC_PAGESIZE;
            song_info  = &sg_song_list.song_info_list[song_index];
        } else {
            if (sg_song_list.curr_play_index / QCLOUD_IOT_KGMUSIC_PAGESIZE + 1 == sg_song_list.page_num) {
                song_index = sg_song_list.curr_play_index % QCLOUD_IOT_KGMUSIC_PAGESIZE;
                song_info  = &sg_song_list.song_info_list[song_index];
            } else {
                get_song_list = true;
            }
        }
    } else {
        int start_index = sg_song_list.curr_play_index % QCLOUD_IOT_KGMUSIC_PAGESIZE;
        for (index = start_index; index < QCLOUD_IOT_KGMUSIC_PAGESIZE; index++) {
            if ((sg_song_list.song_info_list[index].song_id != NULL) &&
                (sg_song_list.song_info_list[index].has_info == false)) {
                song_info = &sg_song_list.song_info_list[index];
            }
        }
    }

    if (get_song_list == true || song_info->song_id == NULL) {
        _kgmusic_query_song_list(client);
    }

    if (song_info == NULL) {
        return QCLOUD_RET_SUCCESS;
    }

    if (song_info->song_id == NULL) {
        return QCLOUD_RET_SUCCESS;
    }

    if (false == song_info->has_info) {
        ret = IOT_KGMUSIC_syncquery_song(client, song_info->song_id, &sg_iot_kgmusic, 5000);
    }

    // get failed or success setted
    if (ret != QCLOUD_IOT_KGMUSIC_SYNC_SEND_FAILED) {
        song_info->has_info = true;
    }

    return ret;
}

static int _kgmusic_find_index_from_list(void *client, char *song_id)
{
    int i = 0;

    int end_index = sg_song_list.page_num * QCLOUD_IOT_KGMUSIC_PAGESIZE - 1;
    end_index     = end_index >= sg_song_list.list_total_count ? sg_song_list.list_total_count - 1 : end_index;

    int start_index = end_index + 1 - QCLOUD_IOT_KGMUSIC_PAGESIZE;
    start_index     = start_index < 0 ? 0 : start_index;

    sg_song_list.song_reply_index = sg_song_list.curr_play_index % QCLOUD_IOT_KGMUSIC_PAGESIZE;

    if (!(start_index <= sg_song_list.curr_play_index && sg_song_list.curr_play_index <= end_index)) {
        _kgmusic_query_song_list(client);
    }

    for (i = 0; i < QCLOUD_IOT_KGMUSIC_PAGESIZE; i++) {
        char *index_song_id = sg_song_list.song_info_list[i].song_id;
        if (index_song_id != NULL && strcmp(song_id, index_song_id) == 0) {
            break;
        }
    }

    if (i == QCLOUD_IOT_KGMUSIC_PAGESIZE) {
        _kgmusic_song_info_clear(&sg_song_list.song_info_list[i]);

        sg_song_list.song_info_list[i].song_id = HAL_Malloc(strlen(song_id) + 1);

        strcpy(sg_song_list.song_info_list[i].song_id, song_id);

        sg_song_list.not_in_list = true;
        return -1;
    }

    return i + ((sg_song_list.page_num - 1) * QCLOUD_IOT_KGMUSIC_PAGESIZE);
}

static void _kgmusic_next_prev_play_song(void *client, bool manual, bool next)
{
    int curr_play_index = sg_song_list.curr_play_index;

    sg_ProductData.m_pause_play           = 1;
    sg_DataTemplate[M_PAUSE_PLAY_E].state = eCHANGED;

    if (!manual && (sg_song_list.list_total_count == 0 || sg_ProductData.m_play_mode == KGMUSIC_MODE_SINGLE)) {
        int find_index = _kgmusic_find_index_from_list(client, sg_ProductData.m_cur_song_id);
        if (find_index != -1) {
            sg_song_list.curr_play_index = find_index;
        }
        Log_d("single song");
        return;
    }

    if (sg_ProductData.m_play_mode == KGMUSIC_MODE_RANDOM) {
        srand((unsigned)time(NULL));
        while (curr_play_index == sg_song_list.curr_play_index) {
            curr_play_index = rand() % sg_song_list.list_total_count;
        }
        Log_d("random song index:%d", curr_play_index);
    }

    // next
    if (sg_ProductData.m_play_mode == KGMUSIC_MODE_CYCLE && next) {
        curr_play_index += 1;
        if (curr_play_index >= sg_song_list.list_total_count) {
            curr_play_index = 0;
            Log_d("cycle play head");
        }
    }

    // prev
    if (sg_ProductData.m_play_mode == KGMUSIC_MODE_CYCLE && !next) {
        if (curr_play_index <= 0) {
            curr_play_index = sg_song_list.list_total_count - 1;
            Log_d("cycle play tail");
        } else {
            curr_play_index -= 1;
        }
    }

    // check song id is in list
    int end_index   = sg_song_list.page_num * QCLOUD_IOT_KGMUSIC_PAGESIZE - 1;
    end_index       = end_index >= sg_song_list.list_total_count ? sg_song_list.list_total_count - 1 : end_index;
    int start_index = end_index + 1 - QCLOUD_IOT_KGMUSIC_PAGESIZE;
    start_index     = start_index < 0 ? 0 : start_index;

    sg_song_list.curr_play_index  = curr_play_index;
    sg_song_list.song_reply_index = curr_play_index % QCLOUD_IOT_KGMUSIC_PAGESIZE;

    if (start_index > curr_play_index || curr_play_index > end_index) {
        _kgmusic_query_song_list(client);
    }

    Log_d("play index;%d", sg_song_list.curr_play_index);
}

static void _kgmusic_next_song(void *client, bool manual)
{
    _kgmusic_next_prev_play_song(client, manual, true);
    return;
}

static void _kgmusic_prev_song(void *client, bool manual)
{
    _kgmusic_next_prev_play_song(client, manual, false);
    return;
}

static void _kgmusic_passive_play_next()
{
    sg_song_list.passive_next = true;
}

static char *sg_play_params_list_id[] = {
    "play_params.playlist_id",
    "play_params.album_id",
    "play_params.top_id",
};

static int _kgmusic_parse_curr_play_list(void *client, char *data)
{
    Log_d("parse play list data: %s", data);

    int data_len   = 0;
    int curr_index = 0;
    while (data[data_len] != '\0') {
        if (data[data_len] == '\\') {
            data_len++;
            continue;
        }
        data[curr_index] = data[data_len];
        curr_index++;
        data_len++;
    }

    int   i       = 0;
    char *list_id = NULL;
    for (i = 0; i < sizeof(sg_play_params_list_id) / sizeof(char *); i++) {
        list_id = LITE_json_value_of(sg_play_params_list_id[i], data);
        if (list_id != NULL) {
            break;
        }
    }

    /* clear old data */
    for (int i = 0; i < QCLOUD_IOT_KGMUSIC_PAGESIZE; i++) {
        _kgmusic_song_info_clear(&sg_song_list.song_info_list[i]);
    }
    HAL_Free(sg_song_list.list_id);
    HAL_Free(sg_song_list.list_img);
    HAL_Free(sg_song_list.list_name);
    HAL_Free(sg_song_list.list_type);
    memset(&sg_song_list, 0, sizeof(sg_song_list));
    /* clear old data end */

    sg_song_list.list_type = LITE_json_value_of("play_type", data);
    sg_song_list.list_img  = LITE_json_value_of("play_img", data);
    sg_song_list.list_name = LITE_json_value_of("play_name", data);
    sg_song_list.list_id   = list_id;

    return _kgmusic_query_song_list(client);
}

static bool _kgmusic_passive_play_proc(void *client)
{
    if (true == sg_song_list.passive_next) {
        _kgmusic_next_song(client, false);
        return true;
    }

    return false;
}

static bool _kgmusic_user_play_proc(void *client)
{
    if (true == sg_song_list.user_next) {
        _kgmusic_next_song(client, true);
        return true;
    }

    if (true == sg_song_list.user_prev) {
        _kgmusic_prev_song(client, true);
        return true;
    }

    return false;
}

typedef struct kgmusic_player {
    int  play_duration;
    int  play_position;
    int  curr_volume;
    bool start_play;
    bool end_play;
    void (*set_volume)(struct kgmusic_player *player, int volume);
    void (*set_position)(struct kgmusic_player *player, int position_sec);
    int (*get_position)(struct kgmusic_player *player);
    int (*get_volume)(struct kgmusic_player *player);
    void (*play)(struct kgmusic_player *player, PlayerSongInfo *play_song_info);
    bool (*check_play_end)(struct kgmusic_player *player);
    void (*pause)(struct kgmusic_player *player);
    void (*unpause)(struct kgmusic_player *player, int position);
} KgmusicPlayer;

#ifdef WIN32
#include <Windows.h>

HANDLE hStdInRead;
HANDLE hStdInWrite = -1;

HANDLE hStdOutRead = -1;
HANDLE hStdOutWrite;

HANDLE hStdErrWrite;

STARTUPINFO siStartInfo;

PROCESS_INFORMATION piProcInfo;

static void _player_destory(KgmusicPlayer *player)
{
    if (hStdOutRead != -1) {
        CloseHandle(hStdOutRead);
        hStdOutRead = -1;
    }

    if (hStdInWrite != -1) {
        CloseHandle(hStdInWrite);
        hStdInWrite = -1;
    }

    TerminateProcess(piProcInfo.hProcess, 300);
    CloseHandle(piProcInfo.hProcess);
    CloseHandle(piProcInfo.hThread);

    player->start_play    = false;
    player->end_play      = true;
    player->play_position = 0;

    Log_e("player end");
}

static void _player_read_pipe(KgmusicPlayer *player)
{
    char  buf[256];
    int   size    = 0;
    int   dwAvail = 0;
    Timer read_timer;
    countdown(&read_timer, 1);
    do {
        if (hStdOutRead == -1) {
            _player_destory(player);
            return;
        }
        // int write_len = 0;
        // WriteFile(sg_pipe_write, "get\n", sizeof("get\n") - 1, &write_len, NULL);
        // char *line = fgets(buf, sizeof(buf), sg_pipe_file);
        memset(buf, 0, 256);
        if (!PeekNamedPipe(hStdOutRead, NULL, NULL, &size, &dwAvail, NULL) || dwAvail <= 0) {
            break;
        }
        ReadFile((HANDLE)(hStdOutRead), buf, 255, &size, NULL);
        Log_d("size:%d, %s", size, buf);
        // buf[size] = '\0';
        // size = read(sg_fd_pipe[0], buf, sizeof(buf));
        // if (size <= 0) {
        //  continue;
        //}
        // if (line == NULL) {
        //    continue;
        //}

        Log_d("size:%d, %s", size, buf);

        char *temp = strstr(buf, "Starting playback");
        if (NULL != temp) {
            Log_d("Starting playback");
            player->start_play = true;
            break;
        }

        temp = strstr(buf, "ANS_TIME_POSITION=");
        if (NULL != temp) {
            temp = strstr(temp, "=");
            temp += 1;
            player->play_position = atoi(temp) + 1;
            Log_d("position break %d", player->play_position);
            break;
        }

        temp = strstr(buf, "ANS_volume=");
        if (NULL != temp) {
            temp = strstr(temp, "=");
            temp += 1;
            player->curr_volume = atoi(temp);
            Log_d("volume break");
            break;
        }

        if (NULL != strstr(buf, "Position:")) {
            break;
        }

        if (NULL != strstr(buf, "Exiting...") || NULL != strstr(buf, "cannot reopen stream for resync")) {
            _player_destory(player);
            break;
        }

        if (player->play_position * 1000 >= player->play_duration) {
            _player_destory(player);
            break;
        }
    } while (hStdOutRead != -1 && false == expired(&read_timer));
}

static bool _player_play_end_check(KgmusicPlayer *player)
{
    if (hStdOutRead == -1) {
        _player_destory(player);
        return true;
    }

    if (player->start_play == false) {
        _player_read_pipe(player);
    }

    return player->end_play;
}

static void _player_setvolume(KgmusicPlayer *player, int volume)
{
    char cmd[128];
    if (hStdInWrite == -1 || volume == player->curr_volume) {
        return;
    }

    int size = HAL_Snprintf(cmd, 128, "pausing_keep_force volume %d 1\n", volume);

    int wite_len = 0;
    WriteFile(hStdInWrite, cmd, size, &wite_len, NULL);

    Log_d("player set volume %d, size %d", volume, size);
}

static int _player_getvolume(KgmusicPlayer *player)
{
    if (player->start_play == true && hStdInWrite != -1) {
        int wite_len = 0;
        WriteFile(hStdInWrite, "pausing_keep_force get_property volume\n",
                  sizeof("pausing_keep_force get_property volume\n") - 1, &wite_len, NULL);

        Log_d("write size:");

        _player_read_pipe(player);
    }

    return player->curr_volume;
}

static void _player_play(KgmusicPlayer *player, PlayerSongInfo *play_song_info)
{
    Log_d("play song %s-%s-%s-%d-%d", play_song_info->song_name, play_song_info->singer_name, play_song_info->url,
          play_song_info->duration, play_song_info->quality);

    Log_d("ret");

    _player_destory(player);

    SECURITY_ATTRIBUTES sa;
    sa.nLength              = sizeof(sa);
    sa.lpSecurityDescriptor = 0;
    sa.bInheritHandle       = true;
    if (!CreatePipe(&hStdInRead, &hStdInWrite, &sa, 0))
        return;
    if (!CreatePipe(&hStdOutRead, &hStdOutWrite, &sa, 0))
        return;

    if (!DuplicateHandle(GetCurrentProcess(), hStdOutWrite, GetCurrentProcess(), &hStdErrWrite, 0, TRUE,
                         DUPLICATE_SAME_ACCESS))
        return;

    ZeroMemory(&siStartInfo, sizeof(STARTUPINFO));
    siStartInfo.cb = sizeof(STARTUPINFO);
    siStartInfo.dwFlags |= STARTF_USESHOWWINDOW;
    siStartInfo.dwFlags |= STARTF_USESTDHANDLES;
    siStartInfo.hStdOutput = hStdOutWrite;
    siStartInfo.hStdError  = hStdErrWrite;
    siStartInfo.hStdInput  = hStdInRead;

    char cmd_line[512];
    HAL_Snprintf(cmd_line, sizeof(cmd_line), "./mplayer/mplayer.exe -cache 8192 -cache-min 15 -framedrop -slave %s",
                 play_song_info->url);

    BOOL result = CreateProcess(NULL, cmd_line, NULL, NULL, TRUE, 0, NULL, NULL, &siStartInfo, &piProcInfo);

    if (result == false) {
        CloseHandle(hStdInWrite);
        CloseHandle(hStdOutRead);
        return;
    }

    player->play_duration = play_song_info->duration;
    player->end_play      = false;

    return;
}

static void _player_pause(KgmusicPlayer *player)
{
    int write_len = 0;
    WriteFile(hStdInWrite, "pause\n", sizeof("pause\n") - 1, &write_len, NULL);
    Log_d("player pause or unpause %d", write_len);
}

static void _player_unpause(KgmusicPlayer *player, int last_position)
{
    int write_len = 0;
    Log_d("write size");
    WriteFile(hStdInWrite, "pause\n", sizeof("pause\n") - 1, &write_len, NULL);

    Log_d("player pause or unpause ");
}

static int _player_getposition(KgmusicPlayer *player)
{
    if (player->start_play == false || hStdInWrite == -1) {
        return -1;
    }

    int write_len = 0;
    Log_d("write size");
    WriteFile(hStdInWrite, "pausing_keep_force get_time_pos\n", sizeof("pausing_keep_force get_time_pos\n") - 1,
              &write_len, NULL);

    Log_d("write size");

    _player_read_pipe(player);

    return player->play_position;
}

static void _player_setposition(KgmusicPlayer *player, int position_sec)
{
    char cmd[128];

    if (hStdInWrite == -1) {
        return;
    }

    int write_len = 0;
    int size      = HAL_Snprintf(cmd, 128, "pausing_keep_force seek %d 2\n", position_sec);

    WriteFile(hStdInWrite, cmd, size, &write_len, NULL);

    Log_d("player set position %d, size %d", position_sec, size);
}

#elif defined(__linux) || defined(__linux__) || defined(linux)
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>

static int sg_fd_fifo = -1;
static int sg_fd_pipe[2];
static FILE *sg_pipe_file = NULL;

static void _player_destory(KgmusicPlayer *player)
{
    if (sg_fd_fifo != -1) {
        close(sg_fd_fifo);
        sg_fd_fifo = -1;
    }

    if (sg_pipe_file != NULL) {
        fclose(sg_pipe_file);
        sg_pipe_file = NULL;
    }

    int ret = system("killall -9 mplayer");

    player->start_play = false;
    player->end_play = true;
    player->play_position = 0;

    Log_e("player end %d", ret);
}

static void _player_read_pipe(KgmusicPlayer *player)
{
    char buf[256];
    Timer read_timer;
    countdown(&read_timer, 1);
    do {
        if (sg_fd_fifo == -1) {
            _player_destory(player);
            return;
        }

        char *line = fgets(buf, sizeof(buf), sg_pipe_file);
        // size = read(sg_fd_pipe[0], buf, sizeof(buf));
        // if (size <= 0) {
        //  continue;
        //}
        if (line == NULL) {
            continue;
        }

        Log_d("%s", buf);

        char *temp = strstr(buf, "Starting playback");
        if (NULL != temp) {
            Log_d("Starting playback");
            player->start_play = true;
            break;
        }

        temp = strstr(buf, "ANS_TIME_POSITION=");
        if (NULL != temp) {
            temp = strstr(temp, "=");
            temp += 1;
            player->play_position = atoi(temp) + 1;
            Log_d("position break %d", player->play_position);
            break;
        }

        temp = strstr(buf, "ANS_volume=");
        if (NULL != temp) {
            temp = strstr(temp, "=");
            temp += 1;
            player->curr_volume = atoi(temp);
            Log_d("volume break");
            break;
        }

        if (NULL != strstr(buf, "Position:")) {
            break;
        }

        if (NULL != strstr(buf, "Exiting...") || NULL != strstr(buf, "cannot reopen stream for resync")) {
            _player_destory(player);
            break;
        }

        if (player->play_position * 1000 >= player->play_duration) {
            _player_destory(player);
            break;
        }
    } while (sg_fd_fifo != -1 && false == expired(&read_timer));
}

static bool _player_play_end_check(KgmusicPlayer *player)
{
    if (sg_fd_fifo == -1) {
        _player_destory(player);
        return true;
    }

    if (player->start_play == false) {
        _player_read_pipe(player);
    }

    return player->end_play;
}

static void _player_setvolume(KgmusicPlayer *player, int volume)
{
    char cmd[128];
    if (sg_fd_fifo == -1 || volume == player->curr_volume) {
        return;
    }

    HAL_Snprintf(cmd, 128, "pausing_keep_force volume %d 1\n", volume);

    int size = write(sg_fd_fifo, cmd, strlen(cmd));

    Log_d("player set volume %d, size %d", volume, size);
}

static int _player_getvolume(KgmusicPlayer *player)
{
    if (player->start_play == true && sg_fd_fifo != -1) {
        int size = write(sg_fd_fifo, "pausing_keep_force get_property volume\n",
                         sizeof("pausing_keep_force get_property volume\n") - 1);

        Log_d("write size:%d", size);

        _player_read_pipe(player);
    }

    return player->curr_volume;
}

#if 0
static void _player_signal()
{
    wait(0);

    if (sg_fd_fifo != -1) {
        close(sg_fd_fifo);
        sg_fd_fifo = -1;
    }

    if (sg_pipe_file != NULL) {
        fclose(sg_pipe_file);
        sg_pipe_file = NULL;
    }
}
#endif

static void _player_play(KgmusicPlayer *player, PlayerSongInfo *play_song_info)
{
    Log_d("play song %s-%s-%s-%d-%d", play_song_info->song_name, play_song_info->singer_name, play_song_info->url,
          play_song_info->duration, play_song_info->quality);

    signal(17, NULL);
    int ret = system("killall -9 mplayer");

    ret = system("unlink ./kgmusic_mplayer");
    ret = system("rm -rf ./kgmusic_mplayer");
    ret = system("mkfifo ./kgmusic_mplayer");

    Log_d("ret %d", ret);

    if (pipe(sg_fd_pipe) < 0) {
        perror("pipe error \n");
        exit(-1);
    }

    pid_t pid;
    pid = fork();
    if (pid < 0) {
        perror("fork");
    }

    if (pid == 0) {
        close(sg_fd_pipe[0]);
        dup2(sg_fd_pipe[1], 1);

        execlp("mplayer", "mplayer", "-quiet", "-cache", "8192", "-framedrop", "-slave", "-input",
               "file=./kgmusic_mplayer", play_song_info->url, NULL);
    } else {
        // signal(17, _player_signal);
        close(sg_fd_pipe[1]);
        if (sg_fd_fifo != -1) {
            close(sg_fd_fifo);
        }
        sg_fd_fifo = open("./kgmusic_mplayer", O_WRONLY);
        sg_pipe_file = fdopen(sg_fd_pipe[0], "r");
        int flag = fcntl(sg_fd_pipe[0], F_GETFL, 0);
        flag |= O_NONBLOCK;
        if (fcntl(sg_fd_pipe[0], F_SETFL, flag) < 0) {
            perror("fcntl");
            exit(1);
        }
        player->play_duration = play_song_info->duration;
        player->end_play = false;
    }
    return;
}

static void _player_pause(KgmusicPlayer *player)
{
    int size = write(sg_fd_fifo, "pause\n", sizeof("pause\n") - 1);
    Log_d("player pause or unpause %d", size);
}

static void _player_unpause(KgmusicPlayer *player, int last_position)
{
    int size = write(sg_fd_fifo, "pause\n", sizeof("pause\n") - 1);
    Log_d("player pause or unpause %d", size);
}

static int _player_getposition(KgmusicPlayer *player)
{
    if (player->start_play == false || sg_fd_fifo == -1) {
        return -1;
    }

    int size = write(sg_fd_fifo, "pausing_keep_force get_time_pos\n", sizeof("pausing_keep_force get_time_pos\n") - 1);

    Log_d("write size:%d", size);

    _player_read_pipe(player);

    return player->play_position;
}

static void _player_setposition(KgmusicPlayer *player, int position_sec)
{
    char cmd[128];

    if (sg_fd_fifo == -1) {
        return;
    }

    HAL_Snprintf(cmd, 128, "pausing_keep_force seek %d 2\n", position_sec);

    int size = write(sg_fd_fifo, cmd, strlen(cmd));

    Log_d("player set position %d, size %d", position_sec, size);
}
#endif

static KgmusicPlayer sg_player = {
    -1,
    -1,
    -1,
    false,
    true,
    _player_setvolume,
    _player_setposition,
    _player_getposition,
    _player_getvolume,
    _player_play,
    _player_play_end_check,
    _player_pause,
    _player_unpause,
};

static void OnControlMsgCallback(void *pClient, const char *pJsonValueBuffer, uint32_t valueLength,
                                 DeviceProperty *pProperty)
{
    int i = 0;

    for (i = 0; i < TOTAL_PROPERTY_COUNT; i++) {
        /* handle self defined string/json here. Other properties are dealed in _handle_delta()*/
        if (strcmp(sg_DataTemplate[i].data_property.key, pProperty->key) == 0) {
            sg_DataTemplate[i].state = eCHANGED;
            Log_i("Property=%s changed", pProperty->key);
            sg_control_msg_arrived = true;
            return;
        }
    }

    Log_e("Property=%s changed no match", pProperty->key);
}

// register data template properties
static int _register_data_template_property(void *pTemplate_client)
{
    int i, rc;

    for (i = 0; i < TOTAL_PROPERTY_COUNT; i++) {
        rc = IOT_Template_Register_Property(pTemplate_client, &sg_DataTemplate[i].data_property, OnControlMsgCallback);
        if (rc != QCLOUD_RET_SUCCESS) {
            Log_e("register device data template property failed, err: %d", rc);
            return rc;
        } else {
            Log_i("data template property=%s registered.", sg_DataTemplate[i].data_property.key);
        }
    }

    return QCLOUD_RET_SUCCESS;
}

static KgmusicSongInfo *_kgmusic_get_play_songinfo(void *client)
{
    int song_index = 0;
    // select song index
    if (sg_song_list.not_in_list == true) {
        song_index                    = QCLOUD_IOT_KGMUSIC_PAGESIZE;
        sg_song_list.song_reply_index = song_index;
    } else if (sg_song_list.switch_quality == true) {
        song_index = sg_song_list.song_reply_index;
    } else {
        song_index                    = sg_song_list.curr_play_index % QCLOUD_IOT_KGMUSIC_PAGESIZE;
        sg_song_list.song_reply_index = song_index;
    }

    // get song info from list
    KgmusicSongInfo *song_info = &sg_song_list.song_info_list[song_index];
    if (song_info->song_id == NULL) {
        // retry get song list
        if (QCLOUD_RET_SUCCESS != _kgmusic_query_song_list(client) || sg_song_list.list_total_count == 0) {
            // get list happen error set to play curr song id
            sg_song_list.not_in_list      = true;
            song_index                    = QCLOUD_IOT_KGMUSIC_PAGESIZE;
            sg_song_list.song_reply_index = song_index;
            song_info                     = &sg_song_list.song_info_list[song_index];
        }
    }

    if (song_info->song_id == NULL) {
        _kgmusic_passive_play_next();
        sg_song_list.not_in_list = false;
        return NULL;
    }

    // check song info getted?
    if (song_info->has_info == false) {
        if (QCLOUD_RET_SUCCESS != _kgmusic_get_song_info(client, false)) {
            _kgmusic_passive_play_next();
            sg_song_list.not_in_list = false;
            return NULL;
        }
    }

    return song_info;
}

static void _kgmusic_playsong(void *client)
{
    KgmusicSongInfo *song_info = _kgmusic_get_play_songinfo(client);

    if (song_info == NULL) {
        return;
    }
    char *song_url = NULL;
    int   quality  = sg_song_list.set_play_quality;

    if (quality == 2) {
        if (song_info->song_url_sq != NULL) {
            song_url = song_info->song_url_sq;
        } else {
            quality = 1;
        }
    }

    if (quality == 1) {
        if (song_info->song_url_hq != NULL) {
            song_url = song_info->song_url_sq;
        } else {
            quality = 0;
        }
    }

    if (quality == 0) {
        song_url = song_info->song_url;
    }

    // set play time
    sg_song_list.curr_song_duration = song_info->duration;

    // try play 60s
    if (song_url == NULL) {
        song_url                        = song_info->try_url;
        sg_song_list.curr_song_duration = song_info->try_end - song_info->try_begin;
    }

    // to play new song
    if (song_url != NULL) {
        sg_song_list.passive_next = false;
        sg_song_list.user_next    = false;
        sg_song_list.user_prev    = false;

        PlayerSongInfo player_song_info;
        player_song_info.url         = song_url;
        player_song_info.singer_name = song_info->singer_name;
        player_song_info.duration    = sg_song_list.curr_song_duration;
        player_song_info.song_name   = song_info->song_name;
        player_song_info.quality     = quality;

        sg_player.play(&sg_player, &player_song_info);

        // The sound quality switch does not report the song information
        if (sg_song_list.switch_quality) {
            sg_player.set_position(&sg_player, sg_ProductData.m_play_position);
        }

        // report song id play position
        if (!sg_song_list.switch_quality) {
            strncpy(sg_ProductData.m_cur_song_id, song_info->song_id, sizeof(sg_ProductData.m_cur_song_id) - 1);
            sg_DataTemplate[M_CUR_SONG_ID_E].state   = eCHANGED;
            sg_DataTemplate[M_PAUSE_PLAY_E].state    = eCHANGED;
            sg_ProductData.m_play_position           = 0;
            sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;

            // in list set curr play index to report
            if (!sg_song_list.not_in_list) {
                sg_ProductData.m_song_index           = sg_song_list.curr_play_index;
                sg_DataTemplate[m_SONG_INDEX_E].state = eCHANGED;
            }
        } else {
            sg_song_list.switch_quality = false;
        }

        sg_ProductData.m_recommend_quality           = quality;
        sg_DataTemplate[M_RECOMMEND_QUALITY_E].state = eCHANGED;

        sg_song_list.start_play = true;
    } else {
        _kgmusic_passive_play_next();
    }

    sg_song_list.not_in_list = false;

    return;
}

/*control data may be for get status replay*/
static void _kgmusic_get_status_cb(void *pClient, Method method, ReplyAck replyAck, const char *pReceivedJsonDocument,
                                   void *pUserdata)
{
    Request *request = (Request *)pUserdata;

    Log_d("replyAck=%d", replyAck);
    if (NULL == pReceivedJsonDocument) {
        Log_d("Received Json Document is NULL");
    } else {
        Log_d("Received Json Document=%s", pReceivedJsonDocument);
    }

    if (*((ReplyAck *)request->user_context) == ACK_ACCEPTED) {
        IOT_Template_ClearControl(pClient, request->client_token, NULL, QCLOUD_IOT_MQTT_COMMAND_TIMEOUT);
    } else {
        *((ReplyAck *)request->user_context) = replyAck;
    }

    int   i        = 0;
    char *reported = LITE_json_value_of("data.reported", (char *)pReceivedJsonDocument);
    if (reported == NULL) {
        return;
    }
    int len = strlen(reported);
    for (i = 0; i < TOTAL_PROPERTY_COUNT; i++) {
        /* handle self defined string/json here. Other properties are dealed in _handle_delta()*/
        if (sg_DataTemplate[i].state == eCHANGED) {
            continue;
        }
        if (true == update_value_if_key_match(reported, &(sg_DataTemplate[i].data_property))) {
            OnControlMsgCallback(pClient, reported, len, &(sg_DataTemplate[i].data_property));
        }
    }

    HAL_Free(reported);

    return;
}

static int _kgmusic_get_status_sync(void *pClient, uint32_t timeout_ms)
{
    IOT_FUNC_ENTRY;
    int rc = QCLOUD_RET_SUCCESS;

    POINTER_SANITY_CHECK(pClient, QCLOUD_ERR_INVAL);
    NUMBERIC_SANITY_CHECK(timeout_ms, QCLOUD_ERR_INVAL);

    Qcloud_IoT_Template *pTemplate = (Qcloud_IoT_Template *)pClient;
    Timer                wait_timer;
    InitTimer(&wait_timer);

    if (IOT_MQTT_IsConnected(pTemplate->mqtt) == false) {
        IOT_FUNC_EXIT_RC(QCLOUD_ERR_MQTT_NO_CONN);
    }

    ReplyAck reply = ACK_NONE;
    rc             = IOT_Template_GetStatus(pClient, _kgmusic_get_status_cb, &reply, timeout_ms);
    if (rc != QCLOUD_RET_SUCCESS) {
        return rc;
    }
    countdown_ms(&wait_timer, timeout_ms);
    while (false == expired(&wait_timer) && reply == ACK_NONE) {
        IOT_Template_Yield(pClient, 200);
    }

    if (ACK_ACCEPTED == reply) {
        rc = QCLOUD_RET_SUCCESS;
    } else {
        rc = QCLOUD_ERR_FAILURE;
    }

    return rc;
}

static bool _kgmusic_downmsg_proc_playpause(void *client)
{
    bool play_song = false;

    if (sg_DataTemplate[M_PAUSE_PLAY_E].state == eCHANGED) {
        sg_DataTemplate[M_PAUSE_PLAY_E].state = eNOCHANGE;
        if (sg_ProductData.m_pause_play == 0 && sg_song_list.start_play == true) {
            sg_player.pause(&sg_player);
            Log_d("player pause");
        } else if (sg_ProductData.m_pause_play == 1 && sg_song_list.start_play == true) {
            sg_player.unpause(&sg_player, sg_song_list.curr_play_position);
            Log_d("player unpause");
        } else if (sg_ProductData.m_pause_play == 1 && sg_song_list.start_play == false) {
            if (sg_song_list.start_play == false) {
                play_song = true;
                Log_d("play song");
            }
        }
    }

    return play_song;
}

static bool _kgmusic_downmsg_proc_playlist(void *client)
{
    bool play_song = false;

    if (sg_DataTemplate[M_CUR_PLAY_LIST_E].state == eCHANGED) {
        sg_DataTemplate[M_CUR_PLAY_LIST_E].state = eNOCHANGE;
        _kgmusic_parse_curr_play_list(client, sg_ProductData.m_cur_play_list);
        play_song = true;
    }

    return play_song;
}

static bool _kgmusic_downmsg_proc_song_index(void *client)
{
    bool play_song = false;

    if (sg_DataTemplate[m_SONG_INDEX_E].state == eCHANGED) {
        sg_DataTemplate[m_SONG_INDEX_E].state = eNOCHANGE;
        sg_song_list.curr_play_index          = sg_ProductData.m_song_index;
        play_song                             = true;
    }
    return play_song;
}

static bool _kgmusic_downmsg_proc_songid(void *client)
{
    bool play_song = false;

    // song id check
    if (sg_DataTemplate[M_CUR_SONG_ID_E].state == eCHANGED) {
        sg_DataTemplate[M_CUR_SONG_ID_E].state = eNOCHANGE;
        int song_index                         = sg_song_list.curr_play_index % QCLOUD_IOT_KGMUSIC_PAGESIZE;

        if ((sg_song_list.song_info_list[song_index].song_id == NULL) ||
            (0 != strcmp(sg_ProductData.m_cur_song_id, sg_song_list.song_info_list[song_index].song_id))) {
            int find_index = _kgmusic_find_index_from_list(client, sg_ProductData.m_cur_song_id);
            if (find_index != -1) {
                sg_song_list.curr_play_index = find_index;
            }
            play_song = true;
            Log_d("play song id %s, %d", sg_ProductData.m_cur_song_id, find_index);
        }
    }
    return play_song;
}

static bool _kgmusic_downmsg_proc_nextprev(void *client)
{
    bool play_song = false;

    if (sg_DataTemplate[M_PRE_NEXT_E].state == eCHANGED) {
        sg_DataTemplate[M_PRE_NEXT_E].state = eNOCHANGE;
        if (sg_ProductData.m_pre_next == 2) {
            _kgmusic_next_song(client, true);
            Log_d("play next song %d", sg_song_list.curr_play_index);
        } else if (sg_ProductData.m_pre_next == 1) {
            _kgmusic_prev_song(client, true);
            Log_d("play prev song %d", sg_song_list.curr_play_index);
        }
        play_song = true;
    }

    return play_song;
}

static bool _kgmusic_downmsg_play_proc(void *client)
{
    // check chanaged
    bool play_song = false;

    // first check
    if (sg_DataTemplate[M_PLAY_POSITION_E].state == eCHANGED && sg_song_list.start_play == true) {
        sg_DataTemplate[M_PLAY_POSITION_E].state = eNOCHANGE;
        if (sg_song_list.curr_song_duration < sg_ProductData.m_play_position * 1000) {
            _kgmusic_passive_play_next();
        } else if (sg_song_list.curr_play_position != sg_ProductData.m_play_position * 1000) {
            sg_player.set_position(&sg_player, sg_ProductData.m_play_position);
            sg_song_list.seek_play_position = sg_ProductData.m_play_position;
        }
    } else if (sg_DataTemplate[M_PLAY_POSITION_E].state == eCHANGED && sg_song_list.start_play == false) {
        sg_DataTemplate[M_PLAY_POSITION_E].state = eNOCHANGE;
        sg_ProductData.m_play_position           = 0;
    }

    // second check
    play_song |= _kgmusic_downmsg_proc_song_index(client);
    play_song |= _kgmusic_downmsg_proc_playlist(client);
    play_song |= _kgmusic_downmsg_proc_songid(client);
    play_song |= _kgmusic_downmsg_proc_playpause(client);

    if (play_song == false) {
        play_song |= _kgmusic_downmsg_proc_nextprev(client);
    }

    if (sg_DataTemplate[M_VOLUME_E].state == eCHANGED) {
        sg_DataTemplate[M_VOLUME_E].state = eNOCHANGE;
        sg_player.set_volume(&sg_player, sg_ProductData.m_volume);
        sg_song_list.curr_play_volume = sg_ProductData.m_volume;
    }

    if (sg_DataTemplate[M_RECOMMEND_QUALITY_E].state == eCHANGED) {
        sg_DataTemplate[M_RECOMMEND_QUALITY_E].state = eNOCHANGE;
        sg_song_list.set_play_quality                = sg_ProductData.m_recommend_quality;

        if (play_song == false && sg_song_list.start_play == true) {
            sg_song_list.switch_quality = true;
            play_song                   = true;
            Log_d("chanage quality");
        }
    }

    return play_song;
}

static void _kgmusic_get_player_play_position()
{
    if (sg_song_list.start_play == true) {
        int position = sg_player.get_position(&sg_player);
        if (position != -1 && position < sg_song_list.seek_play_position) {
            sg_player.set_position(&sg_player, sg_song_list.seek_play_position);
        } else if (position != -1 && position > sg_song_list.seek_play_position &&
                   position != sg_ProductData.m_play_position) {
            sg_song_list.seek_play_position          = 0;
            sg_song_list.curr_play_position          = position * 1000;
            sg_ProductData.m_play_position           = position;
            sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;
        }
    }
}

static void _kgmusic_get_player_volume()
{
    if (sg_song_list.start_play == true) {
        int volume = sg_player.get_volume(&sg_player);
        if (volume != sg_ProductData.m_volume) {
            sg_ProductData.m_volume           = volume;
            sg_DataTemplate[M_VOLUME_E].state = eCHANGED;
        }
    }
}

static void _kgmusic_check_player_end()
{
    if (sg_song_list.start_play == true && sg_player.check_play_end(&sg_player) == true) {
        sg_song_list.start_play         = false;
        sg_song_list.curr_play_position = 0;
        _kgmusic_passive_play_next();
    }
}

int main(int argc, char **argv)
{
    int             rc;
    sReplyPara      replyPara;
    DeviceProperty *pReportDataList[TOTAL_PROPERTY_COUNT];
    int             ReportCont;

    // init log level
    IOT_Log_Set_Level(eLOG_DEBUG);
    // parse arguments for device info file
    rc = parse_arguments(argc, argv);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("parse arguments error, rc = %d", rc);
        return rc;
    }

    // init connection
    TemplateInitParams init_params = DEFAULT_TEMPLATE_INIT_PARAMS;
    rc                             = _setup_connect_init_params(&init_params);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("init params err,rc=%d", rc);
        return rc;
    }

    void *client = IOT_Template_Construct(&init_params, NULL);
    if (client != NULL) {
        Log_i("Cloud Device Construct Success");
    } else {
        Log_e("Cloud Device Construct Failed");
        return QCLOUD_ERR_FAILURE;
    }

#ifdef MULTITHREAD_ENABLED
    if (QCLOUD_RET_SUCCESS != IOT_Template_Start_Yield_Thread(client)) {
        Log_e("start template yield thread fail");
        goto exit;
    }
#endif

    // user init
    _usr_init();

    // init data template
    _init_data_template();

    rc = _register_data_template_property(client);

    // register data template propertys here
    // rc = _register_data_template_property(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template propertys Success");
    } else {
        Log_e("Register data template propertys Failed: %d", rc);
        goto exit;
    }

    memset(&sg_song_list, 0, sizeof(sg_song_list));
    sg_song_list.curr_play_index      = -1;
    sg_iot_kgmusic.mqtt_client        = IOT_Template_Get_MQTT_Client(client);
    sg_iot_kgmusic.user_data          = NULL;
    sg_iot_kgmusic.song_info_callback = _kgmusic_query_songinfo_callback;
    sg_iot_kgmusic.song_list_callback = _kgmusic_query_songlist_callback;
    IOT_KGMUSIC_enable(IOT_Template_Get_MQTT_Client(client), init_params.product_id, init_params.device_name,
                       &sg_iot_kgmusic);

    // register data template actions here
#ifdef ACTION_ENABLED
    rc = _register_data_template_action(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template actions Success");
    } else {
        Log_e("Register data template actions Failed: %d", rc);
        goto exit;
    }
#endif

    // report device info, then you can manager your product by these info, like position
    rc = _get_sys_info(client, sg_data_report_buffer, sg_data_report_buffersize);
    if (QCLOUD_RET_SUCCESS == rc) {
        rc = IOT_Template_Report_SysInfo_Sync(client, sg_data_report_buffer, sg_data_report_buffersize,
                                              QCLOUD_IOT_MQTT_COMMAND_TIMEOUT);
        if (rc != QCLOUD_RET_SUCCESS) {
            Log_e("Report system info fail, err: %d", rc);
        }
    } else {
        Log_e("Get system info fail, err: %d", rc);
    }

    // get the property changed during offline
    rc = _kgmusic_get_status_sync(client, QCLOUD_IOT_MQTT_COMMAND_TIMEOUT);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("Get data status fail, err: %d", rc);
    } else {
        Log_d("Get data status success");
    }

    while (IOT_Template_IsConnected(client) || rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT ||
           rc == QCLOUD_RET_MQTT_RECONNECTED || QCLOUD_RET_SUCCESS == rc) {
        rc = IOT_Template_Yield(client, 200);
        if (rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT) {
            HAL_SleepMs(1000);
            continue;
        } else if (rc != QCLOUD_RET_SUCCESS) {
            Log_e("Exit loop caused of errCode: %d", rc);
        }

        /* handle control msg from server */
        if (sg_control_msg_arrived) {
            deal_down_stream_user_logic(client, &sg_ProductData);

            /* control msg should reply, otherwise server treat device didn't receive and retain the msg which would be
             * get by  get status*/
            memset((char *)&replyPara, 0, sizeof(sReplyPara));
            replyPara.code          = eDEAL_SUCCESS;
            replyPara.timeout_ms    = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
            replyPara.status_msg[0] = '\0';  // add extra info to replyPara.status_msg when error occured

            rc = IOT_Template_ControlReply(client, sg_data_report_buffer, sg_data_report_buffersize, &replyPara);
            if (rc == QCLOUD_RET_SUCCESS) {
                Log_d("Contol msg reply success");
            } else {
                Log_e("Contol msg reply failed, err: %d", rc);
            }
        } else {
            Log_d("No control msg received...");
        }

        /*report msg to server*/
        /*report the lastest properties's status*/
        if (QCLOUD_RET_SUCCESS == deal_up_stream_user_logic(pReportDataList, &ReportCont)) {
            rc = IOT_Template_JSON_ConstructReportArray(client, sg_data_report_buffer, sg_data_report_buffersize,
                                                        ReportCont, pReportDataList);
            if (rc == QCLOUD_RET_SUCCESS) {
                rc = IOT_Template_Report(client, sg_data_report_buffer, sg_data_report_buffersize,
                                         OnReportReplyCallback, NULL, QCLOUD_IOT_MQTT_COMMAND_TIMEOUT);
                if (rc == QCLOUD_RET_SUCCESS) {
                    Log_i("data template reporte success");
                } else {
                    Log_e("data template reporte failed, err: %d", rc);
                }
            } else {
                Log_e("construct reporte data failed, err: %d", rc);
            }

        } else {
            // Log_d("no data need to be reported or someting goes wrong");
        }
        
        void *mqtt_client = IOT_Template_Get_MQTT_Client(client);
        if (_kgmusic_downmsg_play_proc(mqtt_client)) {
            _kgmusic_playsong(mqtt_client);
        } else if (_kgmusic_user_play_proc(mqtt_client)) {
            _kgmusic_playsong(mqtt_client);
        } else if (_kgmusic_passive_play_proc(mqtt_client)) {
            _kgmusic_playsong(mqtt_client);
        } else {
            _kgmusic_get_player_play_position();
            _kgmusic_get_player_volume();
        }

        _kgmusic_check_player_end();
    }

exit:

#ifdef MULTITHREAD_ENABLED
    IOT_Template_Stop_Yield_Thread(client);
#endif
    rc = IOT_Template_Destroy(client);

    return rc;
}
