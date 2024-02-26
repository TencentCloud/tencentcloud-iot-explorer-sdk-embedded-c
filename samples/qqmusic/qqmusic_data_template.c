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
#include "data_config.c"

#ifdef AUTH_MODE_CERT
static char sg_cert_file[PATH_MAX + 1];  // full path of device cert file
static char sg_key_file[PATH_MAX + 1];   // full path of device key file
#endif

static DeviceInfo    sg_devInfo;
static MQTTEventType sg_subscribe_event_result = MQTT_EVENT_UNDEF;
static bool          sg_control_msg_arrived    = false;
static int           sg_control_reply_code     = 0;
static char          sg_data_report_buffer[2048];
static size_t        sg_data_report_buffersize = sizeof(sg_data_report_buffer) / sizeof(sg_data_report_buffer[0]);

static void    *sg_qq_music          = NULL;
static void    *sg_music_player      = NULL;
static uint32_t sg_need_play_song_id = 0;

#include "action_config.c"

// action : regist action and set the action handle callback, add your aciton logic here
static void OnActionCallback(void *pClient, const char *pClientToken, DeviceAction *pAction)
{
    int        i;
    sReplyPara replyPara;

    // do something base on input, just print as an sample
    DeviceProperty *pActionInput = pAction->pInput;
    for (i = 0; i < pAction->input_num; i++) {
        if (!strncmp(pActionInput[i].key, "pre_or_next", sizeof("pre_or_next") - 1)) {
            int              pre_or_next  = *((int *)pActionInput[i].data);
            QQMusicSongInfo *playSongInfo = IOT_QQMusic_GetPlaySongInfo(0, pre_or_next, sg_qq_music);
            if (playSongInfo != NULL) {
                Log_d("song name : %s", playSongInfo->songName);
                Log_d("signer name : %s", playSongInfo->singerName);
                Log_d("song id : %d", playSongInfo->songId);
                sg_pre_or_next_out_code =
                    HAL_Music_Play(&sg_music_player, playSongInfo->songUrl, playSongInfo->songPlayTime);
                if (!sg_pre_or_next_out_code) {
                    // change property & report
                    sg_DataTemplate[M_PLAY_PAUSE_E].state    = eCHANGED;
                    sg_ProductData.m_pause_play              = 1;  // play
                    sg_DataTemplate[M_CUR_SONG_ID_E].state   = eCHANGED;
                    sg_ProductData.m_song_id                 = playSongInfo->songId;
                    sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;
                    sg_ProductData.m_play_position           = 0;
                    HAL_Music_SetPlayPosition(sg_music_player, sg_ProductData.m_play_position);
                }
            }
            break;
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

static int _play_song(QQMusicSongInfo *playSongInfo)
{
    Log_d("song name : %s", playSongInfo->songName);
    Log_d("signer name : %s", playSongInfo->singerName);
    Log_d("song id : %u", playSongInfo->songId);
    if (!HAL_Music_Play(&sg_music_player, playSongInfo->songUrl, playSongInfo->songPlayTime)) {
        // change property & report
        sg_DataTemplate[M_PLAY_PAUSE_E].state    = eCHANGED;
        sg_ProductData.m_pause_play              = 1;  // play
        sg_DataTemplate[M_CUR_SONG_ID_E].state   = eCHANGED;
        sg_ProductData.m_song_id                 = playSongInfo->songId;
        sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;
        sg_ProductData.m_play_position           = 0;
        HAL_Music_SetPlayPosition(sg_music_player, sg_ProductData.m_play_position);
        Log_d("play success.");
        return QCLOUD_RET_SUCCESS;
    }
    Log_e("play failed!");
    return QCLOUD_ERR_FAILURE;
}

static void OnControlMsgCallback(void *pClient, const char *pJsonValueBuffer, uint32_t valueLength,
                                 DeviceProperty *pProperty)
{
    Log_i("Property=%s changed", pProperty->key);
    sg_control_msg_arrived = true;
    sg_control_reply_code  = 0;
    /* handle self defined string/json here. Other properties are dealed in _handle_delta()*/
    if (!strcmp(sg_DataTemplate[M_PLAY_MODE_E].data_property.key, pProperty->key)) {
        sg_DataTemplate[M_PLAY_MODE_E].state = eCHANGED;
    } else if (!strcmp(sg_DataTemplate[M_VOLUME_E].data_property.key, pProperty->key)) {
        sg_DataTemplate[M_VOLUME_E].state = eCHANGED;
        HAL_Music_SetVolume(sg_music_player, sg_ProductData.m_volume);
        sg_ProductData.m_volume = HAL_Music_GetVolume(sg_music_player);
    } else if (!strcmp(sg_DataTemplate[M_PLAY_POSITION_E].data_property.key, pProperty->key)) {
        sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;
        sg_ProductData.m_play_position = HAL_Music_SetPlayPosition(sg_music_player, sg_ProductData.m_play_position);
    } else if (!strcmp(sg_DataTemplate[M_RECOMMEND_QUALITY_E].data_property.key, pProperty->key)) {
        sg_DataTemplate[M_RECOMMEND_QUALITY_E].state = eCHANGED;
        // TODO : recommend quality
    } else if (!strcmp(sg_DataTemplate[M_CUR_SONG_ID_E].data_property.key, pProperty->key)) {
        // play from song id
        sg_DataTemplate[M_CUR_SONG_ID_E].state = eCHANGED;
        QQMusicSongInfo *playSongInfo =
            IOT_QQMusic_GetPlaySongInfo(sg_ProductData.m_song_id, QQMUSIC_GET_NONE, sg_qq_music);
        if (!playSongInfo) {
            // find from cloud
            playSongInfo = IOT_QQMusic_GetSongInfoFromCloud(sg_qq_music, sg_ProductData.m_song_id,
                                                            sg_ProductData.m_recommend_quality, NULL, 5000);
        }
        if (playSongInfo != NULL) {
            _play_song(playSongInfo);
        } else {
            sg_control_reply_code = -1;
            Log_w("can not find song id  %d in song list.", sg_ProductData.m_song_id);
            sg_ProductData.m_song_id                 = 0;
            sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;
        }
    } else if (!strcmp(sg_DataTemplate[M_PLAY_PAUSE_E].data_property.key, pProperty->key)) {
        sg_DataTemplate[M_PLAY_PAUSE_E].state = eCHANGED;
        sg_ProductData.m_pause_play           = HAL_Music_PlayPause(sg_music_player, sg_ProductData.m_pause_play);
    }
}

static void OnReportReplyCallback(void *pClient, Method method, ReplyAck replyAck, const char *pJsonDocument,
                                  void *pUserdata)
{
    Log_i("recv report reply response, reply ack: %d", replyAck);
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
            pReportDataList[j++]     = &(sg_DataTemplate[i].data_property);
            sg_DataTemplate[i].state = eNOCHANGE;
        }
    }

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
        {.key = "append_info", .type = TYPE_TEMPLATE_STRING, .data = "qq music sample"},
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

static int _try_play_next_song(int mode)
{
    int              rc           = -1;
    QQMusicSongInfo *playSongInfo = NULL;
    switch (sg_ProductData.m_play_mode) {
        // 顺序播放
        case 0: {
            playSongInfo = IOT_QQMusic_GetPlaySongInfo(0, QQMUSIC_GET_NEXT_SONG, sg_qq_music);
        } break;
        // 单曲循环
        case 1: {
            playSongInfo = IOT_QQMusic_GetPlaySongInfo(0, QQMUSIC_GET_CURRENT_SONG, sg_qq_music);
        } break;
        // 随机播放
        case 2: {
            playSongInfo = IOT_QQMusic_GetPlaySongInfo(0, QQMUSIC_GET_RANDOM_SONG, sg_qq_music);
        } break;
        default:
            Log_e("unknow play mode : %d", mode);
            break;
    }
    if (!playSongInfo) {
        return rc;
    }

    Log_d("song name : %s", playSongInfo->songName);
    Log_d("signer name : %s", playSongInfo->singerName);
    Log_d("song id : %d", playSongInfo->songId);
    rc = HAL_Music_Play(&sg_music_player, playSongInfo->songUrl, playSongInfo->songPlayTime);
    if (!rc) {
        // change property & report
        sg_ProductData.m_pause_play    = 1;  // play
        sg_ProductData.m_song_id       = playSongInfo->songId;
        sg_ProductData.m_play_position = 1;
    } else {
        sg_ProductData.m_pause_play    = 0;  // pause
        sg_ProductData.m_song_id       = 0;
        sg_ProductData.m_play_position = 0;
    }
    sg_DataTemplate[M_PLAY_PAUSE_E].state    = eCHANGED;
    sg_DataTemplate[M_CUR_SONG_ID_E].state   = eCHANGED;
    sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;
    return rc;
}

static void _sync_song_list_callback(char *song_list_str, int song_list_len)
{
    Log_d("%.*s", song_list_len, song_list_str);
    memcpy(sg_ProductData.m_song_list, song_list_str, song_list_len);
    sg_ProductData.m_song_list[song_list_len] = '\0';
    sg_DataTemplate[M_SONG_LIST_E].state      = eCHANGED;
}

static void _play_song_in_song_list(uint32_t song_id)
{
    Log_d("will play song id : %u", song_id);
    sg_need_play_song_id = song_id;
    return;
}

int main(int argc, char **argv)
{
    int              rc;
    sReplyPara       replyPara;
    DeviceProperty  *pReportDataList[TOTAL_PROPERTY_COUNT];
    QQMusicSongInfo *playSongInfo;
    int              ReportCont;
    int              currentPlayPosition = 0;
    bool             playEnd             = false;
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

    // register data template propertys here
    rc = _register_data_template_property(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template propertys Success");
    } else {
        Log_e("Register data template propertys Failed: %d", rc);
        goto exit;
    }

    // register data template actions here
    rc = _register_data_template_action(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template actions Success");
    } else {
        Log_e("Register data template actions Failed: %d", rc);
        goto exit;
    }

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
    rc = IOT_Template_GetStatus_sync(client, QCLOUD_IOT_MQTT_COMMAND_TIMEOUT);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("Get data status fail, err: %d", rc);
    } else {
        Log_d("Get data status success");
    }

    QQMusicCallback qq_music_callback;
    qq_music_callback.sync_song_list_callback = _sync_song_list_callback;
    qq_music_callback.play_song_in_song_list  = _play_song_in_song_list;
    rc = IOT_QQMusic_Enable(IOT_Template_Get_MQTT_Client(client), qq_music_callback, &sg_qq_music);
    if (rc) {
        Log_e(" enable qqmusic failed");
        goto exit;
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
            replyPara.code          = sg_control_reply_code;
            replyPara.timeout_ms    = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
            replyPara.status_msg[0] = '\0';  // add extra info to replyPara.status_msg when error occured

            rc = IOT_Template_ControlReply(client, sg_data_report_buffer, sg_data_report_buffersize, &replyPara);
            if (rc == QCLOUD_RET_SUCCESS) {
                Log_d("Contol msg reply success");
                sg_control_msg_arrived = false;
            } else {
                Log_e("Contol msg reply failed, err: %d", rc);
            }
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
        // HAL_SleepMs(3000);
        // music play control here
        if (sg_ProductData.m_pause_play) {
            playEnd             = HAL_Music_PlayEndCheck(sg_music_player);
            currentPlayPosition = HAL_Music_GetPlayPosition(sg_music_player);
            if (!playEnd && (((currentPlayPosition - sg_ProductData.m_play_position >= 10) &&
                              (currentPlayPosition - sg_ProductData.m_play_position <= 20)))) {
                sg_DataTemplate[M_PLAY_POSITION_E].state = eCHANGED;
                sg_DataTemplate[M_CUR_SONG_ID_E].state   = eCHANGED;
                sg_ProductData.m_play_position           = currentPlayPosition;
            }
            if (playEnd) {
                _try_play_next_song(sg_ProductData.m_play_mode);
            }
        }
        if (sg_need_play_song_id) {
            playSongInfo = IOT_QQMusic_GetPlaySongInfo(sg_need_play_song_id, QQMUSIC_GET_NONE, sg_qq_music);
            if (playSongInfo) {
                sg_need_play_song_id = 0;
                _play_song(playSongInfo);
            }
        }
    }

exit:

#ifdef MULTITHREAD_ENABLED
    IOT_Template_Stop_Yield_Thread(client);
#endif
    rc = IOT_Template_Destroy(client);
    IOT_QQMusic_Disable(&sg_qq_music);
    return rc;
}
