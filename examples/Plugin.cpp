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

#include "ocpayload.h"
#include "ocstack.h"
#include <map>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <string.h>
#ifdef WITH_POSIX
#include <openssl/sha.h>
#endif

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

static void DeriveDi(OCUUIdentity *di, const char *name)
{
    static const OCUUIdentity ns =
    {
        0x8f, 0x0e, 0x4e, 0x90, 0x79, 0xe5, 0x11, 0xe6,
        0xbd, 0xf4, 0x08, 0x00, 0x20, 0x0c, 0x9a, 0x66
    };
    uint8_t digest[20];
#ifdef WITH_POSIX
    SHA_CTX ctx;
    int ret = SHA1_Init(&ctx);
    if (ret)
    {
        ret = SHA1_Update(&ctx, ns.id, UUID_IDENTITY_SIZE);
    }
    if (ret)
    {
        ret = SHA1_Update(&ctx, name, strlen(name));
    }
    if (ret)
    {
        ret = SHA1_Final(digest, &ctx);
    }
    if (!ret)
    {
        LOG(LOG_ERR, "SHA1 - %d\n", ret);
    }
#elif _WIN32
    BCRYPT_ALG_HANDLE hAlg = NULL;
    BCRYPT_HASH_HANDLE hHash = NULL;
    DWORD cbHashObject;
    uint8_t *pbHashObject = NULL;
    NTSTATUS status;
    status = BCryptOpenAlgorithmProvider(&hAlg, BCRYPT_SHA1_ALGORITHM, NULL, 0);
    if (BCRYPT_SUCCESS(status))
    {
        DWORD cbResult;
        status = BCryptGetProperty(hAlg, BCRYPT_OBJECT_LENGTH, (PBYTE)&cbHashObject, sizeof(DWORD),
                                   &cbResult, 0);
    }
    if (BCRYPT_SUCCESS(status))
    {
        pbHashObject = new uint8_t[cbHashObject];
        status = BCryptCreateHash(hAlg, &hHash, pbHashObject, cbHashObject, NULL, 0, 0);
    }
    if (BCRYPT_SUCCESS(status))
    {
        status = BCryptHashData(hHash, (PUCHAR)ns.id, UUID_IDENTITY_SIZE, 0);
    }
    if (BCRYPT_SUCCESS(status))
    {
        status = BCryptHashData(hHash, (PUCHAR)name, strlen(name), 0);
    }
    if (BCRYPT_SUCCESS(status))
    {
        status = BCryptFinishHash(hHash, digest, 20, 0);
    }
    if (!BCRYPT_SUCCESS(status))
    {
        LOG(LOG_ERR, "SHA1 - 0x%x\n", status);
    }
    if (hAlg)
    {
        BCryptCloseAlgorithmProvider(hAlg, 0);
    }
    if (hHash)
    {
        BCryptDestroyHash(hHash);
    }
    delete[] pbHashObject;
#else
#error Missing implemention of SHA-1 hash
#endif
    digest[7] = (digest[7] & 0x0f) | 0x50;
    digest[8] = (digest[8] & 0x3f) | 0x80;
    memcpy(di->id, digest, UUID_IDENTITY_SIZE);
}

OCStackResult SetPlatformAndDeviceInfo(ajn::AboutObjectDescription &objectDescription, ajn::AboutData &aboutData)
{
    char *deviceId;
    aboutData.GetDeviceId(&deviceId);
    OCUUIdentity di;
    DeriveDi(&di, deviceId);

    char *value = NULL;
    aboutData.GetAppName(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SPEC_VERSION, "0.3");
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_ID, OCGetServerInstanceIDString());
    std::set<std::string> dataModelVersions;
    size_t numPaths = objectDescription.GetPaths(NULL, 0);
    const char **paths = new const char *[numPaths];
    objectDescription.GetPaths(paths, numPaths);
    for (size_t p = 0; p < numPaths; ++p)
    {
        size_t numIfaces = objectDescription.GetInterfaces(paths[p], NULL, 0);
        const char **ifaces = new const char *[numIfaces];
        objectDescription.GetInterfaces(paths[p], ifaces, numIfaces);
        for (size_t i = 0; i < numIfaces; ++i)
        {
            dataModelVersions.insert(std::string(ifaces[i]) + ".1");
        }
        delete[] ifaces;
    }
    delete[] paths;
    OCStringLL *dmv = NULL;
    for (std::string dataModelVersion : dataModelVersions)
    {
        OCResourcePayloadAddStringLL(&dmv, dataModelVersion.c_str());
    }
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DATA_MODEL_VERSION, dmv);
    OCFreeOCStringLL(dmv);
    value = NULL;
    aboutData.GetSoftwareVersion(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SOFTWARE_VERSION, value);
    value = NULL;
    aboutData.GetManufacturer(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MFG_NAME, value);
    value = NULL;
    aboutData.GetModelNumber(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MODEL_NUM, value);

    unsigned int id[16];
    if (sscanf(deviceId, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               &id[0], &id[1], &id[2], &id[3], &id[4], &id[5], &id[6], &id[7],
               &id[8], &id[9], &id[10], &id[11], &id[12], &id[13], &id[14], &id[15]) == 16 ||
        sscanf(deviceId, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
               &id[0], &id[1], &id[2], &id[3], &id[4], &id[5], &id[6], &id[7],
               &id[8], &id[9], &id[10], &id[11], &id[12], &id[13], &id[14], &id[15]) == 16)
    {
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, deviceId);
    }
    else
    {
        char uuid[UUID_IDENTITY_SIZE * 2 + 5];
        snprintf(uuid, UUID_IDENTITY_SIZE * 2 + 5,
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 di.id[0], di.id[1], di.id[2], di.id[3],
                 di.id[4], di.id[5],
                 di.id[6], di.id[7],
                 di.id[8], di.id[9],
                 di.id[10], di.id[11], di.id[12], di.id[13], di.id[14], di.id[15]);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, uuid);
    }
    value = NULL;
    aboutData.GetManufacturer(&value);
    if (value)
    {
        char mfgName[17];
        strncpy(mfgName, value, 17);
        mfgName[16] = '\0';
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_NAME, mfgName);
    }
    ajn::MsgArg *valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnml", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_URL, value);
    }
    value = NULL;
    aboutData.GetModelNumber(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MODEL_NUM, value);
    value = NULL;
    aboutData.GetDateOfManufacture(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, value);
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnpv", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_VERSION, value);
    }
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnos", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_OS_VERSION, value);
    }
    value = NULL;
    aboutData.GetHardwareVersion(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, value);
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnfv", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_FIRMWARE_VERSION, value);
    }
    value = NULL;
    aboutData.GetSupportUrl(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, value);
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.st", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SYSTEM_TIME, value);
    }
    value = NULL;
    aboutData.GetDeviceId(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_VID, value);

    return OC_STACK_OK;
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
