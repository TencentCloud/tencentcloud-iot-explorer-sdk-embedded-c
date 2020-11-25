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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "lite-utils.h"
#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"

// resource list to record all the download resource
#define RESOURCE_FILE_LIST_PATH "./resource_list.json"

#define KEY_NAME     "name"
#define KEY_VER      "version"
#define KEY_SIZE     "downloaded_size"
#define KEY_RES_LIST "resource_list"

#define TYPE_FILE_STR "FILE"

#define MAX_RESOURCE_LIST_ONE_LINE_LEN (256)

// NOTE: this is according to memory of system left for resource and QCLOUD_IOT_MQTT_TX_BUF_LEN.
// when publish message should no longer than QCLOUD_IOT_MQTT_TX_BUF_LEN.
// always keep RESOURCE_MAX_NUM * RESOURCE_INFO_LEN_MAX  < min(QCLOUD_IOT_MQTT_TX_BUF_LEN, system memory)
#define RESOURCE_MAX_NUM 20

#define RESOURCE_BUF_LEN            5000
#define RESOURCE_INFO_FILE_DATA_LEN 128
#define RESOURCE_FILE_PATH_MAX_LEN  128

typedef struct {
    // handle & mqtt
    void *resource_handle;
    void *mqtt_client;

    // to make sure report is acked
    bool report_pub_ack;
    int  report_packet_id;

    // resource list to record all the resource infos, which include file name, version, file type of resource
    char resource_list_file_path[RESOURCE_FILE_PATH_MAX_LEN];

    // local file path and info path of resource downloading now, update when receive a new resource update message
    char local_info_file_path[RESOURCE_FILE_PATH_MAX_LEN];
    char local_file_path[RESOURCE_FILE_PATH_MAX_LEN];

    // resource info of resource downloading now, update when receive a new resource update message
    char     local_version[RESOURCE_VERSION_STR_LEN_MAX + 1];
    char     remote_version[RESOURCE_VERSION_STR_LEN_MAX + 1];
    char     resource_name[RESOURCE_NAME_STR_LEN_MAX + 1];
    char     resource_type[RESOURCE_TYPE_STR_LEN_MAX + 1];
    uint32_t file_size;
    uint32_t downloaded_size;
} ResourceContextData;

