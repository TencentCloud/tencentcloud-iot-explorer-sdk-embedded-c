/*
 * Tencent is pleased to support the open source community by making IoT Hub
 available.
 * Copyright (C) 2018-2020 THL A29 Limited, a Tencent company. All rights
 reserved.

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

#ifdef LOG_UPLOAD

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lite-utils.h"
#include "utils_param_check.h"
#include "log_upload.h"
#include "qcloud_iot_common.h"
#include "utils_hmac.h"
#include "utils_httpc.h"
#include "utils_timer.h"

/* log post header format */
#define TIMESTAMP_SIZE                                      (10)
#define SIGNATURE_SIZE                                      (40)
#define CTRL_BYTES_SIZE                                     (4)
// LOG_BUF_FIXED_HEADER_SIZE = 112
#define LOG_BUF_FIXED_HEADER_SIZE                                              \
  (SIGNATURE_SIZE + CTRL_BYTES_SIZE + MAX_SIZE_OF_PRODUCT_ID +                 \
   MAX_SIZE_OF_DEVICE_NAME + TIMESTAMP_SIZE)

/* do immediate log update if buffer is lower than this threshold (about two max log item) */
#define LOG_LOW_BUFFER_THRESHOLD                            (LOG_UPLOAD_BUFFER_SIZE / 4)
#define SIGN_KEY_SIZE                                       (24)

#define POINTER_CHECK_RET_ERR(ptr, err)                    \
    do {                                                   \
        if (NULL == (ptr)) {                               \
            return (err);                                  \
        }                                                  \
    } while (0)

#define POINTER_CHECK_RET(ptr)                             \
    do {                                                   \
        if (NULL == (ptr)) {                               \
            return;                                        \
        }                                                  \
    } while (0)


/* Log upload feature switch */
/* To check log http server return msg or not */
#define LOG_CHECK_HTTP_RET_CODE
/*Log http client*/
typedef struct {
    const char *url;
    const char *ca_crt;
    int port;
    HTTPClient http;          /* http client */
    HTTPClientData http_data; /* http client data */
} LogHTTPStruct;

/*Log  client*/
typedef struct {
    DeviceInfo dev_info;
    void *mqtt_client;
    LogHTTPStruct *http_client;
    bool upload_only_in_comm_err;

    char *log_buffer;
    uint32_t log_index;
    void *lock_buf;
    char sign_key[SIGN_KEY_SIZE + 1];

    long  system_time;
    Timer upload_timer;
    Timer time_update_timer;

    LogSaveFunc save_func;
    LogReadFunc read_func;
    LogDelFunc del_func;
    LogGetSizeFunc get_size_func;

    bool log_save_enabled;
    bool log_client_init_done;
} Qcloud_IoT_Log;
static Qcloud_IoT_Log *sg_log_client = NULL;

static void _set_log_client(void *client)
{
    sg_log_client = client;
}

Qcloud_IoT_Log *get_log_client(void)
{
    return sg_log_client;
}

void *get_log_mqtt_client(void)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    if(pLogClient) {
        return pLogClient->mqtt_client;
    } else {
        return NULL;
    }
}

void * get_log_dev_info(void)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    if(pLogClient) {
        return (void *)&pLogClient->dev_info;
    } else {
        return NULL;
    }
}

bool is_log_uploader_init(void)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    if(pLogClient) {
        return pLogClient->log_client_init_done;
    } else {
        return NULL;
    }
}

#ifdef AUTH_MODE_CERT
static int _gen_key_from_cert_file(char *sign_key, const char *file_path)
{
    FILE *fp;
    int len;
    char line_buf[128] = {0};

    if ((fp = fopen(file_path, "r")) == NULL) {
        UPLOAD_ERR("fail to open cert file %s", file_path);
        return -1;
    }

    /* find the begin line */
    do {
        if (NULL == fgets(line_buf, sizeof(line_buf), fp)) {
            UPLOAD_ERR("fail to fgets file %s", file_path);
            return -1;
        }
    } while (strstr(line_buf, "-----BEGIN ") == NULL);

    if (feof(fp)) {
        UPLOAD_ERR("invalid cert file %s", file_path);
        return -1;
    }

    if (NULL == fgets(line_buf, sizeof(line_buf), fp)) {
        UPLOAD_ERR("fail to fgets file %s", file_path);
        return -1;
    }

    len = strlen(line_buf);
    memcpy(sign_key, line_buf, len > SIGN_KEY_SIZE ? SIGN_KEY_SIZE : len);
    UPLOAD_DBG("sign key %s", sign_key);

    fclose(fp);

    return 0;
}
#endif

