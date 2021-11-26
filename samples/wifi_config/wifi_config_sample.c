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

#include <signal.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"
#include "utils_getopt.h"
#include "lite-utils.h"
#include "utils_timer.h"

#include "qcloud_wifi_config.h"
#include "qcloud_wifi_config_internal.h"

#if !(WIFI_PROV_SOFT_AP_ENABLE || WIFI_PROV_SMART_CONFIG_ENABLE || WIFI_PROV_AIRKISS_CONFIG_ENABLE || \
      WIFI_PROV_SIMPLE_CONFIG_ENABLE || WIFI_PROV_BT_COMBO_CONFIG_ENABLE)

#error "Please choose one set to 1 of them in qcloud_wifi_config.h"
#endif
static bool wifi_config_result_success = false;

#ifdef AUTH_MODE_CERT
static char sg_cert_file[PATH_MAX + 1 + 1024];  // full path of device cert file
static char sg_key_file[PATH_MAX + 1 + 1024];   // full path of device key file
#endif

typedef struct {
    bool     power_off;
    uint8_t  brightness;
    uint16_t color;
    char     device_name[MAX_SIZE_OF_DEVICE_NAME + 1];
} LedInfo;

static DeviceInfo sg_devInfo;
static LedInfo    sg_led_info;

// led attributes, corresponding to struct LedInfo
static char *sg_property_name[] = {"power_switch", "brightness", "color", "name"};

// user's log print callback
static bool log_handler(const char *message)
{
    // return true if print success
    return false;
}

