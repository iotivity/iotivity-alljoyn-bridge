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

#include "DeviceConfigurationResource.h"

#include "Log.h"
#include "Payload.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"

OCStackResult SetDeviceConfigurationProperties(OCRepPayload *payload, AboutData *aboutData)
{
    assert(payload);
    assert(aboutData);

    OCStackResult result = OC_STACK_ERROR;
    size_t numLangs = aboutData->GetSupportedLanguages();
    const char **langs = new const char *[numLangs];
    aboutData->GetSupportedLanguages(langs, numLangs);
    size_t nf = aboutData->GetFields();
    const char *fs[nf] = { 0 };
    aboutData->GetFields(fs, nf);

    char *s = NULL;
    aboutData->GetDefaultLanguage(&s);
    if (s)
    {
        OCRepPayloadSetPropString(payload, "dl", s);
    }
    char *name = NULL;
    aboutData->GetAppName(&name);
    if (name)
    {
        // TODO This is a mandatory parameter on OC side, but not AJ side.
        OCRepPayloadSetPropString(payload, OC_RSRVD_DEVICE_NAME, name);
    }
    ajn::MsgArg *arg = NULL;
    size_t nd;
    double *d;
    aboutData->GetField("org.openconnectivity.loc", arg);
    if (arg && (arg->Get("ad", &nd, &d) == ER_OK))
    {
        size_t dim[MAX_REP_ARRAY_DEPTH] = { nd, 0, 0 };
        OCRepPayloadSetDoubleArray(payload, "loc", d, dim);
    }
    arg = NULL;
    aboutData->GetField("org.openconnectivity.locn", arg);
    if (arg && (arg->Get("s", &s) == ER_OK))
    {
        OCRepPayloadSetPropString(payload, "locn", s);
    }
    arg = NULL;
    aboutData->GetField("org.openconnectivity.c", arg);
    if (arg && (arg->Get("s", &s) == ER_OK))
    {
        OCRepPayloadSetPropString(payload, "c", s);
    }
    arg = NULL;
    aboutData->GetField("org.openconnectivity.r", arg);
    if (arg && (arg->Get("s", &s) == ER_OK))
    {
        OCRepPayloadSetPropString(payload, "r", s);
    }
    size_t lnsDim[MAX_REP_ARRAY_DEPTH] = { numLangs, 0, 0 };
    OCRepPayload **lns = NULL;
    if (name && numLangs)
    {
        OCRepPayload **lns = (OCRepPayload **) OICCalloc(numLangs, sizeof(OCRepPayload *));
        if (!lns)
        {
            goto exit;
        }
        for (size_t i = 0; i < numLangs; ++i)
        {
            lns[i] = OCRepPayloadCreate();
            if (!lns[i])
            {
                goto exit;
            }
            s = NULL;
            aboutData->GetAppName(&s, langs[i]);
            if (s)
            {
                OCRepPayloadSetPropString(lns[i], "value", s);
                OCRepPayloadSetPropString(lns[i], "language", langs[i]);
            }
            else
            {
                LOG(LOG_INFO, "Invalid AboutData - missing AppName in supported language");
            }
        }
        if (!OCRepPayloadSetPropObjectArrayAsOwner(payload, "ln", lns, lnsDim))
        {
            goto exit;
        }
    }
    for (size_t i = 0; i < nf; ++i)
    {
        if (!AboutData::IsVendorField(fs[i]))
        {
            continue;
        }
        arg = NULL;
        aboutData->GetField(fs[i], arg);
        if (arg && (arg->Get("s", &s) == ER_OK))
        {
            std::string name = std::string("x.") + fs[i];
            OCRepPayloadSetPropString(payload, name.c_str(), s);
        }
    }
    result = OC_STACK_OK;

exit:
    if (result != OC_STACK_OK)
    {
        if (lns)
        {
            size_t dimTotal = calcDimTotal(lnsDim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(lns[i]);
            }
            OICFree(lns);
        }
    }
    return result;
}
