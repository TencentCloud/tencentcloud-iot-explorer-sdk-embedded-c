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

#ifndef QCLOUD_IOT_EXPORT_QQMUSIC_H_
#define QCLOUD_IOT_EXPORT_QQMUSIC_H_

#ifdef __cplusplus
extern "C" {
#endif

#include "utils_list.h"

#define QQMUSIC_SONG_URL_SIZE         (512)
#define QQMUSIC_SONG_NAME_SIZE        (128)
#define QQMUSIC_SONG_SINGER_NAME_SIZE (128)
#define QQMUSIC_SONG_LIST_STR_SIZE    (2048)

typedef enum {
    QQMUSIC_GET_CURRENT_SONG  = 0,
    QQMUSIC_GET_PREVIOUS_SONG = 1,
    QQMUSIC_GET_NEXT_SONG     = 2,
    QQMUSIC_GET_RANDOM_SONG   = 3,
    QQMUSIC_GET_NONE,
} QQMusicGetSongInfoType;

typedef struct {
    int      userOwnRule;
    uint64_t songId;
    int      songPlayTime;
    char     songUrl[QQMUSIC_SONG_URL_SIZE];
    char     songName[QQMUSIC_SONG_NAME_SIZE];
    char     singerName[QQMUSIC_SONG_SINGER_NAME_SIZE];
} QQMusicSongInfo;

typedef struct {
    void (*sync_song_list_callback)(char *song_list_str, int song_list_len);
    void (*play_song_in_song_list)(uint32_t song_id);
} QQMusicCallback;

/**
 * @brief Initialize the QQMusic module.
 *
 * @param mqtt_client
 * @param qq_music @see QCloudIoTQQMusic
 * @return int 0: success, other: fail
 */
int IOT_QQMusic_Enable(void *mqtt_client, QQMusicCallback callback, void **qq_music);

/**
 * @brief Deinitialize the QQMusic module.
 *
 * @param qq_music @see QCloudIoTQQMusic
 */
void IOT_QQMusic_Disable(void **qq_music);

/**
 * @brief Get the current playing song.
 *
 * @param type @see QQMusicGetSongInfoType
 * @param qq_music @see QCloudIoTQQMusic
 * @return QQMusicSongInfo or NULL
 */
QQMusicSongInfo *IOT_QQMusic_GetPlaySongInfo(uint64_t song_id, QQMusicGetSongInfoType type, void *qq_music);

QQMusicSongInfo *IOT_QQMusic_GetSongInfoFromCloud(void *qq_music, uint64_t song_id, int quality, char *token,
                                                  int timeout_ms);

#ifdef __cplusplus
}
#endif

#endif /* QCLOUD_IOT_EXPORT_QQMUSIC_H_ */