static void _event_handler(void *pClient, void *handle_context, MQTTEventMsg *msg)
{
    uintptr_t            packet_id = (uintptr_t)msg->msg;
    ResourceContextData *res_ctx   = (ResourceContextData *)handle_context;

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
            if (res_ctx->report_packet_id == packet_id) {
                res_ctx->report_pub_ack = true;
            }
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

static int _setup_connect_init_params(MQTTInitParams *initParams, void *ota_ctx, DeviceInfo *device_info)
{
    initParams->region      = device_info->region;
    initParams->product_id  = device_info->product_id;
    initParams->device_name = device_info->device_name;

#ifdef AUTH_MODE_CERT
    char  certs_dir[16] = "certs";
    char  current_path[128];
    char *cwd = getcwd(current_path, sizeof(current_path));

    if (cwd == NULL) {
        Log_e("getcwd return NULL");
        return QCLOUD_ERR_FAILURE;
    }

#ifdef WIN32
    HAL_Snprintf(initParams->cert_file, RESOURCE_FILE_PATH_MAX_LEN, "%s\\%s\\%s", current_path, certs_dir,
                 device_info->dev_cert_file_name);
    HAL_Snprintf(initParams->key_file, RESOURCE_FILE_PATH_MAX_LEN, "%s\\%s\\%s", current_path, certs_dir,
                 device_info->dev_key_file_name);
#else
    HAL_Snprintf(initParams->cert_file, RESOURCE_FILE_PATH_MAX_LEN, "%s/%s/%s", current_path, certs_dir,
                 device_info->dev_cert_file_name);
    HAL_Snprintf(initParams->key_file, RESOURCE_FILE_PATH_MAX_LEN, "%s/%s/%s", current_path, certs_dir,
                 device_info->dev_key_file_name);
#endif

#else
    initParams->device_secret = device_info->device_secret;
#endif

    initParams->command_timeout        = QCLOUD_IOT_MQTT_COMMAND_TIMEOUT;
    initParams->keep_alive_interval_ms = QCLOUD_IOT_MQTT_KEEP_ALIVE_INTERNAL;

    initParams->auto_connect_enable  = 1;
    initParams->event_handle.h_fp    = _event_handler;
    initParams->event_handle.context = ota_ctx;

    return QCLOUD_RET_SUCCESS;
}

static void _wait_for_pub_ack(ResourceContextData *res_ctx, int packet_id)
{
    int wait_cnt              = 10;
    res_ctx->report_pub_ack   = false;
    res_ctx->report_packet_id = packet_id;

    while (!res_ctx->report_pub_ack) {
        HAL_SleepMs(500);
        IOT_MQTT_Yield(res_ctx->mqtt_client, 500);
        if (wait_cnt-- == 0) {
            Log_e("wait report pub ack timeout!");
            break;
        }
    }
    res_ctx->report_pub_ack = false;
    return;
}

/**********************************************************************************
 * resource file operations START
 * these are platform-dependant functions
 * POSIX FILE is used in this sample code
 **********************************************************************************/
/* calculate left MD5 for resuming download from break point */
static int _cal_exist_resource_md5(ResourceContextData *res_ctx)
{
    char   buff[RESOURCE_BUF_LEN];
    size_t rlen, total_read = 0;
    int    ret = QCLOUD_RET_SUCCESS;

    ret = IOT_Resource_ResetClientMD5(res_ctx->resource_handle);
    if (ret) {
        Log_e("reset MD5 failed: %d", ret);
        return QCLOUD_ERR_FAILURE;
    }

    // read local file
    void *fp = HAL_FileOpen(res_ctx->local_file_path, "ab+");
    if (!fp) {
        Log_e("open file %s failed", res_ctx->local_file_path);
        return QCLOUD_ERR_FAILURE;
    }

    size_t size = res_ctx->downloaded_size;

    while ((size > 0) && (!HAL_FileEof(fp))) {
        rlen = (size > RESOURCE_BUF_LEN) ? RESOURCE_BUF_LEN : size;
        if (rlen != HAL_FileRead(buff, 1, rlen, fp)) {
            Log_e("read data len not expected");
            ret = QCLOUD_ERR_FAILURE;
            break;
        }
        IOT_Resource_UpdateClientMd5(res_ctx->resource_handle, buff, rlen);
        size -= rlen;
        total_read += rlen;
    }

    HAL_FileClose(fp);
    Log_d("total read: %d", total_read);
    return ret;
}

/* update local resource info for resuming download from break point */
static int _update_local_resource_info(ResourceContextData *res_ctx)
{
    void *fp;
    int   wlen;
    int   ret = QCLOUD_RET_SUCCESS;
    char  data_buf[RESOURCE_INFO_FILE_DATA_LEN];

    memset(data_buf, 0, sizeof(data_buf));

    // record local resource info
    HAL_Snprintf(data_buf, sizeof(data_buf), "{\"%s\":\"%s\", \"%s\":\"%s\", \"%s\":%d}", KEY_NAME,
                 res_ctx->resource_name, KEY_VER, res_ctx->remote_version, KEY_SIZE, res_ctx->downloaded_size);

    fp = HAL_FileOpen(res_ctx->local_info_file_path, "w");
    if (!fp) {
        Log_e("open file %s failed", res_ctx->local_info_file_path);
        ret = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    wlen = HAL_FileWrite(data_buf, 1, strlen(data_buf), fp);
    if (wlen != strlen(data_buf)) {
        Log_e("save version to file err");
        ret = QCLOUD_ERR_FAILURE;
    }

exit:
    if (fp) {
        HAL_FileClose(fp);
    }

    return ret;
}

/* get local resource info */
static int _get_local_resource_info(const char *file_path, char *local_version)
{
    int  len;
    int  rlen;
    char json_doc[RESOURCE_INFO_FILE_DATA_LEN] = {0};

    void *fp = HAL_FileOpen(file_path, "r");
    if (!fp) {
        Log_e("open file %s failed", file_path);
        return 0;
    }

    HAL_FileSeek(fp, 0L, SEEK_END);
    len = ftell(fp);
    if (len > RESOURCE_INFO_FILE_DATA_LEN) {
        Log_e("%s is too big, pls check", file_path);
        HAL_FileClose(fp);
        return 0;
    }

    HAL_FileRewind(fp);
    rlen = HAL_FileRead(json_doc, 1, len, fp);
    if (len != rlen) {
        Log_e("read data len (%d) less than needed (%d), %s", rlen, len, json_doc);
        HAL_FileClose(fp);
        return 0;
    }

    char *version = LITE_json_value_of(KEY_VER, json_doc);
    char *size    = LITE_json_value_of(KEY_SIZE, json_doc);

    if ((!version) || (!size)) {
        HAL_Free(version);
        HAL_Free(size);
        HAL_FileClose(fp);
        return 0;
    }

    int local_size = atoi(size);
    HAL_Free(size);

    if (local_size <= 0) {
        Log_w("local info offset invalid: %d", local_size);
        HAL_Free(version);
        HAL_FileClose(fp);
        return 0;
    }

    strncpy(local_version, version, RESOURCE_VERSION_STR_LEN_MAX);
    HAL_Free(version);
    HAL_FileClose(fp);
    return local_size;
}

/* get local firmware offset for resuming download from break point */
static int _update_resource_downloaded_size(ResourceContextData *res_ctx)
{
    int local_size = _get_local_resource_info(res_ctx->local_info_file_path, res_ctx->local_version);
    if (local_size == 0) {
        res_ctx->downloaded_size = 0;
        return 0;
    }

    if ((0 != strcmp(res_ctx->local_version, res_ctx->remote_version)) ||
        (res_ctx->downloaded_size > res_ctx->file_size)) {
        res_ctx->downloaded_size = 0;
        return 0;
    }

    res_ctx->downloaded_size = local_size;
    Log_i("calc MD5 for resuming download from offset: %d", res_ctx->downloaded_size);
    int ret = _cal_exist_resource_md5(res_ctx);
    if (ret) {
        Log_e("calculate MD5 error: %d", ret);
        HAL_FileRemove(res_ctx->local_info_file_path);
        res_ctx->downloaded_size = 0;
        return 0;
    }
    Log_d("local MD5 update done!");
    return local_size;
}

/* delete file */
static int _delete_file(const char *file_path)
{
    return HAL_FileRemove(file_path);
}

/* save data to file */
static int _save_resource_data_to_file(const char *file_path, uint32_t offset, char *buf, int len)
{
    void *fp;
    if (offset > 0) {
        if (NULL == (fp = HAL_FileOpen(file_path, "ab+"))) {
            Log_e("open file failed");
            return QCLOUD_ERR_FAILURE;
        }
    } else {
        if (NULL == (fp = HAL_FileOpen(file_path, "wb+"))) {
            Log_e("open file failed");
            return QCLOUD_ERR_FAILURE;
        }
    }

    HAL_FileSeek(fp, offset, SEEK_SET);

    if (1 != HAL_FileWrite(buf, len, 1, fp)) {
        Log_e("write data to file failed");
        HAL_FileClose(fp);
        return QCLOUD_ERR_FAILURE;
    }
    HAL_FileFlush(fp);
    HAL_FileClose(fp);

    return 0;
}

/* get resource list from resource list file */
static int _get_resource_list(const char *file_path, resInfo *res_info_list[], int *res_num)
{
    void *   fp = NULL;
    char     line_buf[MAX_RESOURCE_LIST_ONE_LINE_LEN + 1];
    char *   file_name;
    char *   version;
    char *   file_type;
    resInfo *pInfo;
    int      ret  = QCLOUD_ERR_FAILURE;
    int      rlen = 0;

    *res_num = 0;

    fp = HAL_FileOpen(file_path, "r");
    if (!fp) {
        Log_e("open file %s fail", file_path);
        return ret;
    }

    while (!HAL_FileEof(fp)) {
        memset(line_buf, '\0', MAX_RESOURCE_LIST_ONE_LINE_LEN + 1);
        rlen = HAL_FileRead(line_buf, 1, MAX_RESOURCE_LIST_ONE_LINE_LEN, fp);
        if (0 == rlen) {
            break;
        }

        file_name = LITE_json_value_of("resource_name", line_buf);
        version   = LITE_json_value_of("version", line_buf);
        file_type = LITE_json_value_of("resource_type", line_buf);

        if (!file_name || !version || !file_type) {
            HAL_Free(file_name);
            HAL_Free(version);
            HAL_Free(file_type);
            continue;
        } else if (0 == strcmp(version, "NULL")) {  // deleted resource
            HAL_Free(file_name);
            HAL_Free(version);
            HAL_Free(file_type);
            continue;
        } else {
            pInfo = (resInfo *)HAL_Malloc(sizeof(resInfo));
            if (!pInfo) {
                Log_e("malloc err");
                HAL_Free(file_name);
                HAL_Free(version);
                HAL_Free(file_type);
                goto exit;
            }
            pInfo->res_name         = file_name;
            pInfo->res_ver          = version;
            pInfo->res_type         = file_type;
            res_info_list[*res_num] = pInfo;
            (*res_num)++;
        }
    }

    ret = QCLOUD_RET_SUCCESS;
exit:
    if (fp) {
        HAL_FileClose(fp);
    }
    return ret;
}

/* update resource list when the resource has deleted or updated */
static int _update_resource_list(const char *file_path, const char *resource_name, const char *version,
                                 const char *type)
{
    FILE *   fp                = NULL;
    bool     record_exist_flag = false;
    char     line_buf[MAX_RESOURCE_LIST_ONE_LINE_LEN + 1];
    char     new_line_buf[MAX_RESOURCE_LIST_ONE_LINE_LEN + 1];
    uint32_t offset = 0;
    int      ret    = QCLOUD_ERR_FAILURE;
    int      rlen;

    memset(new_line_buf, ' ', MAX_RESOURCE_LIST_ONE_LINE_LEN + 1);
    HAL_Snprintf(new_line_buf, MAX_RESOURCE_LIST_ONE_LINE_LEN,
                 "{\"resource_name\":\"%s\", \"version\":\"%s\", \"resource_type\":\"%s\"}", resource_name,
                 (NULL == version) ? "NULL" : version, type);
    new_line_buf[strlen(new_line_buf)]               = ' ';
    new_line_buf[MAX_RESOURCE_LIST_ONE_LINE_LEN - 1] = '\n';

    fp = access(file_path, 0) ? HAL_FileOpen(file_path, "w+") : HAL_FileOpen(file_path, "r+");
    if (!fp) {
        Log_e("open file %s fail", file_path);
        return ret;
    }

    while (!HAL_FileEof(fp)) {
        memset(line_buf, '\0', MAX_RESOURCE_LIST_ONE_LINE_LEN + 1);
        rlen = HAL_FileRead(line_buf, 1, MAX_RESOURCE_LIST_ONE_LINE_LEN, fp);
        if (strstr(line_buf, resource_name)) {
            HAL_FileRewind(fp);
            HAL_FileSeek(fp, offset, SEEK_SET);
            HAL_FileWrite(new_line_buf, MAX_RESOURCE_LIST_ONE_LINE_LEN, 1, fp);
            record_exist_flag = true;
            break;
        }
        offset += rlen;
    }

    if (!record_exist_flag) {
        HAL_FileRewind(fp);
        HAL_FileSeek(fp, 0, SEEK_END);
        HAL_FileWrite(new_line_buf, MAX_RESOURCE_LIST_ONE_LINE_LEN, 1, fp);
    }
    ret = QCLOUD_RET_SUCCESS;

    if (fp) {
        HAL_FileClose(fp);
    }
    return ret;
}

/* update resource list when the resource has deleted or updated */
static int _recover_resource_list(const char *file_path, const char *resp, uint32_t len)
{
    int   ret      = QCLOUD_RET_SUCCESS;
    char *res_list = LITE_json_value_of(KEY_RES_LIST, (char *)resp);

    if (!res_list) {
        return QCLOUD_ERR_FAILURE;
    }

    Log_d("parse resource list:%s", res_list);

    char *pNext;
    char  TempBuff[MAX_RESOURCE_LIST_ONE_LINE_LEN + 1];
    char  WriteBuff[MAX_RESOURCE_LIST_ONE_LINE_LEN + 1];

    void *fp = HAL_FileOpen(file_path, "w+");
    if (!fp) {
        Log_e("open file %s fail", file_path);
        return QCLOUD_ERR_FAILURE;
    }

    pNext = (char *)strtok(res_list, "}");
    while (pNext) {
        memset(TempBuff, '\0', MAX_RESOURCE_LIST_ONE_LINE_LEN);
        HAL_Snprintf(TempBuff, MAX_RESOURCE_LIST_ONE_LINE_LEN, "%s}", pNext);
        char *pos = strchr(TempBuff, '{');
        if (NULL == pos) {
            if (NULL == strstr(TempBuff, "]}")) {
                ret = QCLOUD_ERR_FAILURE;
            }
            break;
        }
        memset(WriteBuff, ' ', MAX_RESOURCE_LIST_ONE_LINE_LEN + 1);
        HAL_Snprintf(WriteBuff, MAX_RESOURCE_LIST_ONE_LINE_LEN, "%s", pos);
        WriteBuff[strlen(WriteBuff)]                  = ' ';
        WriteBuff[MAX_RESOURCE_LIST_ONE_LINE_LEN - 1] = '\n';
        HAL_FileWrite(WriteBuff, MAX_RESOURCE_LIST_ONE_LINE_LEN, 1, fp);
        pNext = (char *)strtok(NULL, "}");
    }

    HAL_FileClose(fp);
    if (QCLOUD_RET_SUCCESS != ret) {
        HAL_FileRemove(file_path);
    }
    HAL_Free(res_list);
    return ret;
}

/**********************************************************************************
 * resource file operations END
 **********************************************************************************/

static int sg_resource_list_need_recover;

static int _resource_event_usr_cb(void *pContext, const char *msg, uint32_t msgLen, IOT_RES_UsrEvent event)
{
    int  ret = QCLOUD_RET_SUCCESS;
    char local_info_file_path[RESOURCE_FILE_PATH_MAX_LEN];

    switch (event) {
        case IOT_RES_EVENT_REPORT_VERSION_RESP:
            // recover resource info list from this msg if needed
            if (sg_resource_list_need_recover) {
                Log_d("recover res info list: %.*s", msgLen, msg);
                if (QCLOUD_RET_SUCCESS == _recover_resource_list(RESOURCE_FILE_LIST_PATH, msg, msgLen)) {
                    sg_resource_list_need_recover = false;
                    Log_d("recover local resource list success");
                } else {
                    Log_e("recover local resource list fail");
                }
            }
            break;

        case IOT_RES_EVENT_DEL_RESOURCE:
            // when event is IOT_RES_EVENT_DEL_RESOURCE, msg is file name. More can see
            // qcloud_resource_mqtt_del_resource
            Log_d("to delete file %s", msg);
			_delete_file(msg);
            // also delete info file in case of download no finish
            HAL_Snprintf(local_info_file_path, RESOURCE_FILE_PATH_MAX_LEN, "./%s_info.json", msg);
            _delete_file(local_info_file_path);
            _update_resource_list(RESOURCE_FILE_LIST_PATH, msg, NULL, TYPE_FILE_STR);
            break;

        default:
            Log_d("event not supported");
            break;
    }

    return ret;
}

static int _report_version(ResourceContextData *res_ctx)
{
    int      rc;
    resInfo *res_info_list[RESOURCE_MAX_NUM] = {NULL};

    int resNum = 0;
    int i      = 0;

    rc = _get_resource_list(res_ctx->resource_list_file_path, res_info_list, &resNum);
    if (resNum == 0 || QCLOUD_RET_SUCCESS != rc) {
        sg_resource_list_need_recover = true;
        // report NULL and recover resourcelist form cloud
        rc = IOT_Resource_ReportVersion(res_ctx->resource_handle, 0, NULL);
    } else {
        rc = IOT_Resource_ReportVersion(res_ctx->resource_handle, resNum, res_info_list);
    }

    // release memory of res_info_list
    for (i = 0; i < resNum; i++) {
        resInfo *pInfo = res_info_list[i];
        if (!pInfo) {
            continue;
        }
        HAL_Free(pInfo->res_name);
        HAL_Free(pInfo->res_ver);
        HAL_Free(pInfo->res_type);
        HAL_Free(pInfo);
    }

    if (rc < 0) {
        Log_e("report OTA version failed");
        return QCLOUD_ERR_FAILURE;
    }

    return QCLOUD_RET_SUCCESS;
}

// main resource cycle
bool process_resource(ResourceContextData *res_ctx)
{
    bool  download_finished     = false;
    bool  upgrade_fetch_success = true;
    char  buf_res[RESOURCE_BUF_LEN];
    int   rc;
    void *res_handle = res_ctx->resource_handle;

    /* Must report version first */
    if (QCLOUD_RET_SUCCESS != _report_version(res_ctx)) {
        Log_e("report OTA version failed");
        return false;
    }

    do {
        IOT_MQTT_Yield(res_ctx->mqtt_client, 200);

        Log_i("wait for ota upgrade command...");

        // recv the upgrade cmd
        if (IOT_Resource_IsFetching(res_handle)) {
            IOT_Resource_Ioctl(res_handle, IOT_RES_CMD_FILE_NAME, res_ctx->resource_name,
                               sizeof(res_ctx->resource_name));
            IOT_Resource_Ioctl(res_handle, IOT_RES_CMD_VERSION, res_ctx->remote_version,
                               sizeof(res_ctx->remote_version));
            IOT_Resource_Ioctl(res_handle, IOT_RES_CMD_FILE_TYPE, res_ctx->resource_type,
                               sizeof(res_ctx->resource_type));
            IOT_Resource_Ioctl(res_handle, IOT_RES_CMD_FILE_SIZE, &res_ctx->file_size, 4);

            HAL_Snprintf(res_ctx->local_file_path, RESOURCE_FILE_PATH_MAX_LEN, "./%s", res_ctx->resource_name);
            HAL_Snprintf(res_ctx->local_info_file_path, RESOURCE_FILE_PATH_MAX_LEN, "./%s_info.json",
                         res_ctx->resource_name);

            /* check if pre-downloading finished or not */
            /* if local resource downloaded size (res_ctx->downloaded_size) is not zero, it
             * will do resuming download */
            int local_offset = _update_resource_downloaded_size(res_ctx);
            if (res_ctx->file_size == local_offset) {  // have already downloaded
                upgrade_fetch_success = true;
                goto end_of_download;
            }

            /*set offset and start http connect*/
            rc = IOT_Resource_StartDownload(res_handle, res_ctx->downloaded_size, res_ctx->file_size);
            if (QCLOUD_RET_SUCCESS != rc) {
                Log_e("OTA download start err,rc:%d", rc);
                upgrade_fetch_success = false;
                break;
            }

            // download and save the resource
            do {
                int len = IOT_Resource_FetchYield(res_handle, buf_res, RESOURCE_BUF_LEN, 1);
                if (len > 0) {
                    rc = _save_resource_data_to_file(res_ctx->local_file_path, res_ctx->downloaded_size, buf_res, len);
                    if (rc) {
                        Log_e("write data to file failed");
                        upgrade_fetch_success = false;
                        break;
                    }
                } else if (len < 0) {
                    Log_e("download fail rc=%d", len);
                    upgrade_fetch_success = false;
                    break;
                }

                /* get resource information and update local info */
                IOT_Resource_Ioctl(res_handle, IOT_RES_CMD_FETCHED_SIZE, &res_ctx->downloaded_size, 4);
                rc = _update_local_resource_info(res_ctx);
                if (QCLOUD_RET_SUCCESS != rc) {
                    Log_e("update local fw info err,rc:%d", rc);
                }

                // quit resource process as something wrong with mqtt
                rc = IOT_MQTT_Yield(res_ctx->mqtt_client, 100);
                if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
                    Log_e("MQTT error: %d", rc);
                    return false;
                }
            } while (!IOT_OTA_IsFetchFinish(res_handle));

        end_of_download:
            /* Must check MD5 match or not */
            if (upgrade_fetch_success) {
                // download is finished, delete the resource info file
                _delete_file(res_ctx->local_info_file_path);

                uint32_t firmware_valid;
                IOT_Resource_Ioctl(res_handle, IOT_RES_CMD_CHECK_FIRMWARE, &firmware_valid, 4);
                if (0 == firmware_valid) {
                    Log_e("The firmware is invalid");
                    upgrade_fetch_success = false;
                } else {
                    Log_i("The firmware is valid");
                    _update_resource_list(res_ctx->resource_list_file_path, res_ctx->resource_name,
                                          res_ctx->remote_version, res_ctx->resource_type);
                    upgrade_fetch_success = true;
                }
            }

            download_finished = true;
        }

        if (!download_finished)
            HAL_SleepMs(1000);

    } while (!download_finished);

    // do some post-download stuff for your need
    IOT_Resource_ReportUpgradeBegin(res_handle);

    // report result
    int packet_id;
    if (upgrade_fetch_success) {
        packet_id = IOT_Resource_ReportUpgradeSuccess(res_handle, NULL);
    } else {
        packet_id = IOT_Resource_ReportUpgradeFail(res_handle, NULL);
    }

    _wait_for_pub_ack(res_ctx, packet_id);
    return upgrade_fetch_success;
}

int main(int argc, char **argv)
{
    int                  rc;
    ResourceContextData *res_ctx     = NULL;
    void *               mqtt_client = NULL;
    void *               res_handle  = NULL;

    IOT_Log_Set_Level(eLOG_DEBUG);
    res_ctx = (ResourceContextData *)HAL_Malloc(sizeof(ResourceContextData));
    if (res_ctx == NULL) {
        Log_e("malloc failed");
        goto exit;
    }
    memset(res_ctx, 0, sizeof(ResourceContextData));
	
	DeviceInfo sg_devInfo;
    rc = HAL_GetDevInfo(&sg_devInfo);
    if (QCLOUD_RET_SUCCESS != rc) {
        Log_e("get device info failed: %d", rc);
        goto exit;
    }

    // setup MQTT init params
    MQTTInitParams init_params = DEFAULT_MQTTINIT_PARAMS;
    rc                         = _setup_connect_init_params(&init_params, res_ctx, &sg_devInfo);
    if (rc != QCLOUD_RET_SUCCESS) {
        Log_e("init params err,rc=%d", rc);
        return rc;
    }

    // create MQTT mqtt_client and connect to server
    mqtt_client = IOT_MQTT_Construct(&init_params);
    if (mqtt_client) {
        Log_i("Cloud Device Construct Success");
    } else {
        Log_e("Cloud Device Construct Failed");
        return QCLOUD_ERR_FAILURE;
    }

    // init OTA handle
    res_handle = IOT_Resource_Init(sg_devInfo.product_id, sg_devInfo.device_name, mqtt_client, _resource_event_usr_cb, NULL);
    if (!res_handle) {
        Log_e("init resource client failed");
        goto exit;
    }

    res_ctx->resource_handle = res_handle;
    res_ctx->mqtt_client     = mqtt_client;
    strcpy(res_ctx->resource_list_file_path, RESOURCE_FILE_LIST_PATH);

    bool resource_update_success;
    do {
        // mqtt should be ready first
        rc = IOT_MQTT_Yield(mqtt_client, 500);
        if (rc == QCLOUD_ERR_MQTT_ATTEMPTING_RECONNECT) {
            HAL_SleepMs(1000);
            continue;
        } else if (rc != QCLOUD_RET_SUCCESS && rc != QCLOUD_RET_MQTT_RECONNECTED) {
            Log_e("exit with error: %d", rc);
            break;
        }

        // resource process
        resource_update_success = process_resource(res_ctx);
        if (!resource_update_success) {
            Log_e("resource process failed! Do it again");
            HAL_SleepMs(2000);
        }
    } while (!resource_update_success);

exit:
    HAL_Free(res_ctx);
    IOT_Resource_Destroy(res_handle);
    IOT_MQTT_Destroy(&mqtt_client);
    return 0;
}
