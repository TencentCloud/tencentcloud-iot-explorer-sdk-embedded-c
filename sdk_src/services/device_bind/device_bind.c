/**
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2021 THL A29 Limited, a Tencent company.All rights reserved.
 *
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */

#ifdef __cplusplus
extern "C" {
#endif

#include <string.h>

#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"
#include "mqtt_client.h"
#include "service_mqtt.h"
#include "json_parser.h"

static bool sg_unbind_device_reply = false;

static void _unbind_device_reply_cb(void *user_data, const char *payload, unsigned int payload_len)
{
    if (payload == NULL) {
        Log_e("params is null");
        return;
    }

    Log_d("recv: %s", payload);

    char *method = LITE_json_value_of("method", (char *)payload);
    char *code   = LITE_json_value_of("code", (char *)payload);

    if (code == NULL || atoi(code) != 0) {
        Log_e("query reply code error. code : %s", code == NULL ? "" : code);
        goto end;
    }
    if (method == NULL || 0 == strncmp(method, METHOD_UNBIND_DEVICE_REPLY, sizeof(METHOD_UNBIND_DEVICE_REPLY) - 1)) {
        // proc
        sg_unbind_device_reply = true;
    }
end:
    HAL_Free(code);
    HAL_Free(method);
}

int IOT_Unbind_Device_Register(void *callback, void *context)
{
    POINTER_SANITY_CHECK(callback, QCLOUD_ERR_INVAL);
    OnServiceMessageCallback cb = (OnServiceMessageCallback)callback;
    return qcloud_service_mqtt_event_register(eSERVICE_UNBIND_DEV, cb, context);
}

int IOT_Unbind_Device_ByCloud(void *mqtt_client, void *callback, void *context)
{
    int rc = -1;
    POINTER_SANITY_CHECK(mqtt_client, QCLOUD_ERR_INVAL);
    Qcloud_IoT_Client *m_client = (Qcloud_IoT_Client *)mqtt_client;
    rc = qcloud_service_mqtt_init(m_client->device_info.product_id, m_client->device_info.device_name, mqtt_client);
    if (rc < 0) {
        Log_e("service init failed: %d", rc);
        return rc;
    }
    return IOT_Unbind_Device_Register(callback, context);
}

int IOT_Unbind_Device_Request(void *mqtt_client, int timeout_ms)
{
    int        rc;
    char       message[256]           = {0};
    static int sg_unbind_device_index = 0;
    POINTER_SANITY_CHECK(mqtt_client, QCLOUD_ERR_INVAL);
    Qcloud_IoT_Client *m_client = (Qcloud_IoT_Client *)mqtt_client;
    rc = qcloud_service_mqtt_init(m_client->device_info.product_id, m_client->device_info.device_name, mqtt_client);
    if (rc < 0) {
        Log_e("service init failed: %d", rc);
        return rc;
    }
    rc = qcloud_service_mqtt_event_register(eSERVICE_UNBIND_DEV_REPLY, _unbind_device_reply_cb, NULL);
    if (QCLOUD_RET_SUCCESS != rc) {
        Log_e("register service event %d fail", eSERVICE_UNBIND_DEV_REPLY);
        return rc;
    }
    sg_unbind_device_index++;
    HAL_Snprintf(message, sizeof(message),
                 "{\"method\":\"unbind_device_request\", \"clientToken\":\"%s-%d\", \"deviceName\":\"%s\", "
                 "\"productId\":\"%s\"}",
                 m_client->device_info.product_id, sg_unbind_device_index, m_client->device_info.device_name,
                 m_client->device_info.product_id);
    rc = qcloud_service_mqtt_post_msg(mqtt_client, message, QOS0);
    if (QCLOUD_RET_SUCCESS > rc) {
        Log_e("service mqtt post msg failed");
        return rc;
    }

    Timer wait_timer;
    InitTimer(&wait_timer);
    countdown_ms(&wait_timer, timeout_ms);
    sg_unbind_device_reply = false;
    while (!expired(&wait_timer) && sg_unbind_device_reply == false) {
        if (QCLOUD_RET_SUCCESS != IOT_MQTT_Yield(mqtt_client, 200)) {
            break;
        }
        HAL_SleepMs(200);
    }
    if (sg_unbind_device_reply == false) {
        Log_e("unbind device request happen error.");
        return QCLOUD_ERR_FAILURE;
    }
    return QCLOUD_RET_SUCCESS;
}

#ifdef __cplusplus
}
#endif
