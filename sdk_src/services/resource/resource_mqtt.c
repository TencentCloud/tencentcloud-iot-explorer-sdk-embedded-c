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

#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"

#include "mqtt_client.h"
#include "resource_client.h"
#include "qcloud_iot_export_resource.h"

#include <string.h>

/* signal channel */
typedef struct {
    void *      mqtt;  // MQTT client
    const char *product_id;
    const char *device_name;

    char  topic_resource[MAX_SIZE_OF_CLOUD_TOPIC];  // Resource manage MQTT Topic
    bool  resource_topic_ready;
    void *context;

    OnResourceMessageCallback  msg_callback;
    OnResourceEventUsrCallback event_callback;
} RESOURCE_MQTT_Struct_t;

static void _resource_mqtt_sub_event_handler(void *pClient, MQTTEventType event_type, void *pUserData)
{
    RESOURCE_MQTT_Struct_t *handle = (RESOURCE_MQTT_Struct_t *)pUserData;

    switch (event_type) {
        case MQTT_EVENT_SUBCRIBE_SUCCESS:
            Log_d("resource topic subscribe success");
            handle->resource_topic_ready = true;
            break;

        case MQTT_EVENT_SUBCRIBE_TIMEOUT:
            Log_i("resource topic subscribe timeout");
            handle->resource_topic_ready = false;
            break;

        case MQTT_EVENT_SUBCRIBE_NACK:
            Log_i("resource topic subscribe NACK");
            handle->resource_topic_ready = false;
            break;
        case MQTT_EVENT_UNSUBSCRIBE:
            Log_i("resource topic has been unsubscribed");
            handle->resource_topic_ready = false;
            break;
        case MQTT_EVENT_CLIENT_DESTROY:
            Log_i("mqtt client has been destroyed");
            handle->resource_topic_ready = false;
            break;

        default:
            return;
    }
}

/* callback after resource topic is subscribed */
/* Parse resource info (version/URL/file size/MD5/method) from JSON text */
static void _resource_mqtt_upgrade_cb(void *pClient, MQTTMessage *message, void *pContext)
{
    RESOURCE_MQTT_Struct_t *handle = (RESOURCE_MQTT_Struct_t *)pContext;

    Log_d("topic=%.*s", message->topic_len, message->ptopic);
    Log_d("len=%u, topic_msg=%.*s", message->payload_len, message->payload_len, (char *)message->payload);

    if (handle->msg_callback) {
        handle->msg_callback(handle->context, message->payload, message->payload_len);
    }
}

static int _resource_mqtt_subscribe(RESOURCE_MQTT_Struct_t *handle, const char *productId, const char *deviceName,
                                    OnResourceMessageCallback MsgCb)
{
    int ret;

    /* subscribe the OTA topic: "$ota/update/$(product_id)/$(device_name)" */
    memset(handle->topic_resource, '\0', MAX_SIZE_OF_CLOUD_TOPIC);
    ret = HAL_Snprintf(handle->topic_resource, MAX_SIZE_OF_CLOUD_TOPIC, "$thing/down/service/%s/%s", handle->product_id,
                       handle->device_name);
    if (ret < 0) {
        Log_e("generate topic name of resource failed");
        return ret;
    }

    handle->msg_callback            = MsgCb;
    SubscribeParams sub_params      = DEFAULT_SUB_PARAMS;
    sub_params.on_message_handler   = _resource_mqtt_upgrade_cb;
    sub_params.on_sub_event_handler = _resource_mqtt_sub_event_handler;
    sub_params.qos                  = QOS1;
    sub_params.user_data            = handle;

    ret = IOT_MQTT_Subscribe(handle->mqtt, handle->topic_resource, &sub_params);
    if (ret < 0) {
        Log_e("resource mqtt subscribe failed!");
    }

    return ret;
}

static int _resource_mqtt_publish(RESOURCE_MQTT_Struct_t *handle, int qos, const char *msg)
{
    int  ret;
    char topic_name[MAX_SIZE_OF_CLOUD_TOPIC] = {0};

    PublishParams pub_params = DEFAULT_PUB_PARAMS;

    if (0 == qos) {
        pub_params.qos = QOS0;
    } else {
        pub_params.qos = QOS1;
    }
    pub_params.payload     = (void *)msg;
    pub_params.payload_len = strlen(msg);

    ret = HAL_Snprintf(topic_name, MAX_SIZE_OF_CLOUD_TOPIC, "$thing/up/service/%s/%s", handle->product_id,
                       handle->device_name);
    if (ret < 0) {
        Log_e("generate topic name of info failed");
        return QCLOUD_ERR_FAILURE;
    }

    ret = IOT_MQTT_Publish(handle->mqtt, topic_name, &pub_params);
    if (ret < 0) {
        Log_e("publish to topic: %s failed", topic_name);
    }

    return ret;
}

