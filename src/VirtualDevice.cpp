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

#include "VirtualDevice.h"

#include "Plugin.h"
#include "ocpayload.h"
#ifdef WITH_POSIX
#include <openssl/sha.h>
#endif

const char *VirtualDevice::m_specVersion = "0.3";

VirtualDevice::VirtualDevice(const char *name, ajn::SessionId sessionId)
    : m_name(name)
    , m_sessionId(sessionId)
{
    LOG(LOG_INFO, "[%p] name=%s,sessionId=%d",
        this, name, sessionId);
}

VirtualDevice::~VirtualDevice()
{
    LOG(LOG_INFO, "[%p]",
        this);

    OCStackResult result = StopPresence();
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "StopPresence - %d", result);
    }
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

void VirtualDevice::SetInfo(ajn::AboutObjectDescription &objectDescription,
                            ajn::AboutData &aboutData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aboutData = aboutData;

    OCStackResult result;
    char *deviceId;
    m_aboutData.GetDeviceId(&deviceId);
    OCUUIdentity di;
    DeriveDi(&di, deviceId);
    OCDeviceInfo device;
    memset(&device, 0, sizeof(device));
    m_aboutData.GetAppName(&device.deviceName);
    device.specVersion = (char *) m_specVersion;
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
    for (std::string dataModelVersion : dataModelVersions)
    {
        OCResourcePayloadAddStringLL(&device.dataModelVersions, dataModelVersion.c_str());
    }
    if (!OCResourcePayloadAddStringLL(&device.types, "oic.wk.d"))
    {
        LOG(LOG_ERR, "OCResourcePayloadAddStringLL - fail");
    }
    result = SetDeviceInfo(&di, device);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "SetDeviceInfo - %d", result);
    }

    OCPlatformInfo platform;
    memset(&platform, 0, sizeof(platform));
    m_aboutData.GetDeviceId(&platform.platformID);
    m_aboutData.GetManufacturer(&platform.manufacturerName);
    m_aboutData.GetModelNumber(&platform.modelNumber);
    m_aboutData.GetDateOfManufacture(&platform.dateOfManufacture);
    m_aboutData.GetHardwareVersion(&platform.hardwareVersion);
    m_aboutData.GetSupportUrl(&platform.supportUrl);
    result = SetPlatformInfo(platform);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "SetPlatformInfo - %d", result);
    }
}

void VirtualDevice::StartPresence()
{
    OCStackResult result = ::StartPresence();
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "StartPresence - %d", result);
    }
}
