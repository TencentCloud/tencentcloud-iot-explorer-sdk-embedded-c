/**
 * @file HAL_Music_freertos.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2024-01-17
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
 * 2024-01-17		1.0			hubertxxu		first commit
 * </table>
 */

#include "qcloud_iot_import.h"
#include "qcloud_iot_export.h"

#ifdef QQMUSIC_ENABLED

int HAL_Music_Play(void **player, const char *url, int duration)
{
    return 0;
}

int HAL_Music_Stop(void **player)
{
    POINTER_SANITY_CHECK(*player, QCLOUD_ERR_INVAL);
    return 0;
}

bool HAL_Music_PlayPause(void *player, uint8_t playPause)
{
    POINTER_SANITY_CHECK(player, false);
    return false;
}

int HAL_Music_SetVolume(void *player, int volume)
{
    POINTER_SANITY_CHECK(player, -1);
    return 0;
}

int HAL_Music_GetVolume(void *player)
{
    POINTER_SANITY_CHECK(player, -1);
    return 0;
}

bool HAL_Music_PlayEndCheck(void *player)
{
    POINTER_SANITY_CHECK(player, true);
    return 0;
}

int HAL_Music_GetPlayPosition(void *player)
{
    POINTER_SANITY_CHECK(player, -1);
    return 0;
}

int HAL_Music_SetPlayPosition(void *player, int position)
{
    POINTER_SANITY_CHECK(player, -1);

    return 0;
}

#endif  // QQMUSIC_ENABLED
