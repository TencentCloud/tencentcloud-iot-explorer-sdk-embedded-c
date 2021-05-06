/*
 * Copyright (c) 2017-2019 Tencent Group. All rights reserved.
 * License-Identifier: Apache-2.0
 *
 * Licensed under the Apache License, Version 2.0 (the "License"); you may
 * not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS, WITHOUT
 * WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 */
/**
 * Edit by shockcao@tencent.com 2018/3/15
 */

#include "json_parser.h"
#include "lite-utils.h"
#include "qcloud_iot_export_error.h"
#include "qcloud_iot_export_method.h"
#include "qcloud_iot_export_log.h"

#ifndef SCNi8
#define SCNi8 "hhi"
#endif

#ifndef SCNu8
#define SCNu8 "hhu"
#endif

char *LITE_json_value_of(char *key, char *src)
{
    char *value     = NULL;
    int   value_len = -1;
    char *ret       = NULL;

    char *delim = NULL;
    char *key_iter;
    char *key_next;
    int   key_len;
    char *src_iter;

    src_iter = src;
    key_iter = key;

    do {
        if ((delim = strchr(key_iter, '.')) != NULL) {
            key_len  = delim - key_iter;
            key_next = HAL_Malloc(key_len + 1);
            strncpy(key_next, key_iter, key_len);
            key_next[key_len] = '\0';
            value             = json_get_value_by_name(src_iter, strlen(src_iter), key_next, &value_len, 0);

            if (value == NULL) {
                HAL_Free(key_next);
                return NULL;
            }

            src_iter = value;
            key_iter = delim + 1;
            HAL_Free(key_next);
        }
    } while (delim);

    value = json_get_value_by_name(src_iter, strlen(src_iter), key_iter, &value_len, 0);
    if (NULL == value) {
        return NULL;
    }
    ret = HAL_Malloc((value_len + 1) * sizeof(char));
    if (NULL == ret) {
        return NULL;
    }
    HAL_Snprintf(ret, value_len + 1, "%s", value);
    return ret;
}

list_head_t *LITE_json_keys_of(char *src, char *prefix)
{
    static LIST_HEAD(keylist);

    char *pos = 0, *key = 0, *val = 0;
    int   klen = 0, vlen = 0, vtype = 0;

    if (src == NULL || prefix == NULL) {
        return NULL;
    }

    if (!strcmp("", prefix)) {
        INIT_LIST_HEAD(&keylist);
    }

    json_object_for_each_kv(src, pos, key, klen, val, vlen, vtype)
    {
        if (key && klen && val && vlen) {
            json_key_t *entry = NULL;

            entry = HAL_Malloc(sizeof(json_key_t));
            memset(entry, 0, sizeof(json_key_t));
            entry->key = LITE_format_string("%s%.*s", prefix, klen, key);
            list_add_tail(&entry->list, &keylist);

            if (JSOBJECT == vtype) {
                char *iter_val = LITE_format_string("%.*s", vlen, val);
                char *iter_pre = LITE_format_string("%s%.*s.", prefix, klen, key);
                LITE_json_keys_of(iter_val, iter_pre);
                HAL_Free(iter_val);
                HAL_Free(iter_pre);
            }
        }
    }

    if (!strcmp("", prefix)) {
        json_key_t *entry = NULL;

        entry = HAL_Malloc(sizeof(json_key_t));
        memset(entry, 0, sizeof(json_key_t));
        list_add_tail(&entry->list, &keylist);

        return &keylist;
    }

    return NULL;
}

void LITE_json_keys_release(list_head_t *keylist)
{
    json_key_t *pos, *tmp;

    list_for_each_entry_safe(pos, tmp, keylist, list, json_key_t)
    {
        if (pos->key) {
            HAL_Free(pos->key);
        }
        list_del(&pos->list);
        HAL_Free(pos);
    }
}

void LITE_string_strip_char(char *src, char ch)
{
    char *end = src + strlen(src) + 1;

    while (*src != '\0') {
        if (*src == ch) {
            memmove(src, src + 1, end - src);
            end--;
        }
        src++;
    }
}

char *LITE_json_string_value_strip_transfer(char *key, char *src)
{
    char *str = LITE_json_value_of(key, src);

    if (NULL != str) {
        LITE_string_strip_char(str, '\\');
    }
    return str;
}

