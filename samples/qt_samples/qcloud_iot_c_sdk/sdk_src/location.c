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

int IOT_Location_Capture_Fence_Aleart_Register(void *mqtt_client, void *callback, void *context)
{
    int rc = -1;
    POINTER_SANITY_CHECK(mqtt_client, QCLOUD_ERR_INVAL);
    Qcloud_IoT_Client *m_client = (Qcloud_IoT_Client *)mqtt_client;
    rc = qcloud_service_mqtt_init(m_client->device_info.product_id, m_client->device_info.device_name, mqtt_client);
    if (rc < 0) {
        Log_e("service init failed: %d", rc);
        return rc;
    }
    return qcloud_service_mqtt_event_register(eSERVICE_LOCATION, callback, context);
}

int IOT_Location_Fence_Aleart_Reply(void *mqtt_client, char *client_token, int reply_code, char *reply_msg)
{
    int  rc;
    char message[256] = {0};
    POINTER_SANITY_CHECK(mqtt_client, QCLOUD_ERR_INVAL);
    HAL_Snprintf(message, sizeof(message),
                 "{\"method\":\"alert_fence_event_reply\", \"clientToken\":\"%s\", \"timestamp\":\"%ld\", "
                 "\"code\":\"%d\", \"status\":\"%s\"}",
                 client_token, HAL_Timer_current_sec(), reply_code, reply_msg);
    rc = qcloud_service_mqtt_post_msg(mqtt_client, message, QOS0);
    if (QCLOUD_RET_SUCCESS > rc) {
        Log_e("location fence aleart reply failed.");
    }
    return rc;
}

#ifdef __cplusplus
}
#endif
