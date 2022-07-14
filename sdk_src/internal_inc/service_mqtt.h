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

#ifndef IOT_SERVICE_COM_H_
#define IOT_SERVICE_COM_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

#define FIELD_METHOD                                 "method"
#define METHOD_RES_REPORT_VERSION_RSP                "report_version_rsp"
#define METHOD_RES_UPDATE_RESOURCE                   "update_resource"
#define METHOD_RES_DELETE_RESOURCE                   "del_resource"
#define METHOD_RES_REQ_URL_RESP                      "request_url_resp"
#define METHOD_FACE_AI_REPLY                         "call_service_reply"
#define METHOD_GATEWAY_AUTOMATION_SET                "set_automation"
#define METHOD_GATEWAY_AUTOMATION_DEL                "del_automation"
#define METHOD_GATEWAY_AUTOMATION_LIST               "get_automation_list_reply"
#define METHOD_UNBIND_DEVICE                         "unbind_device"
#define METHOD_UNBIND_DEVICE_REPLY                   "unbind_device_reply"
#define METHOD_KGMUSIC_QUERY_SONG_REPLY              "kugou_query_song_reply"
#define METHOD_KGMUSIC_QUERY_PID_REPLY               "kugou_query_pid_reply"
#define METHOD_KGMUSIC_QUERY_SONGLIST_REPLY          "kugou_user_command_reply"
#define METHOD_ALEART_FENCE_EVENT                    "alert_fence_event"
#define METHOD_ALEART_FENCE_EVENT_REPLY              "alert_fence_event_reply"
#define METHOD_GATEWAY_SCENE_HANDLES                 "gateway_scene_handles"
#define METHOD_GATEWAY_RUN_SCENE                     "gateway_run_scene"
#define METHOD_GATEWAY_DELETE_SCENE                  "gateway_delete_scene"
#define METHOD_GATEWAY_RELOAD_SCENE_HANDLES          "gateway_reload_scene_handles"
#define METHOD_GATEWAY_RELOAD_SCENE_HANDLES_REPLY    "gateway_reload_scene_handles_reply"
#define METHOD_GATEWAY_REPORT_INNER_SCENE_LIST       "gateway_report_inner_scene_list"
#define METHOD_GATEWAY_REPORT_INNER_SCENE_LIST_REPLY "gateway_report_inner_scene_list_reply"
#define METHOD_GATEWAY_RELOAD_GROUP_DEVICES          "gateway_reload_group_devices"
#define METHOD_GATEWAY_RELOAD_GROUP_DEVICES_REPLY    "gateway_reload_group_devices_reply"
#define METHOD_GATEWAY_DELETE_GROUP                  "gateway_delete_group"
#define METHOD_GATEWAY_GROUP_DEVICES                 "gateway_group_devices"
#define METHOD_GATEWAY_GROUP_CONTROL                 "gateway_group_control"

typedef enum {
    eSERVICE_RESOURCE           = 0,
    eSERVICE_FACE_AI            = 1,
    eSERVICE_UNBIND_DEV         = 2,
    eSERVICE_UNBIND_DEV_REPLY   = 3,
    eSERVICE_GATEWAY_AUTOMATION = 4,
    eSERVICE_KGMUSIC            = 5,
    eSERVICE_LOCATION           = 6,
    eSERVICE_GATEWAY_SCENE      = 7,
    eSERVICE_GATEWAY_GROUP      = 8,
    eSERVICE_DEFAULT            = 0xff
} eServiceEvent;

typedef void (*OnServiceMessageCallback)(void *pContext, const char *msg, uint32_t msgLen);

int  qcloud_service_mqtt_init(const char *productId, const char *deviceName, void *mqtt_client);
void qcloud_service_mqtt_deinit(void *mqtt_client);
int  qcloud_service_mqtt_post_msg(void *mqtt_client, const char *msg, int qos);
int  qcloud_service_mqtt_event_register(eServiceEvent evt, OnServiceMessageCallback callback, void *context);

#ifdef __cplusplus
}
#endif

#endif /* IOT_SERVICE_COM_H_ */
