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

#include <limits.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "qcloud_iot_export.h"
#include "utils_getopt.h"
#include "lite-utils.h"
#include "json_parser.h"

#define MAX_SIZE_OF_TOPIC (128)
#define MAX_SIZE_OF_DATA  (128)

static int               sg_sub_packet_id = -1;
static int               sg_loop_count    = 1;
static GatewayDeviceInfo sg_GWdevInfo;

#ifdef AUTH_MODE_CERT
static char sg_cert_file[PATH_MAX + 1];  // full path of device cert file
static char sg_key_file[PATH_MAX + 1];   // full path of device key file
#endif

static bool sg_unbind_all_subdev = false;

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

        case MQTT_EVENT_GATEWAY_UNBIND_ALL:
            sg_unbind_all_subdev = true;
            Log_d("gateway all subdev have been unbind");
            break;

        default:
            Log_i("Should NOT arrive here.");
            break;
    }
}

static void _message_handler(void *client, MQTTMessage *message, void *user_data)
{
    if (message == NULL) {
        return;
    }

    Log_i("Receive Message With topicName:%.*s, payload:%.*s", (int)message->topic_len, message->ptopic,
          (int)message->payload_len, (char *)message->payload);
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

/**
 * show gateway dynamic bind/unbind sub-devices
 */
#ifdef GATEWAY_DYN_BIND_SUBDEV_ENABLED
static int add_new_binded_sub_dev(GatewayDeviceInfo *pGateway, DeviceInfo *pNewSubDev)
{
    int ret;
    if (pGateway->sub_dev_num < MAX_NUM_SUB_DEV) {
        memcpy((char *)&pGateway->sub_dev_info[pGateway->sub_dev_num], (char *)pNewSubDev, sizeof(DeviceInfo));
        pGateway->sub_dev_num++;
        ret = QCLOUD_RET_SUCCESS;  // you can save gateway info to local flash for persistent storage
    } else {
        ret = QCLOUD_ERR_FAILURE;
    }

    return ret;
}

static int show_subdev_bind_unbind(void *client, GatewayParam *param)
{
    int rc;

    // ajust for your bind device info
    DeviceInfo subDev;
    memset((char *)&subDev, 0, sizeof(DeviceInfo));
    strncpy(subDev.product_id, "BIND_PID", MAX_SIZE_OF_PRODUCT_ID);
    strncpy(subDev.device_name, "BIND_DEV_NAME", MAX_SIZE_OF_DEVICE_NAME);
#ifdef AUTH_MODE_CERT
    strncpy(subDev.dev_cert_file_name, "BIND_CERT_FILE_NAME", MAX_SIZE_OF_DEVICE_CERT_FILE_NAME);
    strncpy(subDev.dev_key_file_name, "BIND_KEY_FILE_NAME", MAX_SIZE_OF_DEVICE_SECRET_FILE_NAME);

#else
    strncpy(subDev.device_secret, "BIND_DEV_PSK", MAX_SIZE_OF_DEVICE_SECRET);
#endif
    Log_d("bind subdev %s/%s", subDev.product_id, subDev.device_name);

    // bind sub dev
    rc = IOT_Gateway_Subdev_Bind(client, param, &subDev);
    if (QCLOUD_ERR_BIND_REPEATED_REQ == rc) {
        Log_d("%s/%s has been binded", subDev.product_id, subDev.device_name);
        rc = IOT_Gateway_Subdev_Unbind(client, param, &subDev);
        if (QCLOUD_RET_SUCCESS != rc) {
            Log_e("unbind %s/%s fail,rc:%d", subDev.product_id, subDev.device_name, rc);
        } else {
            Log_d("unbind %s/%s success", subDev.product_id, subDev.device_name);
            rc = IOT_Gateway_Subdev_Bind(client, param, &subDev);
        }
    }

    if (QCLOUD_RET_SUCCESS == rc) {
        Log_d("bind %s/%s success", subDev.product_id, subDev.device_name);
        add_new_binded_sub_dev(&sg_GWdevInfo, &subDev);
    } else {
        Log_e("bind %s/%s fail,rc:%d", subDev.product_id, subDev.device_name, rc);
    }

    return rc;
}
#endif

static void _property_topic_publish(void *pClient, const char *pid, const char *dname, const char *message,
                                    int message_len)
{
    char topic[256] = {0};
    int  size;

    size = HAL_Snprintf(topic, 256, "$thing/up/property/%s/%s", pid, dname);

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

#ifdef GATEWAY_AUTOMATION_ENABLED

#define MAX_SIZE_OF_ALIAS_NAME     MAX_SIZE_OF_DEVICE_NAME
#define MAX_SIZE_OF_ICON_URL       (128)
#define MAX_SIZE_OF_PROPERTY_ID    MAX_SIZE_OF_DEVICE_NAME
#define MAX_SIZE_OF_OP             (4)
#define MAX_SIZE_OF_PROPERTY_VALUE (1024)
#define MAX_SIZE_OF_AUTOMATION_ID  (64)
#define MAX_SIZE_OF_CONDITION_ID   (64)
#define MAX_SIZE_OF_ACTION_RESULT  (64)
#define MAX_SIZE_OF_ACTION_DATA    (64)

/* local automation action */
/**
struct tm {
    int tm_sec;
    int tm_min;
    int tm_hour;
    int tm_mday;
    int tm_mon;
    int tm_year;
    int tm_wday;
    int tm_yday;
    int tm_isdst;
};
*/
static void _get_current_datetime(struct tm *time_value)
{
    time_t now;

    time(&now);
#ifdef WIN32
    localtime_s(time_value, &now);
#else
    *time_value = *localtime(&now);
#endif
}

typedef enum gateway_automation_action_type_e {
    GATEWAY_AUTOMATION_ACTION_DEVICE_E = 0,
    GATEWAY_AUTOMATION_ACTION_DELAY_E  = 1,
    GATEWAY_AUTOMATION_ACTION_SCENE_E  = 2
} GATEWAY_AUTOMATION_ACTION_TYPE_E;

typedef struct gateway_automation_action {
    GATEWAY_AUTOMATION_ACTION_TYPE_E e_action_type;
    char                             product_id[MAX_SIZE_OF_PRODUCT_ID + 2];
    char                             device_name[MAX_SIZE_OF_DEVICE_NAME];
    char                             data[MAX_SIZE_OF_ACTION_DATA];
    char                             result[MAX_SIZE_OF_ACTION_RESULT];
} GATEWAY_AUTOMATION_ACTION_T;

typedef enum gateway_automation_condition_type_e {
    GATEWAY_AUTOMATION_CONDITION_PROPERTY_E = 0,
    GATEWAY_AUTOMATION_CONDITION_TIMER_E    = 1,
} GATEWAY_AUTOMATION_CONDITION_TYPE_E;

typedef struct {
    char product_id[MAX_SIZE_OF_PRODUCT_ID + 2];
    char device_name[MAX_SIZE_OF_DEVICE_NAME];
    char alias_name[MAX_SIZE_OF_ALIAS_NAME];
    char icon_url[MAX_SIZE_OF_ICON_URL];
    char property_id[MAX_SIZE_OF_PROPERTY_ID];
    char op[MAX_SIZE_OF_OP];  // eq ne gt lt ge le
    char value[MAX_SIZE_OF_PROPERTY_VALUE];
} GATEWAY_AUTOMATION_CONDITION_PROPERTY_T;

typedef struct {
    char week_days[7];   // 0 is off, 1 is on, 7 1 2 3 4 5 6
    int  time_point[2];  // start time point
    bool checked;
    int  expire;
} GATEWAY_AUTOMATION_CONDITION_TIMER_T;

typedef struct gateway_automation_condition {
    char                                cond_id[MAX_SIZE_OF_CONDITION_ID];
    GATEWAY_AUTOMATION_CONDITION_TYPE_E cond_type;  // 0 device_property cond; 1 timer cond
    union {
        GATEWAY_AUTOMATION_CONDITION_PROPERTY_T property;
        GATEWAY_AUTOMATION_CONDITION_TIMER_T    timer;
    } condition_timerproperty;
} GATEWAY_AUTOMATION_CONDITION_T;

#define GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT    10
#define GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT 10
#define GATEWAY_AUTOMATION_MAXCOUNT            5

typedef struct gateway_local_automation {
    char                           automation_id[MAX_SIZE_OF_AUTOMATION_ID];
    int                            status;           // off is 0, on is 1
    int                            cond_match_type;  // 0 is all conds, 1 is one of conds
    int                            effective_begintime[2];
    int                            effective_endtime[2];
    char                           effective_days[7];  // 0 is off, 1 is on, 7 1 2 3 4 5 6
    GATEWAY_AUTOMATION_ACTION_T    actions[GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT];
    GATEWAY_AUTOMATION_CONDITION_T conditions[GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT];
} GATEWAY_LOCAL_AUTOMATION_T;

static GATEWAY_LOCAL_AUTOMATION_T sg_automation[GATEWAY_AUTOMATION_MAXCOUNT];
static char                       sg_automation_log_buf[1024];

static void _gateway_local_automation_del_conds(
    GATEWAY_AUTOMATION_CONDITION_T cond[GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT])
{
    for (int i = 0; i < GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT; i++) {
        if (cond[i].cond_id[0] == 0) {
            continue;
        }
        memset(&cond[i], 0, sizeof(cond[i]));
    }
}

static int _getway_local_automation_conds_init(
    GATEWAY_AUTOMATION_CONDITION_T cond[GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT], int index, char *jsondoc)
{
    int   rc          = QCLOUD_RET_SUCCESS;
    char *product_id  = NULL;
    char *device_name = NULL;
    char *alias_name  = NULL;
    char *icon_url    = NULL;
    char *property_id = NULL;
    char *op          = NULL;
    char *value       = NULL;
    char *week_days   = NULL;
    char *time_point  = NULL;
    char *expire_time = NULL;
    // proc
    char *cond_id   = LITE_json_value_of("CondId", jsondoc);
    char *cond_type = LITE_json_value_of("CondType", jsondoc);
    if (cond_id == NULL || cond_type == NULL) {
        HAL_Free(cond_id);
        HAL_Free(cond_type);
        rc = QCLOUD_ERR_FAILURE;
        goto exit;
    }
    strncpy(cond[index].cond_id, cond_id, sizeof(cond[index].cond_id));
    cond[index].cond_type = atoi(cond_type);
    if (cond[index].cond_type == GATEWAY_AUTOMATION_CONDITION_PROPERTY_E) {
        product_id  = LITE_json_value_of("Property.ProductId", jsondoc);
        device_name = LITE_json_value_of("Property.DeviceName", jsondoc);
        alias_name  = LITE_json_value_of("Property.AliasName", jsondoc);
        icon_url    = LITE_json_value_of("Property.IconUrl", jsondoc);
        property_id = LITE_json_value_of("Property.PropertyId", jsondoc);
        op          = LITE_json_value_of("Property.Op", jsondoc);
        value       = LITE_json_value_of("Property.Value", jsondoc);
        if (product_id == NULL || device_name == NULL || property_id == NULL || op == NULL || value == NULL) {
            memset(cond[index].cond_id, 0, sizeof(cond[index].cond_id));
            rc = QCLOUD_ERR_FAILURE;
            goto exit;
        } else {
            strncpy(cond[index].condition_timerproperty.property.product_id, product_id,
                    sizeof(cond[index].condition_timerproperty.property.product_id));
            strncpy(cond[index].condition_timerproperty.property.device_name, device_name,
                    sizeof(cond[index].condition_timerproperty.property.device_name));
            if (alias_name) {
                strncpy(cond[index].condition_timerproperty.property.alias_name, alias_name,
                        sizeof(cond[index].condition_timerproperty.property.alias_name));
            }
            if (icon_url) {
                strncpy(cond[index].condition_timerproperty.property.icon_url, icon_url,
                        sizeof(cond[index].condition_timerproperty.property.icon_url));
            }

            strncpy(cond[index].condition_timerproperty.property.property_id, property_id,
                    sizeof(cond[index].condition_timerproperty.property.property_id));
            strncpy(cond[index].condition_timerproperty.property.op, op,
                    sizeof(cond[index].condition_timerproperty.property.op));
            strncpy(cond[index].condition_timerproperty.property.value, value,
                    sizeof(cond[index].condition_timerproperty.property.value));
        }

    } else if (cond[index].cond_type == GATEWAY_AUTOMATION_CONDITION_TIMER_E) {
        week_days   = LITE_json_value_of("Timer.Days", jsondoc);
        time_point  = LITE_json_value_of("Timer.TimePoint", jsondoc);
        expire_time = LITE_json_value_of("Timer.Expire", jsondoc);
        if (week_days != NULL && time_point != NULL) {
            sscanf(time_point, "%02d:%02d", &cond[index].condition_timerproperty.timer.time_point[0],
                   &cond[index].condition_timerproperty.timer.time_point[1]);
            strncpy(cond[index].condition_timerproperty.timer.week_days, week_days,
                    sizeof(cond[index].condition_timerproperty.timer.week_days));
            if (expire_time != NULL) {
                cond[index].condition_timerproperty.timer.expire = atoi(expire_time);
            }
        } else {
            rc = QCLOUD_ERR_FAILURE;
        }

    } else {
        Log_e("cond type error %s", cond_type);
        rc = QCLOUD_ERR_FAILURE;
    }

exit:
    HAL_Free(product_id);
    HAL_Free(device_name);
    HAL_Free(alias_name);
    HAL_Free(icon_url);
    HAL_Free(property_id);
    HAL_Free(op);
    HAL_Free(value);
    HAL_Free(cond_id);
    HAL_Free(cond_type);
    HAL_Free(week_days);
    HAL_Free(time_point);
    HAL_Free(expire_time);
    return rc;
}

static void _gateway_local_automation_set_conds(
    char *conds_str, GATEWAY_AUTOMATION_CONDITION_T cond[GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT])
{
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    int cond_count = 0;
    json_array_for_each_entry(conds_str, pos, entry, entry_len, entry_type)
    {
        if (cond_count >= GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT) {
            break;
        }

        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);
        Log_d("conds :%s", entry);
        // proc
        if (_getway_local_automation_conds_init(cond, cond_count, entry) != QCLOUD_RET_SUCCESS) {
            Log_e("parse json array error. ");
            break;
        }

        cond_count++;

        restore_json_str_last_char(entry, entry_len, old_ch);
    }
}

static void _gateway_local_automation_del_actions(
    GATEWAY_AUTOMATION_ACTION_T action[GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT])
{
    for (int i = 0; i < GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT; i++) {
        action[i].e_action_type = 0;
        memset(&action[i], 0, sizeof(action[i]));
    }
}

static void _gateway_local_automation_set_actions(
    char *actions_str, GATEWAY_AUTOMATION_ACTION_T action[GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT])
{
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    int action_count = 0;
    json_array_for_each_entry(actions_str, pos, entry, entry_len, entry_type)
    {
        if (action_count >= GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT) {
            break;
        }

        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);
        Log_d("actions :%s", entry);
        // proc
        char *data        = LITE_json_value_of("Data", entry);
        char *action_type = LITE_json_value_of("ActionType", entry);
        char *product_id  = LITE_json_value_of("ProductId", entry);
        char *device_name = LITE_json_value_of("DeviceName", entry);

        if (data == NULL || action_type == NULL) {
            HAL_Free(data);
            HAL_Free(action_type);
            continue;
        }
        strncpy(action[action_count].data, data, sizeof(action[action_count].data));
        action[action_count].e_action_type = atoi(action_type);
        switch (action[action_count].e_action_type) {
            case GATEWAY_AUTOMATION_ACTION_DEVICE_E: {
                if (product_id) {
                    strncpy(action[action_count].product_id, product_id, sizeof(action[action_count].product_id));
                }
                if (device_name) {
                    strncpy(action[action_count].device_name, device_name, sizeof(action[action_count].device_name));
                }
            } break;

            default:
                break;
        }

        action_count++;

        HAL_Free(data);
        HAL_Free(action_type);
        HAL_Free(product_id);
        HAL_Free(device_name);

        restore_json_str_last_char(entry, entry_len, old_ch);
    }
}

