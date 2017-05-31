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

#include "PlatformConfigurationResource.h"

#include "ocpayload.h"
#include "oic_malloc.h"
#include <assert.h>

OCStackResult SetPlatformConfigurationProperties(OCRepPayload *payload, AboutData *aboutData)
{
    assert(payload);
    assert(aboutData);

    OCStackResult result = OC_STACK_ERROR;
    size_t numLangs = aboutData->GetSupportedLanguages();
    const char **langs = new const char *[numLangs];
    aboutData->GetSupportedLanguages(langs, numLangs);

    size_t mnpnsDim[MAX_REP_ARRAY_DEPTH] = { numLangs, 0, 0 };
    OCRepPayload **mnpns = NULL;
    if (numLangs)
    {
        OCRepPayload **mnpns = (OCRepPayload **) OICCalloc(numLangs, sizeof(OCRepPayload *));
        if (!mnpns)
        {
            goto exit;
        }
        for (size_t i = 0; i < numLangs; ++i)
        {
            mnpns[i] = OCRepPayloadCreate();
            if (!mnpns[i])
            {
                goto exit;
            }
            char *s = NULL;
            aboutData->GetDeviceName(&s, langs[i]);
            OCRepPayloadSetPropString(mnpns[i], "value", s);
            OCRepPayloadSetPropString(mnpns[i], "language", langs[i]);
        }
        if (!OCRepPayloadSetPropObjectArrayAsOwner(payload, "mnpn", mnpns, mnpnsDim))
        {
            goto exit;
        }
    }
    result = OC_STACK_OK;

exit:
    if (result != OC_STACK_OK)
    {
        if (mnpns)
        {
            size_t dimTotal = calcDimTotal(mnpnsDim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(mnpns[i]);
            }
            OICFree(mnpns);
        }
    }
    return result;
}
