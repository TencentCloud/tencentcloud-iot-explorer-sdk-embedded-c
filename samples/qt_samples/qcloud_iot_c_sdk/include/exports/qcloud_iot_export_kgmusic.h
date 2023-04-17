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

#ifndef QCLOUD_IOT_EXPORT_KGMUSIC_H_
#define QCLOUD_IOT_EXPORT_KGMUSIC_H_

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    E_KGMUSIC_QUERY_REPLY_OK    = 0,
    E_KGMUSIC_QUERY_REPLY_ERROR = 1,
    E_KGMUSIC_QUERY_REPLY_TIMEOUT
} QcloudIotKgmusicReplyResult;

typedef struct {
    void *                      mqtt_client;                          // mqtt client handle
    void *                      user_data;                            // user data
    QcloudIotKgmusicReplyResult e_reply_result;                       // sync query result
    int (*song_info_callback)(char *song_info_json, char *userdata);  // proc query song info reply data
    int (*song_list_callback)(char *song_list_json, char *userdata);  // proc query song list reply datas
} QcloudIotKgmusic;

typedef struct {
    int   page;       // query page of lists
    int   page_size;  // page how num songs [1-10]
    char *list_id;
    char *list_type;
    char *except_fields;
} QcloudIotKgmusicSongListParams;

#define QCLOUD_IOT_KGMUSIC_QUERY_SEND_FAILED  -1  // publish failed
#define QCLOUD_IOT_KGMUSIC_QUERY_RECV_TIMEOUT -2  // publish success but no reply
#define QCLOUD_IOT_KGMUSIC_QUERY_RECV_ERROR   -3  // publish success but reply error data

/**
 * @brief device query song play list wait reply data
 *
 * @param client               mqtt client handle
 * @param list_query_params    play list query params
 * @param kgmusic              QCLOUD_IOT_kgmusic_enable kgmusic param
 * @param timeout_ms           wait reply data until timeout_ms
 * @return QCLOUD_RET_SUCCESS is success other is failure
 */
int IOT_KGMUSIC_syncquery_songlist(void *client, QcloudIotKgmusicSongListParams *list_query_params,
                                   QcloudIotKgmusic *kgmusic, int timeout_ms);

/**
 * @brief device query song info wait reply data
 *
 * @param client     mqtt client handle
 * @param song_id    song id
 * @param kgmusic    QCLOUD_IOT_kgmusic_enable kgmusic param
 * @param timeout_ms wait reply data until timeout_ms
 * @return QCLOUD_RET_SUCCESS is success other is failure
 */
int IOT_KGMUSIC_syncquery_song(void *client, char *song_id, QcloudIotKgmusic *kgmusic, int timeout_ms);

/**
 * @brief device enable kgmusic
 *
 * @param mqtt_client     mqtt client handle
 * @param product_id      device productid
 * @param device_name     device name
 * @param kgmusic         middle var. stored callback etc.
 * @return QCLOUD_RET_SUCCESS is success other is failure
 */
int IOT_KGMUSIC_enable(void *mqtt_client, char *product_id, char *device_name, QcloudIotKgmusic *kgmusic);

#ifdef __cplusplus
}
#endif

#endif /* QCLOUD_IOT_EXPORT_KGMUSIC_H_ */