static int _gateway_local_automation_delete(char *automation_id, void *user_data)
{
    GATEWAY_LOCAL_AUTOMATION_T *automation = NULL;

    for (int i = 0; i < sizeof(sg_automation) / sizeof(GATEWAY_LOCAL_AUTOMATION_T); i++) {
        if ((0 != sg_automation[i].automation_id[0]) && (0 == strcmp(automation_id, sg_automation[i].automation_id))) {
            automation = &sg_automation[i];
            break;
        }
    }

    if (automation == NULL) {
        return QCLOUD_ERR_FAILURE;
    }

    _gateway_local_automation_del_actions(automation->actions);
    _gateway_local_automation_del_conds(automation->conditions);

    memset(automation, 0, sizeof(GATEWAY_LOCAL_AUTOMATION_T));

    return QCLOUD_RET_SUCCESS;
}

static int _gateway_local_automation_set(char *automation_id, int mation_status, char *params, void *user_data)
{
    GATEWAY_LOCAL_AUTOMATION_T *automation = NULL;

    if (automation_id == NULL || params == NULL) {
        return QCLOUD_ERR_INVAL;
    }

    _gateway_local_automation_delete(automation_id, user_data);

    for (int i = 0; i < sizeof(sg_automation) / sizeof(GATEWAY_LOCAL_AUTOMATION_T); i++) {
        if (sg_automation[i].automation_id[0] == 0) {
            automation = &sg_automation[i];
            break;
        }
    }

    if (automation == NULL) {
        return QCLOUD_ERR_FAILURE;
    }

    memset(automation, 0, sizeof(GATEWAY_LOCAL_AUTOMATION_T));

    strcpy(automation->automation_id, automation_id);

    char *actions    = LITE_json_value_of("actions", params);
    char *conditions = LITE_json_value_of("conditions", params);

    char *match_type            = LITE_json_value_of("matchType", params);
    automation->cond_match_type = atoi(match_type);

    char *effective_begintime = LITE_json_value_of("effectiveBeginTime", params);
    char *effective_endtime   = LITE_json_value_of("effectiveEndTime", params);
    char *effective_days      = LITE_json_value_of("effectiveDays", params);

    sscanf(effective_begintime, "%02d:%02d", (int *)&automation->effective_begintime[0],
           (int *)&automation->effective_begintime[1]);
    sscanf(effective_endtime, "%02d:%02d", (int *)&automation->effective_endtime[0],
           (int *)&automation->effective_endtime[1]);
    strncpy(automation->effective_days, effective_days, sizeof(automation->effective_days));

    _gateway_local_automation_set_actions(actions, automation->actions);
    _gateway_local_automation_set_conds(conditions, automation->conditions);

    automation->status = mation_status;

    HAL_Free(match_type);
    HAL_Free(effective_begintime);
    HAL_Free(effective_endtime);
    HAL_Free(effective_days);
    HAL_Free(actions);
    HAL_Free(conditions);

    return QCLOUD_RET_SUCCESS;
}

