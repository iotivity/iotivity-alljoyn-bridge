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

#include "Log.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include "ocstack.h"
#include "rd_client.h"
#include <map>
#include <string>

std::string gRD;

OCStackResult DoResource(OCDoHandle *handle,
        OCMethod method,
        const char *uri,
        const OCDevAddr *destination,
        OCPayload *payload,
        OCCallbackData *cbData,
        OCHeaderOption *options,
        uint8_t numOptions)
{
    return OCDoResource(handle, method, uri, destination, payload, CT_DEFAULT, OC_HIGH_QOS, cbData,
            options, numOptions);
}

OCStackResult DoResource(OCDoHandle *handle,
        OCMethod method,
        const char *uri,
        const std::vector<OCDevAddr> &destinations,
        OCPayload *payload,
        OCCallbackData *cbData,
        OCHeaderOption *options,
        uint8_t numOptions)
{
    /* Prefer secure destination when present otherwise just use the first destination */
    const OCDevAddr *destination = destinations.empty() ? NULL : &destinations[0];
    for (std::vector<OCDevAddr>::const_iterator it = destinations.begin(); it != destinations.end();
         ++it)
    {
        if (it->flags & OC_FLAG_SECURE)
        {
            destination = &(*it);
            break;
        }
    }
    return OCDoResource(handle, method, uri, destination, payload, CT_DEFAULT, OC_HIGH_QOS, cbData,
            options, numOptions);
}

OCStackResult Cancel(OCDoHandle handle, OCQualityOfService qos)
{
    return OCCancel(handle, qos, NULL, 0);
}

static OCStackApplicationResult RDPublishCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) ctx;
    (void) handle;
    LOG(LOG_INFO, "response=%p,response->result=%d", response, response ? response->result : 0);
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
    std::vector<OCResourceHandle> hs;
    for (uint8_t i = 0; i < nr; ++i)
    {
        OCResourceHandle h = OCGetResourceHandle(i);
        if (OCGetResourceProperties(h) & OC_DISCOVERABLE)
        {
            hs.push_back(h);
        }
    }
    OCCallbackData cbData;
    cbData.cb = RDPublishCB;
    cbData.context = NULL;
    cbData.cd = NULL;
    return OCRDPublish(NULL, gRD.c_str(), CT_DEFAULT, &hs[0], hs.size(), &cbData, OC_HIGH_QOS);
}