static void _reset_log_buffer(Qcloud_IoT_Log *pLogClient)
{
    pLogClient->log_index = LOG_BUF_FIXED_HEADER_SIZE;
    memset(pLogClient->log_buffer + LOG_BUF_FIXED_HEADER_SIZE, 0,\
           LOG_UPLOAD_BUFFER_SIZE - LOG_BUF_FIXED_HEADER_SIZE);
}

int init_log_uploader(LogUploadInitParams *init_params)
{
    Qcloud_IoT_Log *pLogClient = NULL;

    if (is_log_uploader_init()) {
        UPLOAD_DBG("log client ");
        return QCLOUD_RET_SUCCESS;
    }

    if (init_params == NULL || init_params->product_id == NULL
        || init_params->device_name == NULL || init_params->sign_key == NULL) {
        UPLOAD_ERR("invalid init parameters");
        return QCLOUD_ERR_INVAL;
    }

    int key_len = strlen(init_params->sign_key);
    if (key_len == 0) {
        UPLOAD_ERR("invalid key length");
        return QCLOUD_ERR_INVAL;
    }

    pLogClient = HAL_Malloc(sizeof(Qcloud_IoT_Log));
    if (NULL == pLogClient) {
        UPLOAD_ERR("allocate for log client failed");
        goto err_exit;
    }
    memset(pLogClient, 0, sizeof(Qcloud_IoT_Log));
    memset(pLogClient->dev_info.product_id, '\0', MAX_SIZE_OF_PRODUCT_ID);
    memset(pLogClient->dev_info.device_name, '\0', MAX_SIZE_OF_DEVICE_NAME);
    strncpy(pLogClient->dev_info.product_id, init_params->product_id, MAX_SIZE_OF_PRODUCT_ID);
    strncpy(pLogClient->dev_info.device_name, init_params->device_name, MAX_SIZE_OF_DEVICE_NAME);
    pLogClient->mqtt_client = NULL;
    pLogClient->system_time = 0;
    pLogClient->upload_only_in_comm_err = false;

    /* all the call back functions are necessary to handle log save and re-upload*/
    if (init_params->save_func != NULL && init_params->read_func != NULL
        && init_params->del_func != NULL && init_params->get_size_func) {
        pLogClient->save_func = init_params->save_func;
        pLogClient->read_func = init_params->read_func;
        pLogClient->del_func = init_params->del_func;
        pLogClient->get_size_func = init_params->get_size_func;
        pLogClient->log_save_enabled = true;
    } else {
        pLogClient->log_save_enabled = false;
    }

    InitTimer(&pLogClient->upload_timer);
    InitTimer(&pLogClient->time_update_timer);
    if ((pLogClient->lock_buf = HAL_MutexCreate()) == NULL) {
        UPLOAD_ERR("mutex create failed");
        goto err_exit;
    }

    pLogClient->log_buffer = HAL_Malloc(LOG_UPLOAD_BUFFER_SIZE);
    if (pLogClient->log_buffer == NULL) {
        UPLOAD_ERR("malloc log buffer failed");
        goto err_exit;
    }
    memset(pLogClient->log_buffer, '#', LOG_BUF_FIXED_HEADER_SIZE);

    /*init sign key*/
    memset(pLogClient->sign_key, 0, SIGN_KEY_SIZE);
#ifdef AUTH_MODE_CERT
    if (_gen_key_from_cert_file(pLogClient->sign_key, init_params->sign_key) != 0) {
        UPLOAD_ERR("gen_key_from_file failed");
        goto err_exit;
    }
    pLogClient->log_buffer[SIGNATURE_SIZE] = 'C';
#else
    memcpy(pLogClient->sign_key, init_params->sign_key, key_len > SIGN_KEY_SIZE ? SIGN_KEY_SIZE : key_len);
    pLogClient->log_buffer[SIGNATURE_SIZE] = 'P';
#endif

    memcpy(pLogClient->log_buffer + SIGNATURE_SIZE + CTRL_BYTES_SIZE,\
           init_params->product_id, MAX_SIZE_OF_PRODUCT_ID);
    memcpy(pLogClient->log_buffer + SIGNATURE_SIZE + CTRL_BYTES_SIZE + MAX_SIZE_OF_PRODUCT_ID,\
           init_params->device_name, strlen(init_params->device_name));

    pLogClient->http_client = HAL_Malloc(sizeof(LogHTTPStruct));
    if (NULL == pLogClient->http_client) {
        UPLOAD_ERR("allocate for LogHTTPStruct failed");
        goto err_exit;
    }
    memset(pLogClient->http_client, 0, sizeof(LogHTTPStruct));

    /* set http request-header parameter */
    pLogClient->http_client->http.header = "Accept:application/json;*/*\r\n";
    pLogClient->http_client->url = LOG_UPLOAD_SERVER_URL;
    pLogClient->http_client->port = LOG_UPLOAD_SERVER_PORT;
    pLogClient->http_client->ca_crt = NULL;

    _reset_log_buffer(pLogClient);
    _set_log_client(pLogClient);
    pLogClient->log_client_init_done = true;

    return QCLOUD_RET_SUCCESS;

err_exit:

    if(pLogClient) {
        if(pLogClient->http_client) {
            HAL_Free(pLogClient->http_client);
            pLogClient->http_client = NULL;
        }

        if(pLogClient->log_buffer) {
            HAL_Free(pLogClient->log_buffer);
            pLogClient->log_buffer = NULL;
        }

        if(pLogClient->lock_buf) {
            HAL_MutexDestroy(pLogClient->lock_buf);
            pLogClient->lock_buf = NULL;
        }

        HAL_Free(pLogClient);
        pLogClient = NULL;
    }

    return QCLOUD_ERR_FAILURE;
}

