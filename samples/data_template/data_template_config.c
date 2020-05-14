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
#include "data_template_config.h"

static void set_property_state(void *pClient, eProperty etype, eDataState state)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

    if (etype < TOTAL_PROPERTY_COUNTS) {
        pTemplateData->property[etype].state = state;
    }
}

static int get_property_state(void *pClient, eProperty etype)
{
    int           ret           = QCLOUD_ERR_FAILURE;
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

    if (etype < TOTAL_PROPERTY_COUNTS) {
        ret = pTemplateData->property[etype].state;
    }

    return ret;
}

static void set_property_value(void *pClient, eProperty etype, void *value)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

    switch (pTemplateData->property[etype].data_property.type) {
        case TYPE_TEMPLATE_BOOL:
            *(TYPE_DEF_TEMPLATE_BOOL *)pTemplateData->property[etype].data_property.data =
                *(TYPE_DEF_TEMPLATE_BOOL *)value;
            set_property_state(pClient, etype, eCHANGED);
            break;

        case TYPE_TEMPLATE_INT:
            *(TYPE_DEF_TEMPLATE_INT *)pTemplateData->property[etype].data_property.data =
                *(TYPE_DEF_TEMPLATE_INT *)value;
            set_property_state(pClient, etype, eCHANGED);
            break;

        case TYPE_TEMPLATE_TIME:
            *(TYPE_DEF_TEMPLATE_TIME *)pTemplateData->property[etype].data_property.data =
                *(TYPE_DEF_TEMPLATE_TIME *)value;
            set_property_state(pClient, etype, eCHANGED);
            break;

        case TYPE_TEMPLATE_FLOAT:
            *(TYPE_DEF_TEMPLATE_FLOAT *)pTemplateData->property[etype].data_property.data =
                *(TYPE_DEF_TEMPLATE_FLOAT *)value;
            set_property_state(pClient, etype, eCHANGED);
            break;

        case TYPE_TEMPLATE_STRING:
            memset(pTemplateData->property[etype].data_property.data, '\0',
                   pTemplateData->property[etype].data_property.data_buff_len);
            strncpy(pTemplateData->property[etype].data_property.data, (char *)value,
                    pTemplateData->property[etype].data_property.data_buff_len);
            set_property_state(pClient, etype, eCHANGED);
            break;

        default:
            Log_e("no property matched");
            break;
    }
}

static void *get_property_value(void *pClient, eProperty etype)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);
    void *        value         = NULL;

    if (etype < TOTAL_PROPERTY_COUNTS) {
        value = pTemplateData->property[etype].data_property.data;
    }

    return value;
}

static sDataPoint *get_property_data_point(void *pClient, eProperty etype)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

    if (etype < TOTAL_PROPERTY_COUNTS) {
        return &pTemplateData->property[etype];
    }

    return NULL;
}

static ProductDataDefine *get_product_para(void *pClient)
{
    DataTemplate *     pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);
    ProductDataDefine *pProductPara  = container_of(pTemplateData->property[ePROPERTY_POWER_SWITCH].data_property.data,
                                                   ProductDataDefine, m_power_switch);

    return pProductPara;
}

/*find property need report*/
static int find_wait_report_property(void *pClient, DeviceProperty *pReportDataList[])
{
    int i, j;

    for (i = 0, j = 0; i < TOTAL_PROPERTY_COUNTS; i++) {
        sDataPoint *pData = get_property_data_point(pClient, i);
        if (eCHANGED == pData->state) {
            pReportDataList[j++] = &(pData->data_property);
            pData->state         = eNOCHANGE;
        }
    }

    return j;
}

#ifdef EVENT_POST_ENABLED
/*get events para, then you can read or modify event's para*/
static EventsPara *get_event_para(void *pClient)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

    EventsPara *pEventsPara = container_of(pTemplateData->events[0].pEventData, EventsPara, evt_para);

    return pEventsPara;
}