static void _gateway_local_automation_report_log(void *clent, GATEWAY_LOCAL_AUTOMATION_T *automation)
{
    char *log_json_str = "{\"code\":0,\"result\":\"success\",\"reportTime\":%d,\"actionResults\":[";

    int   remain_len = 1024;
    char *log_point  = HAL_Malloc(1024);
    if (NULL == log_point) {
        Log_e("malloc failed");
        return;
    }

    char *log_json = log_point;
    int   json_len = HAL_Snprintf(log_json, remain_len, log_json_str, HAL_Timer_current_sec());
    remain_len -= json_len;
    log_json += json_len;

    for (int i = 0; i < GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT; i++) {
        if (0 == automation->actions[i].data[0]) {
            continue;
        }

        if (automation->actions[i].e_action_type == GATEWAY_AUTOMATION_ACTION_DEVICE_E) {
            json_len =
                HAL_Snprintf(log_json, remain_len, "{\"productId\":\"%s\",\"deviceName\":\"%s\",\"result\":\"%s\"},",
                             automation->actions[i].product_id, automation->actions[i].device_name,
                             automation->actions[i].result == NULL ? "success" : automation->actions[i].result);
        } else {
            json_len = HAL_Snprintf(log_json, remain_len, "{\"result\":\"%s\"},",
                                    automation->actions[i].result == NULL ? "success" : automation->actions[i].result);
        }

        remain_len -= json_len;
        log_json += json_len;
    }

    if (',' != *(log_json - 1)) {
        HAL_Free(log_point);
        return;
    }

    log_json -= 1;
    remain_len += 1;
    json_len = HAL_Snprintf(log_json, remain_len, "]}");
    remain_len -= json_len;
    log_json += json_len;

    json_len = IOT_Gateway_LocalAutoMationLogCreate(sg_automation_log_buf, sizeof(sg_automation_log_buf) - 1, clent,
                                                    automation->automation_id, log_point);

    if (QCLOUD_RET_SUCCESS != IOT_Gateway_LocalAutoMationReportLog(clent, sg_automation_log_buf, json_len)) {
        Log_e("report log error, automationid:%s", automation->automation_id);
    } else {
        Log_e("report log success, automationid:%s", automation->automation_id);
    }

    HAL_Free(log_point);
}

