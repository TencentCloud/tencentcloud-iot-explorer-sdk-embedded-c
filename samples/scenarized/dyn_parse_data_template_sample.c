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
#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"
#include "utils_getopt.h"
#include "lite-utils.h"
#include "json_parser.h"
#include "utils_timer.h"

#define MAX_SIZE_OF_JSON_SCHEMA_FILE_PATH 128
static char sg_json_schema_file[MAX_SIZE_OF_JSON_SCHEMA_FILE_PATH + 1] = {"example_schema.json"};

#define DYN_DATA_TEMPLATE_TEST 1

#define DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN 512

#define DATA_TEMPLATE_JSON_KEY_OBJECT_PRODUCT_ID "profile.ProductId"
#define DATA_TEMPLATE_JSON_KEY_ARRAY_PROPERTYS   "properties"
#define DATA_TEMPLATE_JSON_KEY_ARRAY_EVENTS      "events"
#define DATA_TEMPLATE_JSON_KEY_ARRAY_ACTIONS     "actions"

/* json level 0 key */
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ID           "id"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_TYPE         "define.type"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAX          "define.max"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MIN          "define.min"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAPPING      "define.mapping"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_SPECS "define.specs"
/* json level 0 array key */
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_ARRAYINFO    "define.arrayInfo"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_TYPE         "type"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_MAX          "max"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_MIN          "min"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_MAPPING      "mapping"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_STRUCT_SPECS "specs"
/* json level x key */
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_TYPE      "dataType.type"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAX       "dataType.max"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MIN       "dataType.min"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAPPING   "dataType.mapping"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_ARRAYINFO "dataType.arrayInfo"

typedef struct {
    const char * string_type;
    JsonDataType type;
    int          size;
    const char * format;
} DataTemplatePropertyType;

DataTemplatePropertyType IOT_Template_GetDataTemplatePropertyType(const char *string_type, int type)
{
    DataTemplatePropertyType temp;
    memset(&temp, 0, sizeof(temp));

    DataTemplatePropertyType template_types[] = {
        {"int", TYPE_TEMPLATE_INT, sizeof(int32_t), "%" SCNi32},
        {"enum", TYPE_TEMPLATE_ENUM, sizeof(int32_t), "%" SCNi32},
        {"float", TYPE_TEMPLATE_FLOAT, sizeof(float), "%f"},
        {"bool", TYPE_TEMPLATE_BOOL, sizeof(int8_t), "%hhi"},
        {"string", TYPE_TEMPLATE_STRING, sizeof(char), "%s"},
        {"timestamp", TYPE_TEMPLATE_TIME, sizeof(uint32_t), "%" SCNu32},
        {"struct", TYPE_TEMPLATE_JOBJECT, sizeof(char), ""},
        {"stringenum", TYPE_TEMPLATE_STRING, sizeof(char), "%s"},
        {"array", TYPE_TEMPLATE_ARRAY, sizeof(char), ""},
    };

    for (int i = 0; i < sizeof(template_types) / sizeof(DataTemplatePropertyType); i++) {
        if (string_type && strstr(template_types[i].string_type, string_type)) {
            return template_types[i];
        } else if (!string_type && type != -1 && template_types[i].type == type) {
            return template_types[i];
        }
    }

    return temp;
}

void IOT_Template_FreeDevicePropertyArray(_IN_ void **property_struct_array, _IN_ int *property_struct_array_length,
                                          _IN_ int array_element_size, _IN_ int device_property_in_array_element_pos)
{
    int i = 0;
    while (i < *property_struct_array_length) {
        DeviceProperty *property = (DeviceProperty *)(((char *)(*property_struct_array) + i * array_element_size) +
                                                      device_property_in_array_element_pos);

        if (property->type == TYPE_TEMPLATE_JOBJECT) {
            IOT_Template_FreeDevicePropertyArray(property->data, (int *)&property->struct_obj_num, array_element_size,
                                                 device_property_in_array_element_pos);
            property->data           = NULL;
            property->struct_obj_num = 0;
        } else {
            HAL_Free(property->data);
            property->data = NULL;
        }
        HAL_Free(property->key);
        property->key = NULL;

        i++;
    }

    HAL_Free(*property_struct_array);
    *property_struct_array        = NULL;
    *property_struct_array_length = 0;
}

/* parse json */
int IOT_Template_ParseDevicePropertyArray_FromJson(_IN_ char *propertys_json, _IN_ int array_element_size,
                                                   _IN_ int device_property_in_array_element_pos,
                                                   _IN_ DeviceProperty *parent_property,
                                                   _OU_ void **         property_struct_array,
                                                   _OU_ int *           property_struct_array_length);

int IOT_Template_ParseArrayProperty_FromJson(_IN_ char *property_json, _IN_ DeviceProperty *array_property)
{
    /* get json type */
    char *string_type = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_TYPE, property_json);
    if (!string_type) {
        Log_e("json buf no \"type\" key, %s", property_json);
        goto error;
    }
    /* get type by json type */
    DataTemplatePropertyType property_type = IOT_Template_GetDataTemplatePropertyType(string_type, -1);
    if (!property_type.string_type) {
        Log_e("get type by %s json type error", string_type);
        HAL_Free(string_type);
        goto error;
    }
    /* free */
    HAL_Free(string_type);

    /* array type */
    array_property->array_info.array_type = property_type.type;

    /* array don't parse jobject, array string set json value by user*/
    /* array data is json value */
    /* data need free */
    array_property->data = HAL_Malloc(array_property->array_info.array_size + 1);
    if (!array_property->data) {
        goto error;
    }
    return QCLOUD_RET_SUCCESS;

error:
    return QCLOUD_ERR_FAILURE;
}

int IOT_Template_ParseDevicePropertyArray_FromJson(_IN_ char *propertys_json, _IN_ int array_element_size,
                                                   _IN_ int device_property_in_array_element_pos,
                                                   _IN_ DeviceProperty *parent_property,
                                                   _OU_ void **         property_struct_array,
                                                   _OU_ int *           property_struct_array_length)
{
    if (!propertys_json) {
        *property_struct_array_length = 0;
        *property_struct_array        = NULL;
        return QCLOUD_ERR_FAILURE;
    }
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    /* first get array num */
    json_array_for_each_entry(propertys_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        (*property_struct_array_length)++;
    }

    /* malloc; need free array */
    *property_struct_array = (DeviceProperty *)HAL_Malloc(*property_struct_array_length * array_element_size);
    if (!(*property_struct_array)) {
        goto error;
    }
    /* clear zero */
    memset(*property_struct_array, 0, *property_struct_array_length * array_element_size);

    int i = 0;
    /* parse */
    json_array_for_each_entry(propertys_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);

        /* get DeviceProperty struct */
        DeviceProperty *property = (DeviceProperty *)(((char *)(*property_struct_array) + i * array_element_size) +
                                                      device_property_in_array_element_pos);

        char *id = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ID, entry);
        if (!id) {
            goto error;
        }

        /* need free */
        property->key = id;

        /* get type */
        char *property_type_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_TYPE;
        if (parent_property) {
            property_type_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_TYPE;
        }
        /* need free; goto free */
        char *type = LITE_json_value_of(property_type_key, entry);
        if (!type) {
            goto error;
        }
        DataTemplatePropertyType property_type = IOT_Template_GetDataTemplatePropertyType(type, -1);
        if (!property_type.string_type) {
            HAL_Free(type);
            goto error;
        }

        property->type = property_type.type;
        /* basic type size */
        property->data_buff_len = property_type.size;

        /* get string size */
        if (property->type == TYPE_TEMPLATE_STRING && strcmp(type, "string") == 0) {
            char *property_max_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAX;
            if (parent_property) {
                property_max_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAX;
            }
            /* need free */
            char *max_key = LITE_json_value_of(property_max_key, entry);
            if (!max_key) {
                HAL_Free(type);
                goto error;
            }
            /* string max size */
            property->data_buff_len = atoi(max_key) + 1;
            HAL_Free(max_key);
        } else if (property->type == TYPE_TEMPLATE_STRING && strcmp(type, "stringenum") == 0) {
            char *property_mapping_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAPPING;
            if (parent_property) {
                property_mapping_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAPPING;
            }
            /* need free */
            char *mapping_key = LITE_json_value_of(property_mapping_key, entry);
            if (!mapping_key) {
                HAL_Free(type);
                goto error;
            }
            char *pos;
            char *key;
            int   klen;
            char *val;
            int   vlen;
            int   vtype;
            int   max_len = 0;
            /* get json key max_len, key is string enum */
            json_object_for_each_kv(mapping_key, pos, key, klen, val, vlen, vtype)
            {
                max_len = klen > max_len ? klen : max_len;
            }

            if (max_len == 0) {
                Log_e("stringenum is error, no enum");
            }
            /* stringenum max size */
            property->data_buff_len = max_len + 1;

            HAL_Free(mapping_key);
        }

        if (property->type == TYPE_TEMPLATE_JOBJECT) {
            /* struct */
            /* cycle parse */
            /* need free */
            char *struct_json = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_SPECS, entry);
            if (!struct_json) {
                HAL_Free(type);
                goto error;
            }

            int struct_count = 0;
            /* cycle parese */
            if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson(
                                          struct_json, array_element_size, device_property_in_array_element_pos,
                                          parent_property == NULL ? property : parent_property, &property->data,
                                          &struct_count)) {
                HAL_Free(type);
                HAL_Free(struct_json);
                goto error;
            }
            /* set struct property count */
            property->struct_obj_num = struct_count;

            HAL_Free(struct_json);
        } else if (property->type == TYPE_TEMPLATE_ARRAY) {
            /* array */
            /* cycle parse */
            property->array_info.array_size = DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN;
            char *property_array_info_key   = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_ARRAYINFO;
            if (parent_property) {
                property_array_info_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_ARRAYINFO;
            }
            /* need free */
            char *array_json = LITE_json_value_of(property_array_info_key, entry);
            if (!array_json) {
                goto error;
            }

            if (QCLOUD_RET_SUCCESS != IOT_Template_ParseArrayProperty_FromJson(array_json, property)) {
                HAL_Free(array_json);
                HAL_Free(type);
                goto error;
            }

            HAL_Free(array_json);
        } else {
            /* basic type malloc data buff , array and object don't malloc */
            /* need free */
            property->data = HAL_Malloc(property->data_buff_len);
        }

        /* type free */
        HAL_Free(type);

        restore_json_str_last_char(entry, entry_len, old_ch);
        i++;
    }

    return QCLOUD_RET_SUCCESS;