static void set_event_timestamp(void *pClient, eEvent event)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

#ifdef EVENT_TIMESTAMP_USED
    pTemplateData->events[event].timestamp = HAL_Timer_current_sec();  // should be UTC and accurate
#else
    pTemplateData->events[event].timestamp   = 0;
#endif
}

static int init_events(sEvent *pEvents)
{
    EventsPara *pEventsPara = NULL;
    int         para_no     = 0;
    int         ret         = QCLOUD_ERR_FAILURE;

    if (TOTAL_EVENT_COUNTS < 1) {
        goto exit;
    }

    pEventsPara = (EventsPara *)HAL_Malloc(sizeof(EventsPara));
    if (NULL == pEventsPara) {
        Log_e("malloc EventsPara fail");
        goto exit;
    }
    memset((char *)pEventsPara, 0, sizeof(EventsPara));

    ADD_EVENT_PARA(status_report, status, TYPE_TEMPLATE_BOOL, pEventsPara, para_no++);
    ADD_EVENT_PARA_STR(status_report, message, TYPE_TEMPLATE_STRING, pEventsPara, para_no++);
    ADD_EVENT(status_report, info, &pEvents[eEVENT_STATUS_REPORT], &pEventsPara->evt_para[0],
              EVENT_STATUS_REPORT_PARA_NUM);

    ADD_EVENT_PARA(low_voltage, voltage, TYPE_TEMPLATE_FLOAT, pEventsPara, para_no++);
    ADD_EVENT(low_voltage, alert, &pEvents[eEVENT_LOW_VOLTAGE], &pEventsPara->evt_para[EVENT_STATUS_REPORT_PARA_NUM],
              EVENT_LOW_VOLTAGE_PARA_NUM);

    ADD_EVENT_PARA_STR(hardware_fault, name, TYPE_TEMPLATE_STRING, pEventsPara, para_no++);
    ADD_EVENT_PARA(hardware_fault, error_code, TYPE_TEMPLATE_INT, pEventsPara, para_no++);
    ADD_EVENT(hardware_fault, fault, &pEvents[eEVENT_HARDWARE_FAULT],
              &pEventsPara->evt_para[EVENT_STATUS_REPORT_PARA_NUM + EVENT_LOW_VOLTAGE_PARA_NUM],
              EVENT_HARDWARE_FAULT_PARA_NUM);

    ret = QCLOUD_RET_SUCCESS;

exit:

    return ret;
}
#endif

#ifdef ACTION_ENABLED
/*get action para, then you can read or modify action's para*/
static ActionsPara *get_action_para(void *pClient)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

    ActionsPara *pActionsPara = container_of(pTemplateData->actions[0].pInput, ActionsPara, action_para);

    return pActionsPara;
}

static void set_action_timestamp(void *pClient, eAction action)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

#ifdef EVENT_TIMESTAMP_USED
    pTemplateData->actions[action].timestamp = HAL_Timer_current_sec();  // should be UTC and accurate
#else
    pTemplateData->actions[action].timestamp = 0;
#endif
}

static int init_actions(DeviceAction *pActions)
{
    ActionsPara *pActionsPara = NULL;
    int          para_no      = 0;
    int          ret          = QCLOUD_ERR_FAILURE;

    if (TOTAL_ACTION_COUNTS < 1) {
        goto exit;
    }

    pActionsPara = (ActionsPara *)HAL_Malloc(sizeof(ActionsPara));
    if (NULL == pActionsPara) {
        Log_e("malloc ActionsPara fail");
        goto exit;
    }
    memset((char *)pActionsPara, 0, sizeof(pActionsPara));

    ADD_ACTION_PARA(blink, period, TYPE_TEMPLATE_INT, pActionsPara, para_no++, input);
    ADD_ACTION_PARA(blink, result, TYPE_TEMPLATE_BOOL, pActionsPara, para_no++, output);
    ADD_ACTION(blink, &pActions[eACTION_BLINK], &pActionsPara->action_para[0], ACTION_BLINK_INPUT_PARA_NUM,
               &pActionsPara->action_para[0 + ACTION_BLINK_INPUT_PARA_NUM], ACTION_BLINK_OUTPUT_PARA_NUM);

    ret = QCLOUD_RET_SUCCESS;

exit:

    return ret;
}

