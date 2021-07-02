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

#include <arpa/inet.h>
#include <errno.h>
#include <limits.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "lite-utils.h"
#include "qcloud_iot_export.h"
#include "utils_getopt.h"

#define MAX_SIZE_OF_TOPIC (128)
#define MAX_SIZE_OF_DATA  (128)

#define LOCAL_SERVER_PORT (20000)

#define MAX_SUB_CLIENTS (32)

static int               sg_sub_packet_id = -1;
static int               sg_loop_count    = 1;
static GatewayDeviceInfo sg_GWdevInfo;

#ifdef AUTH_MODE_CERT
static char sg_cert_file[PATH_MAX + 1];  // full path of device cert file
static char sg_key_file[PATH_MAX + 1];   // full path of device key file
#endif

#define TOPIC_PROP_UP_FMT   "$thing/up/property/%s/%s"
#define TOPIC_PROP_DOWN_FMT "$thing/down/property/%s/%s"

enum {
    SUBDEV_STATUS_INIT,
    SUBDEV_STATUS_ONLINE,
    SUBDEV_STATUS_OFFLINE,
};

struct subdev_local_info {
    int  fd;
    char pid[MAX_SIZE_OF_PRODUCT_ID + 1];
    char dname[MAX_SIZE_OF_DEVICE_NAME + 1];
    int  status;
};
static struct subdev_local_info sg_clients_info[MAX_SUB_CLIENTS];

void _event_handler(void *client, void *context, MQTTEventMsg *msg)
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
            sg_sub_packet_id = packet_id;
            break;

        case MQTT_EVENT_SUBCRIBE_TIMEOUT:
            Log_i("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            sg_sub_packet_id = packet_id;
            break;

        case MQTT_EVENT_SUBCRIBE_NACK:
            Log_i("subscribe nack, packet-id=%u", (unsigned int)packet_id);
            sg_sub_packet_id = packet_id;
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
        case MQTT_EVENT_GATEWAY_SEARCH:
            Log_d("gateway search subdev status:%d", *(int32_t *)(msg->msg));
            break;

        case MQTT_EVENT_GATEWAY_CHANGE: {
            gw_change_notify_t *p_notify = (gw_change_notify_t *)(msg->msg);
            Log_d("Get change notice: sub devices %s to %s", p_notify->devices, p_notify->status ? "bind" : "unbind");
        } break;

        default:
            Log_i("Should NOT arrive here.");
            break;
    }
}

static void _message_handler(void *client, MQTTMessage *message, void *user_data)
{
    struct subdev_local_info *subdev = (struct subdev_local_info *)user_data;
    if (!message || !subdev || subdev->fd <= 0 || message->topic_len > 127) {
        return;
    }

    Log_i("Receive Message With topicName:%.*s, payload:%.*s", (int)message->topic_len, message->ptopic,
          (int)message->payload_len, (char *)message->payload);
    char tmp_topic[128];
    HAL_Snprintf(tmp_topic, 127, TOPIC_PROP_DOWN_FMT, subdev->pid, subdev->dname);
    if (strncmp(tmp_topic, message->ptopic, message->topic_len)) {
        Log_e("Not my topic");
        return;
    }
    char *msg = (char *)HAL_Malloc(message->payload_len + 32);
    memcpy(msg, message->payload, message->payload_len);
    msg[message->payload_len] = '\0';

    if (send(subdev->fd, msg, message->payload_len + 1, 0) < 0) {
        Log_e("Failed to send subdev msg %s", msg);
    }
    HAL_Free(msg);
}

static int _setup_connect_init_params(GatewayInitParam *init_params)
{
    int rc;

    rc = HAL_GetGwDevInfo((void *)&sg_GWdevInfo);
    if (QCLOUD_RET_SUCCESS != rc) {
        Log_e("Get gateway dev info err,rc:%d", rc);
        return rc;
    }

    init_params->init_param.region      = sg_GWdevInfo.gw_info.region;
    init_params->init_param.product_id  = sg_GWdevInfo.gw_info.product_id;
    init_params->init_param.device_name = sg_GWdevInfo.gw_info.device_name;

#ifdef AUTH_MODE_CERT
    char  certs_dir[PATH_MAX + 1] = "certs";
    char  current_path[PATH_MAX + 1];
    char *cwd = getcwd(current_path, sizeof(current_path));
    if (cwd == NULL) {
        Log_e("getcwd return NULL");
        return QCLOUD_ERR_FAILURE;
    }
    sprintf(sg_cert_file, "%s/%s/%s", current_path, certs_dir, sg_GWdevInfo.gw_info.dev_cert_file_name);
    sprintf(sg_key_file, "%s/%s/%s", current_path, certs_dir, sg_GWdevInfo.gw_info.dev_key_file_name);

    init_params->init_param.cert_file = sg_cert_file;
    init_params->init_param.key_file  = sg_key_file;
#else
    init_params->init_param.device_secret = sg_GWdevInfo.gw_info.device_secret;
#endif

    init_params->init_param.command_timeout        = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    init_params->init_param.auto_connect_enable    = 1;
    init_params->init_param.event_handle.h_fp      = _event_handler;
    init_params->init_param.keep_alive_interval_ms = QCLOUD_IOT_MQTT_KEEP_ALIVE_INTERNAL;

    return QCLOUD_RET_SUCCESS;
}

