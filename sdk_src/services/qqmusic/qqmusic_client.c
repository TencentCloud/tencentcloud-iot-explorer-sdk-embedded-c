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

#ifdef __cplusplus
extern "C" {
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "utils_timer.h"
#include "utils_bitmap.h"
#include "lite-utils.h"
#include "mqtt_client.h"
#include "service_mqtt.h"
#include "qcloud_iot_import.h"
#include "qcloud_iot_export.h"

#define PAGE_SIZE (10)

typedef struct {
    int              type;
    uint64_t         id;
    int              quality;
    char             name[QQMUSIC_SONG_NAME_SIZE];
    int              totalSong;
    uint8_t          playIndex;
    UtilsBitmap      pageBitmap;
    QQMusicSongInfo *songInfoArray;
} QQMusicSongListInfo;

typedef struct {
    void               *mqtt_client;  // mqtt client handle
    void               *user_data;    // user data
    QQMusicCallback     callback;
    char                song_list_str[QQMUSIC_SONG_LIST_STR_SIZE];
    QQMusicSongListInfo song_list;
} QCloudIoTQQMusic;

static uint8_t sg_flag_get_song_rely = 0;

static void _printf_qqmusic_song_list(QQMusicSongListInfo *song_list)
{
    HAL_Printf("\r\ntype\t: %d\r\n", song_list->type);
    HAL_Printf("id\t: %lu\r\n", song_list->id);
    HAL_Printf("quality\t: %d\r\n", song_list->quality);
    HAL_Printf("name\t: %s\r\n", song_list->name);
    HAL_Printf("total\t: %d\r\n", song_list->totalSong);
    for (int i = 0; i < song_list->totalSong; i++) {
        HAL_Printf(
            "[%d] userOwnRule : %d,\tsongId : %d,\tsongPlayTime : %d,\tsongName : %s,\tsingerName : %s,\tsongURL : "
            "%.50s,\r\n",
            i, song_list->songInfoArray[i].userOwnRule, song_list->songInfoArray[i].songId,
            song_list->songInfoArray[i].songPlayTime, song_list->songInfoArray[i].songName,
            song_list->songInfoArray[i].singerName, song_list->songInfoArray[i].songUrl);
    }
    HAL_Printf("\r\n");
}

static void _format_song_list_string(QCloudIoTQQMusic *qq_music)
{
    if (qq_music->song_list.totalSong == 0) {
        return;
    }
    memset(qq_music->song_list_str, 0, QQMUSIC_SONG_LIST_STR_SIZE);
    int format_len = HAL_Snprintf(qq_music->song_list_str, QQMUSIC_SONG_LIST_STR_SIZE,
                                  "{\\\"songListId\\\":%lu,\\\"songListType\\\":%d,\\\"songList\\\":[",
                                  qq_music->song_list.id, qq_music->song_list.type);
    for (int i = 0; i < qq_music->song_list.totalSong; i++) {
        format_len += HAL_Snprintf(qq_music->song_list_str + format_len, QQMUSIC_SONG_LIST_STR_SIZE - format_len, "%d,",
                                   qq_music->song_list.songInfoArray[i].songId);
    }
    HAL_Snprintf(qq_music->song_list_str + format_len - 1, QQMUSIC_SONG_LIST_STR_SIZE - format_len - 1, "]}");

    if (qq_music->callback.sync_song_list_callback) {
        qq_music->callback.sync_song_list_callback(qq_music->song_list_str, strlen((char *)qq_music->song_list_str));
    }
}

static int _parse_song_info(char *json_str, QQMusicSongInfo *song_info)
{
    int   ret = 0;
    char *p   = NULL;

    memset(song_info, 0, sizeof(QQMusicSongInfo));

    ret = LITE_json_get_int32("userOwnRule", json_str, &(song_info->userOwnRule));
    ret |= LITE_json_get_uint64("songId", json_str, &(song_info->songId));
    ret |= LITE_json_get_int32("songPlayTime", json_str, &(song_info->songPlayTime));

    if (!(p = LITE_json_value_of("songURL", json_str))) {
        Log_w("songURL is NULL");
    } else {
        memcpy(song_info->songUrl, p, strlen(p));
        HAL_Free(p);
    }

    if (!(p = LITE_json_value_of("songName", json_str))) {
        Log_w("songName is NULL");
    } else {
        memcpy(song_info->songName, p, strlen(p));
        HAL_Free(p);
    }

    if (!(p = LITE_json_value_of("singerName", json_str))) {
        Log_w("singerName is NULL");
    } else {
        memcpy(song_info->singerName, p, strlen(p));
        HAL_Free(p);
    }
    return ret;
}

static int _parse_song_list_obj(const char *json_str, size_t str_len, void *obj, size_t obj_len)
{
    if (obj_len != sizeof(QQMusicSongInfo)) {
        Log_e("obj_len %zu != %zu", obj_len, sizeof(QQMusicSongInfo));
        return QCLOUD_ERR_FAILURE;
    }
    QQMusicSongInfo *p_rs = (QQMusicSongInfo *)obj;

    char temp[512];
    memcpy(temp, json_str, str_len);
    temp[str_len] = '\0';
    return _parse_song_info(temp, p_rs);
}

static int _qqmusic_query_song_list(QCloudIoTQQMusic *qqmusic, int page, int pageSize)
{
    char query_buf[256];
    int  query_len = HAL_Snprintf(
        query_buf, sizeof(query_buf),
        "{\"method\":\"qqmusic_query_song_list\",\"clientToken\":\"qqmusic_query_song_list-%d\",\"params\":{"
         "\"type\":%d,\"quality\":%d,\"id\":%lu,\"page\":%d,\"pageSize\":%d}}",
        HAL_GetTimeMs(), qqmusic->song_list.type, qqmusic->song_list.quality, qqmusic->song_list.id, page, pageSize);
    query_buf[query_len] = '\0';
    return qcloud_service_mqtt_post_msg(qqmusic->mqtt_client, query_buf, QOS0);
}

static int _qqmusic_parse_song_list(char *song_list_info, QCloudIoTQQMusic *qqmusic)
{
    int      rc    = 0;
    int      total = 0, page = 0, pageSize = 0;
    uint64_t play_song_id = 0;

    // update song list
    if (qqmusic->song_list.songInfoArray) {
        HAL_Free(qqmusic->song_list.songInfoArray);
    }
    memset(&qqmusic->song_list, 0, sizeof(qqmusic->song_list));

    rc = LITE_json_get_int32("type", (char *)song_list_info, &(qqmusic->song_list.type));
    rc |= LITE_json_get_int32("quality", (char *)song_list_info, &(qqmusic->song_list.quality));
    rc |= LITE_json_get_int32("total", (char *)song_list_info, &total);
    rc |= LITE_json_get_uint64("id", (char *)song_list_info, &(qqmusic->song_list.id));
    rc |= LITE_json_get_int32("pageSize", (char *)song_list_info, &pageSize);
    rc |= LITE_json_get_int32("page", (char *)song_list_info, &page);
    rc |= LITE_json_get_uint64("playSongId", (char *)song_list_info, &play_song_id);
    if (rc) {
        Log_e("parse song list info failed %d", rc);
        return rc;
    }
    char *name = LITE_json_value_of("name", (char *)song_list_info);
    if (name) {
        memcpy((char *)qqmusic->song_list.name, name, strlen(name));
        HAL_Free(name);
    }
    char *songList = LITE_json_value_of("songList", (char *)song_list_info);
    if (!songList) {
        Log_e("get song list failed");
        return -1;
    }
    utils_set_bitmap_onebit(&qqmusic->song_list.pageBitmap, page);
    if (total <= pageSize) {
        qqmusic->song_list.totalSong = total;
    } else {
        qqmusic->song_list.totalSong = pageSize;
        _qqmusic_query_song_list(qqmusic, utils_get_bitmap_first_zero(&qqmusic->song_list.pageBitmap),
                                 total - pageSize > PAGE_SIZE ? PAGE_SIZE : total - pageSize);
    }

    qqmusic->song_list.songInfoArray = HAL_Malloc(sizeof(QQMusicSongInfo) * qqmusic->song_list.totalSong);
    if (qqmusic->song_list.songInfoArray == NULL) {
        Log_e("malloc failed");
        HAL_Free(songList);
        return -1;
    }
    memset(qqmusic->song_list.songInfoArray, 0, sizeof(QQMusicSongInfo) * qqmusic->song_list.totalSong);
    // get song list
    rc = LITE_dt_parse_obj_array(qqmusic->song_list.songInfoArray,
                                 sizeof(QQMusicSongInfo) * qqmusic->song_list.totalSong, sizeof(QQMusicSongInfo),
                                 songList, _parse_song_list_obj);
    if (rc < 0) {
        Log_e("parse song list failed");
        HAL_Free(qqmusic->song_list.songInfoArray);
        qqmusic->song_list.songInfoArray = NULL;
        qqmusic->song_list.totalSong     = 0;
    } else {
        // update qqmusic song list str
        _format_song_list_string(qqmusic);
        _printf_qqmusic_song_list(&qqmusic->song_list);
    }
    HAL_Free(songList);
    if (play_song_id && qqmusic->callback.play_song_in_song_list) {
        qqmusic->callback.play_song_in_song_list(play_song_id);
    }
    return 0;
}

static void _qqmusic_down_message_callback(void *user_data, const char *payload, unsigned int payload_len)
{
    int               rc      = -1;
    QCloudIoTQQMusic *qqmusic = (QCloudIoTQQMusic *)user_data;

    char reply_buf[512];
    int  reply_len = 0;

    char *method       = LITE_json_value_of("method", (char *)payload);
    char *client_token = LITE_json_value_of("clientToken", (char *)payload);

    Log_d("method: %s", method);
    if (!strncmp(method, METHOD_QQMUSIC_SYNC_SONG_LIST, sizeof(METHOD_QQMUSIC_SYNC_SONG_LIST) - 1)) {
        char *song_list_info = LITE_json_value_of("params", (char *)payload);
        if (song_list_info) {
            rc = _qqmusic_parse_song_list(song_list_info, qqmusic);
            HAL_Free(song_list_info);
        }
        // reply
        reply_len            = HAL_Snprintf(reply_buf, sizeof(reply_buf),
                                            "{\"method\":\"qqmusic_sync_song_list_reply\",\"clientToken\":\"%s\",\"params\":{"
                                                       "\"code\":%d,\"msg\":\"ok\"}}",
                                            client_token, rc);
        reply_buf[reply_len] = '\0';
        qcloud_service_mqtt_post_msg(qqmusic->mqtt_client, reply_buf, QOS0);
    } else if (!strncmp(method, METHOD_QQMUSIC_QUERY_SONG_REPLY, sizeof(METHOD_QQMUSIC_QUERY_SONG_REPLY) - 1)) {
        int song_id = 0, code = 0;
        sg_flag_get_song_rely = 1;
        rc                    = LITE_json_get_int32("code", (char *)payload, &code);
        if (code || rc) {
            Log_e("query song reply code not zero : %d", code);
            goto exit;
        }
        rc = LITE_json_get_int32("params.songId", (char *)payload, &song_id);
        if (rc) {
            Log_e("parse query song id failed %d", rc);
            goto exit;
        }
        QQMusicSongInfo *song = IOT_QQMusic_GetPlaySongInfo(song_id, QQMUSIC_GET_NONE, qqmusic);
        if (song) {
            char *songURL = LITE_json_value_of("params.songURL", (char *)payload);
            memcpy(song->songUrl, songURL, strlen(songURL));
            HAL_Free(songURL);
        } else {
            // new song add to song list
            QQMusicSongInfo *newSongArray =
                (QQMusicSongInfo *)HAL_Malloc(sizeof(QQMusicSongInfo) * (qqmusic->song_list.totalSong + 1));
            if (newSongArray) {
                memset(newSongArray, 0, sizeof(QQMusicSongInfo) * (qqmusic->song_list.totalSong + 1));
                memcpy(newSongArray, qqmusic->song_list.songInfoArray,
                       sizeof(QQMusicSongInfo) * qqmusic->song_list.totalSong);
                char *songInfo = LITE_json_value_of("params", (char *)payload);
                _parse_song_info(songInfo, &newSongArray[qqmusic->song_list.totalSong]);
                HAL_Free(songInfo);
                HAL_Free(qqmusic->song_list.songInfoArray);
                qqmusic->song_list.songInfoArray = newSongArray;
                qqmusic->song_list.totalSong += 1;
                _format_song_list_string(qqmusic);
                _printf_qqmusic_song_list(&qqmusic->song_list);
            }
        }

    } else if (!strncmp(method, METHOD_QQMUSIC_QUERY_SONG_LIST_REPLY,
                        sizeof(METHOD_QQMUSIC_QUERY_SONG_LIST_REPLY) - 1)) {
        int      pageSize = 0, page = 0, total = 0, code = 0;
        uint64_t songListId = 0;

        rc = LITE_json_get_int32("code", (char *)payload, &code);
        if (code || rc) {
            Log_e("query song list reply code not zero  %d", code);
            goto exit;
        }
        rc = LITE_json_get_uint64("params.id", (char *)payload, &songListId);
        if (rc) {
            Log_e("parse query song list id failed %d", rc);
            goto exit;
        }
        if (songListId != qqmusic->song_list.id) {
            Log_e(" query song list reply : songListId %d != %d", songListId, qqmusic->song_list.id);
            goto exit;
        }
        // append song to song list
        rc = LITE_json_get_int32("params.pageSize", (char *)payload, &pageSize);
        rc |= LITE_json_get_int32("params.page", (char *)payload, &page);
        rc |= LITE_json_get_int32("params.total", (char *)payload, &total);

        QQMusicSongInfo *newSongArray =
            (QQMusicSongInfo *)HAL_Malloc(sizeof(QQMusicSongInfo) * (qqmusic->song_list.totalSong + pageSize));
        if (!newSongArray) {
            Log_e("malloc failed");
            goto exit;
        }

        memset(newSongArray, 0, sizeof(QQMusicSongInfo) * (qqmusic->song_list.totalSong + pageSize));
        memcpy(newSongArray, qqmusic->song_list.songInfoArray,
               sizeof(QQMusicSongInfo) * (qqmusic->song_list.totalSong));
        // get new song list
        char *songList = LITE_json_value_of("params.songList", (char *)payload);
        rc = LITE_dt_parse_obj_array(&newSongArray[qqmusic->song_list.totalSong], sizeof(QQMusicSongInfo) * pageSize,
                                     sizeof(QQMusicSongInfo), songList, _parse_song_list_obj);
        HAL_Free(songList);
        if (rc < 0) {
            Log_e("parse song list failed");
            HAL_Free(newSongArray);
        } else {
            // update qqmusic song list
            qqmusic->song_list.totalSong += pageSize;
            HAL_Free(qqmusic->song_list.songInfoArray);
            qqmusic->song_list.songInfoArray = newSongArray;
            utils_set_bitmap_onebit(&qqmusic->song_list.pageBitmap, page);
            if (qqmusic->song_list.totalSong < total) {
                // get remain again
                _qqmusic_query_song_list(qqmusic, utils_get_bitmap_first_zero(&qqmusic->song_list.pageBitmap),
                                         total - qqmusic->song_list.totalSong > PAGE_SIZE
                                             ? PAGE_SIZE
                                             : total - qqmusic->song_list.totalSong);
            }
            _format_song_list_string(qqmusic);
            _printf_qqmusic_song_list(&qqmusic->song_list);
        }
    }
exit:
    HAL_Free(method);
    HAL_Free(client_token);
}

static QQMusicSongInfo *_qqmusic_get_play_song_info_from_id(int song_id, QCloudIoTQQMusic *music)
{
    for (int i = 0; i < music->song_list.totalSong; i++) {
        if (music->song_list.songInfoArray[i].songId == song_id) {
            music->song_list.playIndex = i;
            return &music->song_list.songInfoArray[i];
        }
    }
    return NULL;
}

static QQMusicSongInfo *_qqmusic_get_play_song_info_from_type(QQMusicGetSongInfoType type, QCloudIoTQQMusic *music)
{
    QQMusicSongInfo *rc = NULL;

    switch (type) {
        case QQMUSIC_GET_CURRENT_SONG:
            rc = &music->song_list.songInfoArray[music->song_list.playIndex];
            break;
        case QQMUSIC_GET_NEXT_SONG:
            music->song_list.playIndex += 1;
            music->song_list.playIndex = music->song_list.playIndex % music->song_list.totalSong;
            rc                         = &music->song_list.songInfoArray[music->song_list.playIndex];
            break;
        case QQMUSIC_GET_PREVIOUS_SONG:
            music->song_list.playIndex =
                (music->song_list.playIndex - 1 + music->song_list.totalSong) % music->song_list.totalSong;
            rc = &music->song_list.songInfoArray[music->song_list.playIndex];
            break;
        case QQMUSIC_GET_RANDOM_SONG: {
            srand((unsigned)HAL_GetTimeMs());
            if (music->song_list.playIndex == rand() % music->song_list.totalSong) {
                // again
                srand((unsigned)HAL_GetTimeMs());
            }
            music->song_list.playIndex = rand() % music->song_list.totalSong;
            rc                         = &music->song_list.songInfoArray[music->song_list.playIndex];
        } break;
        default: {
            Log_e("unknow type %d", type);
        } break;
    }
    Log_d("music->song_list.playIndex : %d", music->song_list.playIndex);
    if (rc && (type != QQMUSIC_GET_CURRENT_SONG) && (!rc->songUrl[0] || (rc->userOwnRule != 1))) {
        Log_w("song id : %lu signer : %s. url is null . get next song!", rc->songId, rc->singerName);
        rc = _qqmusic_get_play_song_info_from_type(type, music);
    }
    return rc;
}

QQMusicSongInfo *IOT_QQMusic_GetPlaySongInfo(uint64_t song_id, QQMusicGetSongInfoType type, void *qq_music)
{
    POINTER_SANITY_CHECK(qq_music, NULL);
    QCloudIoTQQMusic *music = (QCloudIoTQQMusic *)qq_music;
    if (music->song_list.totalSong == 0) {
        return NULL;
    }
    Log_d("song_id : %lu type : %d", song_id, type);
    if (!song_id) {
        return _qqmusic_get_play_song_info_from_type(type, music);
    }
    return _qqmusic_get_play_song_info_from_id(song_id, music);
}

int IOT_QQMusic_Enable(void *mqtt_client, QQMusicCallback callback, void **qq_music)
{
    POINTER_SANITY_CHECK(mqtt_client, QCLOUD_ERR_INVAL);

    DeviceInfo       *dev_info = IOT_MQTT_GetDeviceInfo(mqtt_client);
    QCloudIoTQQMusic *music    = HAL_Malloc(sizeof(QCloudIoTQQMusic));
    if (!music) {
        Log_e("malloc failed");
        return QCLOUD_ERR_FAILURE;
    }
    memset(music, 0, sizeof(QCloudIoTQQMusic));
    music->song_list.songInfoArray = NULL;
    music->mqtt_client             = mqtt_client;
    music->callback                = callback;
    qcloud_service_mqtt_event_register(eSERVICE_QQMUSIC, _qqmusic_down_message_callback, music);
    *qq_music = music;
    return qcloud_service_mqtt_init(dev_info->product_id, dev_info->device_name, mqtt_client);
}

void IOT_QQMusic_Disable(void **qq_music)
{
    POINTER_SANITY_CHECK_RTN(*qq_music);
    QCloudIoTQQMusic *music = (QCloudIoTQQMusic *)*qq_music;
    if (music->song_list.songInfoArray) {
        HAL_Free(music->song_list.songInfoArray);
        music->song_list.songInfoArray = NULL;
    }
    HAL_Free(qq_music);
    qq_music = NULL;
}

QQMusicSongInfo *IOT_QQMusic_GetSongInfoFromCloud(void *qq_music, uint64_t song_id, int quality, char *token,
                                                  int timeout_ms)
{
    POINTER_SANITY_CHECK(qq_music, NULL);
    QCloudIoTQQMusic *music = (QCloudIoTQQMusic *)qq_music;
    char              payload[512];
    int               payload_len = HAL_Snprintf(payload, sizeof(payload),
                                                 "{\"method\":\"qqmusic_query_song\",\"clientToken\":\"qqmusic_query_song-%u\","
                                                               "\"params\":{\"songId\":%lu,\"quality\":1,\"token\":\"%s\"}}",
                                                 HAL_GetTimeMs(), song_id, token ? token : "null");
    payload[payload_len]          = '\0';
    int rc                        = qcloud_service_mqtt_post_msg(music->mqtt_client, payload, QOS0);
    if (rc) {
        return NULL;
    }

    Timer wait_timer;
    HAL_Timer_countdown_ms(&wait_timer, timeout_ms);
    sg_flag_get_song_rely = 0;
    // wait reply
    while (!HAL_Timer_expired(&wait_timer) && !sg_flag_get_song_rely) {
        if (QCLOUD_RET_SUCCESS != IOT_MQTT_Yield(music->mqtt_client, 200)) {
            break;
        }
        HAL_SleepMs(200);
    }

    if (HAL_Timer_expired(&wait_timer)) {
        Log_e("query song info timeout");
        return NULL;
    }
    return IOT_QQMusic_GetPlaySongInfo(song_id, QQMUSIC_GET_NONE, qq_music);
}

#ifdef __cplusplus
}
#endif