error:
    /* cycle free propertys_struct */
    IOT_Template_FreeDevicePropertyArray(property_struct_array, property_struct_array_length, array_element_size,
                                         device_property_in_array_element_pos);
    HAL_Free(*property_struct_array);
    *property_struct_array        = NULL;
    *property_struct_array_length = 0;
    return QCLOUD_ERR_FAILURE;
}

static int         sg_property_array_count         = 0;
static sDataPoint *sg_data_template_property_array = NULL;

void IOT_Template_FreePropertyArray()
{
    int device_property_in_sDataPoint_pos = (char *)(&(((sDataPoint *)0)->data_property)) - (char *)0;

    IOT_Template_FreeDevicePropertyArray((void *)&sg_data_template_property_array, &sg_property_array_count,
                                         sizeof(sDataPoint), device_property_in_sDataPoint_pos);
    HAL_Free(sg_data_template_property_array);
    sg_data_template_property_array = NULL;
    sg_property_array_count         = 0;
}

int IOT_Template_ParsePropertyArrayFromJson(char *propertys_json)
{
    int device_property_in_sDataPoint_pos = (char *)(&(((sDataPoint *)0)->data_property)) - (char *)0;
    if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson(
                                  propertys_json, sizeof(sDataPoint), device_property_in_sDataPoint_pos, NULL,
                                  (void *)&sg_data_template_property_array, &sg_property_array_count)) {
        sg_data_template_property_array = NULL;
        sg_property_array_count         = 0;
        return QCLOUD_ERR_FAILURE;
    }

    return QCLOUD_RET_SUCCESS;
}

static sDataPoint *_find_property_in_array(sDataPoint *propertys_array, int propertys_count, char *key, int key_len)
{
    int i = 0;
    while (i < propertys_count) {
        sDataPoint *data_point = propertys_array + i;

        if (!strncmp(data_point->data_property.key, key, key_len)) {
            return data_point;
        }

        i++;
    }

    return NULL;
}

int static _set_get_property_data(char *level_0_key, char *level_1_key, void *data, int data_len, int set)
{
    sDataPoint *level_0_property = NULL;
    sDataPoint *level_1_property = NULL;

    sDataPoint *property_arrays      = sg_data_template_property_array;
    int         property_array_count = sg_property_array_count;

    if (!level_0_key) {
        return QCLOUD_ERR_FAILURE;
    }

    /* find level 0 */
    level_0_property = _find_property_in_array(property_arrays, property_array_count, level_0_key, strlen(level_0_key));

    if (!level_0_property) {
        return QCLOUD_ERR_FAILURE;
    }

    if (level_0_property->data_property.type == TYPE_TEMPLATE_JOBJECT) {
        property_arrays      = (sDataPoint *)level_0_property->data_property.data;
        property_array_count = level_0_property->data_property.struct_obj_num;
    }

    if (level_1_key && (level_0_property->data_property.type == TYPE_TEMPLATE_JOBJECT)) {
        level_1_property =
            _find_property_in_array(property_arrays, property_array_count, level_1_key, strlen(level_1_key));
    } else {
        level_1_property = level_0_property;
    }

    /* set value */
    if (level_1_property) {
        if (set) {
            if (data_len > level_1_property->data_property.data_buff_len) {
                return QCLOUD_ERR_FAILURE;
            }

            memset(level_1_property->data_property.data, 0, level_1_property->data_property.data_buff_len);
            memcpy(level_1_property->data_property.data, data, data_len);

            level_1_property->state = eCHANGED;
            level_0_property->state = eCHANGED;

            return QCLOUD_RET_SUCCESS;
        } else {
            int copy_len = data_len > level_1_property->data_property.data_buff_len
                               ? level_1_property->data_property.data_buff_len
                               : data_len;
            memcpy(data, level_1_property->data_property.data, copy_len);
            return copy_len;
        }
    }

    return QCLOUD_ERR_FAILURE;
}

int IOT_Template_SetPropertyValue(char *key, void *data, int data_len)
{
    return _set_get_property_data(key, NULL, data, data_len, 1);
}

int IOT_Template_SetStructPropertyValue(char *struct_key, char *member_key, void *member_data, int data_len)
{
    return _set_get_property_data(struct_key, member_key, member_data, data_len, 1);
}

int IOT_Template_GetPropertyValue(char *key, void *data, int data_len)
{
    return _set_get_property_data(key, NULL, data, data_len, 0);
}

int IOT_Template_GetStructPropertyValue(char *struct_key, char *member_key, void *member_data, int data_len)
{
    return _set_get_property_data(struct_key, member_key, member_data, data_len, 0);
}

#ifdef ACTION_ENABLED
static int           sg_action_array_count         = 0;
static DeviceAction *sg_data_template_action_array = NULL;

#define DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_ID           "id"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_INPUT_ARRAY  "input"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_OUTPUT_ARRAY "output"

void IOT_Template_FreeActionArray()
{
    int i = 0;
    while (i < sg_action_array_count) {
        DeviceAction *action = sg_data_template_action_array + i;
        IOT_Template_FreeDevicePropertyArray((void *)&action->pInput, (int *)&action->input_num, sizeof(DeviceProperty),
                                             0);
        IOT_Template_FreeDevicePropertyArray((void *)&action->pOutput, (int *)&action->output_num,
                                             sizeof(DeviceProperty), 0);
        HAL_Free(action->pActionId);
    }
    HAL_Free(sg_data_template_action_array);
    sg_data_template_action_array = NULL;
    sg_action_array_count         = 0;
}

int IOT_Template_ParseActionArrayFromJson(char *actions_json)
{
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    json_array_for_each_entry(actions_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        sg_action_array_count++;
    }

    /* need free */
    sg_data_template_action_array = (DeviceAction *)HAL_Malloc(sg_action_array_count * sizeof(DeviceAction));
    if (!sg_data_template_action_array) {
        goto error;
    }
    /* zero clear */
    memset(sg_data_template_action_array, 0, sg_action_array_count * sizeof(DeviceAction));

    int i = 0;
    json_array_for_each_entry(actions_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);

        DeviceAction *data_template_action = &(sg_data_template_action_array[i]);
        char *        id                   = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_ID, entry);
        if (!id) {
            goto error;
        }

        /* need free */
        data_template_action->pActionId = id;

        /* need free */
        char *input_array = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_INPUT_ARRAY, entry);
        if (!input_array) {
            goto error;
        }
        int array_length = 0;
        if (QCLOUD_RET_SUCCESS !=
            IOT_Template_ParseDevicePropertyArray_FromJson(input_array, sizeof(DeviceProperty), 0, NULL,
                                                           (void *)&(data_template_action->pInput), &array_length)) {
            HAL_Free(input_array);
            goto error;
        }

        data_template_action->input_num = array_length;
        HAL_Free(input_array);

        /* need free */
        char *output_array = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_OUTPUT_ARRAY, entry);
        if (!output_array) {
            goto error;
        }
        array_length = 0;
        if (QCLOUD_RET_SUCCESS !=
            IOT_Template_ParseDevicePropertyArray_FromJson(output_array, sizeof(DeviceProperty), 0, NULL,
                                                           (void *)&(data_template_action->pOutput), &array_length)) {
            HAL_Free(output_array);
            goto error;
        }
        data_template_action->output_num = array_length;

        HAL_Free(output_array);

        restore_json_str_last_char(entry, entry_len, old_ch);
        i++;
    }

    return QCLOUD_RET_SUCCESS;

error:
    IOT_Template_FreeActionArray();
    return QCLOUD_ERR_FAILURE;
}

int IOT_Template_SetActionOutputValue(char *action_id, char *action_output_property_key, void *property_value,
                                      int value_len)
{
    DeviceAction *  action   = NULL;
    DeviceProperty *property = NULL;

    if (!action_id) {
        return QCLOUD_ERR_FAILURE;
    }

    int action_index = 0;
    while (action_index < sg_action_array_count) {
        action = &(sg_data_template_action_array[action_index]);
        /* find event */
        if (!strcmp(action->pActionId, action_id)) {
            break;
        }
        action = NULL;
        action_index++;
    }

    if (!action) {
        return QCLOUD_ERR_FAILURE;
    }

    int property_index = 0;
    while (property_index < action->output_num) {
        property = action->pOutput + property_index;
        if (!strcmp(property->key, action_output_property_key)) {
            break;
        }
        property = NULL;
        property_index++;
    }
    if (!property) {
        return QCLOUD_ERR_FAILURE;
    }

    if (value_len > property->data_buff_len) {
        return QCLOUD_ERR_FAILURE;
    }

    /* clear old value */
    memset(property->data, 0, property->data_buff_len);
    memcpy(property->data, property_value, value_len);

    return QCLOUD_RET_SUCCESS;
}

int IOT_Template_GetActionInputValue(char *action_id, char *action_input_property_key, void *property_value_buf,
                                     int value_buf_len)
{
    DeviceAction *  action   = NULL;
    DeviceProperty *property = NULL;

    if (!action_id) {
        return QCLOUD_ERR_FAILURE;
    }

    int action_index = 0;
    while (action_index < sg_action_array_count) {
        action = &(sg_data_template_action_array[action_index]);
        /* find event */
        if (!strcmp(action->pActionId, action_id)) {
            break;
        }
        action = NULL;
        action_index++;
    }

    if (!action) {
        return QCLOUD_ERR_FAILURE;
    }

    int property_index = 0;
    while (property_index < action->input_num) {
        property = action->pInput + property_index;
        if (!strcmp(property->key, action_input_property_key)) {
            break;
        }
        property = NULL;
        property_index++;
    }
    if (!property) {
        return QCLOUD_ERR_FAILURE;
    }

    int copy_len = value_buf_len > property->data_buff_len ? property->data_buff_len : value_buf_len;
    memcpy(property_value_buf, property->data, copy_len);

    return QCLOUD_RET_SUCCESS;
}
#endif

#ifdef EVENT_POST_ENABLED
static int     sg_event_array_count         = 0;
static sEvent *sg_data_template_event_array = NULL;

#define DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_ID          "id"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_TYPE        "type"
#define DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_PARAM_ARRAY "params"

void IOT_Template_FreeEventArray()
{
    int i = 0;
    while (i < sg_event_array_count) {
        sEvent *event = sg_data_template_event_array + i;
        IOT_Template_FreeDevicePropertyArray((void *)&event->pEventData, (int *)&event->eventDataNum,
                                             sizeof(DeviceProperty), 0);
        HAL_Free(event->event_name);
        HAL_Free(event->type);
    }
    HAL_Free(sg_data_template_event_array);
    sg_data_template_event_array = NULL;
    sg_event_array_count         = 0;
}

