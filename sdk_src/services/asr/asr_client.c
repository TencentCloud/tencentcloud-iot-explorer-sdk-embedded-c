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

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif
#include "qcloud_iot_export.h"
#include "utils_param_check.h"
#include "utils_timer.h"
#include "utils_list.h"
#include "json_parser.h"
#include "asr_client.h"

#define RESOURCE_TOKEN          "resource_token"
#define REQUEST_ID              "request_id"

typedef struct _AsrHandle_{
	void *resource_handle;
	OnAsrResourceEventUsrCallback usrCb;	
	void 		*mutex;
	List 		*asr_req_list;
}AsrHandle;

typedef struct _AsrReq_ {
	int req_type;
	uint32_t request_id;
	eAsrEngineType engine_type;
	int ch_num;
	char *asr_token;

	Timer timer; 
	int request_timeout_ms;

	OnAsrResultCB result_cb;
} AsrReq;


static char *_engine_type_to_str(eAsrEngineType eType)
{
    char *type;
    switch(eType) {
        case eENGINE_8K_EN:
            type = "8k_en";
            break;

        case eENGINE_8K_ZH:
            type = "8k_zh";
            break;

        case eENGINE_8K_ZH_S:
            type = "8k_zh_s";
            break;

        case eENGINE_16K_ZH_VIDEO:
            type = "16k_zh_video";
            break;

        case eENGINE_16K_EN:
            type = "16k_en";
            break;

        case eENGINE_16K_CA:
            type = "16k_ca";
            break;

        case eENGINE_16K_JA:
            type = "16k_ja";
            break;

        case eENGINE_16K_WUU_SH:
            type = "6k_wuu-SH";
            break;
		
		case eENGINE_16K_ZH:
    	default:
            type = "16k_zh";
            break;
    }

    return type;
}

static int _gen_asr_request_msg(char *msg_buff, int len, AsrReq *req)
{
    int ret;
    POINTER_SANITY_CHECK(msg_buff, QCLOUD_ERR_INVAL);
    memset(msg_buff, 0, len);
    ret = HAL_Snprintf(msg_buff, len,
                       "{\"method\":\"requst_asr\",\"params\":{\"req_type\":%d,"
                       "\"engine_type\":\"%s\",\"channel_num\":\"%d\",\"resource_token\":\"%s\"}}",
                       req->req_type, _engine_type_to_str(req->engine_type), req->ch_num, req->asr_token);

	return ret;					   
}

static int _add_request_to_asr_req_list(AsrHandle *pAsrClient, AsrReq *request)
{
    IOT_FUNC_ENTRY;

    HAL_MutexLock(pAsrClient->mutex);
    if (pAsrClient->asr_req_list->len >= MAX_ASR_REQUEST) {
        HAL_MutexUnlock(pAsrClient->mutex);
        IOT_FUNC_EXIT_RC(QCLOUD_ERR_MAX_APPENDING_REQUEST);
    }

    ListNode *node = list_node_new(request);
    if (NULL == node) {
        HAL_MutexUnlock(pAsrClient->mutex);
        Log_e("run list_node_new is error!");
        IOT_FUNC_EXIT_RC(QCLOUD_ERR_FAILURE);
    }
    list_rpush(pAsrClient->asr_req_list, node);

    HAL_MutexUnlock(pAsrClient->mutex);

    IOT_FUNC_EXIT_RC(QCLOUD_RET_SUCCESS);
}

static AsrReq * _get_req_node_by_request_id(AsrHandle *pAsrClient, uint32_t request_id)
{
    POINTER_SANITY_CHECK(pAsrClient, NULL);
    AsrReq *req = NULL;
    HAL_MutexLock(pAsrClient->mutex);
    if (pAsrClient->asr_req_list->len) {
        ListIterator *iter;
        ListNode *    node = NULL;

        if (NULL == (iter = list_iterator_new(pAsrClient->asr_req_list, LIST_TAIL))) {
            HAL_MutexUnlock(pAsrClient->mutex);
            return NULL;
        }

        for (;;) {
            node = list_iterator_next(iter);
            if (NULL == node) {
                break;
            }

            if (NULL == node->val) {
                Log_e("node's value is invalid!");
                continue;
            }
            req = (AsrReq *)node->val;

            if(req->request_id == request_id) {
                break;
            } else {
                req = NULL;
            }
        }

        list_iterator_destroy(iter);
    }
    HAL_MutexUnlock(pAsrClient->mutex);

    return req;
}