// for reply code, pls check https://cloud.tencent.com/document/product/634/45960
#define GATEWAY_RC_REPEAT_BIND 809

static int parse_arguments(int argc, char **argv)
{
    int c;
    while ((c = utils_getopt(argc, argv, "c:l:")) != EOF) switch (c) {
            case 'c':
                if (HAL_SetDevInfoFile(utils_optarg))
                    return -1;
                break;

            case 'l':
                sg_loop_count = atoi(utils_optarg);
                if (sg_loop_count > 10000)
                    sg_loop_count = 10000;
                else if (sg_loop_count < 0)
                    sg_loop_count = 1;
                break;

            default:
                HAL_Printf(
                    "usage: %s [options]\n"
                    "  [-c <config file for DeviceInfo>] \n"
                    "  [-l <loop count>] \n",
                    argv[0]);
                return -1;
        }
    return 0;
}

static void _property_topic_publish(void *pClient, const char *pid, const char *dname, const char *message,
                                    int message_len)
{
    char topic[256] = {0};
    int  size;
    Log_d("pid %s dname %s", pid, dname);

    size = HAL_Snprintf(topic, 256, "$thing/up/property/%s/%s", pid, dname);
    if (size < 0 || size > 256 - 1) {
        Log_e("buf size < topic length!");
        return;
    }

    PublishParams pubParams = DEFAULT_PUB_PARAMS;
    pubParams.qos           = QOS0;
    pubParams.payload_len   = message_len;
    pubParams.payload       = (void *)message;

    Log_d("topic %s", topic);
    Log_d("publish msg %s", message);

    IOT_MQTT_Publish(IOT_Gateway_Get_Mqtt_Client(pClient), topic, &pubParams);
}

static int _setup_gw_server()
{
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port        = htons(LOCAL_SERVER_PORT);
    addr.sin_family      = AF_INET;
    int fd               = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        goto exit;
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        goto exit;
    }
    if (listen(fd, 5) < 0) {
        goto exit;
    }

    return fd;
exit:
    if (fd > 0)
        close(fd);
    return fd;
}

// subscribe subdev MQTT topic and wait for sub result
static int _subscribe_subdev_topic_wait_result(void *client, char *topic_name, QoS qos, GatewayParam *gw_info,
                                               struct subdev_local_info *subdev)
{
    SubscribeParams sub_params    = DEFAULT_SUB_PARAMS;
    sub_params.qos                = qos;
    sub_params.on_message_handler = _message_handler;
    sub_params.user_data          = subdev;

    int rc = IOT_Gateway_Subscribe(client, topic_name, &sub_params);
    if (rc < 0) {
        Log_e("IOT_Gateway_Subscribe failed.");
        return rc;
    }

    int wait_cnt = 10;
    while (!IOT_Gateway_IsSubReady(client, topic_name) && (wait_cnt-- > 0)) {
        // wait for subscription result
        rc = IOT_Gateway_Yield(client, 1000);
        if (rc) {
            Log_e("MQTT error: %d", rc);
            return rc;
        }
    }

    if (wait_cnt > 0) {
        return QCLOUD_RET_SUCCESS;
    }
    Log_e("wait for subscribe result timeout!");
    return QCLOUD_ERR_FAILURE;
}

static int _get_idle_slot()
{
    int i;
    for (i = 0; i < MAX_SUB_CLIENTS; ++i) {
        if (sg_clients_info[i].fd <= 0)
            return i;
    }

    return -1;
}

static int _accept_new_subdev(int gw_fd)
{
    struct sockaddr_in cli_addr;
    socklen_t          sz;
    int                fd = accept(gw_fd, (struct sockaddr *)&cli_addr, &sz);
    if (fd < 0)
        return -1;
    Log_d("Recv conn from %s %d", inet_ntoa(cli_addr.sin_addr), ntohs(cli_addr.sin_port));

    return fd;
}

