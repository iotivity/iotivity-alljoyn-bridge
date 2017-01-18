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

static FILE *PSOpenCB(const char *defaultPath, const char *mode)
{
    std::string path = gPSPrefix;
    if (sUUID)
    {
        path += std::string(sUUID) + ".dat";
    }
    else
    {
        path += defaultPath;
    }
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

static void AnnouncedCB(const char *uuid, const char *sender)
{
    printf("exec --ps %s --uuid %s --sender %s --rd %s\n",
           gPSPrefix, uuid, sender, OCGetServerInstanceIDString());
    fflush(stdout);
}

static void SessionLostCB()
{
    LOG(LOG_INFO, "SessionLostCB");
    sQuitFlag = true;
}

int main(int argc, char **argv)
{
    int protocols = 0;
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
        }
    }
    /* uuid, sender, and rd must be supplied together and when they are, aj and oc are ignored */
    if (protocols == 0)
    {
        protocols = Bridge::AJ | Bridge::OC;
    }

    signal(SIGINT, SigIntCB);

    QStatus status = AllJoynInit();
    if (status != ER_OK)
    {
        fprintf(stderr, "AllJoynInit - %s\n", QCC_StatusText(status));
        return EXIT_FAILURE;
    }
    status = AllJoynRouterInit();
    if (status != ER_OK)
    {
        fprintf(stderr, "AllJoynRouterInit - %s\n", QCC_StatusText(status));
        return EXIT_FAILURE;
    }

    OCPersistentStorage ps = { PSOpenCB, fread, fwrite, fclose, unlink };
    OCStackResult result = OCRegisterPersistentStorageHandler(&ps);
    if (result != OC_STACK_OK)
    {
        fprintf(stderr, "OCRegisterPersistentStorageHandler - %d\n", result);
        return EXIT_FAILURE;
    }
    std::string dbFilename = std::string(gPSPrefix) + ".db";
    result = OCRDDatabaseSetStorageFilename(dbFilename.c_str());
    if (result != OC_STACK_OK)
    {
        fprintf(stderr, "OCRDDatabaseSetStorageFilename - %d\n", result);
        return EXIT_FAILURE;
    }
    result = OCInit1(OC_CLIENT_SERVER, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS);
    if (result != OC_STACK_OK)
    {
        fprintf(stderr, "OCInit1 - %d\n", result);
        return EXIT_FAILURE;
    }
    if (sSender)
    {
        result = OCStopMulticastServer();
        if (result != OC_STACK_OK)
        {
            fprintf(stderr, "OCStopMulticastServer - %d\n", result);
            return EXIT_FAILURE;
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
            return EXIT_FAILURE;
        }
        while (!sQuitFlag && gRD.empty())
        {
            result = OCProcess();
            if (result != OC_STACK_OK)
            {
                fprintf(stderr, "OCProcess - %d\n", result);
                return EXIT_FAILURE;
            }
        }
    }
    else
    {
        result = OCRDStart();
        if (result != OC_STACK_OK)
        {
            fprintf(stderr, "OCRDStart() - %d\n", result);
            return EXIT_FAILURE;
        }
    }

    Bridge *bridge = NULL;
    if (sUUID && sSender)
    {
        bridge = new Bridge(gPSPrefix, sUUID, sSender);
        bridge->SetSessionLostCB(SessionLostCB);
    }
    else
    {
        bridge = new Bridge(gPSPrefix, (Bridge::Protocol) protocols);
        bridge->SetAnnouncedCB(AnnouncedCB);
    }
    if (!bridge->Start())
    {
        return EXIT_FAILURE;
    }
    while (!sQuitFlag)
    {
        if (!bridge->Process())
        {
            return EXIT_FAILURE;
        }
        OCStackResult result = OCProcess();
        if (result != OC_STACK_OK)
        {
            fprintf(stderr, "OCProcess - %d\n", result);
            return EXIT_FAILURE;
        }
        usleep(10 * 1000);
    }

    bridge->Stop();
    delete bridge;
    if (sSender)
    {
        bool done = false;
        OCCallbackData cbData;
        cbData.cb = RDDeleteCB;
        cbData.context = &done;
        cbData.cd = NULL;
        result = OCRDDelete(gRD.c_str(), CT_DEFAULT, NULL, 0, &cbData, OC_HIGH_QOS);
        while (!done)
        {
            result = OCProcess();
            if (result != OC_STACK_OK)
            {
                break;
            }
        }
    }
    else
    {
        OCRDStop();
    }
    OCStop();
    AllJoynRouterShutdown();
    AllJoynShutdown();
    return EXIT_SUCCESS;
}
