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
#include "ocrandom.h"
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

bool GetPiid(OCUUIdentity *piid, ajn::AboutData *aboutData)
{
    ajn::MsgArg *piidArg = NULL;
    aboutData->GetField("org.openconnectivity.piid", piidArg);
    char *piidStr = NULL;
    if (piidArg && (ER_OK == piidArg->Get("s", &piidStr)))
    {
        return (piidStr && OCConvertStringToUuid(piidStr, piid->id));
    }
    else
    {
        char *deviceId;
        aboutData->GetDeviceId(&deviceId);
        uint8_t *appId;
        size_t n;
        aboutData->GetAppId(&appId, &n);
        DeriveUniqueId(piid, deviceId, appId, n);
        return true;
    }
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
    OCStackResult result;
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    if (handle)
    {
        result = OC_STACK_OK;
    }
    else
    {
        result = OCCreateResource(&handle, typeName, interfaceName, uri,
                                  entityHandler, callbackParam, properties);
    }
    return result;
}

OCStackResult DestroyResource(const char *uri)
{
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    return OCDeleteResource(handle);
}

OCStackResult AddResourceType(const char *uri, const char *typeName)
{
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    return OCBindResourceTypeToResource(handle, typeName);
}

OCStackResult AddInterface(const char *uri, const char *interfaceName)
{
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    return OCBindResourceInterfaceToResource(handle, interfaceName);
}

OCStackResult DoResource(OCDoHandle *handle,
        OCMethod method,
        const char *uri,
        const OCDevAddr *destination,
        OCPayload *payload,
        OCCallbackData *cbData,
        OCHeaderOption *options,
        uint8_t numOptions)
{
    return OCDoResource(handle, method, uri, destination, payload,
            CT_DEFAULT, OC_HIGH_QOS, cbData, options, numOptions);
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
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
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
    OCResourceHandle *hs = new OCResourceHandle[nr];
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
    result = OCRDPublish(NULL, gRD.c_str(), CT_DEFAULT, hs, nhs, &cbData, OC_HIGH_QOS);
    delete[] hs;
    return result;
}
