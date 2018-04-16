//******************************************************************
//
// Copyright 2016 Intel Corporation All Rights Reserved.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//      http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
//-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=-=

#include "Log.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#ifndef _WIN32
#include <unistd.h>
#endif

void LogWriteln(
    const char *file,
    const char *function,
    int32_t line,
    int8_t severity,
    const char *fmt,
    ...
)
{
    static FILE *fps[] = { NULL, NULL, NULL, stderr, NULL, NULL, stderr };
    static const char *levels[] = { NULL, NULL, NULL, "ERR ", NULL, NULL, "INFO" };

    const char *basename = strrchr(file, '/');
    if (basename)
    {
        ++basename;
    }
    else
    {
        basename = file;
    }
    va_list ap;
    va_start(ap, fmt);
#ifdef _WIN32
    fprintf(fps[severity], "[%d] %s %s:%d::%s - ", GetCurrentProcessId(), levels[severity], basename,
            line,
            function);
#else
    fprintf(fps[severity], "[%d] %s %s:%d::%s - ", getpid(), levels[severity], basename, line,
            function);
#endif
    vfprintf(fps[severity], fmt, ap);
    fprintf(fps[severity], "\n");
    fflush(fps[severity]);
    va_end(ap);
}