void fini_log_uploader(void)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET(pLogClient);
    if (!pLogClient->log_client_init_done) {
        return;
    }

    HAL_MutexLock(pLogClient->lock_buf);
    HAL_Free(pLogClient->http_client);
    pLogClient->http_client = NULL;
    HAL_Free(pLogClient->log_buffer);
    pLogClient->log_buffer = NULL;
    HAL_MutexUnlock(pLogClient->lock_buf);
    HAL_MutexDestroy(pLogClient->lock_buf);
    HAL_Free(pLogClient);
    pLogClient = NULL;
}

void set_log_mqtt_client(void *client)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET(pLogClient);
    if (!pLogClient->log_client_init_done) {
        return;
    }

    pLogClient->mqtt_client = client;
}

void set_log_upload_in_comm_err(bool value)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET(pLogClient);
    if (!pLogClient->log_client_init_done) {
        return;
    }

    pLogClient->upload_only_in_comm_err = value;
}

int append_to_upload_buffer(const char *log_content, size_t log_size)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET_ERR(pLogClient, -1);
    if (!pLogClient->log_client_init_done) {
        return -1;
    }

    if (log_content == NULL || log_size == 0) {
        UPLOAD_ERR("invalid log content!");
        return -1;
    }

    if (HAL_MutexTryLock(pLogClient->lock_buf) != 0) {
        UPLOAD_ERR("trylock buffer failed!");
        return -1;
    }

    if ((pLogClient->log_index + log_size + 1) > LOG_UPLOAD_BUFFER_SIZE) {
        countdown_ms(&pLogClient->upload_timer, 0);
        HAL_MutexUnlock(pLogClient->lock_buf);
        UPLOAD_ERR("log upload buffer is not enough!");
        return -1;
    }

    memcpy(pLogClient->log_buffer + pLogClient->log_index, log_content, log_size);
    pLogClient->log_index += log_size;

    /* replace \r\n to \n\f as delimiter */
    pLogClient->log_buffer[pLogClient->log_index - 1] = '\f';
    pLogClient->log_buffer[pLogClient->log_index - 2] = '\n';

    HAL_MutexUnlock(pLogClient->lock_buf);
    return 0;
}

