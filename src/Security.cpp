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

#include "Security.h"

#include "Plugin.h"
#include "pinoxmcommon.h"

bool OCSecurity::Init()
{
#if __WITH_DTLS__
    OCStackResult result = SetDisplayPinWithContextCB(OCSecurity::DisplayPinCB, this);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "SetDisplayPinWithContextCB() - %d", result);
        return false;
    }
    SetClosePinDisplayCB(OCSecurity::ClosePinDisplayCB);
    result = SetRandomPinPolicy(OXM_RANDOM_PIN_DEFAULT_SIZE,
            (OicSecPinType_t) OXM_RANDOM_PIN_DEFAULT_PIN_TYPE);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "SetRandomPinPolicy() - %d", result);
        return false;
    }
#endif
    return true;
}

void OCSecurity::DisplayPinCB(char *pin, size_t pinSize, void *context)
{
    LOG(LOG_INFO, "pin=%s,pinSize=%d,context=%p", pin, pinSize, context);
}

void OCSecurity::ClosePinDisplayCB()
{
    LOG(LOG_INFO, "");
}