typedef struct {
    char *product_id;
    char *device_name;
    char *property_id;
    int   value;
} GATEWAY_AUTOMATION_PROPERTY_T;

static GATEWAY_AUTOMATION_PROPERTY_T sg_automation_property[] = {
    {NULL, NULL, "brightness", 10},
    {NULL, NULL, "power_switch", 1},
    {NULL, NULL, "color", 0},
};

static void _gateway_set_device_property(char *product_id, char *device_name)
{
    for (int i = 0; i < sizeof(sg_automation_property) / sizeof(GATEWAY_AUTOMATION_PROPERTY_T); i++) {
        if (sg_automation_property[i].product_id == NULL) {
            sg_automation_property[i].product_id  = product_id;
            sg_automation_property[i].device_name = device_name;
            break;
        }
    }
}

static void _gateway_automation_set_device_allproperty(void)
{
    for (int i = 0; i < sizeof(sg_automation_property) / sizeof(GATEWAY_AUTOMATION_PROPERTY_T); i++) {
        sg_automation_property[i].value = ~(sg_automation_property[i].value);
    }
}

static bool _gateway_local_automation_cond_property(GATEWAY_AUTOMATION_CONDITION_PROPERTY_T *property_cond)
{
    bool                           result   = false;
    GATEWAY_AUTOMATION_PROPERTY_T *property = NULL;

    for (int i = 0; i < sizeof(sg_automation_property) / sizeof(GATEWAY_AUTOMATION_PROPERTY_T); i++) {
        if (0 == strcmp(sg_automation_property[i].property_id, property_cond->property_id) &&
            0 == strcmp(sg_automation_property[i].product_id, property_cond->product_id) &&
            0 == strcmp(sg_automation_property[i].device_name, property_cond->device_name)) {
            property = &sg_automation_property[i];
        }
    }

    if (property == NULL) {
        // Log_e("error property cond %s ,%s ,%s", property_cond->product_id, property_cond->device_name,
        //      property_cond->property_id);
        return false;
    }
    if (0 == strcmp("eq", property_cond->op)) {
        if (property->value == atoi(property_cond->value)) {
            result = true;
            Log_d("property id :%s, vlaue:%d, op:%s", property->property_id, property->value, property_cond->op);
        }
    } else if (0 == strcmp("ne", property_cond->op)) {
        if (property->value != atoi(property_cond->value)) {
            result = true;
            Log_d("property id :%s, vlaue:%d, op:%s", property->property_id, property->value, property_cond->op);
        }
    } else if (0 == strcmp("gt", property_cond->op)) {
        if (property->value > atoi(property_cond->value)) {
            result = true;
            Log_d("property id :%s, vlaue:%d, op:%s", property->property_id, property->value, property_cond->op);
        }

    } else if (0 == strcmp("lt", property_cond->op)) {
        if (property->value < atoi(property_cond->value)) {
            result = true;
            Log_d("property id :%s, vlaue:%d, op:%s", property->property_id, property->value, property_cond->op);
        }

    } else if (0 == strcmp("ge", property_cond->op)) {
        if (property->value >= atoi(property_cond->value)) {
            result = true;
            Log_d("property id :%s, vlaue:%d, op:%s", property->property_id, property->value, property_cond->op);
        }

    } else if (0 == strcmp("le", property_cond->op)) {
        if (property->value <= atoi(property_cond->value)) {
            result = true;
            Log_d("property id :%s, vlaue:%d, op:%s", property->property_id, property->value, property_cond->op);
        }

    } else {
        Log_e("op error :%s", property_cond->op);
    }

    return result;
}