void clear_upload_buffer(void)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET(pLogClient);
    if (!pLogClient->log_client_init_done) {
        return;
    }

    HAL_MutexLock(pLogClient->lock_buf);
    _reset_log_buffer(pLogClient);
    HAL_MutexUnlock(pLogClient->lock_buf);
}

static long _get_system_time(Qcloud_IoT_Log *pLogClient)
{
#ifdef SYSTEM_COMM
    long sys_time = 0;
    int  rc       = IOT_Get_Sys_Resource(pLogClient->mqtt_client, eRESOURCE_TIME, &pLogClient->dev_info, &sys_time);
    if (rc == QCLOUD_RET_SUCCESS)
        return sys_time;
    else
        return 0;
#else
    return 0;
#endif
}

static void _update_system_time(Qcloud_IoT_Log *pLogClient)
{
    /* to avoid frequent get_system_time */
#define LOG_TIME_UPDATE_INTERVAL 2
    if (!expired(&pLogClient->time_update_timer)) {
        return;
    }
    pLogClient->system_time = _get_system_time(pLogClient);
    countdown(&pLogClient->time_update_timer, LOG_TIME_UPDATE_INTERVAL);
}

static int _check_server_connection(Qcloud_IoT_Log *pLogClient)
{
    int rc;
    rc = qcloud_http_client_connect(&pLogClient->http_client->http, pLogClient->http_client->url, \
                                    pLogClient->http_client->port, pLogClient->http_client->ca_crt);
    if (rc != QCLOUD_RET_SUCCESS) {
        return rc;
    }
    qcloud_http_client_close(&pLogClient->http_client->http);

    return QCLOUD_RET_SUCCESS;
}

#ifdef LOG_CHECK_HTTP_RET_CODE
static bool _get_json_ret_code(char *json, int32_t *res)
{
    char *v = LITE_json_value_of("Retcode", json);
    if (v == NULL) {
        UPLOAD_ERR("Invalid json content: %s", json);
        return false;
    }
    if (LITE_get_int32(res, v) != QCLOUD_RET_SUCCESS) {
        UPLOAD_ERR("Invalid json content: %s", json);
        HAL_Free(v);
        return false;
    }
    HAL_Free(v);
    return true;
}
#endif

static int _post_one_http_to_server(char *post_buf, size_t post_size)
{
    int rc = 0;
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET_ERR(pLogClient, QCLOUD_ERR_INVAL);
    POINTER_CHECK_RET_ERR(pLogClient->http_client, QCLOUD_ERR_INVAL);
    if (!pLogClient->log_client_init_done) {
        return QCLOUD_ERR_FAILURE;
    }

    pLogClient->http_client->http_data.post_content_type = "text/plain;charset=utf-8";
    pLogClient->http_client->http_data.post_buf          = post_buf;
    pLogClient->http_client->http_data.post_buf_len      = post_size;
    rc = qcloud_http_client_common(&pLogClient->http_client->http, pLogClient->http_client->url, \
                                   pLogClient->http_client->port, pLogClient->http_client->ca_crt, \
                                   HTTP_POST, &pLogClient->http_client->http_data);
    if (rc != QCLOUD_RET_SUCCESS) {
        UPLOAD_ERR("qcloud_http_client_common failed, rc = %d", rc);
        return rc;
    }
    UPLOAD_DBG("Log client POST size: %d", post_size);

#ifdef LOG_CHECK_HTTP_RET_CODE
    /* TODO: handle recv data from log server */
#define HTTP_RET_JSON_LENGTH     256
#define HTTP_WAIT_RET_TIMEOUT_MS 1000
    char buf[HTTP_RET_JSON_LENGTH]        = {0};
    pLogClient->http_client->http_data.response_buf     = buf;
    pLogClient->http_client->http_data.response_buf_len = sizeof(buf);

    rc = qcloud_http_recv_data(&pLogClient->http_client->http, HTTP_WAIT_RET_TIMEOUT_MS, &pLogClient->http_client->http_data);
    if (QCLOUD_RET_SUCCESS != rc) {
        UPLOAD_ERR("qcloud_http_recv_data failed, rc = %d", rc);
    } else {
        int32_t ret = -1;

        buf[HTTP_RET_JSON_LENGTH - 1] = '\0';  // json_parse relies on a string
        if (strlen(buf) > 0 && _get_json_ret_code(buf, &ret) && ret == 0) {
            UPLOAD_DBG("Log server return SUCCESS: %s", buf);
        } else {
            UPLOAD_ERR("Log server return FAIL(%d): %s", ret, buf);
            rc = QCLOUD_ERR_HTTP;
        }
    }
#endif

    qcloud_http_client_close(&pLogClient->http_client->http);

    return rc;
}

