/**
 * @file utils_bitmap.c
 * @author {hubert} ({hubertxxu@tencent.com})
 * @brief
 * @version 1.0
 * @date 2024-02-02
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
 * 2024-02-02		1.0			hubertxxu		first commit
 * </table>
 */

#include "utils_bitmap.h"

/**
 * @brief Get the status of a certain bit in the current bitmap
 *
 * @param h_bitmap The bitmap structure to be operated on
 * @param index The index value
 * @return int8_t On success, return the status value. On failure, return -1
 */
int8_t utils_get_bitmap_onebit(UtilsBitmap *h_bitmap, uint8_t index)
{
    if (index > BITMAP_MAX_LENGTH) {
        return -1;
    }
    uint32_t and_temp = h_bitmap->bitmap_group[index / 32];
    and_temp >>= ((index % 32));
    and_temp &= 0x00000001;
    return (int8_t)and_temp;
}

/**
 * @brief Get the index of the first 0 in the current bitmap
 *
 * @param h_bitmap The bitmap structure to be operated on
 * @return int8_t On success, return the index value. On failure, return -1
 */
int8_t utils_get_bitmap_first_zero(UtilsBitmap *h_bitmap)
{
    int8_t i = -1;
    for (i = 0; i < BITMAP_MAX_LENGTH; i++) {
        if (utils_get_bitmap_onebit(h_bitmap, i) == 0) {
            break;
        }
    }
    if (i == BITMAP_MAX_LENGTH) {
        i = -1;
    }
    return i;
}

/**
 * @brief Get the index of the first 1 in the current bitmap
 *
 * @param h_bitmap The bitmap structure to be operated on
 * @return int8_t On success, return the index value. On failure, return -1
 */
int8_t utils_get_bitmap_first_set(UtilsBitmap *h_bitmap)
{
    int8_t i = -1;
    for (i = 0; i < BITMAP_MAX_LENGTH; i++) {
        if (utils_get_bitmap_onebit(h_bitmap, i) == 1) {
            break;
        }
    }
    if (i == BITMAP_MAX_LENGTH) {
        i = -1;
    }
    return i;
}

/**
 * @brief Set a certain bit in the bitmap to 1
 *
 * @param h_bitmap The bitmap structure to be operated on
 * @param index The index value
 * @return int8_t On success, return 0. On failure, return -1
 */
int8_t utils_set_bitmap_onebit(UtilsBitmap *h_bitmap, uint8_t index)
{
    if (index > BITMAP_MAX_LENGTH) {
        return -1;
    }
    h_bitmap->bitmap_group[index / 32] |= (1 << (index % 32));
    return 0;
}

/**
 * @brief Set a certain bit in the bitmap to 0
 *
 * @param h_bitmap The bitmap structure to be operated on
 * @param index The index value
 * @return int8_t On success, return 0. On failure, return -1
 */
int8_t utils_clear_bitmap_onebit(UtilsBitmap *h_bitmap, uint8_t index)
{
    if (index > BITMAP_MAX_LENGTH) {
        return -1;
    }
    h_bitmap->bitmap_group[index / 32] &= (~(1 << (index % 32)));
    return 0;
}

/**
 * @brief Initialize the bitmap structure
 *
 * @param h_bitmap The bitmap structure to be operated on
 * @param init_state The initial state
 * @return int8_t Always return 0
 */
int8_t utils_init_bitmap(UtilsBitmap *h_bitmap, uint8_t init_state)
{
    for (int i = 0; i < BITMAP_MAX_LENGTH / 32; i++) {
        if (init_state != 0) {
            h_bitmap->bitmap_group[i] = 0xffffffff;
        } else {
            h_bitmap->bitmap_group[i] = 0x00000000;
        }
    }
    return 0;
}

/**
 * @brief Check if two bitmap data are equal
 *
 * @param h_bitmap0 The first bitmap structure to be compared
 * @param h_bitmap1 The second bitmap structure to be compared
 * @return int8_t If equal, return 0. If not equal, return -1
 */
int8_t utils_check_bitmap_is_equal(UtilsBitmap *h_bitmap0, UtilsBitmap *h_bitmap1)
{
    int16_t i = -1;
    for (i = 0; i < BITMAP_MAX_LENGTH; i++) {
        if (utils_get_bitmap_onebit(h_bitmap0, i) != utils_get_bitmap_onebit(h_bitmap1, i)) {
            break;
        }
    }
    if (i == BITMAP_MAX_LENGTH) {
        return -1;
    } else {
        return 0;
    }
}
