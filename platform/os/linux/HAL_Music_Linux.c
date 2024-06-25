/**
 * @file HAL_Music_Linux.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2024-01-11
 *
 * @copyright
 *
 * Tencent is pleased to support the open source community by making IoT Hub available.
 * Copyright(C) 2018 - 2024 THL A29 Limited, a Tencent company.All rights reserved.
 *
 * Licensed under the MIT License(the "License"); you may not use this file except in
 * compliance with the License. You may obtain a copy of the License at
 * http://opensource.org/licenses/MIT
 *
 * Unless required by applicable law or agreed to in writing, software distributed under the License is
 * distributed on an "AS IS" basis, WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND,
 * either express or implied. See the License for the specific language governing permissions and
 * limitations under the License.
 *
 * @par Change Log:
 * <table>
 * Date				Version		Author			Description
 * 2024-01-11		1.0			hubertxxu		first commit
 * </table>
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <string.h>
#include <sys/wait.h>
#include "utils_param_check.h"
#include "qcloud_iot_import.h"
#include "qcloud_iot_export.h"

#ifdef QQMUSIC_ENABLED

typedef struct {
    int  playDuration;
    int  playPosition;
    int  currentVolume;
    bool startPlay;
    bool endPlay;
    bool playPause;
} HALMusicPlayer;

static int   sg_fd_fifo = -1;
static int   sg_fd_pipe[2];
static FILE *sg_pipe_file = NULL;

static void _player_destroy(HALMusicPlayer *player)
{
    if (player->endPlay) {
        return;
    }

    if (sg_fd_fifo != -1) {
        close(sg_fd_fifo);
        sg_fd_fifo = -1;
    }

    if (sg_pipe_file != NULL) {
        fclose(sg_pipe_file);
        sg_pipe_file = NULL;
    }

    int ret = system("killall -9 mplayer");

    Log_w("player end %d", ret);

    player->startPlay    = false;
    player->endPlay      = true;
    player->playPause    = false;
    player->playPosition = 0;
}

static void _player_read_pipe(HALMusicPlayer *player)
{
    char  buf[256];
    Timer read_timer;
    HAL_Timer_countdown(&read_timer, 1);
    do {
        if (sg_fd_fifo == -1) {
            _player_destroy(player);
            return;
        }
        char *line = fgets(buf, sizeof(buf), sg_pipe_file);

        if (line == NULL) {
            continue;
        }

        // Log_d("%s", buf);

        char *temp = strstr(buf, "Starting playback");
        if (NULL != temp) {
            Log_d("Starting playback");
            player->startPlay = true;
            player->playPause = true;
            break;
        }

        temp = strstr(buf, "ANS_time_pos=");
        if (NULL != temp) {
            temp = strstr(temp, "=");
            temp += 1;
            player->playPosition = atoi(temp) + 1;
            // Log_d("position break %d", player->playPosition);
            break;
        }

        temp = strstr(buf, "ANS_volume=");
        if (NULL != temp) {
            temp = strstr(temp, "=");
            temp += 1;
            player->currentVolume = atoi(temp);
            Log_d("volume break");
            break;
        }

        if (NULL != strstr(buf, "Position:")) {
            break;
        }

        if (NULL != strstr(buf, "Exiting...") || NULL != strstr(buf, "cannot reopen stream for resync")) {
            _player_destroy(player);
            break;
        }

        if (player->playPosition >= player->playDuration) {
            _player_destroy(player);
            break;
        }
    } while (sg_fd_fifo != -1 && false == HAL_Timer_expired(&read_timer));
}

static bool _player_play_end_check(HALMusicPlayer *player)
{
    if (sg_fd_fifo == -1) {
        _player_destroy(player);
        return true;
    }

    if (player->startPlay == false) {
        _player_read_pipe(player);
    }

    return player->endPlay;
}

static void _player_set_volume(HALMusicPlayer *player, int volume)
{
    char cmd[128];
    if (sg_fd_fifo == -1 || volume == player->currentVolume) {
        return;
    }

    HAL_Snprintf(cmd, 128, "pausing_keep_force volume %d 1\n", volume);

    int size = write(sg_fd_fifo, cmd, strlen(cmd));

    Log_d("player set volume %d, size %d", volume, size);
    player->currentVolume = volume;
}

static int _player_get_play_info(int type, HALMusicPlayer *player)
{
    size_t write_size = 0;
    if (player->startPlay == true && sg_fd_fifo != -1) {
        switch (type) {
            case 0:
                write_size = write(sg_fd_fifo, "pausing_keep_force get_property volume\n",
                                   sizeof("pausing_keep_force get_property volume\n") - 1);
                if (write_size) {
                    _player_read_pipe(player);
                }

                return player->currentVolume;
            case 1:
                write_size = write(sg_fd_fifo, "get_property time_pos\n", sizeof("get_property time_pos\n") - 1);
                if (write_size) {
                    _player_read_pipe(player);
                }
                return player->playPosition;
            default:
                return -1;
        }
    }

    return -1;
}

static int _player_play(HALMusicPlayer *player, const char *url, int duration)
{
    Log_d("play song %s-%d", url, duration);

    signal(17, NULL);
    int ret = system("killall -9 mplayer");
    ret     = system("mkdir /tmp/qqmusic/");
    ret     = system("unlink /tmp/qqmusic/mplayer");
    ret     = system("rm -rf /tmp/qqmusic/mplayer");
    ret     = system("mkfifo /tmp/qqmusic/mplayer");

    Log_d("mkfifo ret: %d", ret);
    ret = 0;
    if (pipe(sg_fd_pipe) < 0) {
        perror("pipe error \n");
        exit(-1);
        ret = -1;
    }

    pid_t pid;
    pid = fork();
    if (pid < 0) {
        perror("fork");
    }

    if (pid == 0) {
        close(sg_fd_pipe[0]);
        dup2(sg_fd_pipe[1], 1);

        execlp("mplayer", "mplayer", "-quiet", "-cache", "8192", "-framedrop", "-slave", "-input",
               "file=/tmp/qqmusic/mplayer", url, NULL);
    } else {
        // signal(17, _player_signal);
        close(sg_fd_pipe[1]);
        if (sg_fd_fifo != -1) {
            close(sg_fd_fifo);
        }
        sg_fd_fifo   = open("/tmp/qqmusic/mplayer", O_WRONLY);
        sg_pipe_file = fdopen(sg_fd_pipe[0], "r");
        int flag     = fcntl(sg_fd_pipe[0], F_GETFL, 0);
        flag |= O_NONBLOCK;
        if (fcntl(sg_fd_pipe[0], F_SETFL, flag) < 0) {
            perror("fcntl");
            exit(1);
        }
        player->playDuration = duration;
        player->endPlay      = false;
    }
    return ret;
}

int HAL_Music_Play(void **player, const char *url, int duration)
{
    if (!*player) {
        HALMusicPlayer *music_player = HAL_Malloc(sizeof(HALMusicPlayer));
        if (!music_player) {
            Log_e("malloc player fail");
            return -1;
        }
        memset(music_player, 0, sizeof(HALMusicPlayer));
        *player = music_player;
    }
    return _player_play(*player, url, duration);
}

int HAL_Music_Stop(void **player)
{
    POINTER_SANITY_CHECK(*player, QCLOUD_ERR_INVAL);
    _player_destroy((HALMusicPlayer *)*player);
    HAL_Free(*player);
    player = NULL;
    return 0;
}

bool HAL_Music_PlayPause(void *player, uint8_t playPause)
{
    POINTER_SANITY_CHECK(player, false);
    HALMusicPlayer *myPlayer = (HALMusicPlayer *)player;
    if (myPlayer->playPause == playPause) {
        return playPause;
    }

    size_t write_size = write(sg_fd_fifo, "pause\n", sizeof("pause\n") - 1);
    if (write_size) {
        myPlayer->playPause = playPause;
        return playPause;
    }
    return false;
}

int HAL_Music_SetVolume(void *player, int volume)
{
    POINTER_SANITY_CHECK(player, -1);
    _player_set_volume((HALMusicPlayer *)player, volume);
    return 0;
}

int HAL_Music_GetVolume(void *player)
{
    POINTER_SANITY_CHECK(player, -1);
    return _player_get_play_info(0, (HALMusicPlayer *)player);
}

bool HAL_Music_PlayEndCheck(void *player)
{
    POINTER_SANITY_CHECK(player, true);
    return _player_play_end_check((HALMusicPlayer *)player);
}

int HAL_Music_GetPlayPosition(void *player)
{
    POINTER_SANITY_CHECK(player, 0);
    return _player_get_play_info(1, (HALMusicPlayer *)player);
}

int HAL_Music_SetPlayPosition(void *player, int position)
{
    POINTER_SANITY_CHECK(player, 0);
    char cmd_buf[128];
    memset(cmd_buf, 0, sizeof(cmd_buf));
    int    write_len  = HAL_Snprintf(cmd_buf, sizeof(cmd_buf), "seek %d 2\n", position);
    size_t write_size = write(sg_fd_fifo, cmd_buf, write_len);
    if (write_size) {
        return _player_get_play_info(1, (HALMusicPlayer *)player);
    }
    return 0;
}

#endif