enum {
    LOCAL_MSG_TYPE_ONLINE,
    LOCAL_MSG_TYPE_OFFLINE,
    LOCAL_MSG_TYPE_REPORT,

    LOCAL_MSG_TYPE_SZ,
    LOCAL_MSG_TYPE_UNKNOWN = -1,
};

static int _get_type_index(const char *type)
{
    if (!strcmp(type, "online"))
        return LOCAL_MSG_TYPE_ONLINE;
    if (!strcmp(type, "offline"))
        return LOCAL_MSG_TYPE_OFFLINE;
    if (!strcmp(type, "report"))
        return LOCAL_MSG_TYPE_REPORT;

    return LOCAL_MSG_TYPE_UNKNOWN;
}

static int _subdev_leave(void *gw_client, GatewayParam *param)
{
    char sub_topic[128];
    HAL_Snprintf(sub_topic, 127, TOPIC_PROP_DOWN_FMT, param->subdev_product_id, param->subdev_device_name);
    if (QCLOUD_RET_SUCCESS != IOT_Gateway_Unsubscribe(gw_client, sub_topic)) {
        Log_e("Failed to unsubscribe %s", sub_topic);
    }
    if (QCLOUD_RET_SUCCESS != IOT_Gateway_Subdev_Offline(gw_client, param)) {
        Log_e("Failed to set %s %d to offline", param->subdev_product_id, param->subdev_device_name);
        return -1;
    }
    return 0;
}

static int _deal_with_subdev_msg(char *msg, void *gw_client, GatewayParam *param, int idx)
{
    int ret = 0;
    Log_d("Get msg %s from subdev", msg);
    char *sub_pid = NULL, *sub_dname = NULL;
    char *type = LITE_json_value_of("subdev_type", msg);
    if (!type) {
        Log_e("Failed to parse type");
        if (sg_clients_info[idx].status == SUBDEV_STATUS_ONLINE) {
            _property_topic_publish(gw_client, sg_clients_info[idx].pid, sg_clients_info[idx].dname, msg, strlen(msg));
        }
        goto exit;
    }
    int itype = _get_type_index(type);
    if (itype == LOCAL_MSG_TYPE_UNKNOWN)
        goto exit;
    sub_pid   = LITE_json_value_of("product_id", msg);
    sub_dname = LITE_json_value_of("device_name", msg);
    if (!sub_pid || !sub_dname)
        goto exit;
    param->subdev_product_id  = sub_pid;
    param->subdev_device_name = sub_dname;
    char sub_topic[128];
    Log_d("itype is %d, pid %s dname %s", itype, sub_pid, sub_dname);
    switch (itype) {
        case LOCAL_MSG_TYPE_OFFLINE:
            sg_clients_info[idx].status = SUBDEV_STATUS_OFFLINE;
            ret                         = _subdev_leave(gw_client, param);
            break;
        case LOCAL_MSG_TYPE_ONLINE:
            ret = IOT_Gateway_Subdev_Online(gw_client, param);
            if (QCLOUD_RET_SUCCESS != ret) {
                Log_e("Failed to set %s %d to %d", param->subdev_product_id, param->subdev_device_name, itype);
                break;
            }
            strncpy(sg_clients_info[idx].pid, sub_pid, MAX_SIZE_OF_PRODUCT_ID + 1);
            strncpy(sg_clients_info[idx].dname, sub_dname, MAX_SIZE_OF_DEVICE_NAME + 1);
            HAL_Snprintf(sub_topic, 127, TOPIC_PROP_DOWN_FMT, sub_pid, sub_dname);
            if (_subscribe_subdev_topic_wait_result(gw_client, sub_topic, 1, param, &sg_clients_info[idx])) {
                Log_e("Failed to subscribe subdev topic");
            } else {
                sg_clients_info[idx].status = SUBDEV_STATUS_ONLINE;
                Log_d("sub subdev OK");
            }
            break;
        case LOCAL_MSG_TYPE_REPORT: {
            Log_d("msg %s", msg);
            char *client_token = LITE_json_value_of("msg.clientToken", msg);
            char *timestamp    = LITE_json_value_of("msg.timestamp", msg);
            char *pwr          = LITE_json_value_of("msg.power_switch", msg);
            char *color        = LITE_json_value_of("msg.color", msg);
            char *brightness   = LITE_json_value_of("msg.brightness", msg);
#define REPORT_SUBDEV_FMT                                                                                          \
    "{\"method\":\"report\",\"clientToken\":\"%s\",\"timestamp\":%s,\"params\":{\"power_switch\":%s,\"color\":%s," \
    "\"brightness\":%s}}"
            char report_msg[512];
            int  _report_len =
                HAL_Snprintf(report_msg, 512, REPORT_SUBDEV_FMT, client_token, timestamp, pwr, color, brightness);
            if (_report_len > 0 && _report_len < 512)
                _property_topic_publish(gw_client, sub_pid, sub_dname, report_msg, _report_len);
            HAL_Free(client_token);
            HAL_Free(timestamp);
            HAL_Free(pwr);
            HAL_Free(color);
            HAL_Free(brightness);
        } break;
        default:
            break;
    }
exit:
    HAL_Free(type);
    HAL_Free(sub_pid);
    HAL_Free(sub_dname);

    return ret;
}

