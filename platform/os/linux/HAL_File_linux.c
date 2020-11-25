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

void *HAL_FileOpen(const char *filename, const char *mode)
{
    return (void *)fopen(filename, mode);
}

size_t HAL_FileRead(void *ptr, size_t size, size_t nmemb, void *fp)
{
    return fread(ptr, size, nmemb, (FILE *)fp);
}

size_t HAL_FileWrite(const void *ptr, size_t size, size_t nmemb, void *fp)
{
    return fwrite(ptr, size, nmemb, (FILE *)fp);
}

int HAL_FileSeek(void *fp, long int offset, int whence)
{
    return fseek((FILE *)fp, offset, whence);
}

int HAL_FileClose(void *fp)
{
    return fclose((FILE *)fp);
}

int HAL_FileRemove(const char *filename)
{
    return remove(filename);
}

int HAL_FileRewind(void *fp)
{
    rewind((FILE *)fp);

    return 0;
}

long HAL_FileTell(void *fp)
{
    return ftell((FILE *)fp);
}

long HAL_FileSize(void *fp)
{
    long size = 0;

    fseek((FILE *)fp, 0, SEEK_END);
    size = ftell((FILE *)fp);
    rewind((FILE *)fp);

    return size;
}

char *HAL_FileGets(char *str, int n, void *fp)
{
    return fgets(str, n, (FILE *)fp);
}

int HAL_FileRename(const char *old_filename, const char *new_filename)
{
    return rename(old_filename, new_filename);
}

int HAL_FileEof(void *fp)
{
    return feof((FILE *)fp);
}

int HAL_FileFlush(void *fp)
{
    return fflush((FILE *)fp);
}