int LITE_get_int32(int32_t *value, char *src)
{
    return (sscanf(src, "%" SCNi32, value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_int16(int16_t *value, char *src)
{
    return (sscanf(src, "%" SCNi16, value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_int8(int8_t *value, char *src)
{
    return (sscanf(src, "%" SCNi8, value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_uint32(uint32_t *value, char *src)
{
    return (sscanf(src, "%" SCNu32, value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_uint16(uint16_t *value, char *src)
{
    return (sscanf(src, "%" SCNu16, value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_uint8(uint8_t *value, char *src)
{
    return (sscanf(src, "%" SCNu8, value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_float(float *value, char *src)
{
    return (sscanf(src, "%f", value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_double(double *value, char *src)
{
    return (sscanf(src, "%lf", value) == 1) ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

int LITE_get_boolean(bool *value, char *src)
{
    if (!strcmp(src, "false")) {
        *value = false;
    } else {
        *value = true;
    }

    return QCLOUD_RET_SUCCESS;
}

int LITE_get_string(int8_t *value, char *src, uint16_t max_len)
{
    int rc;

    if (NULL != strncpy((char *)value, src, max_len)) {
        value[Min(strlen(src), max_len)] = '\0';
        rc                               = QCLOUD_RET_SUCCESS;
    } else {
        rc = QCLOUD_ERR_FAILURE;
    }

    return rc;
}

/* Input string must be like \"aaaa\" or {\"aaaa\":1234} */
int LITE_dt_format_strobj_array(char *out_res, size_t out_sz, char *items[], size_t item_size)
{
    char * p       = out_res;
    size_t left_sz = out_sz;
    int    i, ret = HAL_Snprintf(p, left_sz, "[");
    if (ret <= 0)
        return QCLOUD_ERR_FAILURE;
    p += ret;
    left_sz -= ret;

    for (i = 0; i < (int)item_size - 1; ++i) {
        ret = HAL_Snprintf(p, left_sz, "%s, ", items[i]);
        if (ret <= 0)
            return QCLOUD_ERR_FAILURE;
        p += ret;
        left_sz -= ret;
    }
    if (i < item_size) {
        ret = HAL_Snprintf(p, left_sz, "%s]", items[i]);
    } else {
        ret = HAL_Snprintf(p, left_sz, "]");
    }
    if (ret < 0)
        return QCLOUD_ERR_FAILURE;

    return QCLOUD_RET_SUCCESS;
}

int LITE_dt_format_primitive_array(char *out_res, size_t out_sz, void *data, size_t data_size, int type)
{
    char * p       = out_res;
    size_t left_sz = out_sz;
    int    ret, i, item_sz;

    if (JINT32 != type && JFLOAT != type) {
        Log_e("type %d not supported", type);
        return QCLOUD_ERR_FAILURE;
    }
    if (0 == data_size) {
        ret = HAL_Snprintf(p, left_sz, "[]");
        return ret > 0 ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
    }

    if ((ret = HAL_Snprintf(p, left_sz, "[")) <= 0)
        return QCLOUD_ERR_FAILURE;
    left_sz -= ret;
    p += ret;

    int32_t *int_data   = (int32_t *)data;
    float *  float_data = (float *)data;
    item_sz             = (JINT32 == type) ? (data_size / sizeof(int32_t)) : (data_size / sizeof(float));
    for (i = 0; i < item_sz - 1; i++) {
        if (JINT32 == type)
            ret = HAL_Snprintf(p, left_sz, "%d,", int_data[i]);
        else
            ret = HAL_Snprintf(p, left_sz, "%f,", float_data[i]);
        if (ret <= 0)
            return QCLOUD_ERR_JSON;
        left_sz -= ret;
        p += ret;
    }
    if (JINT32 == type)
        ret = HAL_Snprintf(p, left_sz, "%d]", int_data[i]);
    else
        ret = HAL_Snprintf(p, left_sz, "%f]", float_data[i]);

    return ret > 0 ? QCLOUD_RET_SUCCESS : QCLOUD_ERR_FAILURE;
}

/* json_str [1,2,3] or  [1.23, 3.45] */
int LITE_dt_parse_primitive_array(void *out_res, size_t out_len, const char *json_str, int type)
{
    if (type != JINT32 && type != JFLOAT) {
        Log_e("type %d not supported", type);
        return QCLOUD_ERR_FAILURE;
    }

    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;
    int   res        = 0;

    json_array_for_each_entry((char *)json_str, pos, entry, entry_len, entry_type)
    {
        if (!entry || entry_type != JSNUMBER)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);
        if ((ssize_t)out_len <= 0)
            break;
        if (type == JINT32) {
            int32_t *out_val = (int32_t *)((char *)out_res + (res++) * sizeof(int32_t));
            LITE_get_int32(out_val, entry);
            out_len -= sizeof(int32_t);
        } else {
            float *out_val = (float *)((char *)out_res + (res++) * sizeof(float));
            LITE_get_float(out_val, entry);
            out_len -= sizeof(float);
        }
        restore_json_str_last_char(entry, entry_len, old_ch);
    }

    return res;
}

int LITE_dt_parse_str_array(char *out_res[], size_t arr_len, size_t str_len, const char *json_str)
{
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;
    int   res        = 0;

    json_array_for_each_entry((char *)json_str, pos, entry, entry_len, entry_type)
    {
        if (!entry || entry_type != JSSTRING)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);
        if (res >= arr_len)
            break;

        char *val = out_res[res++];
        strncpy(val, entry, str_len - 1);
        val[str_len - 1] = '\0';
        restore_json_str_last_char(entry, entry_len, old_ch);
    }

    return res;
}

int LITE_dt_parse_obj_array(void *out_res, size_t out_len, size_t obj_len, const char *json_str,
                            json_object_parse_t parse_fn)
{
    if (!parse_fn) {
        Log_e("Parsing function unavailable");
        return QCLOUD_ERR_FAILURE;
    }
    char *pos        = NULL;
    char *entry      = NULL;
    int   entry_len  = 0;
    int   entry_type = 0;
    char  old_ch     = 0;
    int   res        = 0;

    json_array_for_each_entry((char *)json_str, pos, entry, entry_len, entry_type)
    {
        if (!entry || entry_type != JSOBJECT)
            continue;
        backup_json_str_last_char(entry, entry_len, old_ch);
        if ((ssize_t)out_len <= 0)
            break;

        if (0 != parse_fn(entry, entry_len, out_res, obj_len)) {
            Log_e("%s parsed error", entry);
        }
        out_res = (char *)out_res + obj_len;
        out_len -= obj_len;
        res++;
        restore_json_str_last_char(entry, entry_len, old_ch);
    }

    return res;
}