int IOT_Template_ParseEventArrayFromJson(char *events_json)
{
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    /* get array num */
    json_array_for_each_entry(events_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        sg_event_array_count++;
    }

    /* need free */
    sg_data_template_event_array = (sEvent *)HAL_Malloc(sg_event_array_count * sizeof(sEvent));
    if (!sg_data_template_event_array) {
        goto error;
    }
    /* zero clear */
    memset(sg_data_template_event_array, 0, sg_event_array_count * sizeof(sEvent));

    int i = 0;
    json_array_for_each_entry(events_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);

        sEvent *event = &(sg_data_template_event_array[i]);

        char *id = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_ID, entry);
        if (!id) {
            goto error;
        }
        /* need free */
        event->event_name = id;

        char *type = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_TYPE, entry);
        if (!type) {
            goto error;
        }
        /* need free */
        event->type = type;

        /* need free */
        char *params = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_PARAM_ARRAY, entry);
        if (!params) {
            goto error;
        }

        int array_length = 0;
        if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson(params, sizeof(DeviceProperty), 0,
                                                                                 NULL, (void *)&(event->pEventData),
                                                                                 &array_length)) {
            HAL_Free(params);
            goto error;
        }
        event->eventDataNum = array_length;

        HAL_Free(params);

        restore_json_str_last_char(entry, entry_len, old_ch);
        i++;
    }

    return QCLOUD_RET_SUCCESS;

error:
    IOT_Template_FreeEventArray();
    return QCLOUD_ERR_FAILURE;
}

/* return event flag >= 0 */
int IOT_Template_SetEventValue(char *event_name, char *event_property_key, void *_property_value, int value_len)
{
    sEvent *        event    = NULL;
    DeviceProperty *property = NULL;

    if (!event_name) {
        return QCLOUD_ERR_FAILURE;
    }

    int event_index = 0;
    while (event_index < sg_event_array_count) {
        event = &(sg_data_template_event_array[event_index]);
        /* find event */
        if (!strcmp(event->event_name, event_name)) {
            break;
        }
        event = NULL;
        event_index++;
    }

    if (!event) {
        return QCLOUD_ERR_FAILURE;
    }

    int property_index = 0;
    while (property_index < event->eventDataNum) {
        property = event->pEventData + property_index;
        if (!strcmp(property->key, event_property_key)) {
            break;
        }
        property = NULL;
        property_index++;
    }
    if (!property) {
        return QCLOUD_ERR_FAILURE;
    }

    if (value_len > property->data_buff_len) {
        return QCLOUD_ERR_FAILURE;
    }

    /* clear old value */
    memset(property->data, 0, property->data_buff_len);
    memcpy(property->data, _property_value, value_len);

    return event_index;
}

#endif

void IOT_Template_FreeParse()
{
    IOT_Template_FreePropertyArray();

#ifdef EVENT_POST_ENABLED
    IOT_Template_FreeEventArray();
#endif

#ifdef ACTION_ENABLED
    IOT_Template_FreeActionArray();
#endif
}

int IOT_Template_Parse_FromJson(char *json)
{
    char *events    = NULL;
    char *actions   = NULL;
    char *propertys = NULL;

    propertys = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_ARRAY_PROPERTYS, json);
    if (!propertys) {
        goto error;
    }
    IOT_Template_ParsePropertyArrayFromJson(propertys);
    HAL_Free(propertys);

#ifdef ACTION_ENABLED
    actions = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_ARRAY_ACTIONS, json);
    if (actions) {
        IOT_Template_ParseActionArrayFromJson(actions);
        HAL_Free(actions);
    }

#endif

#ifdef EVENT_POST_ENABLED

    events = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_ARRAY_EVENTS, json);
    if (events) {
        IOT_Template_ParseEventArrayFromJson(events);
        HAL_Free(events);
    }

#endif
    return QCLOUD_RET_SUCCESS;

error:
    HAL_Free(propertys);
    HAL_Free(actions);
    HAL_Free(events);

    return QCLOUD_ERR_FAILURE;
}

