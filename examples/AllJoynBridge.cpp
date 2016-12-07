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
#include <stdlib.h>
#include "ocstack.h"
#include "Bridge.h"

static volatile sig_atomic_t gQuitFlag = false;
static const char *gPSPath = NULL;

static void SigIntCB(int sig)
{
    (void) sig;
    gQuitFlag = true;
}

static FILE *PSOpenCB(const char *defaultPath, const char *mode)
{
    std::string path = gPSPath ? gPSPath : defaultPath;
    return fopen(path.c_str(), mode);
}

int main(int argc, char **argv)
{
    int protocols = 0;
    if (argc > 1)
    {
        for (int i = 1; i < argc; ++i)
        {
            if (!strcmp(argv[i], "--aj"))
            {
                protocols |= Bridge::AJ;
            }
            else if (!strcmp(argv[i], "--oc"))
            {
                protocols |= Bridge::OC;
            }
            else if (!strcmp(argv[i], "--ps") && (i < (argc - 1)))
            {
                gPSPath = argv[++i];
            }
        }
    }
    if (protocols == 0)
    {
        protocols = Bridge::AJ | Bridge::OC;
    }

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
    result = OCInit(NULL, 0, OC_CLIENT_SERVER);
    if (result != OC_STACK_OK)
    {
        fprintf(stderr, "OCInit - %d\n", result);
        return EXIT_FAILURE;
    }

    Bridge *bridge = new Bridge((Bridge::Protocol) protocols);
    if (!bridge->Start())
    {
        return EXIT_FAILURE;
    }
    signal(SIGINT, SigIntCB);
    while (!gQuitFlag)
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
    }

    bridge->Stop();
    delete bridge;
    OCStop();
    AllJoynRouterShutdown();
    AllJoynShutdown();
    return EXIT_SUCCESS;
}