static bool _gateway_local_automation_cond_timer(GATEWAY_AUTOMATION_CONDITION_TIMER_T *timer_cond)
{
    int timestamp = HAL_Timer_current_sec();
    if ((timer_cond->expire != 0) && (timestamp > timer_cond->expire)) {
        return false;
    }

    struct tm time_value;
    _get_current_datetime(&time_value);

    if (timer_cond->week_days[time_value.tm_wday] == '0' && timer_cond->expire == 0) {
        return false;
    }

    if ((time_value.tm_hour == timer_cond->time_point[0]) && (time_value.tm_min == timer_cond->time_point[1])) {
        // one minute only one time
        if (timer_cond->checked == false) {
            Log_d("time point");
            timer_cond->checked = true;
            return true;
        }
    } else {
        timer_cond->checked = false;
    }

    return false;
}

static bool _gateway_local_automation_cond_check(
    int match_type, GATEWAY_AUTOMATION_CONDITION_T conds[GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT])
{
    bool cond_result      = false;
    bool result           = false;
    int  cond_valid_count = 0;

    // set cond init value
    cond_result = match_type == 0 ? true : false;

    for (int i = 0; i < GATEWAY_AUTOMATION_CONDITIONS_MAXCOUNT; i++) {
        if (conds[i].cond_id == NULL) {
            continue;
        }

        cond_valid_count++;
        if (conds[i].cond_type == GATEWAY_AUTOMATION_CONDITION_PROPERTY_E) {
            // device property compare
            result = _gateway_local_automation_cond_property(&(conds[i].condition_timerproperty.property));
        }
        if (conds[i].cond_type == GATEWAY_AUTOMATION_CONDITION_TIMER_E) {
            // device timer compare
            result = _gateway_local_automation_cond_timer(&(conds[i].condition_timerproperty.timer));
        }
        if (match_type == 0) {
            cond_result &= result;
        }

        if (match_type == 1) {
            cond_result |= result;
        }
    }

    if (cond_valid_count == 0) {
        cond_result = true;
    }

    return cond_result;
}