int IOT_Template_Parse_FromJsonFile(const char *data_template_json_file_name)
{
    FILE *   fp;
    uint32_t len;
    uint32_t rlen;
    char *   JsonDoc = NULL;
    int      ret     = QCLOUD_RET_SUCCESS;

    fp = fopen(data_template_json_file_name, "r");
    if (NULL == fp) {
        Log_e("open json schema file \"%s\" failed", STRING_PTR_PRINT_SANITY_CHECK(data_template_json_file_name));
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    fseek(fp, 0L, SEEK_END);
    len = ftell(fp);
    if (len > 30 * 1024) {
        Log_e("device info file \"%s\" is too big!", STRING_PTR_PRINT_SANITY_CHECK(data_template_json_file_name));
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    JsonDoc = (char *)HAL_Malloc(len + 10);
    memset(JsonDoc, 0, len + 10);
    if (NULL == JsonDoc) {
        Log_e("malloc buffer for json file read fail");
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    rewind(fp);
    rlen = fread(JsonDoc, 1, len, fp);

    if (len != rlen) {
        Log_e("read data len (%d) less than needed (%d), %s", rlen, len, JsonDoc);
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    IOT_Template_Parse_FromJson(JsonDoc);

exit:

    HAL_Free(JsonDoc);

    if (fp) {
        fclose(fp);
    }
    return ret;
}

#ifdef DYN_DATA_TEMPLATE_TEST

typedef struct {
    char *key;   // Key of this JSON node
    void *data;  // Value of this JSON node
    union {
        uint16_t data_buff_len;   // data buff len, for string type value update
        uint16_t struct_obj_num;  // member number of struct
        struct {
            uint16_t     array_size;
            JsonDataType array_type;
        } array_info;
    };
    union {
        struct {
            int32_t min;
            int32_t max;
            int32_t step;
            int32_t start;
        } int32_limit;

        struct {
            float min;
            float max;
            float step;
            float start;
        } float_limit;

        struct {
            int32_t enum_count;
        } enum_limit;
    };

    JsonDataType type;  // Data type of this JSON node
} DevicePropertyTest;

typedef struct {
    DevicePropertyTest data_property;
    eDataState         state;
} sDataPointTest;

typedef struct {
    char *              pActionId;   // action id
    uint32_t            timestamp;   // action timestamp
    uint8_t             input_num;   // input num
    uint8_t             output_num;  // output mun
    DevicePropertyTest *pInput;      // input
    DevicePropertyTest *pOutput;     // output
} DeviceActionTest;

typedef struct {
    char *              event_name;    // event name
    char *              type;          // event type
    uint32_t            timestamp;     // event timestamp
    uint8_t             eventDataNum;  // number of event properties
    DevicePropertyTest *pEventData;    // event properties
} sEventTest;

void IOT_Template_FreeDevicePropertyArray_Test(_IN_ void **property_struct_array,
                                               _IN_ int *property_struct_array_length, _IN_ int array_element_size,
                                               _IN_ int device_property_in_array_element_pos)
{
    int i = 0;
    while (i < *property_struct_array_length) {
        DevicePropertyTest *property =
            (DevicePropertyTest *)(((char *)(*property_struct_array) + i * array_element_size) +
                                   device_property_in_array_element_pos);

        if (property->type == TYPE_TEMPLATE_JOBJECT) {
            IOT_Template_FreeDevicePropertyArray_Test(property->data, (int *)&property->struct_obj_num,
                                                      array_element_size, device_property_in_array_element_pos);
            property->data           = NULL;
            property->struct_obj_num = 0;
        } else if (property->type == TYPE_TEMPLATE_ARRAY && property->array_info.array_type == TYPE_TEMPLATE_JOBJECT) {
            IOT_Template_FreeDevicePropertyArray_Test(property->data, (int *)&property->array_info.array_size,
                                                      array_element_size, device_property_in_array_element_pos);
            property->data                  = NULL;
            property->array_info.array_size = 0;
        } else {
            HAL_Free(property->data);
            property->data = NULL;
        }
        HAL_Free(property->key);
        property->key = NULL;

        i++;
    }

    HAL_Free(property_struct_array);
    *property_struct_array        = NULL;
    *property_struct_array_length = 0;
}

int IOT_Template_ParseDevicePropertyArray_FromJson_Test(_IN_ char *propertys_json, _IN_ int array_element_size,
                                                        _IN_ int device_property_in_array_element_pos,
                                                        _IN_ DevicePropertyTest *parent_property,
                                                        _OU_ void **             property_struct_array,
                                                        _OU_ int *               property_struct_array_length);

static int _property_type_info_parse_test(_IN_ char *property_json, _IN_ DevicePropertyTest *parent_property,
                                          _IN_ char *type, _IN_ DevicePropertyTest *property)
{
    JsonDataType data_type = property->type;
    if (data_type == TYPE_TEMPLATE_ARRAY) {
        data_type = property->array_info.array_type;
    }

    DataTemplatePropertyType property_type = IOT_Template_GetDataTemplatePropertyType(type, -1);
    if (!property_type.string_type) {
        return QCLOUD_ERR_FAILURE;
    }

    if (strcmp(type, "string") == 0 || strcmp(type, "float") == 0 || strcmp(type, "int") == 0) {
        char *property_max_key = "";
        if (!parent_property) {
            property_max_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAX;
        } else if (parent_property->type == TYPE_TEMPLATE_JOBJECT ||
                   (parent_property->type == TYPE_TEMPLATE_ARRAY &&
                    parent_property->array_info.array_type == TYPE_TEMPLATE_JOBJECT)) {
            property_max_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAX;
        } else if (parent_property->type == TYPE_TEMPLATE_ARRAY) {
            property_max_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_MAX;
        }

        char *max_key = LITE_json_value_of(property_max_key, property_json);
        if (!max_key) {
            return QCLOUD_ERR_FAILURE;
        }
        if (data_type == TYPE_TEMPLATE_STRING) {
            property->data_buff_len = atoi(max_key) + 1;
            HAL_Free(max_key);
            return QCLOUD_RET_SUCCESS;
        }

        char *property_min_key = "";
        if (!parent_property) {
            property_min_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MIN;
        } else if (parent_property->type == TYPE_TEMPLATE_JOBJECT ||
                   (parent_property->type == TYPE_TEMPLATE_ARRAY &&
                    parent_property->array_info.array_type == TYPE_TEMPLATE_JOBJECT)) {
            property_min_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MIN;
        } else if (parent_property->type == TYPE_TEMPLATE_ARRAY) {
            property_min_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_MIN;
        }

        char *min_key = LITE_json_value_of(property_min_key, property_json);
        if (!min_key) {
            HAL_Free(max_key);
            return QCLOUD_ERR_FAILURE;
        }

        if (data_type == TYPE_TEMPLATE_INT) {
            property->int32_limit.min = atoi(min_key);
            property->int32_limit.max = atoi(max_key);

        } else if (data_type == TYPE_TEMPLATE_FLOAT) {
            float min_value = 0;
            float max_value = 0;

            sscanf(min_key, "%f", &min_value);
            sscanf(max_key, "%f", &max_value);

            property->float_limit.min = min_value;
            property->float_limit.max = max_value;
        }

        HAL_Free(min_key);
        HAL_Free(max_key);
    } else if (strcmp(type, "stringenum") == 0 || strcmp(type, "enum") == 0) {
        char *property_mapping_key = "";
        if (!parent_property) {
            property_mapping_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_MAPPING;
        } else if (parent_property->type == TYPE_TEMPLATE_JOBJECT ||
                   (parent_property->type == TYPE_TEMPLATE_ARRAY &&
                    parent_property->array_info.array_type == TYPE_TEMPLATE_JOBJECT)) {
            property_mapping_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_MAPPING;
        } else if (parent_property->type == TYPE_TEMPLATE_ARRAY) {
            property_mapping_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_MAPPING;
        }

        char *mapping_key = LITE_json_value_of(property_mapping_key, property_json);
        if (!mapping_key) {
            return QCLOUD_ERR_FAILURE;
        }
        char *pos;
        char *key;
        int   klen;
        char *val;
        int   vlen;
        int   vtype;
        int   max_len    = 0;
        int   enum_count = 0;
        json_object_for_each_kv(mapping_key, pos, key, klen, val, vlen, vtype)
        {
            max_len = klen > max_len ? klen : max_len;
            enum_count++;
        }

        if (data_type == TYPE_TEMPLATE_STRINGENUM) {
            property->array_info.array_size = max_len + 1;
            property->enum_limit.enum_count = enum_count;
        } else if (data_type == TYPE_TEMPLATE_ENUM) {
            property->array_info.array_size = property->data_buff_len;
        }
        property->enum_limit.enum_count = enum_count;
        property->data                  = HAL_Malloc(property->array_info.array_size * property->enum_limit.enum_count);
        if (!property->data) {
            HAL_Free(mapping_key);
            return QCLOUD_ERR_FAILURE;
        }
        memset(property->data, 0, property->array_info.array_size * property->enum_limit.enum_count);
        enum_count = 0;
        json_object_for_each_kv(mapping_key, pos, key, klen, val, vlen, vtype)
        {
            char *temp_data = property->data;
            temp_data += enum_count * property->array_info.array_size;
            if (data_type == TYPE_TEMPLATE_STRINGENUM) {
                strncpy(temp_data, key, klen);
            } else if (data_type == TYPE_TEMPLATE_ENUM) {
                sscanf(key, property_type.format, (int32_t *)temp_data);
            }
            enum_count++;
        }
        HAL_Free(mapping_key);
    }

    return QCLOUD_RET_SUCCESS;
}

int IOT_Template_ParseArrayProperty_FromJson_Test(_IN_ char *property_json, _IN_ DevicePropertyTest *array_property,
                                                  _IN_ int array_element_size,
                                                  _IN_ int device_property_in_array_element_pos)
{
    /* get json type */
    char *type = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_TYPE, property_json);
    if (!type) {
        Log_e("json buf no \"type\" key, %s", property_json);
        goto error;
    }
    /* get type by json string type */
    DataTemplatePropertyType property_type = IOT_Template_GetDataTemplatePropertyType(type, -1);
    if (!property_type.string_type) {
        HAL_Free(type);
        Log_e("get type by %s json type error", type);
        goto error;
    }

    array_property->array_info.array_type = property_type.type;

    if (array_property->array_info.array_type == TYPE_TEMPLATE_JOBJECT) {
        char *array_struct_json =
            LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_STRUCT_SPECS, property_json);
        if (!array_struct_json) {
            Log_e("array jobject no specs key %s", property_json);
            goto error;
        }

        int struct_count = 0;
        if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson_Test(
                                      array_struct_json, array_element_size, device_property_in_array_element_pos,
                                      array_property, &array_property->data, &struct_count)) {
            Log_e("array jobject get property error");
            HAL_Free(array_struct_json);
            goto error;
        }
        HAL_Free(array_struct_json);
        array_property->array_info.array_size = struct_count;
    } else {
        _property_type_info_parse_test(property_json, array_property, type, array_property);
    }

    HAL_Free(type);
    return QCLOUD_RET_SUCCESS;

error:
    return QCLOUD_ERR_FAILURE;
}

int IOT_Template_ParseDevicePropertyArray_FromJson_Test(_IN_ char *propertys_json, _IN_ int array_element_size,
                                                        _IN_ int device_property_in_array_element_pos,
                                                        _IN_ DevicePropertyTest *parent_property,
                                                        _OU_ void **             property_struct_array,
                                                        _OU_ int *               property_struct_array_length)
{
    if (!propertys_json) {
        return QCLOUD_ERR_FAILURE;
    }
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    /* first get array num */
    json_array_for_each_entry(propertys_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        (*property_struct_array_length)++;
    }

    *property_struct_array = (DevicePropertyTest *)HAL_Malloc(*property_struct_array_length * array_element_size);
    if (!(*property_struct_array)) {
        goto error;
    }
    memset(*property_struct_array, 0, *property_struct_array_length * array_element_size);

    int i = 0;
    json_array_for_each_entry(propertys_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);

        DevicePropertyTest *property =
            (DevicePropertyTest *)(((char *)(*property_struct_array) + i * array_element_size) +
                                   device_property_in_array_element_pos);

        char *id = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ID, entry);
        if (!id) {
            goto error;
        }

        /* need free */
        property->key = id;

        char *property_type_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_TYPE;
        if (parent_property) {
            property_type_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_TYPE;
        }
        /* get type; need free */
        char *type = LITE_json_value_of(property_type_key, entry);
        if (!type) {
            goto error;
        }
        DataTemplatePropertyType property_type = IOT_Template_GetDataTemplatePropertyType(type, -1);
        if (!property_type.string_type) {
            HAL_Free(type);
            goto error;
        }

        property->type          = property_type.type;
        property->data_buff_len = property_type.size;

        if (QCLOUD_RET_SUCCESS != _property_type_info_parse_test(entry, parent_property, type, property)) {
            HAL_Free(type);
            goto error;
        }

        if (property->type == TYPE_TEMPLATE_JOBJECT) {
            /* struct */
            /* cycle parse */
            char *struct_json = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_SPECS, entry);
            if (!struct_json) {
                HAL_Free(type);
                goto error;
            }

            int struct_count = 0;
            /* cycle parese */
            if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson_Test(
                                          struct_json, array_element_size, device_property_in_array_element_pos,
                                          parent_property == NULL ? property : parent_property, &property->data,
                                          &struct_count)) {
                HAL_Free(type);
                HAL_Free(struct_json);
                goto error;
            }
            property->struct_obj_num = struct_count;
            HAL_Free(struct_json);

        } else if (property->type == TYPE_TEMPLATE_ARRAY) {
            /* array */
            /* cycle parse */
            property->array_info.array_size = DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN;
            char *property_array_info_key   = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_ARRAY_ARRAYINFO;
            if (parent_property) {
                property_array_info_key = DATA_TEMPLATE_JSON_KEY_OBJECT_PROPERTY_STRUCT_ARRAYINFO;
            }
            char *array_json = LITE_json_value_of(property_array_info_key, entry);
            if (!array_json) {
                HAL_Free(type);
                goto error;
            }

            if (QCLOUD_RET_SUCCESS !=
                IOT_Template_ParseArrayProperty_FromJson_Test(array_json, property, array_element_size,
                                                              device_property_in_array_element_pos)) {
                HAL_Free(array_json);
                HAL_Free(type);
                goto error;
            }

            HAL_Free(array_json);
        }
        HAL_Free(type);

        restore_json_str_last_char(entry, entry_len, old_ch);
        i++;
    }

    return QCLOUD_RET_SUCCESS;

error:
    IOT_Template_FreeDevicePropertyArray_Test(property_struct_array, property_struct_array_length, array_element_size,
                                              device_property_in_array_element_pos);
    return QCLOUD_ERR_FAILURE;
}

static int             sg_property_array_count_test         = 0;
static sDataPointTest *sg_data_template_property_array_test = NULL;

void IOT_Template_FreePropertyArray_Test()
{
    int device_property_in_sDataPoint_pos = (char *)(&(((sDataPointTest *)0)->data_property)) - (char *)0;

    IOT_Template_FreeDevicePropertyArray_Test((void *)&sg_data_template_property_array_test,
                                              &sg_property_array_count_test, sizeof(sDataPointTest),
                                              device_property_in_sDataPoint_pos);
    HAL_Free(sg_data_template_property_array_test);
    sg_data_template_property_array_test = NULL;
    sg_property_array_count_test         = 0;
}

int IOT_Template_ParsePropertyArrayFromJson_Test(char *propertys_json)
{
    int device_property_in_sDataPoint_pos = (char *)(&(((sDataPointTest *)0)->data_property)) - (char *)0;
    if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson_Test(
                                  propertys_json, sizeof(sDataPointTest), device_property_in_sDataPoint_pos, NULL,
                                  (void *)&sg_data_template_property_array_test, &sg_property_array_count_test)) {
        sg_data_template_property_array_test = NULL;
        sg_property_array_count_test         = 0;
        return QCLOUD_ERR_FAILURE;
    }

    return QCLOUD_RET_SUCCESS;
}

static int32_t sg_int32_t_value_test = 0;