#endif

DataTemplate *init_data_template(void)
{
    ProductDataDefine *pData         = NULL;
    DataTemplate *     pDataTemplate = NULL;

    pData = (ProductDataDefine *)HAL_Malloc(sizeof(ProductDataDefine));
    if (NULL == pData) {
        Log_e("malloc ProductDataDefine fail");
        goto exit;
    }

    pDataTemplate = (DataTemplate *)HAL_Malloc(sizeof(DataTemplate));
    if (NULL == pDataTemplate) {
        Log_e("malloc DataTemplate fail");
        goto exit;
    }

    pData->m_power_switch = 0;
    ADD_PROPERTY_PARA(power_switch, TYPE_TEMPLATE_BOOL, &pDataTemplate->property[ePROPERTY_POWER_SWITCH].data_property,
                      &pData->m_power_switch);
    pData->m_color = 0;
    ADD_PROPERTY_PARA(color, TYPE_TEMPLATE_ENUM, &pDataTemplate->property[ePROPERTY_COLOR].data_property,
                      &pData->m_color);
    pData->m_brightness = 1;
    ADD_PROPERTY_PARA(brightness, TYPE_TEMPLATE_INT, &pDataTemplate->property[ePROPERTY_BRIGHTNESS].data_property,
                      &pData->m_brightness);
    pData->m_name[0] = '\0';
    ADD_PROPERTY_PARA_STR(name, TYPE_TEMPLATE_STRING, &pDataTemplate->property[ePROPERTY_NAME].data_property,
                          pData->m_name, sizeof(pData->m_name) / sizeof(pData->m_name[0]));

    pDataTemplate->method.get_property_state        = get_property_state;
    pDataTemplate->method.set_property_state        = set_property_state;
    pDataTemplate->method.set_property_value        = set_property_value;
    pDataTemplate->method.get_property_value        = get_property_value;
    pDataTemplate->method.get_property_data_point   = get_property_data_point;
    pDataTemplate->method.get_product_para          = get_product_para;
    pDataTemplate->method.find_wait_report_property = find_wait_report_property;

#ifdef EVENT_POST_ENABLED
    if (QCLOUD_RET_SUCCESS != init_events(pDataTemplate->events)) {
        Log_e("inite events fail");
        goto exit;
    }

    pDataTemplate->method.get_event_para      = get_event_para;
    pDataTemplate->method.set_event_timestamp = set_event_timestamp;
#endif

#ifdef ACTION_ENABLED
    if (QCLOUD_RET_SUCCESS != init_actions(pDataTemplate->actions)) {
        Log_e("inite actions fail");
        goto exit;
    }

    pDataTemplate->method.get_action_para      = get_action_para;
    pDataTemplate->method.set_action_timestamp = set_action_timestamp;
#endif

    return pDataTemplate;

exit:

    if (pData) {
        HAL_Free(pData);
    }

    if (pDataTemplate) {
        HAL_Free(pDataTemplate);
    }

    return NULL;
};

void deinit_data_template(void *pClient)
{
    DataTemplate *pTemplateData = (DataTemplate *)IOT_Template_Get_DataTemplate(pClient);

    if (pTemplateData) {
        HAL_Free(get_product_para(pClient));

#ifdef EVENT_POST_ENABLED
        HAL_Free(container_of(pTemplateData->events[0].pEventData, EventsPara, evt_para));
#endif

#ifdef ACTION_ENABLED
        HAL_Free(container_of(pTemplateData->actions[0].pInput, ActionsPara, action_para));
#endif

        HAL_Free(pTemplateData);
    }
}
