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

#ifndef __LITE_UTILS_H__
#define __LITE_UTILS_H__
#ifdef __cplusplus
extern "C" {
#endif

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if defined(_PLATFORM_IS_LINUX_)
#include <assert.h>
#endif

#include "lite-list.h"
#include "qcloud_iot_import.h"

#define LITE_TRUE  (1)
#define LITE_FALSE (0)

#ifndef container_of
#define container_of(ptr, type, member) ((type *)((char *)(ptr)-offsetof(type, member)))
#endif

#define LITE_MINIMUM(a, b) (((a) <= (b)) ? (a) : (b))
#define LITE_MAXIMUM(a, b) (((a) >= (b)) ? (a) : (b))
#define LITE_isdigit(c)    (((c) <= '9' && (c) >= '0') ? (LITE_TRUE) : (LITE_FALSE))

#if defined(_PLATFORM_IS_LINUX_)
#define LITE_ASSERT(expr) assert(expr)
#else
#define LITE_ASSERT(expr)                                                                                           \
    do {                                                                                                            \
        if (!(expr)) {                                                                                              \
            HAL_Printf("### %s | %s(%d): ASSERT FAILED ###: %s is FALSE\r\n", __FILE__, __func__, __LINE__, #expr); \
        }                                                                                                           \
    } while (0)
#endif

char *LITE_strdup(const char *src);
char *LITE_format_string(const char *fmt, ...);
char *LITE_format_nstring(const int len, const char *fmt, ...);
void  LITE_hexbuf_convert(unsigned char *digest, char *out, int buflen, int uppercase);
void  LITE_hexstr_convert(char *hexstr, uint8_t *out_buf, int len);
void  LITE_replace_substr(char orig[], char key[], char swap[]);
void  LITE_str_strip_char(char *src, char destCh);

char *       LITE_json_value_of(char *key, char *src);
list_head_t *LITE_json_keys_of(char *src, char *prefix);
void         LITE_json_keys_release(list_head_t *keylist);
char *       LITE_json_string_value_strip_transfer(char *key, char *src);
void         LITE_string_strip_char(char *src, char ch);

int LITE_get_int32(int32_t *value, char *src);
int LITE_get_int16(int16_t *value, char *src);
int LITE_get_int8(int8_t *value, char *src);
int LITE_get_uint32(uint32_t *value, char *src);
int LITE_get_uint16(uint16_t *value, char *src);
int LITE_get_uint8(uint8_t *value, char *src);
int LITE_get_float(float *value, char *src);
int LITE_get_double(double *value, char *src);
int LITE_get_boolean(bool *value, char *src);
int LITE_get_string(int8_t *value, char *src, uint16_t max_len);

typedef struct _json_key_t {
    char *      key;
    list_head_t list;
} json_key_t;

#define foreach_json_keys_in(src, iter_key, keylist, pos)                                                              \
    for (keylist = (void *)LITE_json_keys_of((char *)src, ""),                                                         \
        pos = (void *)list_first_entry((list_head_t *)keylist, json_key_t, list), iter_key = ((json_key_t *)pos)->key; \
         (iter_key = ((json_key_t *)pos)->key); pos = list_next_entry((json_key_t *)pos, list, json_key_t))

typedef struct {
    void * str_data;
    size_t str_len;
} dt_array_elem_t;

/**
 * @brief callback function type of parsing one object json string to struct
 *
 * @param json_str		 json string
 * @param str_len		 the length of json_str
 * @param obj    	     the buffer for the struct
 * @param item_sz 	     the size of struct
 * @return				 0 for success, negative for failure
 */
typedef int (*json_object_parse_t)(const char *json_str, size_t str_len, void *obj, size_t obj_len);

/**
 * @brief format array json string for array of string or object
 *
 * @param out_res		 the buffer for the result string
 * @param out_sz		 the size of out_res
 * @param items 	     the input items, if array is for object, convert items to json string
 * @param item_sz 	     the size of items
 * @return				 true for success
 */
int LITE_dt_format_strobj_array(char *out_res, size_t out_sz, char *items[], size_t item_sz);

/**
 * @brief format array json string for array of primitive types, int & float supported
 *
 * @param out_res		 the buffer for the result string
 * @param out_sz		 the size of out_res
 * @param data   	     the input data storing primitive types
 * @param data_sz 	     the size of data in bytes
 * @param type   		 the primitive type enum
 * @return				 true for success
 */
int LITE_dt_format_primitive_array(char *out_res, size_t out_sz, void *data, size_t data_sz, int type);

/**
 * @brief parse array of primitive type(int32 or float) [12, 34] -> int32 array {12, 34}
 *          [12.34, 56.78] -> float array {12.34, 56.78}
 *
 * @param out_res		 the buffer for array of the primitive type
 * @param out_len		 the buffer length of out_res
 * @param json_str   	 the array json string
 * @param type   		 the primitive type int32 or float
 * @return				 the length of json array, negative if there is error
 */
int LITE_dt_parse_primitive_array(void *out_res, size_t out_len, const char *json_str, int type);

/**
 * @brief parse array of string, ["aaaaa", "bbbbb", "ccccc"] -> char *str[] = {"aaaaa", "bbbbb", "ccccc"}
 *
 * @param out_res		 the buffer for result, a array of char * pointers
 * @param arr_len		 the array length of string
 * @param json_str   	 the array json string
 * @return				 the length of array, negative if there is error
 */
int LITE_dt_parse_str_array(char *out_res[], size_t arr_len, size_t str_len, const char *json_str);

/**
 * @brief parse array of object, [{"Red":100, "Green":128, "Blue":34}, {"Red":20, "Green":28, "Blue":34}] -> struct rgb
 * {int red, green, blue} rgbs[2]
 *                              {{.red = 100, .green = 128, .blue = 34}, {.red = 20, .green = 28, .blue = 34}}
 *
 * @param out_res		 the buffer for result, a array of struct
 * @param arr_len		 the array length
 * @param obj_len		 the length of the struct
 * @param json_str   	 the array json string
 * @param parse_fn   	 the function of parsing a object json string to struct
 * @return				 the length of array, negative if there is error
 */
int LITE_dt_parse_obj_array(void *out_res, size_t arr_len, size_t obj_len, const char *json_str,
                            json_object_parse_t parse_fn);
#ifdef __cplusplus
}
#endif

#endif /* __LITE_UTILS_H__ */