static int _gateway_local_automation_action_execution(
    GATEWAY_AUTOMATION_ACTION_T actions[GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT])
{
    for (int i = 0; i < GATEWAY_AUTOMATION_ACTIONS_MAXCOUNT; i++) {
        if (actions[i].data == NULL) {
            continue;
        }
        strncpy(actions[i].result, "failed", sizeof(actions[i].result));

        if (actions[i].e_action_type == GATEWAY_AUTOMATION_ACTION_DEVICE_E) {
            // to do something for device property
            int data_len = 0;
            int position = 0;
            while (((char *)actions[i].data)[data_len] != '\0') {
                if (((char *)actions[i].data)[data_len] == '\\') {
                    data_len++;
                    continue;
                }
                ((char *)(actions[i].data))[position] = ((char *)(actions[i].data))[data_len];
                position++;
                data_len++;
            }
            ((char *)(actions[i].data))[position]   = '\0';
            GATEWAY_AUTOMATION_PROPERTY_T *property = NULL;
            for (int j = 0; j < sizeof(sg_automation_property) / sizeof(GATEWAY_AUTOMATION_PROPERTY_T); j++) {
                if (0 == strcmp(sg_automation_property[j].product_id, actions[i].product_id) &&
                    0 == strcmp(sg_automation_property[j].device_name, actions[i].device_name)) {
                    property    = &sg_automation_property[j];
                    char *value = LITE_json_value_of(property->property_id, (char *)actions[i].data);
                    if (value != NULL) {
                        property->value = atoi(value);
                        HAL_Free(value);
                        value = NULL;
                        strncpy(actions[i].result, "success", sizeof(actions[i].result));
                        Log_d("set %s-%s property id %s, value :%d", property->product_id, property->device_name,
                              property->property_id, property->value);
                    }
                }
            }
        }

        if (actions[i].e_action_type == GATEWAY_AUTOMATION_ACTION_DELAY_E) {
            int delay_time = atoi(actions[i].data);
            strncpy(actions[i].result, "success", sizeof(actions[i].result));
            Log_d("delay_time :%d, to do delay task", delay_time);

            _gateway_automation_set_device_allproperty();
        }

        if (actions[i].e_action_type == GATEWAY_AUTOMATION_ACTION_SCENE_E) {
            int scene_id = atoi(actions[i].data);
            strncpy(actions[i].result, "success", sizeof(actions[i].result));
            Log_d("scene id :%d, to do something", scene_id);
        }
    }

    return QCLOUD_RET_SUCCESS;
}