static AsrReq * _get_req_node_by_asr_token(AsrHandle *pAsrClient, const char *asr_token)
{
    POINTER_SANITY_CHECK(pAsrClient, NULL);
    AsrReq *req = NULL;
    HAL_MutexLock(pAsrClient->mutex);
    if (pAsrClient->asr_req_list->len) {
        ListIterator *iter;
        ListNode *    node = NULL;

        if (NULL == (iter = list_iterator_new(pAsrClient->asr_req_list, LIST_TAIL))) {
            HAL_MutexUnlock(pAsrClient->mutex);
            return NULL;
        }

        for (;;) {
            node = list_iterator_next(iter);
            if (NULL == node) {
                break;
            }

            if (NULL == node->val) {
                Log_e("node's value is invalid!");
                continue;
            }
            req = (AsrReq *)node->val;

            if(req->asr_token && (0 == strcmp(req->asr_token, asr_token))) {
                break;
            } else {
                req = NULL;
            }
        }

        list_iterator_destroy(iter);
    }
    HAL_MutexUnlock(pAsrClient->mutex);

    return req;
}

int _request_asr_resource_result(void *handle, AsrReq *req)
{
    char asr_request_buff[ASR_REQUEST_BUFF_LEN];
    int ret = _gen_asr_request_msg(asr_request_buff, ASR_REQUEST_BUFF_LEN, req);
    if (ret < 0) {
        Log_e("generate asr request message failed");
        return ret;
    }
	Log_d("%s", asr_request_buff);
    ret = IOT_Resource_Report_Msg(handle, asr_request_buff);

    return ret;
}

static int _resource_event_usr_cb(void *pResourceHandle, const char *msg, uint32_t msgLen, IOT_RES_UsrEvent event)
{
    int  ret = QCLOUD_RET_SUCCESS;
	
    AsrHandle *asr_handle = (AsrHandle *)IOT_Resource_Get_Context(pResourceHandle);
	char *json_str = (char *)msg;
	char *request_id;
	AsrReq *req;
	int id; 

	Log_d("event(%d) msg:%*s",  event, msgLen, msg);
    switch (event) {
        case IOT_RES_EVENT_REQUEST_URL_RESP:
            request_id = LITE_json_value_of(REQUEST_ID, json_str);
            if(!request_id) {
                Log_e("no request_id found");
                goto exit;
            }
            id = atoi(request_id);
            req = _get_req_node_by_request_id(asr_handle, id);
            if(!req) {
                Log_e("no req found according to request_id");
				HAL_Free(request_id);
                goto exit;
            }
            req->asr_token = LITE_json_value_of(RESOURCE_TOKEN, json_str);
            if(!req->asr_token) {
                Log_e("no req->asr_token found");
                HAL_Free(request_id);
                goto exit;
            }
            ret = _request_asr_resource_result(asr_handle->resource_handle, req);
            HAL_Free(request_id);
            HAL_Free(req->asr_token);
            break;

        default:
            break;
    }

    if(asr_handle->usrCb) {
        ret = asr_handle->usrCb(asr_handle->resource_handle, msg, msgLen, event);
    }

exit:

    return ret;
}

void *IOT_Asr_Init(const char *product_id, const char *device_name, void *ch_signal, OnAsrResourceEventUsrCallback usr_cb)
{
    AsrHandle *  asr_handle  = NULL;
    int rc = QCLOUD_RET_SUCCESS;

    asr_handle = HAL_Malloc(sizeof(AsrHandle));
    if (!asr_handle) {
        Log_e("allocate asr client failed");
        rc = QCLOUD_ERR_MALLOC;
        goto exit;
    }

    // init resource client handle
    asr_handle->resource_handle = IOT_Resource_Init(product_id, device_name, ch_signal, _resource_event_usr_cb);
    if (!asr_handle->resource_handle) {
        Log_e("init resource client failed");
        rc = QCLOUD_ERR_FAILURE;
        goto exit;
    }

	Log_d("asr_handle:%p,  resource_handle:%p(%p)", asr_handle, asr_handle->resource_handle, &asr_handle->resource_handle);
    asr_handle->mutex = HAL_MutexCreate();
    if (asr_handle->mutex == NULL) {
        Log_e("create asr mutex fail");
        rc = QCLOUD_ERR_FAILURE;
        goto exit;
    }

    asr_handle->asr_req_list = list_new();
    if (asr_handle->asr_req_list) {
        asr_handle->asr_req_list->free = HAL_Free;
    } else {
        Log_e("no memory to allocate asr_req_list");
        rc = QCLOUD_ERR_FAILURE;
    }
    asr_handle->usrCb = usr_cb;

exit:

    if(rc != QCLOUD_RET_SUCCESS) {
        if(asr_handle) {
            HAL_Free(asr_handle);
            if(asr_handle->resource_handle) {
                IOT_Resource_Destroy(asr_handle->resource_handle);
            }
            if(asr_handle->mutex) {
                HAL_MutexDestroy(asr_handle->mutex);
            }
            if(asr_handle->asr_req_list) {
                list_destroy(asr_handle->asr_req_list);
            }
        }
        asr_handle = NULL;
    }

    return asr_handle;
}

