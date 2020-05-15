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
#include "sub_product_data_template_config.h"
#include "lite-utils.h"
#include "utils_timer.h"

/* anis color control codes */
#define ANSI_COLOR_RED    "\x1b[31m"
#define ANSI_COLOR_GREEN  "\x1b[32m"
#define ANSI_COLOR_YELLOW "\x1b[33m"
#define ANSI_COLOR_BLUE   "\x1b[34m"
#define ANSI_COLOR_RESET  "\x1b[0m"

typedef enum {
    eCOLOR_RED   = 0,
    eCOLOR_GREEN = 1,
    eCOLOR_BLUE  = 2,
} eColor;

#ifdef EVENT_POST_ENABLED
static void event_post_cb(void *pClient, MQTTMessage *msg)
{
    Log_d("Reply:%.*s", msg->payload_len, msg->payload);
    //  IOT_Event_clearFlag(pClient, EVENT_FLAG(eEVENT_STATUS_REPORT) | EVENT_FLAG(eEVENT_LOW_VOLTAGE) |
    //  EVENT_FLAG(eEVENT_HARDWARE_FAULT));
}

// event check and post
static void eventPostCheck(void *pClient)
{
    int           rc, i;
    uint32_t      eflag;
    uint8_t       event_count;
    char          data_report_buffer[1024];
    sEvent *      pEventList[TOTAL_EVENT_COUNTS];
    DataTemplate *pTemplateData = IOT_Template_Get_DataTemplate(pClient);

    eflag = IOT_Event_getFlag(pClient);
    if ((TOTAL_EVENT_COUNTS > 0) && (eflag > 0)) {
        event_count = 0;
        for (i = 0; i < TOTAL_EVENT_COUNTS; i++) {
            if ((eflag & (1 << i)) & ALL_EVENTS_MASK) {
                pEventList[event_count++] = &pTemplateData->events[i];
                pTemplateData->method.set_event_timestamp(pClient, i);
                IOT_Event_clearFlag(pClient, (1 << i) & ALL_EVENTS_MASK);
            }
        }
        rc = IOT_Post_Event(pClient, data_report_buffer, 1024, event_count, pEventList, event_post_cb);
        if (rc < 0) {
            Log_e("events post failed: %d", rc);
        }
    }
}
#endif

#ifdef ACTION_ENABLED
static void OnActionCallback(void *pClient, const char *pClientToken, DeviceAction *pAction)
{
    sReplyPara    replyPara;
    char          data_report_buffer[1024];
    DataTemplate *pTemplateData = IOT_Template_Get_DataTemplate(pClient);
    ActionsPara * pActionPara   = pTemplateData->method.get_action_para(pClient);

    if (!strcmp(pAction->pActionId, pTemplateData->actions[eACTION_BLINK].pActionId)) {
        // get input para
        Log_d("Blink action receved, period:%d", pActionPara->m_blink_input_period);

        // execte action: do blink
        HAL_Printf("%s[lighting blink][****]" ANSI_COLOR_RESET, ANSI_COLOR_RED);
        HAL_SleepMs(pActionPara->m_blink_input_period * 1000);
        HAL_Printf("\r%s[lighting blink][****]" ANSI_COLOR_RESET, ANSI_COLOR_GREEN);
        HAL_SleepMs(pActionPara->m_blink_input_period * 1000);
        HAL_Printf("\r%s[lighting blink][****]\n" ANSI_COLOR_RESET, ANSI_COLOR_RED);

        // set output para
        pActionPara->m_blink_output_result = 0;

        // construct output
        memset((char *)&replyPara, 0, sizeof(sReplyPara));
        replyPara.code       = eDEAL_SUCCESS;
        replyPara.timeout_ms = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
        strcpy(replyPara.status_msg, "action execute success!");  // add the message about the action resault

        // reply action result
        IOT_ACTION_REPLY(pClient, pClientToken, data_report_buffer, 1024, pAction, &replyPara);
    }
}