// MQTT event callback
static void _mqtt_event_handler(void *pclient, void *handle_context, MQTTEventMsg *msg)
{
    MQTTMessage *mqtt_messge = (MQTTMessage *)msg->msg;
    uintptr_t    packet_id   = (uintptr_t)msg->msg;

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

        case MQTT_EVENT_PUBLISH_RECVEIVED:
            Log_i(
                "topic message arrived but without any related handle: topic=%.*s, "
                "topic_msg=%.*s",
                mqtt_messge->topic_len, mqtt_messge->ptopic, mqtt_messge->payload_len, mqtt_messge->payload);
            break;
        case MQTT_EVENT_SUBCRIBE_SUCCESS:
            Log_i("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_TIMEOUT:
            Log_i("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_NACK:
            Log_i("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_UNSUBCRIBE_SUCCESS:
            Log_i("unsubscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_UNSUBCRIBE_TIMEOUT:
            Log_i("unsubscribe timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_UNSUBCRIBE_NACK:
            Log_i("unsubscribe nack, packet-id=%u", (unsigned int)packet_id);
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

// Setup MQTT construct parameters
static int _setup_connect_init_params(MQTTInitParams *initParams)
{
    int ret;

    ret = HAL_GetDevInfo((void *)&sg_devInfo);
    if (QCLOUD_RET_SUCCESS != ret) {
        return ret;
    }

    initParams->region      = sg_devInfo.region;
    initParams->device_name = sg_devInfo.device_name;
    initParams->product_id  = sg_devInfo.product_id;

#ifdef AUTH_MODE_CERT
    char *certs_dir = "certs";
    char  current_path[PATH_MAX + 1];
    char *cwd = getcwd(current_path, sizeof(current_path));

    if (cwd == NULL) {
        Log_e("getcwd return NULL");
        return QCLOUD_ERR_FAILURE;
    }

#ifdef WIN32
    sprintf(sg_cert_file, "%s\\%s\\%s", current_path, certs_dir, sg_devInfo.dev_cert_file_name);
    sprintf(sg_key_file, "%s\\%s\\%s", current_path, certs_dir, sg_devInfo.dev_key_file_name);
#else
    sprintf(sg_cert_file, "%s/%s/%s", current_path, certs_dir, sg_devInfo.dev_cert_file_name);
    sprintf(sg_key_file, "%s/%s/%s", current_path, certs_dir, sg_devInfo.dev_key_file_name);
#endif

    initParams->cert_file = sg_cert_file;
    initParams->key_file  = sg_key_file;
#else
    initParams->device_secret = sg_devInfo.device_secret;
#endif

    initParams->command_timeout        = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    initParams->keep_alive_interval_ms = QCLOUD_IOT_MQTT_KEEP_ALIVE_INTERNAL;

    initParams->auto_connect_enable  = 1;
    initParams->event_handle.h_fp    = _mqtt_event_handler;
    initParams->event_handle.context = NULL;

    return QCLOUD_RET_SUCCESS;
}

static void property_topic_publish(void *pClient, const char *message, int message_len)
{
    char topic[256] = {0};
    int  size;

    size = HAL_Snprintf(topic, 256, "$thing/up/property/%s/%s", sg_devInfo.product_id, sg_devInfo.device_name);

    if (size < 0 || size > 256 - 1) {
        Log_e("buf size < topic length!");
        return;
    }

    PublishParams pubParams = DEFAULT_PUB_PARAMS;
    pubParams.qos           = QOS0;
    pubParams.payload_len   = message_len;
    pubParams.payload       = (void *)message;

    IOT_MQTT_Publish(pClient, topic, &pubParams);
}

static void property_get_status(void *pClient)
{
    char       message[256]        = {0};
    int        message_len         = 0;
    static int sg_get_status_index = 0;

    sg_get_status_index++;
    message_len = HAL_Snprintf(message, sizeof(message), "{\"method\":\"get_status\", \"clientToken\":\"%s-%d\"}",
                               sg_devInfo.product_id, sg_get_status_index);
    property_topic_publish(pClient, message, message_len);
}

static void property_report(void *pClient)
{
    char       message[256]    = {0};
    int        message_len     = 0;
    static int sg_report_index = 0;

    sg_report_index++;
    message_len = HAL_Snprintf(message, sizeof(message),
                               "{\"method\":\"report\", \"clientToken\":\"%s-%d\", "
                               "\"params\":{\"power_switch\":%d, \"color\":%d, \"brightness\":%d, \"name\":\"%s\"}}",
                               sg_devInfo.product_id, sg_report_index, sg_led_info.power_off, sg_led_info.color,
                               sg_led_info.brightness, sg_devInfo.device_name);
    // only change the brightness in the demo
    sg_led_info.brightness %= 100;
    sg_led_info.brightness++;
    property_topic_publish(pClient, message, message_len);
}

static void property_control_handle(void *pClient, const char *token, const char *control_data)
{
    char *params         = NULL;
    char *property_param = NULL;
    char  message[256]   = {0};
    int   message_len    = 0;

    params = LITE_json_value_of("params", (char *)control_data);
    if (NULL == params) {
        Log_e("Fail to parse params");
        return;
    }

    for (int i = 0; i < sizeof(sg_property_name) / sizeof(sg_property_name[0]); i++) {
        property_param = LITE_json_value_of(sg_property_name[i], params);
        if (NULL != property_param) {
            Log_i("\t%-16s = %-10s", sg_property_name[i], property_param);
            if (i == 1) {
                // only change the brightness in the demo
                sg_led_info.brightness = atoi(property_param);
            }
            HAL_Free(property_param);
        }
    }

    // method: control_reply
    message_len = HAL_Snprintf(message, sizeof(message),
                               "{\"method\":\"control_reply\", \"code\":0, \"clientToken\":\"%s\"}", token);
    property_topic_publish(pClient, message, message_len);

    HAL_Free(params);
}

static void property_get_status_reply_handle(const char *get_status_reply_data)
{
    char *data           = NULL;
    char *report         = NULL;
    char *property_param = NULL;

    data = LITE_json_value_of("data", (char *)get_status_reply_data);
    if (NULL == data) {
        Log_e("Fail to parse data");
        return;
    }
    report = LITE_json_value_of("reported", (char *)data);
    if (NULL == report) {
        Log_e("Fail to parse report");
        HAL_Free(data);
        return;
    }

    for (int i = 0; i < sizeof(sg_property_name) / sizeof(sg_property_name[0]); i++) {
        property_param = LITE_json_value_of(sg_property_name[i], report);
        if (NULL != property_param) {
            Log_i("\t%-16s = %-10s", sg_property_name[i], property_param);
            if (i == 1) {
                sg_led_info.brightness = atoi(property_param);
            }
            HAL_Free(property_param);
        }
    }

    HAL_Free(report);
    HAL_Free(data);
}

static void property_report_reply_handle(const char *report_reply_data)
{
    char *status = NULL;

    status = LITE_json_value_of("status", (char *)report_reply_data);
    if (NULL == status) {
        Log_e("Fail to parse data");
        return;
    }
    Log_i("report reply status: %s", status);
    HAL_Free(status);
}

// callback when MQTT msg arrives
static void property_message_callback(void *pClient, MQTTMessage *message, void *userData)
{
    char  property_data_buf[QCLOUD_IOT_MQTT_RX_BUF_LEN + 1] = {0};
    int   property_data_len                                 = 0;
    char *type_str                                          = NULL;
    char *token_str                                         = NULL;

    if (message == NULL) {
        return;
    }
    Log_i("Receive Message With topicName:%.*s, payload:%.*s", (int)message->topic_len, message->ptopic,
          (int)message->payload_len, (char *)message->payload);

    property_data_len =
        sizeof(property_data_buf) > message->payload_len ? message->payload_len : QCLOUD_IOT_MQTT_RX_BUF_LEN;
    memcpy(property_data_buf, message->payload, property_data_len);

    type_str = LITE_json_value_of("method", property_data_buf);
    if (NULL == type_str) {
        Log_e("Fail to parse method");
        return;
    }
    token_str = LITE_json_value_of("clientToken", property_data_buf);
    if (NULL == type_str) {
        Log_e("Fail to parse token");
        HAL_Free(type_str);
        return;
    }

    if (0 == strncmp(type_str, "control", sizeof("control") - 1)) {
        // method: control
        property_control_handle(pClient, token_str, property_data_buf);
    } else if (0 == strncmp(type_str, "get_status_reply", sizeof("get_status_reply") - 1)) {
        // method: get_status_reply
        property_get_status_reply_handle(property_data_buf);
    } else if (0 == strncmp(type_str, "report_reply", sizeof("report_reply") - 1)) {
        // method: report_reply
        property_report_reply_handle(property_data_buf);
    } else {
        // do nothing
    }

    HAL_Free(token_str);
    HAL_Free(type_str);
}

// subscribe MQTT topic and wait for sub result
static int subscribe_property_topic_wait_result(void *client)
{
    char topic_name[128] = {0};

    int size = HAL_Snprintf(topic_name, sizeof(topic_name), "$thing/down/property/%s/%s", sg_devInfo.product_id,
                            sg_devInfo.device_name);
    if (size < 0 || size > sizeof(topic_name) - 1) {
        Log_e("topic content length not enough! content size:%d  buf size:%d", size, (int)sizeof(topic_name));
        return QCLOUD_ERR_FAILURE;
    }

    SubscribeParams sub_params    = DEFAULT_SUB_PARAMS;
    sub_params.qos                = QOS0;
    sub_params.on_message_handler = property_message_callback;

    int rc = IOT_MQTT_Subscribe(client, topic_name, &sub_params);
    if (rc < 0) {
        Log_e("MQTT subscribe FAILED: %d", rc);
        return rc;
    }

    int wait_cnt = 10;
    while (!IOT_MQTT_IsSubReady(client, topic_name) && (wait_cnt > 0)) {
        // wait for subscription result
        rc = IOT_MQTT_Yield(client, 1000);
        if (rc) {
            Log_e("MQTT error: %d", rc);
            return rc;
        }
        wait_cnt--;
    }

    if (wait_cnt > 0) {
        return QCLOUD_RET_SUCCESS;
    } else {
        Log_e("wait for subscribe result timeout!");
        return QCLOUD_ERR_FAILURE;
    }
}

#ifdef LOG_UPLOAD
// init log upload module
static int _init_log_upload(MQTTInitParams *init_params)
{
    LogUploadInitParams log_init_params;
    memset(&log_init_params, 0, sizeof(LogUploadInitParams));

    log_init_params.region      = init_params->region;
    log_init_params.product_id  = init_params->product_id;
    log_init_params.device_name = init_params->device_name;
#ifdef AUTH_MODE_CERT
    log_init_params.sign_key = init_params->cert_file;
#else
    log_init_params.sign_key = init_params->device_secret;
#endif

#if defined(__linux__) || defined(WIN32)
    log_init_params.read_func     = HAL_Log_Read;
    log_init_params.save_func     = HAL_Log_Save;
    log_init_params.del_func      = HAL_Log_Del;
    log_init_params.get_size_func = HAL_Log_Get_Size;
#endif

    return IOT_Log_Init_Uploader(&log_init_params);
}

#endif

static bool sg_loop_test = false;
static int  parse_arguments(int argc, char **argv)
{
    int c;
    while ((c = utils_getopt(argc, argv, "c:l")) != EOF) switch (c) {
            case 'c':
                if (HAL_SetDevInfoFile(utils_optarg))
                    return -1;
                break;

            case 'l':
                sg_loop_test = true;
                break;

            default:
                HAL_Printf(
                    "usage: %s [options]\n"
                    "  [-c <config file for DeviceInfo>] \n"
                    "  [-l ] loop test or not\n",
                    argv[0]);
                return -1;
        }
    return 0;
}

static void _wifi_config_result_cb(eWiFiConfigResult event, void *usr_data)
{
    Log_d("entry...");
    qiot_wifi_config_stop();
    switch (event) {
        case RESULT_WIFI_CONFIG_SUCCESS:
            Log_i("WiFi is ready, to do Qcloud IoT demo");
            Log_d("timestamp now:%d", HAL_Timer_current_sec());
            wifi_config_result_success = true;
            break;

        case RESULT_WIFI_CONFIG_TIMEOUT:
            Log_e("wifi config timeout!");
            wifi_config_result_success = false;
        case RESULT_WIFI_CONFIG_FAILED:
            Log_e("wifi config failed!");
            qiot_wifi_config_send_log();
            wifi_config_result_success = false;
            break;

        default:
            break;
    }
}

static bool qcloud_wifi_config_proc()
{
    int   rc;
    Timer timer;
#if WIFI_PROV_SOFT_AP_ENABLE
    WiFiConfigParams apConf    = {"qcloud-softap", "12345678", 6};
    rc                         = qiot_wifi_config_start(WIFI_CONFIG_TYPE_SOFT_AP, &apConf, _wifi_config_result_cb);
    countdown(&timer, 500);
    while ((rc == QCLOUD_RET_SUCCESS) && (false == wifi_config_result_success) && !expired(&timer)) {
        Log_d("wait wifi config result...");
        HAL_SleepMs(1000);
    }
    qiot_wifi_config_stop();
    if (true == wifi_config_result_success) {
        return true;
    }
#endif  // WIFI_PROV_SOFT_AP_ENABLE

#if WIFI_PROV_SMART_CONFIG_ENABLE
    rc = qiot_wifi_config_start(WIFI_CONFIG_TYPE_SMART_CONFIG, NULL, _wifi_config_result_cb);
    countdown(&timer, 500);
    while ((rc == QCLOUD_RET_SUCCESS) && (false == wifi_config_result_success) && !expired(&timer)) {
        Log_d("wait wifi config result...");
        HAL_SleepMs(1000);
    }
    qiot_wifi_config_stop();
    if (true == wifi_config_result_success) {
        return true;
    }
#endif  // WIFI_PROV_SMART_CONFIG_ENABLE

#if WIFI_PROV_AIRKISS_CONFIG_ENABLE
    rc = qiot_wifi_config_start(WIFI_CONFIG_TYPE_AIRKISS, NULL, _wifi_config_result_cb);
    countdown(&timer, 500);
    while ((rc == QCLOUD_RET_SUCCESS) && (false == wifi_config_result_success) && !expired(&timer)) {
        Log_d("wait wifi config result...");
        HAL_SleepMs(1000);
    }
    qiot_wifi_config_stop();
    if (true == wifi_config_result_success) {
        return true;
    }
#endif  // WIFI_PROV_AIRKISS_CONFIG_ENABLE

#if WIFI_PROV_SIMPLE_CONFIG_ENABLE
    rc = qiot_wifi_config_start(WIFI_CONFIG_TYPE_SIMPLE_CONFIG, NULL, _wifi_config_result_cb);
    countdown(&timer, 500);
    while ((rc == QCLOUD_RET_SUCCESS) && (false == wifi_config_result_success) && !expired(&timer)) {
        Log_d("wait wifi config result...");
        HAL_SleepMs(1000);
    }
    qiot_wifi_config_stop();
    if (true == wifi_config_result_success) {
        return true;
    }
#endif  // WIFI_PROV_SIMPLE_CONFIG_ENABLE

#if WIFI_PROV_BT_COMBO_CONFIG_ENABLE
    rc = qiot_wifi_config_start(WIFI_CONFIG_TYPE_BT_COMBO, NULL, _wifi_config_result_cb);
    countdown(&timer, 500);
    while ((rc == QCLOUD_RET_SUCCESS) && (false == wifi_config_result_success) && !expired(&timer)) {
        Log_d("wait wifi config result...");
        HAL_SleepMs(1000);
    }
    qiot_wifi_config_stop();
    if (true == wifi_config_result_success) {
        return true;
    }
#endif  // WIFI_PROV_BT_COMBO_CONFIG_ENABLE
    return false;
}

int main(int argc, char **argv)
{
    int rc;

    // init log level
    IOT_Log_Set_Level(eLOG_DEBUG);
    IOT_Log_Set_MessageHandler(log_handler);

    // parse arguments for device info file and loop test;
    rc = parse_arguments(argc, argv);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("parse arguments error, rc = %d", rc);
        return rc;
    }

    if (false == qcloud_wifi_config_proc()) {
        Log_d("wifi config failed");
    }

    // init connection
    MQTTInitParams init_params = DEFAULT_MQTTINIT_PARAMS;
    rc                         = _setup_connect_init_params(&init_params);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("init params error, rc = %d", rc);
        return rc;
    }

#ifdef LOG_UPLOAD
    // _init_log_upload should be done after _setup_connect_init_params and before IOT_MQTT_Construct
    rc = _init_log_upload(&init_params);
    if (rc != QCLOUD_RET_SUCCESS)
        Log_e("init log upload error, rc = %d", rc);
#endif

    // create MQTT client and connect with server
    void *client = IOT_MQTT_Construct(&init_params);
    if (client != NULL) {
        Log_i("Cloud Device Construct Success");
    } else {
        rc = IOT_MQTT_GetErrCode();
        Log_e("MQTT Construct failed, rc = %d", rc);
        return QCLOUD_ERR_FAILURE;
    }

#ifdef SYSTEM_COMM
    long time = 0;
    // get system timestamp from server
    rc = IOT_Get_Sys_Resource(client, eRESOURCE_TIME, &sg_devInfo, &time);
    if (QCLOUD_RET_SUCCESS == rc) {
        Log_i("system time is %ld", time);
    } else {
        Log_e("get system time failed!");
    }
#endif

    // subscribe normal topics here
    rc = subscribe_property_topic_wait_result(client);
    if (rc < 0) {
        Log_e("Client Subscribe Topic Failed: %d", rc);
        return rc;
    }

    // method: get_status
    property_get_status(client);
    do {
        rc = IOT_MQTT_Yield(client, 500);
        if (rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT) {
            HAL_SleepMs(1000);
            continue;
        } else if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
            if (rc == QCLOUD_ERR_MQTT_RECONNECT_TIMEOUT) {
                Log_e(
                    "exit. mqtt reconnect timeout! Please check the network connection, or try to increase "
                    "MAX_RECONNECT_WAIT_INTERVAL(%d)",
                    MAX_RECONNECT_WAIT_INTERVAL);
            } else {
                Log_e("exit with error: %d", rc);
            }
            break;
        }

        if (sg_loop_test)
            HAL_SleepMs(1000);

        // method: report
        property_report(client);
    } while (sg_loop_test);

    rc = IOT_MQTT_Destroy(&client);
    IOT_Log_Upload(true);

#ifdef LOG_UPLOAD
    IOT_Log_Fini_Uploader();
#endif
    return rc;
}