int IOT_Asr_Destroy(void *handle)
{
    POINTER_SANITY_CHECK(handle, QCLOUD_ERR_INVAL);
    AsrHandle *  asr_handle = (AsrHandle *)handle;
    int rc = QCLOUD_RET_SUCCESS;

    if(asr_handle->resource_handle) {
        rc =IOT_Resource_Destroy(asr_handle->resource_handle);
    }
    if(asr_handle->mutex) {
        HAL_MutexDestroy(asr_handle->mutex);
    }
    if(asr_handle->asr_req_list) {
        list_destroy(asr_handle->asr_req_list);
    }

    return rc;
}

int IOT_Asr_RecordFile_Request(void *handle, const char *file_name, RecordAsrConf *conf, OnAsrResultCB cb)
{
    POINTER_SANITY_CHECK(handle, QCLOUD_ERR_INVAL);

    AsrHandle *  asr_handle = (AsrHandle *)handle;
    int rc = QCLOUD_RET_SUCCESS;

    AsrReq *req = (AsrReq *)HAL_Malloc(sizeof(AsrReq));
    if(!req) {
        Log_e("malloc mem err");
        rc = QCLOUD_ERR_MALLOC;
        goto exit;
    }

    req->req_type = conf->req_type;
    req->engine_type = conf->engine_type;
    req->ch_num = conf->ch_num;
	
	char time_str[TIME_FORMAT_STR_LEN] = {0};
	HAL_Snprintf(time_str, TIME_FORMAT_STR_LEN, "%d", HAL_Timer_current_sec()); 
    req->request_id = IOT_Resource_Post_Request(asr_handle->resource_handle, file_name,time_str, handle);
    if(req->request_id < 0) {
        Log_e("%s resource post request fail", file_name);
        HAL_Free(req);
        rc = QCLOUD_ERR_FAILURE;
        goto exit;
    }
    req->result_cb = cb;

    InitTimer(&(req->timer));
    conf->request_timeout_ms = (conf->request_timeout_ms > 0)?conf->request_timeout_ms:DEFAULT_REQ_TIMEOUT_MS;
    countdown(&(req->timer), conf->request_timeout_ms);
    rc = _add_request_to_asr_req_list(asr_handle, req);

exit:

    return (rc == QCLOUD_RET_SUCCESS)?req->request_id:rc;
}

int IOT_Asr_Realtime_Request(void *handle, uint8_t *audio_buff, uint8_t len, RealTimeAsrConf *req)
{
    POINTER_SANITY_CHECK(handle, QCLOUD_ERR_INVAL);

    return 0;
}

int IOT_Asr_Result_Notify(void *handle, const char *asr_token, char *res_text, int total_resutl_num, int resutl_seq)
{
    POINTER_SANITY_CHECK(handle, QCLOUD_ERR_INVAL);
    AsrHandle *  asr_handle = (AsrHandle *)handle;
    AsrReq *req = _get_req_node_by_asr_token(asr_handle, asr_token);
    if(req) {
        //TO DO: set result to req for sync request
        if(req->result_cb) {
            req->result_cb(req->request_id, res_text, total_resutl_num, resutl_seq);
        }
        //list_remove(asr_handle, req);
    } else {
        Log_e("asr_token: %s not found", asr_token);
        return QCLOUD_ERR_FAILURE;
    }

    return QCLOUD_RET_SUCCESS;
}

#ifdef __cplusplus
}
#endif
