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

#include "Plugin.h"

#include "ocstack.h"
#include <map>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <string.h>

static std::map<std::string, OCResourceHandle> g_resources;
static uint32_t g_presence;

void LogWriteln(
    const char *file,
    const char *function,
    int32_t line,
    int8_t severity,
    const char *fmt,
    ...
)
{
    static FILE *fps[] = { NULL, NULL, NULL, stderr, NULL, NULL, stdout };
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
    fprintf(fps[severity], "%s %s:%d::%s - ", levels[severity], basename, line, function);
    vfprintf(fps[severity], fmt, ap);
    fprintf(fps[severity], "\n");
    va_end(ap);
}

const char *GetServerInstanceIDString()
{
    return OCGetServerInstanceIDString();
}

OCStackResult SetDeviceInfo(const OCUUIdentity *di, OCDeviceInfo deviceInfo)
{
    char uuid[UUID_IDENTITY_SIZE * 2 + 5];
    snprintf(uuid, UUID_IDENTITY_SIZE * 2 + 5,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             di->id[0], di->id[1], di->id[2], di->id[3],
             di->id[4], di->id[5],
             di->id[6], di->id[7],
             di->id[8], di->id[9],
             di->id[10], di->id[11], di->id[12], di->id[13], di->id[14], di->id[15]);
    LOG(LOG_INFO, "di: %s", uuid);
    return OCSetDeviceInfo(deviceInfo);
}

OCStackResult SetPlatformInfo(OCPlatformInfo platformInfo)
{
    return OCSetPlatformInfo(platformInfo);
}

OCStackResult StartPresence()
{
    if (g_presence++ == 0)
    {
        return OCStartPresence(0);
    }
    else
    {
        return OC_STACK_OK;
    }
}

OCStackResult StopPresence()
{
    if (g_presence > 0 && --g_presence == 0)
    {
        return OCStopPresence();
    }
    else
    {
        return OC_STACK_OK;
    }
}

OCStackResult CreateResource(const char *uri,
                             const char *typeName,
                             const char *interfaceName,
                             OCEntityHandler entityHandler,
                             void *callbackParam,
                             uint8_t properties)
{
    OCResourceHandle handle;
    OCStackResult result = OCCreateResource(&handle, typeName, interfaceName, uri,
                                            entityHandler, callbackParam, properties);
    if (result == OC_STACK_OK)
    {
        g_resources[uri] = handle;
    }
    return result;
}

OCStackResult DestroyResource(const char *uri)
{
    std::map<std::string, OCResourceHandle>::iterator it = g_resources.find(uri);
    if (it == g_resources.end())
    {
        return OC_STACK_ERROR;
    }
    OCResourceHandle handle = it->second;
    g_resources.erase(it);
    return OCDeleteResource(handle);
}

OCStackResult AddResourceType(const char *uri, const char *typeName)
{
    std::map<std::string, OCResourceHandle>::iterator it = g_resources.find(uri);
    if (it == g_resources.end())
    {
        return OC_STACK_ERROR;
    }
    OCResourceHandle handle = it->second;
    return OCBindResourceTypeToResource(handle, typeName);
}

OCStackResult AddInterface(const char *uri, const char *interfaceName)
{
    std::map<std::string, OCResourceHandle>::iterator it = g_resources.find(uri);
    if (it == g_resources.end())
    {
        return OC_STACK_ERROR;
    }
    OCResourceHandle handle = it->second;
    return OCBindResourceInterfaceToResource(handle, interfaceName);
}

OCStackResult DoResource(OCDoHandle *handle,
                         OCMethod method,
                         const char *uri,
                         const OCDevAddr *destination,
                         OCPayload *payload,
                         OCCallbackData *cbData)
{
    return OCDoResource(handle, method, uri, destination, payload,
                        CT_DEFAULT, OC_HIGH_QOS, cbData, NULL, 0);
}

OCStackResult Cancel(OCDoHandle handle, OCQualityOfService qos)
{
    return OCCancel(handle, qos, NULL, 0);
}

OCStackResult DoResponse(OCEntityHandlerResponse *response)
{
    return OCDoResponse(response);
}

OCStackResult NotifyListOfObservers(const char *uri,
                                    OCObservationId  *obsIdList,
                                    uint8_t numberOfIds,
                                    OCRepPayload *payload)
{
    std::map<std::string, OCResourceHandle>::iterator it = g_resources.find(uri);
    if (it == g_resources.end())
    {
        return OC_STACK_ERROR;
    }
    OCResourceHandle handle = it->second;
    return OCNotifyListOfObservers(handle, obsIdList, numberOfIds, payload,
                                   OC_HIGH_QOS);
}