void *qcloud_resource_mqtt_init(const char *productId, const char *deviceName, void *channel, void *context,
                                OnResourceMessageCallback ResourceMsgCb, OnResourceEventUsrCallback usrCb)
{
    int ret;
    int wait_cnt;

    RESOURCE_MQTT_Struct_t *handle = NULL;

    if (NULL == (handle = HAL_Malloc(sizeof(RESOURCE_MQTT_Struct_t)))) {
        Log_e("allocate for h_osc failed");
        goto exit;
    }
    memset(handle, 0, sizeof(RESOURCE_MQTT_Struct_t));

    handle->mqtt                 = channel;
    handle->product_id           = productId;
    handle->device_name          = deviceName;
    handle->event_callback       = usrCb;
    handle->context              = context;
    handle->resource_topic_ready = false;

    ret = _resource_mqtt_subscribe(handle, productId, deviceName, ResourceMsgCb);
    if (ret < 0) {
        Log_e("ota mqtt subscribe failed!");
        goto exit;
    }

    wait_cnt = 10;
    while (!handle->resource_topic_ready && (wait_cnt > 0)) {
        // wait for subscription result
        ret = qcloud_iot_mqtt_yield(channel, 2000);
        wait_cnt--;
    }

    if (wait_cnt == 0) {
        Log_e("ota mqtt subscribe timeout!");
        goto exit;
    }

    return handle;

exit:
    HAL_Free(handle);
    return NULL;
}

int qcloud_resource_mqtt_deinit(void *handle)
{
    if (handle) {
        RESOURCE_MQTT_Struct_t *pHandle = (RESOURCE_MQTT_Struct_t *)handle;
        IOT_MQTT_Unsubscribe(pHandle->mqtt, pHandle->topic_resource);
        HAL_Free(pHandle);
    }
    return QCLOUD_RET_SUCCESS;
}

int qcloud_resource_mqtt_report(void *handle, const char *msg, eResourceReportType type)
{
    switch (type) {
        case eRESOURCE_VERSION:
        case eRESOURCE_UPGRADE_RESULT:
        case eRESOURCE_POST_REQ:
            return _resource_mqtt_publish(handle, QOS1, msg);
        default:
            return _resource_mqtt_publish(handle, QOS0, msg);
    }
}

int qcloud_resource_mqtt_report_version_resp(void *handle, const char *msg, uint32_t msg_len)
{
    RESOURCE_MQTT_Struct_t *pHandle = (RESOURCE_MQTT_Struct_t *)handle;

    int ret = QCLOUD_RET_SUCCESS;

    if (pHandle->event_callback) {
        ret = pHandle->event_callback(pHandle->context, msg, msg_len, IOT_RES_EVENT_REPORT_VERSION_RESP);
    }

    return ret;
}

int qcloud_resource_mqtt_del_resource(void *handle, const char *file_name, const char *version)
{
    RESOURCE_MQTT_Struct_t *pHandle = (RESOURCE_MQTT_Struct_t *)handle;

    int ret = QCLOUD_RET_SUCCESS;

    if (pHandle->event_callback) {
        ret = pHandle->event_callback(pHandle->context, file_name, strlen(file_name), IOT_RES_EVENT_DEL_RESOURCE);
    }

    return ret;
}

int qcloud_resource_mqtt_resource_post_result(void *handle, const char *result_msg, uint32_t msg_len)
{
    RESOURCE_MQTT_Struct_t *pHandle = (RESOURCE_MQTT_Struct_t *)handle;

    int ret = QCLOUD_RET_SUCCESS;

    if (pHandle->event_callback) {
        ret = pHandle->event_callback(pHandle->context, result_msg, msg_len, IOT_RES_EVENT_REQUEST_URL_RESP);
    }

    return ret;
}

#ifdef __cplusplus
}
#endif