static void update_time_and_signature(char *log_buf, size_t log_size)
{
    char timestamp[TIMESTAMP_SIZE + 1] = {0};
    char signature[SIGNATURE_SIZE + 1] = {0};

    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET(pLogClient);
    if (!pLogClient->log_client_init_done) {
        return;
    }

    /* get system time from IoT hub first */
    _update_system_time(pLogClient);

    /* record the timestamp for this log uploading */
    HAL_Snprintf(timestamp, TIMESTAMP_SIZE + 1, "%010ld", pLogClient->system_time);
    memcpy(log_buf + LOG_BUF_FIXED_HEADER_SIZE - TIMESTAMP_SIZE, timestamp, strlen(timestamp));

    /* signature of this log uploading */
    utils_hmac_sha1(log_buf + SIGNATURE_SIZE, log_size - SIGNATURE_SIZE, signature, pLogClient->sign_key, strlen(pLogClient->sign_key));
    memcpy(log_buf, signature, SIGNATURE_SIZE);
}

static int post_log_to_server(char *post_buf, size_t post_size, size_t *actual_post_payload)
{
#define LOG_DELIMITER "\n\f"
    int ret = QCLOUD_RET_SUCCESS;
    /* one shot upload */
    if (post_size < MAX_HTTP_LOG_POST_SIZE) {
        update_time_and_signature(post_buf, post_size);
        ret = _post_one_http_to_server(post_buf, post_size);
        if (QCLOUD_RET_SUCCESS == ret) {
            *actual_post_payload = post_size - LOG_BUF_FIXED_HEADER_SIZE;
        } else {
            UPLOAD_ERR("one time log send failed");
            *actual_post_payload = 0;
        }
        return ret;
    }

    /* Log size is larger than one HTTP post size */
    /* Fragment the log and upload multi-times */
    UPLOAD_DBG("to post large log size %d", post_size);
    *actual_post_payload  = 0;
    size_t delimiter_len  = strlen(LOG_DELIMITER);
    size_t orig_post_size = post_size;
    size_t post_payload, upload_size, possible_size;
    do {
        char *next_log_buf = NULL;
        possible_size      = 0;
        while (possible_size < MAX_HTTP_LOG_POST_SIZE) {
            /*remember last valid position */
            upload_size = possible_size;
            /* locate the delimiter */
            next_log_buf = strstr(post_buf + upload_size, LOG_DELIMITER);
            if (next_log_buf == NULL) {
                UPLOAD_ERR("Invalid log delimiter. Total sent: %d. Left: %d",
                           *actual_post_payload + LOG_BUF_FIXED_HEADER_SIZE, post_size);
                return QCLOUD_ERR_INVAL;
            }
            possible_size = (size_t)(next_log_buf - post_buf + delimiter_len);
            /* end of log */
            if (next_log_buf[delimiter_len] == 0 && possible_size < MAX_HTTP_LOG_POST_SIZE) {
                upload_size = possible_size;
                break;
            }
        }

        if (upload_size == 0) {
            UPLOAD_ERR("Upload size should not be 0! Total sent: %d. Left: %d",
                       *actual_post_payload + LOG_BUF_FIXED_HEADER_SIZE, post_size);
            return QCLOUD_ERR_FAILURE;
        }

        update_time_and_signature(post_buf, upload_size);
        ret = _post_one_http_to_server(post_buf, upload_size);
        if (QCLOUD_RET_SUCCESS != ret) {
            UPLOAD_ERR("Send log failed. Total sent: %d. Left: %d", *actual_post_payload + LOG_BUF_FIXED_HEADER_SIZE,
                       post_size);
            return QCLOUD_ERR_FAILURE;
        }

        /* move the left log forward and do next upload */
        memmove(post_buf + LOG_BUF_FIXED_HEADER_SIZE, post_buf + upload_size, post_size - upload_size);
        post_payload = upload_size - LOG_BUF_FIXED_HEADER_SIZE;
        post_size -= post_payload;
        *actual_post_payload += post_payload;
        memset(post_buf + post_size, 0, orig_post_size - post_size);
        UPLOAD_DBG("post log %d OK. Total sent: %d. Left: %d", upload_size,
                   *actual_post_payload + LOG_BUF_FIXED_HEADER_SIZE, post_size);
    } while (post_size > LOG_BUF_FIXED_HEADER_SIZE);

    return QCLOUD_RET_SUCCESS;
}

