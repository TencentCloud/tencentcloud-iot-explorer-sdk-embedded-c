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

#include <memory.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

#include "qcloud_iot_export_error.h"
#include "qcloud_iot_import.h"
#include "qcloud_iot_export.h"

#ifdef WIFI_CONFIG_ENABLED
#include "utils_ringbuff.h"
#include "utils_timer.h"

typedef struct qcloud_message_queue {
    sRingbuff ring_buff;
    uint32_t  queue_item_size;
    void *    queue_mutex;
} QCLOUD_MESSAGE_QUEUE_T;

void *HAL_QueueCreate(unsigned long queue_length, unsigned long queue_item_size)
{
    QCLOUD_MESSAGE_QUEUE_T *queue = HAL_Malloc(sizeof(QCLOUD_MESSAGE_QUEUE_T));
    char *                  buff  = NULL;
    void *                  mutex = NULL;

    if (NULL == queue) {
        return NULL;
    }
    memset(queue, 0, sizeof(QCLOUD_MESSAGE_QUEUE_T));

    buff = HAL_Malloc(queue_length * queue_item_size);
    if (NULL == buff) {
        HAL_Free(queue);
        return NULL;
    }

    mutex = HAL_MutexCreate();
    if (mutex == NULL) {
        HAL_Free(buff);
        HAL_Free(queue);
        return NULL;
    }

    ring_buff_init(&(queue->ring_buff), buff, queue_length * queue_item_size);

    queue->queue_item_size = queue_item_size;
    queue->queue_mutex     = mutex;

    return queue;
}

void HAL_QueueDestory(void *queue_handle)
{
    QCLOUD_MESSAGE_QUEUE_T *queue = (QCLOUD_MESSAGE_QUEUE_T *)queue_handle;
    void *                  mutex = queue->queue_mutex;

    HAL_MutexLock(mutex);
    HAL_Free(queue->ring_buff.buffer);
    HAL_Free(queue);
    HAL_MutexUnlock(mutex);

    HAL_MutexDestroy(mutex);
}

uint32_t HAL_QueueReset(void *queue_handle)
{
    QCLOUD_MESSAGE_QUEUE_T *queue = (QCLOUD_MESSAGE_QUEUE_T *)queue_handle;
    void *                  mutex = queue->queue_mutex;

    HAL_MutexLock(mutex);
    ring_buff_flush(queue->ring_buff.buffer);
    HAL_MutexUnlock(mutex);

    return QCLOUD_RET_SUCCESS;
}

unsigned long HAL_QueueItemWaitingCount(void *queue_handle)
{
    QCLOUD_MESSAGE_QUEUE_T *queue     = (QCLOUD_MESSAGE_QUEUE_T *)queue_handle;
    sRingbuff *             ring_buff = &(queue->ring_buff);
    void *                  mutex     = queue->queue_mutex;
    int                     len       = 0;

    HAL_MutexLock(mutex);

    if (ring_buff->readpoint > ring_buff->writepoint) {
        len = ring_buff->size - ring_buff->readpoint;
        len += ring_buff->writepoint;
    } else {
        len = ring_buff->writepoint - ring_buff->readpoint;
    }

    HAL_MutexUnlock(mutex);

    if (len % queue->queue_item_size) {
        Log_e("error ring buff size error");
    }

    return len / queue->queue_item_size;
}

unsigned long HAL_QueueItemPop(void *queue_handle, void *const item_buffer, uint32_t wait_timeout)
{
    QCLOUD_MESSAGE_QUEUE_T *queue     = (QCLOUD_MESSAGE_QUEUE_T *)queue_handle;
    sRingbuff *             ring_buff = &(queue->ring_buff);
    void *                  mutex     = queue->queue_mutex;
    int                     ret       = 0;

    Timer timer;
    InitTimer(&timer);
    countdown_ms(&timer, (unsigned int)wait_timeout);

    while (1) {
        HAL_MutexLock(mutex);
        ret = ring_buff_pop_data(ring_buff, item_buffer, queue->queue_item_size);
        HAL_MutexUnlock(mutex);

        if (ret == queue->queue_item_size) {
            break;
        } else if (expired(&timer)) {
            return QCLOUD_ERR_FAILURE;
        }

        HAL_SleepMs(200);
    }

    return QCLOUD_RET_SUCCESS;
}

