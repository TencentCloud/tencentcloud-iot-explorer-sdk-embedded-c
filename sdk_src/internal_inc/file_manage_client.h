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

#ifndef IOT_FILE_MANAGE_CLIENT_H_
#define IOT_FILE_MANAGE_CLIENT_H_

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "qcloud_iot_export_file_manage.h"

#define MAX_FILE_WAIT_POST (10)

typedef enum {
    eFILE_MANAGE_PROGRESS,
    eFILE_MANAGE_VERSION,
    eFILE_MANAGE_UPGRADE_RESULT,
    eFILE_MANAGE_POST_REQ,
    eFILE_MANAGE_POST_RESULT
} eFileManageReportType;

#ifdef __cplusplus
}
#endif

#endif /* IOT_FILE_MANAGE_CLIENT_H_ */