static int _save_log(char *log_buf, size_t log_size)
{
    int rc = 0;
    size_t write_size, current_size;
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET_ERR(pLogClient, QCLOUD_ERR_INVAL);
    if (!pLogClient->log_client_init_done) {
        return QCLOUD_ERR_FAILURE;
    }
    current_size = pLogClient->get_size_func();

    /* overwrite the previous saved log to avoid too many saved logs */
    if ((current_size + log_size) > MAX_LOG_SAVE_SIZE) {
        UPLOAD_ERR("overwrite the previous saved log. %d", current_size);
        rc = pLogClient->del_func();
        if (rc) {
            Log_e("fail to delete previous log");
        }
    }

    write_size = pLogClient->save_func(log_buf, log_size);
    if (write_size != log_size) {
        Log_e("fail to save log. RC %d - log size %d", write_size, log_size);
        rc = -1;
    } else
        rc = 0;

    return rc;
}

static int _handle_saved_log(void)
{
    int    rc             = QCLOUD_RET_SUCCESS;
    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET_ERR(pLogClient, QCLOUD_ERR_INVAL);
    if (!pLogClient->log_client_init_done) {
        return QCLOUD_ERR_FAILURE;
    }

    size_t whole_log_size = pLogClient->get_size_func();
    if (whole_log_size > 0) {
        /* only do the job when connection is OK */
        if (_check_server_connection(pLogClient) != QCLOUD_RET_SUCCESS)
            return QCLOUD_ERR_FAILURE;

        size_t buf_size = whole_log_size + LOG_BUF_FIXED_HEADER_SIZE + 1;
        char * log_buf  = HAL_Malloc(buf_size);
        if (log_buf != NULL) {
            /* read the whole log to buffer */
            size_t read_len = pLogClient->read_func(log_buf + LOG_BUF_FIXED_HEADER_SIZE, whole_log_size);
            if (read_len == whole_log_size) {
                size_t upload_size = whole_log_size + LOG_BUF_FIXED_HEADER_SIZE;

                /* copy header from global log buffer */
                memcpy(log_buf, pLogClient->log_buffer, LOG_BUF_FIXED_HEADER_SIZE);
                log_buf[buf_size - 1] = 0;

                size_t actual_post_payload;
                rc = post_log_to_server(log_buf, upload_size, &actual_post_payload);
                if (rc == QCLOUD_RET_SUCCESS || rc == QCLOUD_ERR_INVAL) {
                    Log_d("handle saved log done! Size: %d. upload paylod: %d", whole_log_size, actual_post_payload);
                    pLogClient->del_func();
                }
                HAL_Free(log_buf);
            } else {
                Log_e("fail to read whole saved log. Size: %u - read: %u", whole_log_size, read_len);
                HAL_Free(log_buf);
                return QCLOUD_ERR_FAILURE;
            }

        } else {
            Log_e("Malloc failed, size: %u", buf_size);
            return QCLOUD_ERR_FAILURE;
        }
    }

    return rc;
}