static int _set_property_get_type_value_test(DevicePropertyTest *property, JsonDataType type, void **type_value,
                                             int *value_len)
{
    if (type == TYPE_TEMPLATE_INT) {
        if (property->data) {
            int32_t enum_test = sg_int32_t_value_test % property->enum_limit.enum_count;
            int     set_len   = property->array_info.array_size;
            *type_value       = HAL_Malloc(set_len);
            if (!*type_value) {
                goto error;
            }
            memcpy(*type_value, (char *)property->data + enum_test * property->array_info.array_size, set_len);
            *value_len = set_len;
        } else {
            int32_t set_value = 0;
            *type_value       = HAL_Malloc(sizeof(int32_t));
            if (!*type_value) {
                goto error;
            }

            int32_t interval = property->int32_limit.max - property->int32_limit.min;
            if (interval) {
                set_value = property->int32_limit.min + sg_int32_t_value_test % interval;
            } else {
                set_value = sg_int32_t_value_test;
            }
            memcpy(*type_value, &set_value, sizeof(set_value));
            *value_len = sizeof(set_value);
        }
    } else if (type == TYPE_TEMPLATE_BOOL) {
        int8_t set_value = sg_int32_t_value_test % 2;
        *type_value      = HAL_Malloc(sizeof(int8_t));
        if (!*type_value) {
            goto error;
        }
        memcpy(*type_value, &set_value, sizeof(set_value));
        *value_len = sizeof(set_value);
    } else if (type == TYPE_TEMPLATE_FLOAT) {
        float set_value = 0;
        *type_value     = HAL_Malloc(sizeof(float));
        if (!*type_value) {
            goto error;
        }

        int32_t interval = property->float_limit.max - property->float_limit.min;
        if (interval) {
            set_value = property->float_limit.min + (sg_int32_t_value_test % interval) * 0.01;
        } else {
            set_value = sg_int32_t_value_test * 0.01;
        }
        memcpy(*type_value, &set_value, sizeof(set_value));
        *value_len = sizeof(set_value);

    } else if (type == TYPE_TEMPLATE_TIME) {
        uint32_t set_value = 0;
        set_value          = HAL_Timer_current_sec();
        *type_value        = HAL_Malloc(sizeof(uint32_t));
        if (!*type_value) {
            goto error;
        }
        memcpy(*type_value, &set_value, sizeof(set_value));
        *value_len = sizeof(set_value);

    } else if (type == TYPE_TEMPLATE_STRING) {
        if (property->data) {
            int32_t enum_test = sg_int32_t_value_test % property->enum_limit.enum_count;
            *type_value       = HAL_Malloc(property->array_info.array_size);
            if (!*type_value) {
                goto error;
            }
            *value_len = property->array_info.array_size;
            memcpy(*type_value, (char *)property->data + enum_test * property->array_info.array_size, *value_len);
        } else {
            int   string_len  = sg_int32_t_value_test % property->data_buff_len + 1;
            char *test_string = HAL_Malloc(string_len);
            if (!test_string) {
                goto error;
            }
            HAL_Snprintf(test_string, string_len, "%0*d", string_len - 1, sg_int32_t_value_test);
            *type_value = test_string;
            *value_len  = strlen(test_string) + 1;
        }
    } else if (type == TYPE_TEMPLATE_ARRAY) {
        char *array_string  = HAL_Malloc(DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN + 1);
        int   one_array_len = 0;
        if (!array_string) {
            goto error;
        }
        if (property->array_info.array_type == TYPE_TEMPLATE_JOBJECT) {
            /* [{key:value}, {key:value}] */
            int cycle_i = 0;
            int len     = 0;

            len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "[");
            while (cycle_i < sg_int32_t_value_test % 10 + 1) {
                len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "{");

                int struct_member = 0;
                while (struct_member < property->array_info.array_size) {
                    sDataPointTest *member_property = ((sDataPointTest *)(property->data)) + struct_member;
                    void *          arr_type_value  = NULL;
                    int             value_len       = 0;

                    if (QCLOUD_RET_SUCCESS != _set_property_get_type_value_test(&(member_property->data_property),
                                                                                member_property->data_property.type,
                                                                                &arr_type_value, &value_len)) {
                        HAL_Free(array_string);
                        goto error;
                    }

                    if (member_property->data_property.type == TYPE_TEMPLATE_INT) {
                        len +=
                            HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "\"%s\":%d,",
                                         member_property->data_property.key, *((int32_t *)arr_type_value));
                    } else if (member_property->data_property.type == TYPE_TEMPLATE_BOOL) {
                        len +=
                            HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "\"%s\":%d,",
                                         member_property->data_property.key, *((int8_t *)arr_type_value));
                    } else if (member_property->data_property.type == TYPE_TEMPLATE_FLOAT) {
                        len +=
                            HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "\"%s\":%f,",
                                         member_property->data_property.key, *((float *)arr_type_value));
                    } else if (member_property->data_property.type == TYPE_TEMPLATE_TIME) {
                        len +=
                            HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "\"%s\":%u,",
                                         member_property->data_property.key, *((uint32_t *)arr_type_value));
                    } else if (member_property->data_property.type == TYPE_TEMPLATE_STRING) {
                        len +=
                            HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len,
                                         "\"%s\":\"%s\",", member_property->data_property.key, (char *)arr_type_value);
                    }
                    HAL_Free(arr_type_value);

                    struct_member++;
                }

                len -= 1;
                len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "},");
                cycle_i++;

                if (cycle_i == 1) {
                    one_array_len = strlen(array_string);
                } else if (one_array_len > DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len) {
                    break;
                }
            }

            len -= 1;
            len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "]");
            *type_value = array_string;
            *value_len  = strlen(array_string) + 1;
        } else {
            /* [xxx,xxx,xxx] */
            int cycle_i = 0;
            int len     = 0;
            len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "[");
            while (cycle_i < sg_int32_t_value_test % 10 + 1) {
                void *arr_type_value = NULL;
                int   value_len      = 0;
                if (QCLOUD_RET_SUCCESS != _set_property_get_type_value_test(property, property->array_info.array_type,
                                                                            &arr_type_value, &value_len)) {
                    HAL_Free(array_string);
                    goto error;
                }
                if (property->array_info.array_type == TYPE_TEMPLATE_INT) {
                    len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "%d,",
                                        *((int32_t *)arr_type_value));
                } else if (property->array_info.array_type == TYPE_TEMPLATE_BOOL) {
                    len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "%d,",
                                        *((int8_t *)arr_type_value));
                } else if (property->array_info.array_type == TYPE_TEMPLATE_FLOAT) {
                    len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "%f,",
                                        *((float *)arr_type_value));
                } else if (property->array_info.array_type == TYPE_TEMPLATE_TIME) {
                    len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "%u,",
                                        *((uint32_t *)arr_type_value));
                } else if (property->array_info.array_type == TYPE_TEMPLATE_STRING) {
                    len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "\"%s\",",
                                        (char *)arr_type_value);
                }

                cycle_i++;

                if (cycle_i == 1) {
                    one_array_len = strlen(array_string);
                } else if (one_array_len > DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len) {
                    break;
                }
            }
            len -= 1;
            len += HAL_Snprintf(array_string + len, DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN - len, "]");
            *type_value = array_string;
            *value_len  = strlen(array_string) + 1;
        }
    }

    return QCLOUD_RET_SUCCESS;
error:
    return QCLOUD_ERR_FAILURE;
}

void static _get_property_value_test(DevicePropertyTest *property, char *parent_key, int is_property)
{
    JsonDataType type = property->type;
    int          ret  = QCLOUD_RET_SUCCESS;

    if (type == TYPE_TEMPLATE_INT) {
        int32_t value = 0;
        if (parent_key) {
            if (is_property) {
                ret = IOT_Template_GetStructPropertyValue(parent_key, property->key, &value, sizeof(value));
            } else {
#ifdef ACTION_ENABLED
                ret = IOT_Template_GetActionInputValue(parent_key, property->key, &value, sizeof(value));
#endif
            }
            Log_d("%s.%s:%d, ret:%d", parent_key, property->key, value, ret);
        } else {
            ret = IOT_Template_GetPropertyValue(property->key, &value, sizeof(value));
            Log_d("%s:%d, ret:%d", property->key, value, ret);
        }

    } else if (type == TYPE_TEMPLATE_BOOL) {
        int8_t value = 0;

        if (parent_key) {
            if (is_property) {
                ret = IOT_Template_GetStructPropertyValue(parent_key, property->key, &value, sizeof(value));
            } else {
#ifdef ACTION_ENABLED
                ret = IOT_Template_GetActionInputValue(parent_key, property->key, &value, sizeof(value));
#endif
            }
            Log_d("%s.%s:%hhd, ret:%d", parent_key, property->key, value, ret);

        } else {
            ret = IOT_Template_GetPropertyValue(property->key, &value, sizeof(value));
            Log_d("%s:%hhd, ret:%d", property->key, value, ret);
        }

    } else if (type == TYPE_TEMPLATE_FLOAT) {
        float value = 0;
        if (parent_key) {
            if (is_property) {
                ret = IOT_Template_GetStructPropertyValue(parent_key, property->key, &value, sizeof(value));
            } else {
#ifdef ACTION_ENABLED
                ret = IOT_Template_GetActionInputValue(parent_key, property->key, &value, sizeof(value));
#endif
            }
            Log_d("%s.%s:%f, ret:%d", parent_key, property->key, value, ret);

        } else {
            ret = IOT_Template_GetPropertyValue(property->key, &value, sizeof(value));
            Log_d("%s:%f, ret:%d", property->key, value, ret);
        }

    } else if (type == TYPE_TEMPLATE_TIME) {
        uint32_t value = 0;
        if (parent_key) {
            if (is_property) {
                ret = IOT_Template_GetStructPropertyValue(parent_key, property->key, &value, sizeof(value));
            } else {
#ifdef ACTION_ENABLED
                ret = IOT_Template_GetActionInputValue(parent_key, property->key, &value, sizeof(value));
#endif
            }

            Log_d("%s.%s:%u, ret:%d", parent_key, property->key, value, ret);

        } else {
            ret = IOT_Template_GetPropertyValue(property->key, &value, sizeof(value));
            Log_d("%s:%u, ret:%d", property->key, value, ret);
        }
    } else if (type == TYPE_TEMPLATE_STRING) {
        int   string_len = property->data_buff_len;
        char *value      = HAL_Malloc(string_len);
        memset(value, 0, string_len);
        if (parent_key) {
            if (is_property) {
                ret = IOT_Template_GetStructPropertyValue(parent_key, property->key, value, string_len - 1);
            } else {
#ifdef ACTION_ENABLED
                ret = IOT_Template_GetActionInputValue(parent_key, property->key, value, string_len - 1);
#endif
            }
            Log_d("%s.%s:%s, ret:%d", parent_key, property->key, value, ret);

        } else {
            ret = IOT_Template_GetPropertyValue(property->key, value, string_len - 1);
            Log_d("%s:%s, ret:%d", property->key, value, ret);
        }

        HAL_Free(value);
    } else if (type == TYPE_TEMPLATE_ARRAY) {
        int   string_len = DATA_TEMPLATE_ARRAY_PROPERTY_BUF_LEN + 1;
        char *value      = HAL_Malloc(string_len);
        memset(value, 0, string_len);

        if (parent_key) {
            if (is_property) {
                ret = IOT_Template_GetStructPropertyValue(parent_key, property->key, value, string_len - 1);
            } else {
#ifdef ACTION_ENABLED
                ret = IOT_Template_GetActionInputValue(parent_key, property->key, value, string_len - 1);
#endif
            }
            Log_d("%s.%s:%s, ret:%d", parent_key, property->key, value, ret);

        } else {
            ret = IOT_Template_GetPropertyValue(property->key, value, string_len - 1);
            Log_d("%s:%s, ret:%d", property->key, value, ret);
        }

        HAL_Free(value);
    }
}