unsigned long HAL_QueueItemPush(void *queue_handle, void *const item_buffer, uint32_t wait_timeout)
{
    QCLOUD_MESSAGE_QUEUE_T *queue     = (QCLOUD_MESSAGE_QUEUE_T *)queue_handle;
    sRingbuff *             ring_buff = &(queue->ring_buff);
    void *                  mutex     = queue->queue_mutex;
    int                     ret       = 0;
    Timer                   timer;

    InitTimer(&timer);
    countdown_ms(&timer, (unsigned int)wait_timeout);

    while (1) {
        HAL_MutexLock(mutex);
        ret = ring_buff_push_data(ring_buff, item_buffer, queue->queue_item_size);
        HAL_MutexUnlock(mutex);

        if (ret == RINGBUFF_OK) {
            break;
        } else if (expired(&timer)) {
            return QCLOUD_ERR_FAILURE;
        }

        HAL_SleepMs(200);
    }

    return QCLOUD_RET_SUCCESS;
}
#endif  // WIFI_CONFIG_ENABLED

void *HAL_MutexCreate(void)
{
#ifdef MULTITHREAD_ENABLED
    HANDLE mutex = CreateMutex(NULL, FALSE, NULL);

    if (mutex == NULL) {
        HAL_Printf("%s: create mutex failed\n", __FUNCTION__);
    }

    return (void *)mutex;
#else
    return (void *)0xFFFFFFFF;
#endif
}

void HAL_MutexDestroy(_IN_ void *mutex)
{
#ifdef MULTITHREAD_ENABLED
    CloseHandle((HANDLE)mutex);
#else
    return;
#endif
}

void HAL_MutexLock(_IN_ void *mutex)
{
#ifdef MULTITHREAD_ENABLED
    WaitForSingleObject((HANDLE)mutex, INFINITE);
#else
    return;
#endif
}

int HAL_MutexTryLock(_IN_ void *mutex)
{
#ifdef MULTITHREAD_ENABLED
    return WaitForSingleObject((HANDLE)mutex, 0) == WAIT_OBJECT_0 ? 0 : -1;
#else
    return 0;
#endif
}

void HAL_MutexUnlock(_IN_ void *mutex)
{
#ifdef MULTITHREAD_ENABLED
    ReleaseMutex((HANDLE)mutex);
#else
    return;
#endif
}

void *HAL_Malloc(_IN_ uint32_t size)
{
    return malloc(size);
}

void HAL_Free(_IN_ void *ptr)
{
    if (ptr)
        free(ptr);
}

void HAL_Printf(_IN_ const char *fmt, ...)
{
    va_list args;

    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);

    fflush(stdout);
}

int HAL_Snprintf(_IN_ char *str, const int len, const char *fmt, ...)
{
    va_list args;
    int     rc;

    va_start(args, fmt);
    rc = vsnprintf(str, len, fmt, args);
    va_end(args);

    return rc;
}

int HAL_Vsnprintf(_IN_ char *str, _IN_ const int len, _IN_ const char *format, va_list ap)
{
    return vsnprintf(str, len, format, ap);
}

uint32_t HAL_GetTimeMs(void)
{
    return (uint32_t)GetTickCount64();
}

void HAL_SleepMs(_IN_ uint32_t ms)
{
    Sleep(ms);
}

#if ((defined(MULTITHREAD_ENABLED)) || (defined AT_TCP_ENABLED) || (defined WIFI_CONFIG_ENABLED))

// platform-dependant thread routine/entry function
static void _HAL_thread_func_wrapper_(void *ptr)
{
    ThreadParams *params = (ThreadParams *)ptr;

    params->thread_func(params->user_arg);

    _endthread();
}

// platform-dependant thread create function
int HAL_ThreadCreate(ThreadParams *params)
{
    if (params == NULL)
        return QCLOUD_ERR_INVAL;

    uintptr_t ret = _beginthread(_HAL_thread_func_wrapper_, params->stack_size, (void *)params);
    if (ret == -1L) {
        HAL_Printf("%s: _beginthread failed: %d\n", __FUNCTION__, errno);
        return QCLOUD_ERR_FAILURE;
    }

    return QCLOUD_RET_SUCCESS;
}

int HAL_ThreadDestroy(void *threadId)
{
    return QCLOUD_RET_SUCCESS;
}

#endif

#ifdef AT_TCP_ENABLED

void *HAL_SemaphoreCreate(void)
{
    return NULL;
}

void HAL_SemaphoreDestroy(void *sem)
{
    return;
}

void HAL_SemaphorePost(void *sem)
{
    return;
}

int HAL_SemaphoreWait(void *sem, uint32_t timeout_ms)
{
    return QCLOUD_RET_SUCCESS;
}

#endif