static int register_data_template_action(void *pClient)
{
    int           i, rc;
    DataTemplate *pTemplateData = IOT_Template_Get_DataTemplate(pClient);

    for (i = 0; i < TOTAL_ACTION_COUNTS; i++) {
        rc = IOT_Template_Register_Action(pClient, &pTemplateData->actions[i], OnActionCallback);
        if (rc != QCLOUD_RET_SUCCESS) {
            rc = IOT_Template_Destroy(pClient);
            Log_e("register device data template action failed, err: %d", rc);
            return rc;
        } else {
            Log_i("data template action=%s registered.", pTemplateData->actions[i].pActionId);
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
            Log_i("subscribe success, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_TIMEOUT:
            Log_i("subscribe wait ack timeout, packet-id=%u", (unsigned int)packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_NACK:
            Log_i("subscribe nack, packet-id=%u", (unsigned int)packet_id);
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
static void usr_init(void)
{
    Log_d("add your init code here");
}

/*control msg from server will trigger this callback*/
static void OnControlMsgCallback(void *pClient, const char *pJsonValueBuffer, uint32_t valueLength,
                                 DeviceProperty *pProperty)
{
    int           i             = 0;
    DataTemplate *pTemplateData = IOT_Template_Get_DataTemplate(pClient);

    if (pTemplateData) {
        for (i = 0; i < TOTAL_PROPERTY_COUNTS; i++) {
            /* handle self defined string/json here. Other properties are dealed in _handle_delta()*/
            sDataPoint *pDataPoint = pTemplateData->method.get_property_data_point(pClient, i);
            if (strcmp(pDataPoint->data_property.key, pProperty->key) == 0) {
                pTemplateData->method.set_property_state(pClient, i, eCHANGED);
                Log_i("Property=%s changed, state:%d", pProperty->key,
                      pTemplateData->method.get_property_state(pClient, i));
                pTemplateData->control_msg_receved = true;
                return;
            }
        }
    }

    Log_e("Property=%s changed no match", pProperty->key);
}

static void OnReportReplyCallback(void *pClient, Method method, ReplyAck replyAck, const char *pJsonDocument,
                                  void *pUserdata)
{
    Log_i("recv report_reply(ack=%d): %s", replyAck, pJsonDocument);
}

// register data template properties
static int register_data_template_property(void *pClient)
{
    int           i, rc;
    DataTemplate *pTemplateData = IOT_Template_Get_DataTemplate(pClient);

    if (pTemplateData) {
        for (i = 0; i < TOTAL_PROPERTY_COUNTS; i++) {
            sDataPoint *pDataPoint = pTemplateData->method.get_property_data_point(pClient, i);
            rc = IOT_Template_Register_Property(pClient, &pDataPoint->data_property, OnControlMsgCallback);
            if (rc != QCLOUD_RET_SUCCESS) {
                Log_e("register device data template property failed, err: %d", rc);
                break;
            } else {
                Log_i("data template property=%s registered.", pDataPoint->data_property.key);
            }
        }
    } else {
        rc = QCLOUD_ERR_INVAL;
    }

    return rc;
}

/* demo for light logic deal */
static void deal_down_stream_user_logic(void *pClient)
{
    int         i;
    const char *ansi_color         = NULL;
    const char *ansi_color_name    = NULL;
    char        brightness_bar[]   = "||||||||||||||||||||";
    int         brightness_bar_len = strlen(brightness_bar);

    DataTemplate *     pTemplateData = IOT_Template_Get_DataTemplate(pClient);
    ProductDataDefine *light         = pTemplateData->method.get_product_para(pClient);

    /* light color */
    switch (light->m_color) {
        case eCOLOR_RED:
            ansi_color      = ANSI_COLOR_RED;
            ansi_color_name = " RED ";
            break;
        case eCOLOR_GREEN:
            ansi_color      = ANSI_COLOR_GREEN;
            ansi_color_name = "GREEN";
            break;
        case eCOLOR_BLUE:
            ansi_color      = ANSI_COLOR_BLUE;
            ansi_color_name = " BLUE";
            break;
        default:
            ansi_color      = ANSI_COLOR_YELLOW;
            ansi_color_name = "UNKNOWN";
            break;
    }
    /* light brightness bar */
    brightness_bar_len =
        (light->m_brightness >= 100) ? brightness_bar_len : (int)((light->m_brightness * brightness_bar_len) / 100);
    for (i = brightness_bar_len; i < strlen(brightness_bar); i++) {
        brightness_bar[i] = '-';
    }

    /*handle power_switch*/
    if (light->m_power_switch) {
        /* light is on , show with the properties*/
        HAL_Printf("%s[  lighting  ]|[color:%s]|[brightness:%s]|[%s]\n" ANSI_COLOR_RESET, ansi_color, ansi_color_name,
                   brightness_bar, light->m_name);
    } else {
        /* light is off */
        HAL_Printf(ANSI_COLOR_YELLOW "[  light is off ]|[color:%s]|[brightness:%s]|[%s]\n" ANSI_COLOR_RESET,
                   ansi_color_name, brightness_bar, light->m_name);
    }

    if (eCHANGED == pTemplateData->method.get_property_state(pClient, ePROPERTY_POWER_SWITCH)) {
#ifdef EVENT_POST_ENABLED
        EventsPara *pEventPara = pTemplateData->method.get_event_para(pClient);
        if (light->m_power_switch) {
            pEventPara->m_status_report_status = TEMPLATE_TRUE;
            memset(pEventPara->m_status_report_message, 0, 64);
            strcpy(pEventPara->m_status_report_message, "light on");
        } else {
            pEventPara->m_status_report_status = TEMPLATE_FALSE;
            memset(pEventPara->m_status_report_message, 0, 64);
            strcpy(pEventPara->m_status_report_message, "light off");
        }

        // the events will be posted by eventPostCheck
        Log_d("post event EVENT_STATUS_REPORT");
        IOT_Event_setFlag(pClient, EVENT_FLAG(eEVENT_STATUS_REPORT));
#else
        Log_d("light switch state changed");
#endif
    }
}

/*example for cycle report, you can delete this for your needs*/
static void cycle_report(void *pClient, Timer *reportTimer)
{
    int           i;
    DataTemplate *pTemplateData = IOT_Template_Get_DataTemplate(pClient);

    if (expired(reportTimer)) {
        for (i = 0; i < TOTAL_PROPERTY_COUNTS; i++) {
            pTemplateData->method.set_property_state(pClient, i, eCHANGED);
            countdown_ms(reportTimer, 5000);
        }
    }
}

/*get local property data, like sensor data*/
static void refresh_local_property(void *pClient, Timer *reportTimer)
{
    // add your local property refresh logic, cycle report for example
    cycle_report(pClient, reportTimer);
}

/* demo for up-stream code */
int deal_up_stream_user_logic(void *pClient, DeviceProperty *pReportDataList[], int *pCount, Timer *reportTimer)
{
    DataTemplate *pTemplateData = IOT_Template_Get_DataTemplate(pClient);

    // refresh local property
    refresh_local_property(pClient, reportTimer);

    /*find propery need report*/
    *pCount = pTemplateData->method.find_wait_report_property(pClient, pReportDataList);

    return (*pCount > 0) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

/*You should get the real info for your device, here just for example*/
static int get_sys_info(void *handle, char *pJsonDoc, size_t sizeOfBuffer)
{
    /*platform info has at least one of module_hardinfo/module_softinfo/fw_ver
     * property*/
    DeviceProperty plat_info[] = {
        {.key = "module_hardinfo", .type = TYPE_TEMPLATE_STRING, .data = "ESP8266"},
        {.key = "module_softinfo", .type = TYPE_TEMPLATE_STRING, .data = "V1.0"},
        {.key = "fw_ver", .type = TYPE_TEMPLATE_STRING, .data = QCLOUD_IOT_DEVICE_SDK_VERSION},
        {.key = "imei", .type = TYPE_TEMPLATE_STRING, .data = "11-22-33-44"},
        {.key = "lat", .type = TYPE_TEMPLATE_STRING, .data = "22.546015"},
        {.key = "lon", .type = TYPE_TEMPLATE_STRING, .data = "113.941125"},
        {NULL, NULL, 0}  // end
    };

    /*self define info*/
    DeviceProperty self_info[] = {
        {.key = "append_info", .type = TYPE_TEMPLATE_STRING, .data = "your self define info"}, {NULL, NULL, 0}  // end
    };

    return IOT_Template_JSON_ConstructSysInfo(handle, pJsonDoc, sizeOfBuffer, plat_info, self_info);
}

void sub_light_thread(void *user_arg, char *product_id, char *device_name)
{
#define REPORT_BUFF_LEN 1024
    char            data_report_buffer[REPORT_BUFF_LEN];
    DeviceProperty *pReportDataList[TOTAL_PROPERTY_COUNTS];
    sReplyPara      replyPara;
    int             ReportCont;
    int             rc;
    Timer           reportTimer;
    DataTemplate *  pTemplateData  = NULL;
    void *          pGateWayClient = user_arg;

    Log_d("sub_%s_%s_thread ...", product_id, device_name);

    // init connection
    TemplateInitParams init_params = DEFAULT_TEMPLATE_INIT_PARAMS;
    init_params.product_id         = product_id;
    init_params.device_name        = device_name;
    init_params.event_handle.h_fp  = event_handler;

    void *client = IOT_Template_Construct(&init_params, IOT_Gateway_Get_Mqtt_Client(pGateWayClient));
    if (client != NULL) {
        Log_i("Cloud Device Construct Success");
    } else {
        Log_e("Cloud Device Construct Failed");
        return;
    }

#ifdef MULTITHREAD_ENABLED
    /*only gateway yield thread read mqtt packet*/
    int  code;
    bool state = IOT_Gateway_Get_Yield_Status(pGateWayClient, &code);
    IOT_Template_Set_Yield_Status(client, state, code);
#endif

    // usr init
    usr_init();

    // init data template
    pTemplateData = init_sub_product_data_template();
    if (NULL == pTemplateData) {
        Log_e("init data template fail");
        goto exit;
    }

    IOT_Template_Set_DataTemplate(client, pTemplateData, deinit_sub_product_data_template);
    pTemplateData->control_msg_receved = false;

    // register data template propertys here
    rc = register_data_template_property(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template propertys Success");
    } else {
        Log_e("Register data template propertys Failed: %d", rc);
        goto exit;
    }

// register data template actions here
#ifdef ACTION_ENABLED
    rc = register_data_template_action(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template actions Success");
    } else {
        Log_e("Register data template actions Failed: %d", rc);
        goto exit;
    }
#endif

    // report device info, then you can manager your product by these info, like
    // position
    rc = get_sys_info(client, data_report_buffer, REPORT_BUFF_LEN);
    if (QCLOUD_RET_SUCCESS == rc) {
        rc = IOT_Template_Report_SysInfo_Sync(client, data_report_buffer, REPORT_BUFF_LEN,
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

    // init a timer for cycle report, you could delete it or not for your needs
    InitTimer(&reportTimer);

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
        if (pTemplateData->control_msg_receved) {
            deal_down_stream_user_logic(client);
            /* control msg should reply, otherwise server treat device didn't receive
             * and retain the msg which would be get by  get status*/
            memset((char *)&replyPara, 0, sizeof(sReplyPara));
            replyPara.code          = eDEAL_SUCCESS;
            replyPara.timeout_ms    = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
            replyPara.status_msg[0] = '\0';  // add extra info to replyPara.status_msg when error occured

            rc = IOT_Template_ControlReply(client, data_report_buffer, REPORT_BUFF_LEN, &replyPara);
            if (rc == QCLOUD_RET_SUCCESS) {
                Log_d("Contol msg reply success");
                pTemplateData->control_msg_receved = false;
            } else {
                Log_e("Contol msg reply failed, err: %d", rc);
            }
        }

        /*report msg to server*/
        /*report the lastest properties's status*/
        if (QCLOUD_RET_SUCCESS == deal_up_stream_user_logic(client, pReportDataList, &ReportCont, &reportTimer)) {
            rc = IOT_Template_JSON_ConstructReportArray(client, data_report_buffer, REPORT_BUFF_LEN, ReportCont,
                                                        pReportDataList);
            if (rc == QCLOUD_RET_SUCCESS) {
                rc = IOT_Template_Report(client, data_report_buffer, REPORT_BUFF_LEN, OnReportReplyCallback, NULL,
                                         QCLOUD_IOT_MQTT_COMMAND_TIMEOUT);
                if (rc == QCLOUD_RET_SUCCESS) {
                    Log_i("data template reporte success");
                } else {
                    Log_e("data template reporte failed, err: %d", rc);
                }
            } else {
                Log_e("construct reporte data failed, err: %d", rc);
            }
        }

#ifdef EVENT_POST_ENABLED
        eventPostCheck(client);
#endif
        HAL_SleepMs(1000);
    }

exit:
    rc = IOT_Template_Destroy_Except_MQTT(client);

    return;
#undef REPORT_BUFF_LEN
}