static void _set_property_value_test(sDataPointTest *property, char *parent_key)
{
    char *key = property->data_property.key;

    void *arr_type_value = NULL;
    int   value_len      = 0;

    if (QCLOUD_RET_SUCCESS != _set_property_get_type_value_test(&(property->data_property),
                                                                property->data_property.type, &arr_type_value,
                                                                &value_len)) {
        return;
    }

    if (parent_key) {
        IOT_Template_SetStructPropertyValue(parent_key, key, arr_type_value, value_len);

    } else {
        IOT_Template_SetPropertyValue(key, arr_type_value, value_len);
    }

    HAL_Free(arr_type_value);
}

static void _set_property_test(sDataPointTest *parent_property, sDataPointTest *property_array, int array_count)
{
    int i = 0;

    if (!parent_property) {
        sg_int32_t_value_test++;
    }

    if (array_count == 0) {
        return;
    }

    int cycle_count = sg_int32_t_value_test % array_count + 1;
    if (parent_property) {
        cycle_count = array_count;
    }

    while (i < cycle_count) {
        sDataPointTest *data_point = property_array + i;
        /* struct */
        if (data_point->data_property.type == TYPE_TEMPLATE_JOBJECT) {
            _set_property_test(data_point, data_point->data_property.data, data_point->data_property.struct_obj_num);
        } else {
            _set_property_value_test(data_point, parent_property == NULL ? NULL : parent_property->data_property.key);
        }

        i++;
    }
}

void set_property_test()
{
    _set_property_test(NULL, sg_data_template_property_array_test, sg_property_array_count_test);
}

static void _get_property_test(sDataPointTest *parent_property, sDataPointTest *property_array, int array_count)
{
    int i = 0;

    while (i < array_count) {
        sDataPointTest *data_point = property_array + i;
        /* struct */
        if (data_point->data_property.type == TYPE_TEMPLATE_JOBJECT) {
            _get_property_test(data_point, data_point->data_property.data, data_point->data_property.struct_obj_num);
        } else {
            _get_property_value_test(&data_point->data_property,
                                     parent_property == NULL ? NULL : parent_property->data_property.key, 1);
        }

        i++;
    }
}

void get_property_test()
{
    _get_property_test(NULL, sg_data_template_property_array_test, sg_property_array_count_test);
}

#if defined(ACTION_ENABLED) || defined(EVENT_POST_ENABLED)

static int _set_action_output_or_event_value_test(DevicePropertyTest *property, char *action_id_or_event_name,
                                                  int action)
{
    char *key = property->key;

    void *arr_type_value = NULL;
    int   value_len      = 0;

    if (QCLOUD_RET_SUCCESS !=
        _set_property_get_type_value_test(property, property->type, &arr_type_value, &value_len)) {
        return QCLOUD_ERR_FAILURE;
    }

    if (action) {
#ifdef ACTION_ENABLED
        return IOT_Template_SetActionOutputValue(action_id_or_event_name, key, arr_type_value, value_len);
#endif
    } else {
#ifdef EVENT_POST_ENABLED
        return IOT_Template_SetEventValue(action_id_or_event_name, key, arr_type_value, value_len);
#endif
    }

    return QCLOUD_ERR_FAILURE;
}

#endif

#ifdef ACTION_ENABLED
static int               sg_action_array_count_test         = 0;
static DeviceActionTest *sg_data_template_action_array_test = NULL;

void IOT_Template_FreeActionArray_Test()
{
    int i = 0;
    while (i < sg_action_array_count_test) {
        DeviceActionTest *action = sg_data_template_action_array_test + i;
        IOT_Template_FreeDevicePropertyArray_Test((void *)&action->pInput, (int *)&action->input_num,
                                                  sizeof(DeviceProperty), 0);
        IOT_Template_FreeDevicePropertyArray_Test((void *)&action->pOutput, (int *)&action->output_num,
                                                  sizeof(DeviceProperty), 0);
        HAL_Free(action->pActionId);
    }
    HAL_Free(sg_data_template_action_array_test);
    sg_data_template_action_array_test = NULL;
    sg_action_array_count_test         = 0;
}

int IOT_Template_ParseActionArrayFromJson_Test(char *actions_json)
{
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    json_array_for_each_entry(actions_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        sg_action_array_count_test++;
    }

    /* need free */
    sg_data_template_action_array_test =
        (DeviceActionTest *)HAL_Malloc(sg_action_array_count_test * sizeof(DeviceActionTest));
    if (!sg_data_template_action_array_test) {
        goto error;
    }
    /* zero clear */
    memset(sg_data_template_action_array_test, 0, sg_action_array_count_test * sizeof(DeviceActionTest));

    int i = 0;
    json_array_for_each_entry(actions_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);

        DeviceActionTest *data_template_action = &(sg_data_template_action_array_test[i]);
        char *            id                   = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_ID, entry);
        if (!id) {
            goto error;
        }

        /* need free */
        data_template_action->pActionId = id;

        /* need free */
        char *input_array = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_INPUT_ARRAY, entry);
        if (!input_array) {
            goto error;
        }
        int array_length = 0;
        if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson_Test(
                                      input_array, sizeof(DevicePropertyTest), 0, NULL,
                                      (void *)&(data_template_action->pInput), &array_length)) {
            HAL_Free(input_array);
            goto error;
        }

        data_template_action->input_num = array_length;
        HAL_Free(input_array);

        /* need free */
        char *output_array = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_ACTION_OUTPUT_ARRAY, entry);
        if (!output_array) {
            goto error;
        }
        array_length = 0;
        if (QCLOUD_RET_SUCCESS != IOT_Template_ParseDevicePropertyArray_FromJson_Test(
                                      output_array, sizeof(DevicePropertyTest), 0, NULL,
                                      (void *)&(data_template_action->pOutput), &array_length)) {
            HAL_Free(output_array);
            goto error;
        }
        data_template_action->output_num = array_length;

        HAL_Free(output_array);

        restore_json_str_last_char(entry, entry_len, old_ch);
        i++;
    }

    return QCLOUD_RET_SUCCESS;

error:
    IOT_Template_FreeActionArray_Test();
    return QCLOUD_ERR_FAILURE;
}

int set_action_output_test(char *action_id)
{
    int i = 0;

    DeviceActionTest *action = NULL;
    while (i < sg_action_array_count_test) {
        action = sg_data_template_action_array_test + i;
        /* struct */
        if (!strcmp(action->pActionId, action_id)) {
            break;
        }
        action = NULL;
        i++;
    }
    if (action == NULL) {
        return QCLOUD_ERR_FAILURE;
    }

    DevicePropertyTest *property = NULL;
    i                            = 0;
    while (i < action->output_num) {
        property = action->pOutput + i;
        _set_action_output_or_event_value_test(property, action->pActionId, 1);
        i++;
    }
    return QCLOUD_RET_SUCCESS;
}

int get_action_input_test(char *action_id)
{
    int i = 0;

    DeviceActionTest *action = NULL;
    while (i < sg_action_array_count_test) {
        action = sg_data_template_action_array_test + i;
        /* struct */
        if (!strcmp(action->pActionId, action_id)) {
            break;
        }
        action = NULL;
        i++;
    }
    if (action == NULL) {
        return QCLOUD_ERR_FAILURE;
    }

    DevicePropertyTest *property = NULL;
    i                            = 0;
    while (i < action->input_num) {
        property = action->pInput + i;
        _get_property_value_test(property, action->pActionId, 0);
        i++;
    }
    return QCLOUD_RET_SUCCESS;
}

#endif

#ifdef EVENT_POST_ENABLED
static int         sg_event_array_count_test         = 0;
static sEventTest *sg_data_template_event_array_test = NULL;

void IOT_Template_FreeEventArray_Test()
{
    int i = 0;
    while (i < sg_event_array_count_test) {
        sEventTest *event = sg_data_template_event_array_test + i;
        IOT_Template_FreeDevicePropertyArray((void *)&event->pEventData, (int *)&event->eventDataNum,
                                             sizeof(DeviceProperty), 0);
        HAL_Free(event->event_name);
        HAL_Free(event->type);
    }
    HAL_Free(sg_data_template_event_array_test);
    sg_data_template_event_array_test = NULL;
    sg_event_array_count_test         = 0;
}

int IOT_Template_ParseEventArrayFromJson_Test(char *events_json)
{
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;

    /* get array num */
    json_array_for_each_entry(events_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        sg_event_array_count_test++;
    }

    /* need free */
    sg_data_template_event_array_test = (sEventTest *)HAL_Malloc(sg_event_array_count_test * sizeof(sEventTest));
    if (!sg_data_template_event_array_test) {
        goto error;
    }
    /* zero clear */
    memset(sg_data_template_event_array_test, 0, sg_event_array_count_test * sizeof(sEventTest));

    int i = 0;
    json_array_for_each_entry(events_json, pos, entry, entry_len, entry_type)
    {
        if (!entry)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);

        sEventTest *event = &(sg_data_template_event_array_test[i]);

        char *id = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_ID, entry);
        if (!id) {
            goto error;
        }
        /* need free */
        event->event_name = id;

        char *type = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_TYPE, entry);
        if (!type) {
            goto error;
        }
        /* need free */
        event->type = type;

        /* need free */
        char *params = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_OBJECT_EVENT_PARAM_ARRAY, entry);
        if (!params) {
            goto error;
        }

        int array_length = 0;
        if (QCLOUD_RET_SUCCESS !=
            IOT_Template_ParseDevicePropertyArray_FromJson_Test(params, sizeof(DevicePropertyTest), 0, NULL,
                                                                (void *)&(event->pEventData), &array_length)) {
            HAL_Free(params);
            goto error;
        }
        event->eventDataNum = array_length;

        HAL_Free(params);

        restore_json_str_last_char(entry, entry_len, old_ch);
        i++;
    }

    return QCLOUD_RET_SUCCESS;

error:
    IOT_Template_FreeEventArray_Test();
    return QCLOUD_ERR_FAILURE;
}

void set_event_test(void *client)
{
    sEventTest *        event    = NULL;
    DevicePropertyTest *property = NULL;

    static int last_event_count = 0;
    int        event_index      = 0;

    if (sg_event_array_count_test == 0) {
        return;
    }
    while (event_index < last_event_count % sg_event_array_count_test + 1) {
        event = &(sg_data_template_event_array_test[event_index]);
        /* find event */
        int property_index = 0;
        while (property_index < sg_int32_t_value_test % event->eventDataNum + 1) {
            property       = event->pEventData + property_index;
            int flag_index = _set_action_output_or_event_value_test(property, event->event_name, 0);
            if (flag_index >= QCLOUD_RET_SUCCESS) {
                IOT_Event_setFlag(client, 1 << flag_index);
            } else {
                Log_e("set value error");
            }
            property_index++;
        }
        event_index++;
    }
    last_event_count++;
}
#endif

