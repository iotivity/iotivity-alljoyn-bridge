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

#include <inttypes.h>
#include <alljoyn/Init.h>
#include <signal.h>
#include <sstream>
#include <stdlib.h>
#include "ocstack.h"
#include "rd_client.h"
#include "rd_server.h"
#include "Bridge.h"
#include "Plugin.h"

static volatile sig_atomic_t sQuitFlag = false;
static const char *gPSPrefix = "AllJoynBridge";
static const char *sUUID = NULL;
static const char *sSender = NULL;
static const char *sRD = NULL;

static void SigIntCB(int sig)
{
    (void) sig;
    sQuitFlag = true;
}

static std::string GetFilename(const char *uuid, const char *suffix)
{
    std::string path = gPSPrefix;
    if (uuid)
    {
        path += std::string(uuid) + "_";
    }
    if (suffix)
    {
        path += suffix;
    }
    return path;
}

static FILE *PSOpenCB(const char *suffix, const char *mode)
{
    std::string path = GetFilename(sUUID, suffix);
    return fopen(path.c_str(), mode);
}

static OCStackApplicationResult DiscoverResourceDirectoryCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) ctx;
    (void) handle;

    if (!response || !response->payload || response->result != OC_STACK_OK)
    {
        return OC_STACK_KEEP_TRANSACTION;
    }
    if (response->payload->type != PAYLOAD_TYPE_DISCOVERY)
    {
        return OC_STACK_KEEP_TRANSACTION;
    }
    OCDiscoveryPayload *payload = (OCDiscoveryPayload *) response->payload;
    if (sRD && !strcmp(payload->sid, sRD))
    {
        std::ostringstream oss;
        oss << response->devAddr.addr << ":" << response->devAddr.port;
        gRD = oss.str();
        return OC_STACK_DELETE_TRANSACTION;
    }
    else
    {
        return OC_STACK_KEEP_TRANSACTION;
    }
}

static OCStackApplicationResult RDDeleteCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    bool *done = (bool *) ctx;
    (void) handle;
    LOG(LOG_INFO, "response=%p,response->result=%d",
        response, response ? response->result : 0);
    *done = true;
    return OC_STACK_DELETE_TRANSACTION;
}

static void ExecCB(const char *uuid, const char *sender, bool isVirtual)
{
    printf("exec --ps %s --uuid %s --sender %s --rd %s %s\n",
            gPSPrefix, uuid, sender, OCGetServerInstanceIDString(),
            isVirtual ? "--virtual" : "");
    fflush(stdout);
}

static void KillCB(const char *uuid)
{
    printf("kill --uuid %s\n", uuid);
    fflush(stdout);
}

static Bridge::SeenState GetSeenStateCB(const char *uuid)
{
    std::string seenPath = GetFilename(uuid, "seen.state");
    FILE *fp = fopen(seenPath.c_str(), "r");
    if (!fp)
    {
        return Bridge::NOT_SEEN;
    }
    char stateStr[16];
    fscanf(fp, "%s", stateStr);
    fclose(fp);
    if (!strcmp("virtual", stateStr))
    {
        return Bridge::SEEN_VIRTUAL;
    }
    else
    {
        return Bridge::SEEN_NATIVE;
    }
}

static void SessionLostCB()
{
    LOG(LOG_INFO, "SessionLostCB");
    sQuitFlag = true;
}