static int _gateway_local_automation_execution(void *client, GATEWAY_LOCAL_AUTOMATION_T *automation)
{
    if (automation->status == 0) {
        return QCLOUD_RET_SUCCESS;
    }

    struct tm time_value;
    _get_current_datetime(&time_value);

    if (automation->effective_days[time_value.tm_wday] == '0') {
        return QCLOUD_RET_SUCCESS;
    }

    if (time_value.tm_hour < automation->effective_begintime[0]) {
        return QCLOUD_RET_SUCCESS;
    }

    if ((time_value.tm_hour == automation->effective_begintime[0]) &&
        (time_value.tm_min < automation->effective_begintime[1])) {
        return QCLOUD_RET_SUCCESS;
    }

    if (time_value.tm_hour > automation->effective_endtime[0]) {
        return QCLOUD_RET_SUCCESS;
    }
    if ((time_value.tm_hour == automation->effective_endtime[0]) &&
        (time_value.tm_min > automation->effective_endtime[1])) {
        return QCLOUD_RET_SUCCESS;
    }

    // check cond
    if (false == _gateway_local_automation_cond_check(automation->cond_match_type, automation->conditions)) {
        return QCLOUD_RET_SUCCESS;
    }

    _gateway_local_automation_action_execution(automation->actions);
    _gateway_local_automation_report_log(client, automation);

    return QCLOUD_RET_SUCCESS;
}

static void _gateway_local_automation_foreach(void *client)
{
    GATEWAY_LOCAL_AUTOMATION_T *automation = NULL;

    for (int i = 0; i < sizeof(sg_automation) / sizeof(GATEWAY_LOCAL_AUTOMATION_T); i++) {
        if (sg_automation[i].automation_id != NULL) {
            automation = &sg_automation[i];
            _gateway_local_automation_execution(client, automation);
        }
    }
}
#endif

static struct {
    bool     power_off;
    uint8_t  brightness;
    uint16_t color;
    char     name[MAX_SIZE_OF_DEVICE_NAME + 1];
} sg_led_info = {
    .power_off  = true,
    .brightness = 10,
    .color      = 0,
};