void IOT_Template_FreeParse_Test()
{
    IOT_Template_FreePropertyArray_Test();

#ifdef EVENT_POST_ENABLED
    IOT_Template_FreeEventArray_Test();
#endif

#ifdef ACTION_ENABLED
    IOT_Template_FreeActionArray_Test();
#endif
}

int IOT_Template_Parse_FromJson_Test(char *json)
{
    char *events    = NULL;
    char *actions   = NULL;
    char *propertys = NULL;

    propertys = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_ARRAY_PROPERTYS, json);
    if (!propertys) {
        goto error;
    }
    IOT_Template_ParsePropertyArrayFromJson_Test(propertys);
    HAL_Free(propertys);

#ifdef ACTION_ENABLED

    actions = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_ARRAY_ACTIONS, json);
    if (actions) {
        IOT_Template_ParseActionArrayFromJson_Test(actions);
        HAL_Free(actions);
    }

#endif

#ifdef EVENT_POST_ENABLED

    events = LITE_json_value_of(DATA_TEMPLATE_JSON_KEY_ARRAY_EVENTS, json);
    if (events) {
        IOT_Template_ParseEventArrayFromJson_Test(events);
        HAL_Free(events);
    }

#endif
    return QCLOUD_RET_SUCCESS;

error:
    HAL_Free(propertys);
    HAL_Free(actions);
    HAL_Free(events);

    return QCLOUD_ERR_FAILURE;
}

