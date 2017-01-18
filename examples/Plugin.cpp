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
#include "rd_client.h"
#include <map>
#include <stdarg.h>
#include <stdio.h>
#include <string>
#include <string.h>
#ifdef WITH_POSIX
#include <openssl/sha.h>
#endif

std::string gRD;
static std::map<std::string, OCResourceHandle> sResources;

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
    fprintf(fps[severity], "[%d] %s %s:%d::%s - ", getpid(), levels[severity], basename, line, function);
    vfprintf(fps[severity], fmt, ap);
    fprintf(fps[severity], "\n");
    fflush(fps[severity]);
    va_end(ap);
}

const char *GetServerInstanceIDString()
{
    return OCGetServerInstanceIDString();
}

void DeriveUniqueId(OCUUIdentity *id, const char *deviceId,
                    uint8_t *appId, size_t n)
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
        ret = SHA1_Update(&ctx, deviceId, strlen(deviceId));
    }
    if (ret && appId && n)
    {
        ret = SHA1_Update(&ctx, appId, n);
    }
    if (ret)
    {
        ret = SHA1_Final(digest, &ctx);
    }
    if (!ret)
    {
        LOG(LOG_ERR, "SHA1 - %d", ret);
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
        status = BCryptHashData(hHash, (PUCHAR)deviceId, strlen(deviceId), 0);
    }
    if (BCRYPT_SUCCESS(status) && appId && n)
    {
        status = BCryptHashData(hHash, (PUCHAR)appId, n, 0);
    }
    if (BCRYPT_SUCCESS(status))
    {
        status = BCryptFinishHash(hHash, digest, 20, 0);
    }
    if (!BCRYPT_SUCCESS(status))
    {
        LOG(LOG_ERR, "SHA1 - 0x%x", status);
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
    memcpy(id->id, digest, UUID_IDENTITY_SIZE);
}

OCStackResult SetPlatformAndDeviceInfo(ajn::AboutObjectDescription &objectDescription,
                                       ajn::AboutData &aboutData)
{
    char *deviceId;
    aboutData.GetDeviceId(&deviceId);

    char *value = NULL;
    aboutData.GetAppName(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SPEC_VERSION, "0.3");
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_ID, OCGetServerInstanceIDString());
    uint8_t *appId;
    size_t n;
    aboutData.GetAppId(&appId, &n);
    OCUUIdentity id;
    DeriveUniqueId(&id, deviceId, appId, n);
    char piid[UUID_IDENTITY_SIZE * 2 + 5];
    snprintf(piid, UUID_IDENTITY_SIZE * 2 + 5,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7],
             id.id[8], id.id[9], id.id[10], id.id[11], id.id[12], id.id[13], id.id[14], id.id[15]);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, piid);
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
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, "x.com.intel.virtual", "true");

    unsigned int pi[16];
    if (sscanf(deviceId, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               &pi[0], &pi[1], &pi[2], &pi[3], &pi[4], &pi[5], &pi[6], &pi[7],
               &pi[8], &pi[9], &pi[10], &pi[11], &pi[12], &pi[13], &pi[14], &pi[15]) == 16)
    {
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, deviceId);
    }
    else if (sscanf(deviceId, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    &pi[0], &pi[1], &pi[2], &pi[3], &pi[4], &pi[5], &pi[6], &pi[7],
                    &pi[8], &pi[9], &pi[10], &pi[11], &pi[12], &pi[13], &pi[14], &pi[15]) == 16)
    {
        char uuid[UUID_IDENTITY_SIZE * 2 + 5];
        snprintf(uuid, UUID_IDENTITY_SIZE * 2 + 5,
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 pi[0], pi[1], pi[2], pi[3], pi[4], pi[5], pi[6], pi[7],
                 pi[8], pi[9], pi[10], pi[11], pi[12], pi[13], pi[14], pi[15]);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, uuid);
    }
    else
    {
        DeriveUniqueId(&id, deviceId, NULL, 0);
        char uuid[UUID_IDENTITY_SIZE * 2 + 5];
        snprintf(uuid, UUID_IDENTITY_SIZE * 2 + 5,
                 "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                 id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7],
                 id.id[8], id.id[9], id.id[10], id.id[11], id.id[12], id.id[13], id.id[14], id.id[15]);
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
    return OC_STACK_OK;
}

OCStackResult StopPresence()
{
    return OC_STACK_OK;
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
        sResources[uri] = handle;
    }
    return result;
}

OCStackResult DestroyResource(const char *uri)
{
    std::map<std::string, OCResourceHandle>::iterator it = sResources.find(uri);
    if (it == sResources.end())
    {
        return OC_STACK_ERROR;
    }
    OCResourceHandle handle = it->second;
    sResources.erase(it);
    return OCDeleteResource(handle);
}

OCStackResult AddResourceType(const char *uri, const char *typeName)
{
    std::map<std::string, OCResourceHandle>::iterator it = sResources.find(uri);
    if (it == sResources.end())
    {
        return OC_STACK_ERROR;
    }
    OCResourceHandle handle = it->second;
    return OCBindResourceTypeToResource(handle, typeName);
}

OCStackResult AddInterface(const char *uri, const char *interfaceName)
{
    std::map<std::string, OCResourceHandle>::iterator it = sResources.find(uri);
    if (it == sResources.end())
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
    std::map<std::string, OCResourceHandle>::iterator it = sResources.find(uri);
    if (it == sResources.end())
    {
        return OC_STACK_ERROR;
    }
    OCResourceHandle handle = it->second;
    return OCNotifyListOfObservers(handle, obsIdList, numberOfIds, payload,
                                   OC_HIGH_QOS);
}

static OCStackApplicationResult RDPublishCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) ctx;
    (void) handle;
    LOG(LOG_INFO, "response=%p,response->result=%d",
        response, response ? response->result : 0);
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackResult RDPublish()
{
    uint8_t nr;
    OCStackResult result = OCGetNumberOfResources(&nr);
    if (result != OC_STACK_OK)
    {
        return result;
    }
    OCResourceHandle hs[nr];
    uint8_t nhs = 0;
    for (uint8_t i = 0; i < nr; ++i)
    {
        OCResourceHandle h = OCGetResourceHandle(i);
        if (OCGetResourceProperties(h) & OC_DISCOVERABLE)
        {
            hs[nhs++] = h;
        }
    }
    OCCallbackData cbData;
    cbData.cb = RDPublishCB;
    cbData.context = NULL;
    cbData.cd = NULL;
    return OCRDPublish(gRD.c_str(), CT_DEFAULT, hs, nhs, &cbData, OC_HIGH_QOS);
}