int main(int argc, char **argv)
{
    int ret = EXIT_FAILURE;
    Bridge *bridge = NULL;
    std::string dbFilename;
    OCStackResult result;
    OCPersistentStorage ps = { PSOpenCB, fread, fwrite, fclose, unlink };

    int protocols = 0;
    bool isVirtual = false;
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (!strcmp(argv[i], "--ps") && (i < (argc - 1)))
            {
                gPSPrefix = argv[++i];
            }
            else if (!strcmp(argv[i], "--aj"))
            {
                protocols |= Bridge::AJ;
            }
            else if (!strcmp(argv[i], "--oc"))
            {
                protocols |= Bridge::OC;
            }
            else if (!strcmp(argv[i], "--uuid") && (i < (argc - 1)))
            {
                sUUID = argv[++i];
            }
            else if (!strcmp(argv[i], "--sender") && (i < (argc - 1)))
            {
                sSender = argv[++i];
            }
            else if (!strcmp(argv[i], "--rd") && (i < (argc - 1)))
            {
                sRD = argv[++i];
            }
            else if (!strcmp(argv[i], "--virtual"))
            {
                isVirtual = true;
            }
        }
    }
    /* uuid, sender, and rd must be supplied together and when they are, aj and oc are ignored */
    if (protocols == 0)
    {
        protocols = Bridge::AJ | Bridge::OC;
    }
    std::string seenPath = GetFilename(sUUID, "seen.state");
    if (sSender)
    {
        FILE *fp = fopen(seenPath.c_str(), "w");
        fprintf(fp, "%s", isVirtual ? "virtual" : "native");
        fclose(fp);
    }

    signal(SIGINT, SigIntCB);

    QStatus status = AllJoynInit();
    if (status != ER_OK)
    {
        fprintf(stderr, "AllJoynInit - %s\n", QCC_StatusText(status));
        goto exit;
    }
    status = AllJoynRouterInit();
    if (status != ER_OK)
    {
        fprintf(stderr, "AllJoynRouterInit - %s\n", QCC_StatusText(status));
        goto exit;
    }

    result = OCRegisterPersistentStorageHandler(&ps);
    if (result != OC_STACK_OK)
    {
        fprintf(stderr, "OCRegisterPersistentStorageHandler - %d\n", result);
        goto exit;
    }
    dbFilename = GetFilename(NULL, "RD.db");
    result = OCRDDatabaseSetStorageFilename(dbFilename.c_str());
    if (result != OC_STACK_OK)
    {
        fprintf(stderr, "OCRDDatabaseSetStorageFilename - %d\n", result);
        goto exit;
    }
    result = OCInit1(OC_CLIENT_SERVER, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS);
    if (result != OC_STACK_OK)
    {
        fprintf(stderr, "OCInit1 - %d\n", result);
        goto exit;
    }
    if (sSender)
    {
        result = OCStopMulticastServer();
        if (result != OC_STACK_OK)
        {
            fprintf(stderr, "OCStopMulticastServer - %d\n", result);
            goto exit;
        }
        OCCallbackData cbData;
        cbData.cb = DiscoverResourceDirectoryCB;
        cbData.context = NULL;
        cbData.cd = NULL;
        result = OCDoResource(NULL, OC_REST_DISCOVER, "/oic/res?rt=oic.wk.rd", NULL, 0,
                CT_DEFAULT, OC_HIGH_QOS, &cbData, NULL, 0);
        if (result != OC_STACK_OK)
        {
            fprintf(stderr, "DoResource(OC_REST_DISCOVER) - %d\n", result);
            goto exit;
        }
        while (!sQuitFlag && gRD.empty())
        {
            result = OCProcess();
            if (result != OC_STACK_OK)
            {
                fprintf(stderr, "OCProcess - %d\n", result);
                goto exit;
            }
        }
    }
    else
    {
        result = OCRDStart();
        if (result != OC_STACK_OK)
        {
            fprintf(stderr, "OCRDStart() - %d\n", result);
            goto exit;
        }
    }

    if (sUUID && sSender)
    {
        bridge = new Bridge(gPSPrefix, sUUID, sSender);
        bridge->SetSessionLostCB(SessionLostCB);
    }
    else
    {
        bridge = new Bridge(gPSPrefix, (Bridge::Protocol) protocols);
        bridge->SetProcessCB(ExecCB, KillCB, GetSeenStateCB);
    }
    if (!bridge->Start())
    {
        goto exit;
    }
    while (!sQuitFlag)
    {
        if (!bridge->Process())
        {
            goto exit;
        }
        OCStackResult result = OCProcess();
        if (result != OC_STACK_OK)
        {
            fprintf(stderr, "OCProcess - %d\n", result);
            goto exit;
        }
#ifdef _WIN32
        Sleep(1);
#else
        usleep(1 * 1000);
#endif
    }
    ret = EXIT_SUCCESS;

exit:
    if (bridge)
    {
        bridge->Stop();
        delete bridge;
    }
    if (sSender)
    {
        bool done = false;
        OCCallbackData cbData;
        cbData.cb = RDDeleteCB;
        cbData.context = &done;
        cbData.cd = NULL;
        result = OCRDDelete(NULL, gRD.c_str(), CT_DEFAULT, NULL, 0, &cbData, OC_HIGH_QOS);
        while (!done)
        {
            result = OCProcess();
            if (result != OC_STACK_OK)
            {
                break;
            }
#ifdef _WIN32
            Sleep(1);
#else
            usleep(1 * 1000);
#endif
        }
        remove(seenPath.c_str());
    }
    else
    {
        OCRDStop();
    }
    OCStop();
    AllJoynRouterShutdown();
    AllJoynShutdown();
    return ret;
}