static int _select_local(int max_fd, fd_set *rd_set, int gw_fd)
{
    int            i;
    struct timeval tv;
    FD_ZERO(rd_set);
    FD_SET(gw_fd, rd_set);
    for (i = 0; i < MAX_SUB_CLIENTS; ++i) {
        if (sg_clients_info[i].fd > 0) {
            FD_SET(sg_clients_info[i].fd, rd_set);
        }
    }
    tv.tv_sec  = 2;
    tv.tv_usec = 0;

    return select(max_fd + 1, rd_set, NULL, NULL, &tv);
}

/*Gateway should enable multithread*/
int main(int argc, char **argv)
{
    int                rc = QCLOUD_ERR_FAILURE;
    int                i;
    void *             client = NULL;
    GatewayDeviceInfo *gw     = &sg_GWdevInfo;
    GatewayParam       param  = DEFAULT_GATEWAY_PARAMS;

    IOT_Log_Set_Level(eLOG_DEBUG);
    // parse arguments for device info file and loop test;
    rc = parse_arguments(argc, argv);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("parse arguments error, rc = %d", rc);
        return rc;
    }

    GatewayInitParam init_params = DEFAULT_GATEWAY_INIT_PARAMS;
    rc                           = _setup_connect_init_params(&init_params);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("init params err,rc=%d", rc);
        return rc;
    }

    client = IOT_Gateway_Construct(&init_params);
    if (client == NULL) {
        Log_e("client constructed failed.");
        return QCLOUD_ERR_FAILURE;
    }
    param.product_id  = gw->gw_info.product_id;
    param.device_name = gw->gw_info.device_name;

    int gw_fd = _setup_gw_server();
    if (gw_fd < 0)
        goto exit;
    int    max_fd = gw_fd;
    fd_set rd_set;
    memset(sg_clients_info, 0, sizeof(sg_clients_info));
    while (1) {
        rc = _select_local(max_fd, &rd_set, gw_fd);
        if (rc < 0) {
            Log_e("select return %s", strerror(errno));
            goto exit;
        }
        // nothing happened
        if (rc == 0) {
            IOT_Gateway_Yield(client, 1000);
            continue;
        }
        int _done = 0;

        if (FD_ISSET(gw_fd, &rd_set)) {
            int slot = _get_idle_slot();
            if (slot < 0) {
                Log_e("No space for new sub device");
            } else {
                sg_clients_info[slot].fd = _accept_new_subdev(gw_fd);
                max_fd                   = max_fd > sg_clients_info[slot].fd ? max_fd : sg_clients_info[slot].fd;
            }
            _done++;
        }
        for (i = 0; i < MAX_SUB_CLIENTS; ++i) {
            if (_done >= rc)  // all was done
                break;
            if (sg_clients_info[i].fd <= 0 || !FD_ISSET(sg_clients_info[i].fd, &rd_set))
                continue;
            static char sg_subdev_msg[512];
            int         rd_len       = read(sg_clients_info[i].fd, sg_subdev_msg, sizeof(sg_subdev_msg) - 1);
            param.subdev_product_id  = sg_clients_info[i].pid;
            param.subdev_device_name = sg_clients_info[i].dname;
            if (rd_len > 0) {
                sg_subdev_msg[rd_len] = '\0';
                _deal_with_subdev_msg(sg_subdev_msg, client, &param, i);
            } else if (rd_len == 0) {
                Log_d("socket of %s:%s closed", sg_clients_info[i].pid, sg_clients_info[i].dname);
                close(sg_clients_info[i].fd);
                sg_clients_info[i].fd = -1;
                _subdev_leave(client, &param);
            }
            _done++;
        }
        IOT_Gateway_Yield(client, 1000);
    }

exit:
    if (gw_fd > 0)
        close(gw_fd);
    for (i = 0; i < MAX_SUB_CLIENTS; ++i) {
        if (sg_clients_info[i].fd > 0)
            close(sg_clients_info[i].fd);
    }
    return IOT_Gateway_Destroy(client);
}