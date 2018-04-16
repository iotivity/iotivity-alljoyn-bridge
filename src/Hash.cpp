//******************************************************************
//
// Copyright 2017 Intel Corporation All Rights Reserved.
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

#include "Hash.h"

#include "Log.h"
#ifdef WITH_POSIX
#include <openssl/sha.h>
#endif

void Hash(OCUUIdentity *id, const char *deviceId, const uint8_t *appId, size_t n)
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
