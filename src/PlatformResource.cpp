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

#include "PlatformResource.h"

#include "AboutData.h"
#include "ocstack.h"

OCStackResult SetPlatformProperties(ajn::AboutData *aboutData)
{
    char pi[UUID_STRING_SIZE] = { 0 };
    GetPlatformId(pi, aboutData);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, pi);
    char *s = NULL;
    aboutData->GetManufacturer(&s);
    if (s)
    {
        char mfgName[17];
        strncpy(mfgName, s, 17);
        mfgName[16] = '\0';
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_NAME, mfgName);
    }
    ajn::MsgArg *arg = NULL;
    aboutData->GetField("org.openconnectivity.mnml", arg);
    if (arg)
    {
        s = NULL;
        arg->Get("s", &s);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_URL, s);
    }
    s = NULL;
    aboutData->GetModelNumber(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MODEL_NUM, s);
    s = NULL;
    aboutData->GetDateOfManufacture(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, s);
    arg = NULL;
    aboutData->GetField("org.openconnectivity.mnpv", arg);
    if (arg)
    {
        s = NULL;
        arg->Get("s", &s);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_VERSION, s);
    }
    arg = NULL;
    aboutData->GetField("org.openconnectivity.mnos", arg);
    if (arg)
    {
        s = NULL;
        arg->Get("s", &s);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_OS_VERSION, s);
    }
    s = NULL;
    aboutData->GetHardwareVersion(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, s);
    arg = NULL;
    aboutData->GetField("org.openconnectivity.mnfv", arg);
    if (arg)
    {
        s = NULL;
        arg->Get("s", &s);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_FIRMWARE_VERSION, s);
    }
    s = NULL;
    aboutData->GetSupportUrl(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, s);
    arg = NULL;
    aboutData->GetField("org.openconnectivity.st", arg);
    if (arg)
    {
        s = NULL;
        arg->Get("s", &s);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SYSTEM_TIME, s);
    }
    s = NULL;
    aboutData->GetDeviceId(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_VID, s);
    return OC_STACK_OK;
}
