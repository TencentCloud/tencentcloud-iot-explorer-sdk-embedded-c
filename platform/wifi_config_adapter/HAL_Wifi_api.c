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

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "qcloud_iot_export.h"
#include "qcloud_iot_import.h"

#include "qcloud_wifi_config.h"

int HAL_Wifi_StaConnect(const char *ssid, const char *psw, uint8_t channel)
{
    Log_i("STA to connect SSID:%s PASSWORD:%s CHANNEL:%d", ssid, psw, channel);
    // TO-DO

    return QCLOUD_RET_SUCCESS;
}

bool HAL_Wifi_IsConnected(void)
{
    // TO-DO, Get IP is true
    return true;
}

int HAL_Wifi_GetLocalIP(char *ipv4_buf, int ipv4_buf_len)
{
    // TO-DO, return QCLOUD_ERR_FAILURE when invalid ip
    // return QCLOUD_RET_SUCCESS when get valid ip
    // copy ipv4 string to ipv4_buf
    strncpy(ipv4_buf, "255.255.255.255", ipv4_buf_len);
    return QCLOUD_ERR_FAILURE;
}

int HAL_Wifi_GetAP_SSID(char *ssid_buf, int ssid_buf_len)
{
    // TO-DO, return connected ap ssid len
    // invalid ssid return QCLOUD_ERR_FAILURE
    // copy ssid string to ssid_buf
    int ssid_len = QCLOUD_ERR_FAILURE;
    strncpy(ssid_buf, "test", ssid_buf_len);
    return ssid_len;
}

int HAL_Wifi_GetAP_PWD(char *pwd_buf, int pwd_buf_len)
{
    // TO-DO, return connected ap password of ssid len
    // invalid password return QCLOUD_ERR_FAILURE
    // copy password string to pwd_buf
    int password_len = QCLOUD_ERR_FAILURE;
    strncpy(pwd_buf, "test", pwd_buf_len);
    return password_len;
}

int HAL_Wifi_read_err_log(uint32_t offset, void *log, size_t log_size)
{
    Log_i("HAL_Wifi_read_err_log");

    return QCLOUD_RET_SUCCESS;
}

int HAL_Wifi_write_err_log(uint32_t offset, void *log, size_t log_size)
{
    Log_i("HAL_Wifi_write_err_log");

    return QCLOUD_RET_SUCCESS;
}

int HAL_Wifi_clear_err_log(void)
{
    Log_i("HAL_Wifi_clear_err_log");

    return QCLOUD_RET_SUCCESS;
}