static bool _check_force_upload( bool force_upload)
{
    Qcloud_IoT_Log *pLogClient = get_log_client();
    if (!force_upload) {
        /* Double check if the buffer is low */
        HAL_MutexLock(pLogClient->lock_buf);
        bool is_low_buffer = (LOG_UPLOAD_BUFFER_SIZE - pLogClient->log_index) < LOG_LOW_BUFFER_THRESHOLD ? true : false;

        /* force_upload is false and upload_only_in_comm_err is true */
        if (pLogClient->upload_only_in_comm_err) {
            /* buffer is low but we couldn't upload now, reset buffer */
            if (is_low_buffer) {
                _reset_log_buffer(pLogClient);
            }
            HAL_MutexUnlock(pLogClient->lock_buf);
            countdown_ms(&pLogClient->upload_timer, LOG_UPLOAD_INTERVAL_MS);
            return false;
        }
        HAL_MutexUnlock(pLogClient->lock_buf);

        if (is_low_buffer) {
            /* buffer is low, handle it right now */
            return true;
        } else {
            return expired(&pLogClient->upload_timer);
        }

    } else {
        return true;
    }
}

int do_log_upload(bool force_upload)
{
    int         rc;
    int         upload_log_size    = 0;
    static bool unhandle_saved_log = true;

    Qcloud_IoT_Log *pLogClient = get_log_client();
    POINTER_CHECK_RET_ERR(pLogClient, QCLOUD_ERR_INVAL);
    if (!pLogClient->log_client_init_done) {
        return QCLOUD_ERR_FAILURE;
    }

    /* double check force upload */
    if (!_check_force_upload(force_upload))
        return QCLOUD_RET_SUCCESS;

    /* handle previously saved log */
    if (pLogClient->log_save_enabled && unhandle_saved_log) {
        rc = _handle_saved_log();
        if (rc == QCLOUD_RET_SUCCESS) {
            unhandle_saved_log = false;
        }
    }

    /* no more log in buffer */
    if (pLogClient->log_index == LOG_BUF_FIXED_HEADER_SIZE) {
        return QCLOUD_RET_SUCCESS;
    }

    HAL_MutexLock(pLogClient->lock_buf);
    upload_log_size = pLogClient->log_index;
    HAL_MutexUnlock(pLogClient->lock_buf);

    size_t actual_post_payload;
    rc = post_log_to_server(pLogClient->log_buffer, upload_log_size, &actual_post_payload);
    if (rc != QCLOUD_RET_SUCCESS) {
        /* save log via user callbacks when log upload fail */
        if (pLogClient->log_save_enabled) {
            /* new error logs should have been added, update log size */
            HAL_MutexLock(pLogClient->lock_buf);
            /* parts of log were uploaded succesfully. Need to move the new logs
             * forward */
            if (actual_post_payload) {
                UPLOAD_DBG("move the new log %d forward", actual_post_payload);
                memmove(pLogClient->log_buffer + upload_log_size - actual_post_payload,\
                        pLogClient->log_buffer + upload_log_size,\
                        pLogClient->log_index - upload_log_size);
                pLogClient->log_index = pLogClient->log_index - actual_post_payload;
                memset(pLogClient->log_buffer + pLogClient->log_index, 0,\
                       LOG_UPLOAD_BUFFER_SIZE - pLogClient->log_index);
            }
            upload_log_size = pLogClient->log_index;
            HAL_MutexUnlock(pLogClient->lock_buf);
            _save_log(pLogClient->log_buffer + LOG_BUF_FIXED_HEADER_SIZE,\
                      upload_log_size - LOG_BUF_FIXED_HEADER_SIZE);
            unhandle_saved_log = true;
        }
    }

    /* move the new log during send_log_to_server */
    HAL_MutexLock(pLogClient->lock_buf);
    if (upload_log_size == pLogClient->log_index) {
        _reset_log_buffer(pLogClient);
    } else {
        memmove(pLogClient->log_buffer + LOG_BUF_FIXED_HEADER_SIZE,\
                pLogClient->log_buffer + upload_log_size, pLogClient->log_index - upload_log_size);
        pLogClient->log_index = pLogClient->log_index - upload_log_size + LOG_BUF_FIXED_HEADER_SIZE;
        memset(pLogClient->log_buffer + pLogClient->log_index, 0,
               LOG_UPLOAD_BUFFER_SIZE - pLogClient->log_index);
    }
    HAL_MutexUnlock(pLogClient->lock_buf);
    countdown_ms(&pLogClient->upload_timer, LOG_UPLOAD_INTERVAL_MS);

    return QCLOUD_RET_SUCCESS;
}

#endif

#ifdef __cplusplus
}
#endif
