/**
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2021 THL A29 Limited, a Tencent company.All rights reserved.
 *
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 */

#ifndef QLCOUD_IOT_EXPORT_DEVICE_BIND_H_
#define QLCOUD_IOT_EXPORT_DEVICE_BIND_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "qcloud_iot_export.h"

/**
 * @brief Register a callback function to receive the device unbinding message
 *        sent by the platform.
 *        before use it you must call qcloud_service_mqtt_init() to sub service topic
 *
 * @param callback [in] a callback function
 * @param context  [in] the program context
 * @return QCLOUD_RET_SUCCESS for success, or err code for failure
 */
int IOT_Unbind_Device_Register(void *callback, void *context);
/**
 * @brief Register a callback function to receive the device unbinding message
 *        sent by the platform
 *
 * @param mqtt_client the mqtt client
 * @param callback [in] a callback function
 * @param context  [in] the program context
 * @return QCLOUD_RET_SUCCESS for success, or err code for failure
 */
int IOT_Unbind_Device_ByCloud(void *mqtt_client, void *callback, void *context);
/**
 * @brief Actively initiate a request to unbind the device to the platform
 *
 * @param mqtt_client the mqtt client
 * @param timeout_ms timeout value (unit: ms) for this operation
 * @return QCLOUD_RET_SUCCESS for success, or err code for failure
 */
int IOT_Unbind_Device_Request(void *mqtt_client, int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif  // QLCOUD_IOT_EXPORT_DEVICE_BIND_H_