int IOT_Template_Parse_FromJsonFile_Test(const char *data_template_json_file_name)
{
    FILE *   fp;
    uint32_t len;
    uint32_t rlen;
    char *   JsonDoc = NULL;
    int      ret     = QCLOUD_RET_SUCCESS;

    fp = fopen(data_template_json_file_name, "r");
    if (NULL == fp) {
        Log_e("open device info file \"%s\" failed", STRING_PTR_PRINT_SANITY_CHECK(data_template_json_file_name));
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    fseek(fp, 0L, SEEK_END);
    len = ftell(fp);
    if (len > 30 * 1024) {
        Log_e("device info file \"%s\" is too big!", STRING_PTR_PRINT_SANITY_CHECK(data_template_json_file_name));
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    JsonDoc = (char *)HAL_Malloc(len + 10);
    memset(JsonDoc, 0, len + 10);
    if (NULL == JsonDoc) {
        Log_e("malloc buffer for json file read fail");
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    rewind(fp);
    rlen = fread(JsonDoc, 1, len, fp);

    if (len != rlen) {
        Log_e("read data len (%d) less than needed (%d), %s", rlen, len, JsonDoc);
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    IOT_Template_Parse_FromJson_Test(JsonDoc);

exit:

    HAL_Free(JsonDoc);

    if (fp) {
        fclose(fp);
    }
    return ret;
}

#endif

#ifdef AUTH_MODE_CERT
static char sg_cert_file[PATH_MAX + 1];  // full path of device cert file
static char sg_key_file[PATH_MAX + 1];   // full path of device key file
#endif

static DeviceInfo    sg_devInfo;
static MQTTEventType sg_subscribe_event_result = MQTT_EVENT_UNDEF;
static bool          sg_control_msg_arrived    = false;
static char          sg_data_report_buffer[2048];
static size_t        sg_data_report_buffersize = sizeof(sg_data_report_buffer) / sizeof(sg_data_report_buffer[0]);

#ifdef EVENT_POST_ENABLED
static void update_events_timestamp(sEvent *pEvents, int count)
{
    int i;

    for (i = 0; i < count; i++) {
        if (NULL == (&pEvents[i])) {
            Log_e("null event pointer");
            return;
        }
#ifdef EVENT_TIMESTAMP_USED
        pEvents[i].timestamp = time(NULL);  // should be UTC and accurate
#else
        pEvents[i].timestamp = 0;
#endif
    }
}

static void event_post_cb(void *pClient, MQTTMessage *msg)
{
    Log_d("Reply:%.*s", msg->payload_len, msg->payload);
    //    IOT_Event_clearFlag(pClient, FLAG_EVENT0 | FLAG_EVENT1 | FLAG_EVENT2);
}

// event check and post
static void eventPostCheck(void *client)
{
    int      i;
    int      rc;
    uint32_t eflag;
    uint8_t  event_count;
    sEvent **pEventList = (sEvent **)HAL_Malloc(sg_event_array_count * sizeof(sEvent *));
    memset(pEventList, 0, sg_event_array_count * sizeof(sEvent *));

    static Timer event_timer;
    if (expired(&event_timer)) {
#ifdef DYN_DATA_TEMPLATE_TEST
        set_event_test(client);
#else
        uint8_t status       = 0;
        int     event_index  = IOT_Template_SetEventValue("status_report", "status", &status, sizeof(status));
        event_index =
            IOT_Template_SetEventValue("status_report", "message", "custom event", sizeof("custom event") - 1);
        IOT_Event_setFlag(client, 1 << event_index);
#endif
        countdown(&event_timer, 15);
    }

    eflag = IOT_Event_getFlag(client);
    if ((sg_event_array_count > 0) && (eflag > 0)) {
        event_count = 0;
        for (i = 0; i < sg_event_array_count; i++) {
            if ((eflag & (1 << i)) & ALL_EVENTS_MASK) {
                pEventList[event_count++] = sg_data_template_event_array + i;
                update_events_timestamp(sg_data_template_event_array + i, 1);
                IOT_Event_clearFlag(client, (1 << i) & ALL_EVENTS_MASK);
            }
        }

        rc = IOT_Post_Event(client, sg_data_report_buffer, sg_data_report_buffersize, event_count, pEventList,
                            event_post_cb);
        if (rc < 0) {
            Log_e("events post failed: %d", rc);
        }
    }

    HAL_Free(pEventList);
}

#endif

#ifdef ACTION_ENABLED

// action : regist action and set the action handle callback, add your aciton logic here
static void OnActionCallback(void *pClient, const char *pClientToken, DeviceAction *pAction)
{
    int        i;
    sReplyPara replyPara;

    // do something base on input, just print as an sample
    DeviceProperty *pActionInput = pAction->pInput;
    for (i = 0; i < pAction->input_num; i++) {
        if (JSTRING == pActionInput[i].type) {
            Log_d("Input:[%s], data:[%s]", pActionInput[i].key, pActionInput[i].data);
        } else {
            if (JINT32 == pActionInput[i].type) {
                Log_d("Input:[%s], data:[%d]", pActionInput[i].key, *((int *)pActionInput[i].data));
            } else if (JFLOAT == pActionInput[i].type) {
                Log_d("Input:[%s], data:[%f]", pActionInput[i].key, *((float *)pActionInput[i].data));
            } else if (JUINT32 == pActionInput[i].type) {
                Log_d("Input:[%s], data:[%u]", pActionInput[i].key, *((uint32_t *)pActionInput[i].data));
            }
        }
    }

    // construct output
    memset((char *)&replyPara, 0, sizeof(sReplyPara));
    replyPara.code       = eDEAL_SUCCESS;
    replyPara.timeout_ms = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    strcpy(replyPara.status_msg, "action execute success!");  // add the message about the action resault

#ifdef DYN_DATA_TEMPLATE_TEST
    get_action_input_test(pAction->pActionId);
    set_action_output_test(pAction->pActionId);
#else
    int err_code = 0;
    IOT_Template_SetActionOutputValue("light_blink", "err_code", &err_code, sizeof(err_code));
#endif

    DeviceProperty *pActionOutnput = pAction->pOutput;
    (void)pActionOutnput;  // elimate warning
    // TO DO: add your aciont logic here and set output properties which will be reported by action_reply

    IOT_Action_Reply(pClient, pClientToken, sg_data_report_buffer, sg_data_report_buffersize, pAction, &replyPara);
}

static int _register_data_template_action(void *pTemplate_client)
{
    int i, rc;

    for (i = 0; i < sg_action_array_count; i++) {
        rc = IOT_Template_Register_Action(pTemplate_client, &sg_data_template_action_array[i], OnActionCallback);
        if (rc != QCLOUD_RET_SUCCESS) {
            rc = IOT_Template_Destroy(pTemplate_client);
            Log_e("register device data template action failed, err: %d", rc);
            return rc;
        } else {
            Log_i("data template action=%s registered.", sg_data_template_action_array[i].pActionId);
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
            sg_subscribe_event_result = msg->event_type;
            Log_i("subscribe success, packet-id=%u", packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_TIMEOUT:
            sg_subscribe_event_result = msg->event_type;
            Log_i("subscribe wait ack timeout, packet-id=%u", packet_id);
            break;

        case MQTT_EVENT_SUBCRIBE_NACK:
            sg_subscribe_event_result = msg->event_type;
            Log_i("subscribe nack, packet-id=%u", packet_id);
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
static void _usr_init(void)
{
    Log_d("add your init code here");
}

// Setup MQTT construct parameters
static int _setup_connect_init_params(TemplateInitParams *initParams)
{
    int ret;

    ret = HAL_GetDevInfo((void *)&sg_devInfo);
    if (QCLOUD_RET_SUCCESS != ret) {
        return ret;
    }

    initParams->device_name = sg_devInfo.device_name;
    initParams->product_id  = sg_devInfo.product_id;

#ifdef AUTH_MODE_CERT
    /* TLS with certs*/
    char  certs_dir[PATH_MAX + 1] = "certs";
    char  current_path[PATH_MAX + 1];
    char *cwd = getcwd(current_path, sizeof(current_path));
    if (cwd == NULL) {
        Log_e("getcwd return NULL");
        return QCLOUD_ERR_FAILURE;
    }
    sprintf(sg_cert_file, "%s/%s/%s", current_path, certs_dir, sg_devInfo.dev_cert_file_name);
    sprintf(sg_key_file, "%s/%s/%s", current_path, certs_dir, sg_devInfo.dev_key_file_name);

    initParams->cert_file = sg_cert_file;
    initParams->key_file  = sg_key_file;
#else
    initParams->device_secret = sg_devInfo.device_secret;
#endif

    initParams->command_timeout        = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    initParams->keep_alive_interval_ms = QCLOUD_IOT_MQTT_KEEP_ALIVE_INTERNAL;
    initParams->auto_connect_enable    = 1;
    initParams->event_handle.h_fp      = event_handler;
    initParams->usr_control_handle     = NULL;

    return QCLOUD_RET_SUCCESS;
}

static void OnControlMsgCallback(void *pClient, const char *pJsonValueBuffer, uint32_t valueLength,
                                 DeviceProperty *pProperty)
{
    int i = 0;

    for (i = 0; i < sg_property_array_count; i++) {
        /* handle self defined string/json here. Other properties are dealed in _handle_delta()*/
        if (strcmp(sg_data_template_property_array[i].data_property.key, pProperty->key) == 0) {
            sg_data_template_property_array[i].state = eCHANGED;
            Log_i("Property=%s changed", pProperty->key);
            sg_control_msg_arrived = true;
            return;
        }
    }

    Log_e("Property=%s changed no match", pProperty->key);
}

static void OnReportReplyCallback(void *pClient, Method method, ReplyAck replyAck, const char *pJsonDocument,
                                  void *pUserdata)
{
    Log_i("recv report reply response, reply ack: %d, %s", replyAck, pJsonDocument);
}

// register data template properties
static int _register_data_template_property(void *pTemplate_client)
{
    int i, rc;

    for (i = 0; i < sg_property_array_count; i++) {
        rc = IOT_Template_Register_Property(pTemplate_client, &sg_data_template_property_array[i].data_property,
                                            OnControlMsgCallback);
        if (rc != QCLOUD_RET_SUCCESS) {
            Log_e("register device data template property failed, err: %d", rc);
            return rc;
        } else {
            Log_i("data template property=%s registered.", sg_data_template_property_array[i].data_property.key);
        }
    }

    return QCLOUD_RET_SUCCESS;
}

// when control msg received, data_template's properties has been parsed in pData you should add your logic how to
// use pData
void deal_down_stream_user_logic(void *client)
{
    Log_d("someting about your own product logic wait to be done");

#ifdef DYN_DATA_TEMPLATE_TEST
    get_property_test();
#endif

    /* foreach sg_data_template_property_array */
#ifdef EVENT_POST_ENABLED
// IOT_Event_setFlag(client, FLAG_EVENT0);  //set the events flag when the evnts your defined occured, see
// events_config.c
#ifdef DYN_DATA_TEMPLATE_TEST
    set_event_test(client);
#endif

#endif
}

/*get local property data, like sensor data*/
static void _refresh_local_property(void)
{
    // add your local property refresh logic
#ifdef DYN_DATA_TEMPLATE_TEST
    if (!sg_control_msg_arrived) {
        set_property_test();
    }
#else
    IOT_Template_SetPropertyValue("name", "custom_report", sizeof("custom_report") - 1);
    int lon = 120;
    int lat = 35;

    IOT_Template_SetStructPropertyValue("position", "longitude", &lon, sizeof(lon));
    IOT_Template_SetStructPropertyValue("position", "latitude", &lat, sizeof(lat));

    uint8_t power_switch = 1;
    IOT_Template_SetPropertyValue("power_switch", &power_switch, sizeof(power_switch));

    int32_t color_enum = 0;
    IOT_Template_SetPropertyValue("color", &color_enum, sizeof(color_enum));
#endif
}

/*find propery need report*/
static int find_wait_report_property(DeviceProperty *pReportDataList[])
{
    int i, j;

    for (i = 0, j = 0; i < sg_property_array_count; i++) {
        if (eCHANGED == sg_data_template_property_array[i].state) {
            pReportDataList[j++]                     = &(sg_data_template_property_array[i].data_property);
            sg_data_template_property_array[i].state = eNOCHANGE;
        }
    }

    return j;
}

// demo for up-stream
// add changed properties to pReportDataList, then the changed properties would be reported
// you should add your own logic for how to get the changed properties
int deal_up_stream_user_logic(DeviceProperty *pReportDataList[], int *pCount)
{
    // refresh local property
    _refresh_local_property();

    /*find propery need report*/
    *pCount = find_wait_report_property(pReportDataList);

    return (*pCount > 0) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

/*You should get the real info for your device, here just for example*/
static int _get_sys_info(void *handle, char *pJsonDoc, size_t sizeOfBuffer)
{
    /*platform info has at least one of module_hardinfo/module_softinfo/fw_ver property*/
    DeviceProperty plat_info[] = {
        {.key = "module_hardinfo", .type = TYPE_TEMPLATE_STRING, .data = "ESP8266"},
        {.key = "module_softinfo", .type = TYPE_TEMPLATE_STRING, .data = "V1.0"},
        {.key = "fw_ver", .type = TYPE_TEMPLATE_STRING, .data = QCLOUD_IOT_DEVICE_SDK_VERSION},
        {.key = "imei", .type = TYPE_TEMPLATE_STRING, .data = "11-22-33-44"},
        {.key = "lat", .type = TYPE_TEMPLATE_STRING, .data = "22.546015"},
        {.key = "lon", .type = TYPE_TEMPLATE_STRING, .data = "113.941125"},
        {.key = NULL, .data = NULL}  // end
    };

    /*self define info*/
    DeviceProperty self_info[] = {
        {.key = "append_info", .type = TYPE_TEMPLATE_STRING, .data = "your self define info"},
        {.key = NULL, .data = NULL}  // end
    };

    return IOT_Template_JSON_ConstructSysInfo(handle, pJsonDoc, sizeOfBuffer, plat_info, self_info);
}

static int parse_arguments(int argc, char **argv)
{
    int c;
    while ((c = utils_getopt(argc, argv, "c:l:s:")) != EOF) switch (c) {
            case 'c':
                if (HAL_SetDevInfoFile(utils_optarg))
                    return -1;
                break;
            case 's':
                memset(sg_json_schema_file, 0, sizeof(sg_json_schema_file));
                strncpy(sg_json_schema_file, utils_optarg, MAX_SIZE_OF_JSON_SCHEMA_FILE_PATH);
                break;
            default:
                HAL_Printf(
                    "usage: %s [options]\n"
                    "  [-c <config file for DeviceInfo> -s <date template json schema file>] \n",
                    argv[0]);
                return -1;
        }
    return 0;
}

int main(int argc, char **argv)
{
    int              rc;
    sReplyPara       replyPara;
    DeviceProperty **pReportDataList = NULL;
    int              ReportCont;

    // init log level
    IOT_Log_Set_Level(eLOG_DEBUG);
    // parse arguments for device info file
    rc = parse_arguments(argc, argv);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("parse arguments error, rc = %d", rc);
        return rc;
    }

    if (QCLOUD_RET_SUCCESS != (rc = IOT_Template_Parse_FromJsonFile(sg_json_schema_file))) {
        return rc;
    }

#ifdef DYN_DATA_TEMPLATE_TEST
    if (QCLOUD_RET_SUCCESS != (rc = IOT_Template_Parse_FromJsonFile_Test(sg_json_schema_file))) {
        return rc;
    }
#endif

    pReportDataList = HAL_Malloc(sg_property_array_count * sizeof(DeviceProperty *));

    // init connection
    TemplateInitParams init_params = DEFAULT_TEMPLATE_INIT_PARAMS;
    rc                             = _setup_connect_init_params(&init_params);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("init params err,rc=%d", rc);
        return rc;
    }

    void *client = IOT_Template_Construct(&init_params, NULL);
    if (client != NULL) {
        Log_i("Cloud Device Construct Success");
    } else {
        Log_e("Cloud Device Construct Failed");
        return QCLOUD_ERR_FAILURE;
    }

#ifdef MULTITHREAD_ENABLED
    if (QCLOUD_RET_SUCCESS != IOT_Template_Start_Yield_Thread(client)) {
        Log_e("start template yield thread fail");
        goto exit;
    }
#endif

    // user init
    _usr_init();

    // init data template
    // _init_data_template();

    // register data template propertys here
    rc = _register_data_template_property(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template propertys Success");
    } else {
        Log_e("Register data template propertys Failed: %d", rc);
        goto exit;
    }

    // register data template actions here
#ifdef ACTION_ENABLED
    rc = _register_data_template_action(client);
    if (rc == QCLOUD_RET_SUCCESS) {
        Log_i("Register data template actions Success");
    } else {
        Log_e("Register data template actions Failed: %d", rc);
        goto exit;
    }
#endif

    // report device info, then you can manager your product by these info, like position
    rc = _get_sys_info(client, sg_data_report_buffer, sg_data_report_buffersize);
    if (QCLOUD_RET_SUCCESS == rc) {
        rc = IOT_Template_Report_SysInfo_Sync(client, sg_data_report_buffer, sg_data_report_buffersize,
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
        if (sg_control_msg_arrived) {
            deal_down_stream_user_logic(client);

            /* control msg should reply, otherwise server treat device didn't receive and retain the msg which would
             * be get by  get status*/
            memset((char *)&replyPara, 0, sizeof(sReplyPara));
            replyPara.code          = eDEAL_SUCCESS;
            replyPara.timeout_ms    = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
            replyPara.status_msg[0] = '\0';  // add extra info to replyPara.status_msg when error occured

            rc = IOT_Template_ControlReply(client, sg_data_report_buffer, sg_data_report_buffersize, &replyPara);
            if (rc == QCLOUD_RET_SUCCESS) {
                Log_d("Contol msg reply success");
                sg_control_msg_arrived = false;
            } else {
                Log_e("Contol msg reply failed, err: %d", rc);
            }
        } else {
            Log_d("No control msg received...");
        }

        /*report msg to server*/
        /*report the lastest properties's status*/
        if (QCLOUD_RET_SUCCESS == deal_up_stream_user_logic(pReportDataList, &ReportCont)) {
            rc = IOT_Template_JSON_ConstructReportArray(client, sg_data_report_buffer, sg_data_report_buffersize,
                                                        ReportCont, pReportDataList);
            if (rc == QCLOUD_RET_SUCCESS) {
                rc = IOT_Template_Report(client, sg_data_report_buffer, sg_data_report_buffersize,
                                         OnReportReplyCallback, NULL, QCLOUD_IOT_MQTT_COMMAND_TIMEOUT);
                if (rc == QCLOUD_RET_SUCCESS) {
                    Log_i("data template reporte success");
                } else {
                    Log_e("data template reporte failed, err: %d", rc);
                }
            } else {
                Log_e("construct reporte data failed, err: %d", rc);
            }

        } else {
            // Log_d("no data need to be reported or someting goes wrong");
        }
#ifdef EVENT_POST_ENABLED
        eventPostCheck(client);
#endif
        HAL_SleepMs(3000);
    }

exit:

#ifdef MULTITHREAD_ENABLED
    IOT_Template_Stop_Yield_Thread(client);
#endif
    rc = IOT_Template_Destroy(client);

    IOT_Template_FreeParse();

#ifdef DYN_DATA_TEMPLATE_TEST
    IOT_Template_FreeParse_Test();
#endif

    return rc;
}