/*Gateway should enable multithread*/
int main(int argc, char **argv)
{
    int                rc       = QCLOUD_ERR_FAILURE;
    int                errCount = 0;
    int                i;
    int                size;
    void *             client = NULL, *mqtt = NULL;
    GatewayDeviceInfo *gw    = &sg_GWdevInfo;
    GatewayParam       param = DEFAULT_GATEWAY_PARAMS;
    DeviceInfo *       sub;

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
    mqtt = IOT_Gateway_Get_Mqtt_Client(client);

    // set GateWay device info
    param.product_id  = gw->gw_info.product_id;
    param.device_name = gw->gw_info.device_name;

#ifdef GATEWAY_DYN_BIND_SUBDEV_ENABLED
    // show gateway dynamic bind/unbind sub-devices
    show_subdev_bind_unbind(client, &param);
#endif

#ifdef GATEWAY_AUTOMATION_ENABLED
    memset(sg_automation, 0, sizeof(sg_automation));
    QCLOUD_IO_GATEWAY_AUTOMATION_T aution = {client, NULL, _gateway_local_automation_set,
                                             _gateway_local_automation_delete};
    _gateway_set_device_property(param.product_id, param.device_name);
    for (i = 0; i < gw->sub_dev_num; i++) {
        sub = &gw->sub_dev_info[i];
        _gateway_set_device_property(sub->product_id, sub->device_name);
    }
    if (QCLOUD_RET_SUCCESS < IOT_Gateway_EnableLocalAutoMation(client, &aution)) {
        Log_e("enable auto mation failed");
    } else {
        IOT_Gateway_GetAutoMationList(client);
    }
#endif

    // make sub-device online
    for (i = 0; i < gw->sub_dev_num; i++) {
        sub                      = &gw->sub_dev_info[i];
        param.subdev_product_id  = sub->product_id;
        param.subdev_device_name = sub->device_name;

        rc = IOT_Gateway_Subdev_Online(client, &param);
        if (rc != QCLOUD_RET_SUCCESS) {
            Log_e("subDev Pid:%s devName:%s online error.", sub->product_id, sub->device_name);
            errCount++;
        } else {
            Log_d("subDev Pid:%s devName:%s online ok.", sub->product_id, sub->device_name);
        }
    }
    if (errCount > 0) {
        Log_e("%d of %d sub devices online error", errCount, gw->sub_dev_num);
    }

    // subscribe sub-device data_template down stream topic for example
    char            topic_filter[MAX_SIZE_OF_TOPIC + 1];
    SubscribeParams sub_param = DEFAULT_SUB_PARAMS;
    for (i = 0; i < gw->sub_dev_num; i++) {
        sub = &gw->sub_dev_info[i];

        memset(topic_filter, 0, MAX_SIZE_OF_TOPIC + 1);
        size = HAL_Snprintf(topic_filter, MAX_SIZE_OF_TOPIC, "$thing/down/property/%s/%s", sub->product_id,
                            sub->device_name);

        if (size < 0 || size > MAX_SIZE_OF_TOPIC) {
            Log_e("buf size < topic length!");
            rc = QCLOUD_ERR_FAILURE;
            goto exit;
        }

        sub_param.on_message_handler = _message_handler;
        rc                           = IOT_Gateway_Subscribe(client, topic_filter, &sub_param);
        if (rc < 0) {
            Log_e("IOT_Gateway_Subscribe failed.");
            goto exit;
        }

        int wait_cnt = 10;
        while (!IOT_MQTT_IsSubReady(mqtt, topic_filter) && (--wait_cnt)) {
            // wait for subscription result
            rc = IOT_MQTT_Yield(mqtt, 1000);
            if (rc) {
                Log_e("MQTT error: %d", rc);
                goto exit;
            }
        }

        if (wait_cnt <= 0) {
            Log_e("wait for subscribe result timeout!");
            goto exit;
        }
    }

    while (sg_loop_count--) {
        for (i = 0; i < gw->sub_dev_num; i++) {
            static int sg_report_index = 0;
            char       message[256]    = {0};
            // only change the brightness in the demo
            sg_led_info.brightness %= 100;
            sg_led_info.brightness++;
            sub = &gw->sub_dev_info[i];
            HAL_Snprintf(sg_led_info.name, sizeof(sg_led_info.name), "%s", sub->device_name);
            int message_len =
                HAL_Snprintf(message, sizeof(message),
                             "{\"method\":\"report\", \"clientToken\":\"%s-%d\", "
                             "\"params\":{\"power_switch\":%d, \"color\":%d, \"brightness\":%d, \"name\":\"%s\"}}",
                             sg_GWdevInfo.gw_info.product_id, sg_report_index++, sg_led_info.power_off,
                             sg_led_info.color, sg_led_info.brightness, sub->device_name);

            _property_topic_publish(mqtt, sub->product_id, sub->device_name, message, message_len);
            // publish msg to sub-device topic

            rc = IOT_Gateway_Yield(client, 200);

            if (rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT) {
                HAL_SleepMs(1000);
                continue;
            }
            if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
                Log_e("exit with error: %d", rc);
                break;
            }

#ifdef GATEWAY_AUTOMATION_ENABLED
            /* local auto mation */
            _gateway_local_automation_foreach(client);
#endif
            if (true == sg_unbind_all_subdev) {
                Log_d("gateway all subdev have been unbind by cloud platform stop publish subdev msg");
                sg_unbind_all_subdev = false;
                sg_loop_count        = 0;
                break;
            }

            HAL_SleepMs(1000);
        }
    }

    // make sub-device offline
    for (i = 0; i < gw->sub_dev_num; i++) {
        sub                      = &gw->sub_dev_info[i];
        param.subdev_product_id  = sub->product_id;
        param.subdev_device_name = sub->device_name;

        rc = IOT_Gateway_Subdev_Offline(client, &param);
        if (rc != QCLOUD_RET_SUCCESS) {
            Log_e("subDev Pid:%s devName:%s online error.", sub->product_id, sub->device_name);
            errCount++;
        } else {
            Log_d("subDev Pid:%s devName:%s online ok.", sub->product_id, sub->device_name);
        }
    }
    if (errCount > 0) {
        Log_e("%d of %d sub devices online error", errCount, gw->sub_dev_num);
    }

exit:
    rc = IOT_Gateway_Destroy(client);

    return rc;
}
