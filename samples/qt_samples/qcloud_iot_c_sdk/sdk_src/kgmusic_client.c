/*
 * Tencent is pleased to support the open source community by making IoT Hub
 available.
 * Copyright (C) 2016 THL A29 Limited, a Tencent company. All rights reserved.

 * Licensed under the MIT License (the "License"); you may not use this file
 except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT

 * Unless required by applicable law or agreed to in writing, software
 distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 KIND,
 * either express or implied. See the License for the specific language
 governing permissions and
 * limitations under the License.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils_timer.h"
#include "lite-utils.h"
#include "mqtt_client.h"
#include "service_mqtt.h"
#include "qcloud_iot_import.h"
#include "qcloud_iot_export.h"

static int sg_kgmusic_client_token_index = 0;

typedef struct {
    char *type_key;
    char *json_format;
    bool  no_page;
    bool  no_list_id;
} KgmusicCommandJsonFormat;

const static KgmusicCommandJsonFormat sg_kgmusic_command_json_format[] = {
    {"album_info", "{\"album_id\":\"%s\",\"page\":%d,\"size\":%d,\"kugou_command\":\"%s\",\"except_fields\":\"%s\"}",
     false, false},
    {"playlist_song",
     "{\"playlist_id\": \"%s\",\"page\":%d,\"size\":%d,\"kugou_command\":\"%s\",\"except_fields\":\"%s\"}", false,
     false},
    {"awesome_everyday", "{\"kugou_command\":\"%s\",\"except_fields\":\"%s\"}", true, true},
    {"awesome_recommend", "{\"kugou_command\":\"%s\",\"except_fields\":\"%s\"}", true, true},
    {"awesome_newsong", "{\"top_id\":%s,\"page\":%d,\"size\":%d,\"kugou_command\":\"%s\",\"except_fields\":\"%s\"}",
     false, false},
    {"top_song", "{\"top_id\":\"%s\",\"page\":%d,\"size\":%d,\"kugou_command\":\"%s\",\"except_fields\":\"%s\"}", false,
     false}};

static int _qcloud_iot_kgmusic_wait_reply(void *mqtt_client, QcloudIotKgmusic *kgmusic, int timeout_ms)
{
    Timer wait_timer;
    InitTimer(&wait_timer);
    countdown_ms(&wait_timer, timeout_ms);
    while (false == expired(&wait_timer) && kgmusic->e_reply_result == E_KGMUSIC_QUERY_REPLY_TIMEOUT) {
        if (QCLOUD_RET_SUCCESS != IOT_MQTT_Yield(mqtt_client, 200)) {
            break;
        }
        HAL_SleepMs(200);
    }

    if (kgmusic->e_reply_result == E_KGMUSIC_QUERY_REPLY_TIMEOUT) {
        Log_e("recv timeout");
        return QCLOUD_IOT_KGMUSIC_QUERY_RECV_TIMEOUT;
    }
    if (kgmusic->e_reply_result == E_KGMUSIC_QUERY_REPLY_ERROR) {
        Log_e("recv error");
        return QCLOUD_IOT_KGMUSIC_QUERY_RECV_ERROR;
    }

    return QCLOUD_RET_SUCCESS;
}

static int _qcloud_iot_kgmusic_publish_msg(void *mqtt_client, char *method, void *params)
{
    char payload[512];

    char *payload_point = payload;
    int   remain_len    = sizeof(payload);

    Qcloud_IoT_Client *client = (Qcloud_IoT_Client *)mqtt_client;

    int payload_len =
        HAL_Snprintf(payload_point, remain_len, "{\"method\":\"%s\",\"clientToken\":\"kugoumusic-%s%s-%d\"", method,
                     client->device_info.product_id, client->device_info.device_name, sg_kgmusic_client_token_index);
    remain_len -= payload_len;
    payload_point += payload_len;

    if (params != NULL) {
        payload_len = HAL_Snprintf(payload_point, remain_len, ",\"params\":%s", params);
        remain_len -= payload_len;
        payload_point += payload_len;
    }

    HAL_Snprintf(payload_point, remain_len, "}");

    sg_kgmusic_client_token_index++;

    if (QCLOUD_RET_SUCCESS > qcloud_service_mqtt_post_msg(mqtt_client, payload, QOS0)) {
        Log_e("publish faliled");
        return QCLOUD_IOT_KGMUSIC_QUERY_SEND_FAILED;
    }

    return QCLOUD_RET_SUCCESS;
}

int IOT_KGMUSIC_syncquery_songlist(void *client, QcloudIotKgmusicSongListParams *list_query_params,
                                   QcloudIotKgmusic *kgmusic, int timeout_ms)
{
    char params[512];

    if (list_query_params->list_type == NULL) {
        return QCLOUD_ERR_FAILURE;
    }

    int i = 0;
    for (i = 0; i < sizeof(sg_kgmusic_command_json_format) / sizeof(KgmusicCommandJsonFormat); i++) {
        if (NULL != strstr(list_query_params->list_type, sg_kgmusic_command_json_format[i].type_key)) {
            if (sg_kgmusic_command_json_format[i].no_page && sg_kgmusic_command_json_format[i].no_list_id) {
                HAL_Snprintf(params, sizeof(params), sg_kgmusic_command_json_format[i].json_format,
                             list_query_params->list_type,
                             list_query_params->except_fields == NULL ? "" : list_query_params->except_fields);
            } else if (sg_kgmusic_command_json_format[i].no_page && !sg_kgmusic_command_json_format[i].no_list_id) {
                HAL_Snprintf(params, sizeof(params), sg_kgmusic_command_json_format[i].json_format,
                             list_query_params->list_id, list_query_params->list_type,
                             list_query_params->except_fields == NULL ? "" : list_query_params->except_fields);
            } else if (!sg_kgmusic_command_json_format[i].no_page && sg_kgmusic_command_json_format[i].no_list_id) {
                HAL_Snprintf(params, sizeof(params), sg_kgmusic_command_json_format[i].json_format,
                             list_query_params->page, list_query_params->page_size, list_query_params->list_type,
                             list_query_params->except_fields == NULL ? "" : list_query_params->except_fields);
            } else {
                HAL_Snprintf(params, sizeof(params), sg_kgmusic_command_json_format[i].json_format,
                             list_query_params->list_id, list_query_params->page, list_query_params->page_size,
                             list_query_params->list_type,
                             list_query_params->except_fields == NULL ? "" : list_query_params->except_fields);
            }

            break;
        }
    }

    if (i == sizeof(sg_kgmusic_command_json_format) / sizeof(KgmusicCommandJsonFormat)) {
        Log_e("error list type :%s", list_query_params->list_type);
        return QCLOUD_ERR_FAILURE;
    }

    kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_TIMEOUT;
    int ret                 = _qcloud_iot_kgmusic_publish_msg(client, "kugou_user_command", params);
    if (QCLOUD_RET_SUCCESS != ret) {
        return ret;
    }

    return _qcloud_iot_kgmusic_wait_reply(client, kgmusic, timeout_ms);
    ;
}

int IOT_KGMUSIC_syncquery_song(void *client, char *song_id, QcloudIotKgmusic *kgmusic, int timeout_ms)
{
    char params[64];

    HAL_Snprintf(params, sizeof(params), "{\"song_id\":\"%s\"}", song_id);

    kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_TIMEOUT;
    int ret                 = _qcloud_iot_kgmusic_publish_msg(client, "kugou_query_song", params);
    if (QCLOUD_RET_SUCCESS != ret) {
        return ret;
    }
    return _qcloud_iot_kgmusic_wait_reply(client, kgmusic, timeout_ms);
}

static int _kgmusic_query_songlist_reply_proc(void *client, char *payload, void *user_data)
{
    if (client == NULL || payload == NULL || user_data == NULL) {
        Log_e("params is null");
        return QCLOUD_ERR_FAILURE;
    }

    QcloudIotKgmusic *kgmusic = (QcloudIotKgmusic *)user_data;
    if (kgmusic->song_list_callback == NULL) {
        kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_ERROR;
        return QCLOUD_ERR_FAILURE;
    }
    // Log_d("recv: %s", payload);

    int ret = QCLOUD_RET_SUCCESS;

    char *code = LITE_json_value_of("code", payload);
    char *data = LITE_json_value_of("data", payload);

    if (code == NULL || 0 != atoi(code) || data == NULL) {
        ret                     = QCLOUD_ERR_FAILURE;
        kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_ERROR;
        goto end;
    }

    kgmusic->song_list_callback(data, kgmusic->user_data);

    kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_OK;

end:
    HAL_Free(code);
    HAL_Free(data);

    return ret;
}

static int _kgmusic_query_song_reply_proc(void *client, char *payload, void *user_data)
{
    if (client == NULL || payload == NULL || user_data == NULL) {
        Log_e("params is null");
        return QCLOUD_ERR_FAILURE;
    }

    QcloudIotKgmusic *kgmusic = (QcloudIotKgmusic *)user_data;
    if (kgmusic->song_info_callback == NULL) {
        kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_ERROR;
        return QCLOUD_ERR_FAILURE;
    }
    // Log_d("recv: %s", payload);

    int   ret  = QCLOUD_RET_SUCCESS;
    char *code = LITE_json_value_of("code", (char *)payload);
    char *data = LITE_json_value_of("data", (char *)payload);
    if (code == NULL || 0 != atoi(code) || data == NULL) {
        ret                     = QCLOUD_ERR_FAILURE;
        kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_ERROR;
        goto end;
    }

    kgmusic->song_info_callback(data, kgmusic->user_data);
    kgmusic->e_reply_result = E_KGMUSIC_QUERY_REPLY_OK;

end:
    HAL_Free(code);
    HAL_Free(data);

    return ret;
}

static void _kgmusic_down_message_callback(void *user_data, const char *payload, unsigned int payload_len)
{
    QcloudIotKgmusic *kgmusic = (QcloudIotKgmusic *)user_data;

    char *method = LITE_json_value_of("method", (char *)payload);

    if (0 == strncmp(method, METHOD_KGMUSIC_QUERY_PID_REPLY, sizeof(METHOD_KGMUSIC_QUERY_PID_REPLY) - 1)) {
        // set auto mation
        //_kgmusic_query_pid_reply_proc(kgmusic->mqtt_client, (char *)payload, user_data);
    } else if (0 == strncmp(method, METHOD_KGMUSIC_QUERY_SONG_REPLY, sizeof(METHOD_KGMUSIC_QUERY_SONG_REPLY) - 1)) {
        // delete auto mation
        _kgmusic_query_song_reply_proc(kgmusic->mqtt_client, (char *)payload, user_data);
    } else if (0 ==
               strncmp(method, METHOD_KGMUSIC_QUERY_SONGLIST_REPLY, sizeof(METHOD_KGMUSIC_QUERY_SONGLIST_REPLY) - 1)) {
        // delete auto mation
        _kgmusic_query_songlist_reply_proc(kgmusic->mqtt_client, (char *)payload, user_data);
    }

    HAL_Free(method);
}

int IOT_KGMUSIC_enable(void *mqtt_client, char *product_id, char *device_name, QcloudIotKgmusic *kgmusic)
{
    if (mqtt_client == NULL || product_id == NULL || device_name == NULL || kgmusic == NULL) {
        Log_e("params is null");
        return QCLOUD_ERR_INVAL;
    }

    qcloud_service_mqtt_event_register(eSERVICE_KGMUSIC, _kgmusic_down_message_callback, kgmusic);

    return qcloud_service_mqtt_init(product_id, device_name, mqtt_client);
}

#ifdef __cplusplus
}
#endif
