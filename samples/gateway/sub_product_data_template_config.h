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
#ifndef _DATA_TEMPLATE_CONFIG_H_
#define _DATA_TEMPLATE_CONFIG_H_

#include "qcloud_iot_import.h"
#include "qcloud_iot_export.h"

#define TOTAL_PROPERTY_COUNTS 4
#define TOTAL_EVENT_COUNTS    3
#define TOTAL_ACTION_COUNTS   1

#ifdef EVENT_POST_ENABLED

#define EVENT_STATUS_REPORT_PARA_NUM  2
#define EVENT_LOW_VOLTAGE_PARA_NUM    1
#define EVENT_HARDWARE_FAULT_PARA_NUM 2

typedef enum {
    eEVENT_STATUS_REPORT,
    eEVENT_LOW_VOLTAGE,
    eEVENT_HARDWARE_FAULT,
} eEvent;

typedef struct _EventsPara {  // m_##event_name##para_name;
    TYPE_DEF_TEMPLATE_BOOL   m_status_report_status;
    TYPE_DEF_TEMPLATE_STRING m_status_report_message[64 + 1];
    TYPE_DEF_TEMPLATE_FLOAT  m_low_voltage_voltage;
    TYPE_DEF_TEMPLATE_STRING m_hardware_fault_name[64 + 1];
    TYPE_DEF_TEMPLATE_INT    m_hardware_fault_error_code;
    DeviceProperty evt_para[EVENT_STATUS_REPORT_PARA_NUM + EVENT_LOW_VOLTAGE_PARA_NUM + EVENT_HARDWARE_FAULT_PARA_NUM];
} EventsPara;

#define ADD_EVENT_PARA(event_name, para_name, para_type, pData, para_no) \
    do {                                                                 \
        EventsPara *    pParas = (EventsPara *)pData;                    \
        DeviceProperty *data   = &pParas->evt_para[para_no];             \
        data->key              = #para_name;                             \
        data->data             = &pParas->m_##event_name##_##para_name;  \
        data->type             = para_type;                              \
    } while (0)

#define ADD_EVENT_PARA_STR(event_name, para_name, para_type, pData, para_no) \
    do {                                                                     \
        EventsPara *    pParas = (EventsPara *)pData;                        \
        DeviceProperty *data   = &pParas->evt_para[para_no];                 \
        data->key              = #para_name;                                 \
        data->data             = pParas->m_##event_name##_##para_name;       \
        data->type             = para_type;                                  \
    } while (0)

#define ADD_EVENT(_event_name_, event_type, pEvent, pData, para_num) \
    do {                                                             \
        sEvent *event       = (sEvent *)pEvent;                      \
        event->event_name   = #_event_name_;                         \
        event->type         = #event_type;                           \
        event->timestamp    = 0;                                     \
        event->eventDataNum = para_num;                              \
        event->pEventData   = pData;                                 \
    } while (0)

#endif

#ifdef ACTION_ENABLED
#define ACTION_BLINK_INPUT_PARA_NUM  1
#define ACTION_BLINK_OUTPUT_PARA_NUM 1

typedef enum {
    eACTION_BLINK,
} eAction;

typedef struct _ActionsPara {
    TYPE_DEF_TEMPLATE_INT  m_blink_input_period;
    TYPE_DEF_TEMPLATE_BOOL m_blink_output_result;

    DeviceProperty action_para[ACTION_BLINK_INPUT_PARA_NUM + ACTION_BLINK_OUTPUT_PARA_NUM];
} ActionsPara;

#define ADD_ACTION_PARA(action_name, para_name, para_type, pData, para_no, para_dir)  \
    do {                                                                              \
        ActionsPara *   pParas = (ActionsPara *)pData;                                \
        DeviceProperty *data   = &pParas->action_para[para_no];                       \
        data->key              = #para_name;                                          \
        data->data             = &pParas->m_##action_name##_##para_dir##_##para_name; \
        data->type             = para_type;                                           \
    } while (0)

