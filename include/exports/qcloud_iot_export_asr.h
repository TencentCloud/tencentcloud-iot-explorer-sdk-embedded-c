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

#ifndef QCLOUD_IOT_EXPORT_ASR_H_
#define QCLOUD_IOT_EXPORT_ASR_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "qcloud_iot_import.h"

//refer to https://cloud.tencent.com/document/product/1093/37823#2.-.E8.BE.93.E5.85.A5.E5.8F.82.E6.95.B0
typedef enum{
	eENGINE_8K_EN = 0,
	eENGINE_8K_ZH = 1,
	eENGINE_8K_ZH_S = 2,
	eENGINE_16K_ZH = 3,
	eENGINE_16K_ZH_VIDEO = 4,
	eENGINE_16K_EN = 5,
	eENGINE_16K_CA = 6,
	eENGINE_16K_JA = 7,
	eENGINE_16K_WUU_SH = 8,	
	eENGINE_DEFAULT = 9,
}eAsrEngineType;

typedef struct _RecordAsrConf_ {
	//Required parameters
	int req_type;
	eAsrEngineType engine_type;
	int ch_num;

	//Optional parameters
	int filter_dirty;
	int filter_modal;
	int filter_punc;

	int request_timeout_ms;
} RecordAsrConf;

typedef struct _RealTimeAsrConf_ {

} RealTimeAsrConf;

typedef void (*OnAsrResultCB)(uint32_t request_id, char *res_text, int total_resutl_num, int resutl_seq);

typedef int (*OnAsrResourceEventUsrCallback)(void *pContext, const char *msg, uint32_t msgLen, int event);

/**
 * @brief Init asr client
 *        MQTT Client should be constructed beforehand
 *
 * @param product_id:   product Id
 * @param device_name:  device name
 * @param ch_signal:    channel: MQTT
 *
 * @return a valid asr client handle when success, or NULL otherwise
 */	
void *IOT_Asr_Init(const char *product_id, const char *device_name, void *ch_signal, OnAsrResourceEventUsrCallback usr_cb);

/**
 * @brief Destroy asr client
 *
 * @param handle: asr client handle
 *
 * @return QCLOUD_RET_SUCCESS when success, or err code for failure
 */
int IOT_Asr_Destroy(void *handle);

/**
 * @brief Record file asr request
 *
 * @param handle: asr client handle
 *
 * @param file_name: record file name with path
 *
 * @param conf: record file parameter and request conf
 *
 * @param cb: callback function when asr result received
 *
 * @return QCLOUD_RET_SUCCESS when success, or err code for failure
 */
int IOT_Asr_RecordFile_Request(void *handle, const char *file_name, RecordAsrConf *conf, OnAsrResultCB cb);


/**
 * @brief Realtime asr
 *
 * @param handle: asr client handle
 *
 * @return QCLOUD_RET_SUCCESS when success, or err code for failure
 */
int IOT_Asr_Realtime_Request(void *handle, uint8_t *audio_buff, uint8_t len, RealTimeAsrConf *config);

#ifdef __cplusplus
}
#endif

#endif /* QCLOUD_IOT_EXPORT_OTA_H_ */