#define ADD_ACTION_PARA_STR(action_name, para_name, para_type, pData, para_no, para_dir) \
    do {                                                                                 \
        ActionsPara *   pParas = (ActionsPara *)pData;                                   \
        DeviceProperty *data   = &pParas->action_para[para_no];                          \
        data->key              = #para_name;                                             \
        data->data             = pParas->m_##action_name##_##para_dir##_##para_name;     \
        data->type             = para_type;                                              \
    } while (0)

#define ADD_ACTION(action_name, pAction, pInputData, input_para_num, pOutputData, output_para_num) \
    do {                                                                                           \
        DeviceAction *action = (DeviceAction *)pAction;                                            \
        action->pActionId    = #action_name;                                                       \
        action->timestamp    = 0;                                                                  \
        action->input_num    = input_para_num;                                                     \
        action->output_num   = output_para_num;                                                    \
        action->pInput       = pInputData;                                                         \
        action->pOutput      = pOutputData;                                                        \
    } while (0)
#endif

#define ADD_PROPERTY_PARA(propery_name, propery_type, pPropery, para_data) \
    do {                                                                   \
        DeviceProperty *property = (DeviceProperty *)pPropery;             \
        property->key            = #propery_name;                          \
        property->data           = para_data;                              \
        property->type           = propery_type;                           \
    } while (0)

#define ADD_PROPERTY_PARA_STR(propery_name, propery_type, pPropery, para_data, para_len) \
    do {                                                                                 \
        DeviceProperty *property = (DeviceProperty *)pPropery;                           \
        property->key            = #propery_name;                                        \
        property->data           = para_data;                                            \
        property->data_buff_len  = para_len;                                             \
        property->type           = propery_type;                                         \
    } while (0)

typedef enum {
    ePROPERTY_POWER_SWITCH,
    ePROPERTY_COLOR,
    ePROPERTY_BRIGHTNESS,
    ePROPERTY_NAME,
} eProperty;

typedef struct _ProductDataDefine {  // m_##para_name;
    TYPE_DEF_TEMPLATE_BOOL   m_power_switch;
    TYPE_DEF_TEMPLATE_ENUM   m_color;
    TYPE_DEF_TEMPLATE_INT    m_brightness;
    TYPE_DEF_TEMPLATE_STRING m_name[64 + 1];
} ProductDataDefine;

typedef struct _TemplateOps_ {
    /*property operation*/
    void (*set_property_state)(void *pClient, eProperty etype, eDataState state);
    int (*get_property_state)(void *pClient, eProperty etype);
    void (*set_property_value)(void *pClient, eProperty etype, void *value);
    void *(*get_property_value)(void *pClient, eProperty etype);
    sDataPoint *(*get_property_data_point)(void *pClient, eProperty etype);
    ProductDataDefine *(*get_product_para)(void *pClient);
    int (*find_wait_report_property)(void *pClient, DeviceProperty *pReportDataList[]);

#ifdef EVENT_POST_ENABLED
    /*event operation*/
    EventsPara *(*get_event_para)(void *pClient);
    void (*set_event_timestamp)(void *pClient, eEvent event);
#endif

#ifdef ACTION_ENABLED
    /*action operation*/
    ActionsPara *(*get_action_para)(void *pClient);
    void (*set_action_timestamp)(void *pClient, eAction action);
#endif
} TemplateOps;

typedef struct _DataTemplate_ {
    /*template data*/
    sDataPoint property[TOTAL_PROPERTY_COUNTS];
#ifdef EVENT_POST_ENABLED
    sEvent events[TOTAL_EVENT_COUNTS];
#endif
#ifdef ACTION_ENABLED
    DeviceAction actions[TOTAL_ACTION_COUNTS];
#endif

    /*template method*/
    TemplateOps method;

    /*control msg flag,for multi sub dev share the same product data_template*/
    bool control_msg_receved;
} DataTemplate;

#ifndef container_of
#define container_of(ptr, type, member) ((type *)((char *)(ptr)-offsetof(type, member)))
#endif

DataTemplate *init_sub_product_data_template(void);
void          deinit_sub_product_data_template(void *pClient);

#endif
