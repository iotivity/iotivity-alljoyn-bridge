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

const char *GetServerInstanceIDString()
{
    return OCGetServerInstanceIDString();
}

OCStackResult StartPresence()
{
    return OC_STACK_OK;
}

OCStackResult StopPresence()
{
    return OC_STACK_OK;
}

OCStackResult CreateResource(const char *uri,
                             const char *typeName,
                             const char *interfaceName,
                             OCEntityHandler entityHandler,
                             void *callbackParam,
                             uint8_t properties)
{
    OCStackResult result;
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    if (handle)
    {
        result = OC_STACK_OK;
    }
    else
    {
        result = OCCreateResource(&handle, typeName, interfaceName, uri,
                                  entityHandler, callbackParam, properties);
    }
    return result;
}

OCStackResult DestroyResource(const char *uri)
{
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    return OCDeleteResource(handle);
}

OCStackResult AddResourceType(const char *uri, const char *typeName)
{
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    return OCBindResourceTypeToResource(handle, typeName);
}

OCStackResult AddInterface(const char *uri, const char *interfaceName)
{
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    return OCBindResourceInterfaceToResource(handle, interfaceName);
}

OCStackResult DoResource(OCDoHandle *handle,
        OCMethod method,
        const char *uri,
        const OCDevAddr *destination,
        OCPayload *payload,
        OCCallbackData *cbData,
        OCHeaderOption *options,
        uint8_t numOptions)
{
    return OCDoResource(handle, method, uri, destination, payload,
            CT_DEFAULT, OC_HIGH_QOS, cbData, options, numOptions);
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
    return OCDoResource(handle, method, uri, destination, payload,
            CT_DEFAULT, OC_HIGH_QOS, cbData, options, numOptions);
}

OCStackResult Cancel(OCDoHandle handle, OCQualityOfService qos)
{
    return OCCancel(handle, qos, NULL, 0);
}

OCStackResult DoResponse(OCEntityHandlerResponse *response)
{
    return OCDoResponse(response);
}

OCStackResult NotifyListOfObservers(const char *uri,
                                    OCObservationId  *obsIdList,
                                    uint8_t numberOfIds,
                                    OCRepPayload *payload)
{
    OCResourceHandle handle = OCGetResourceHandleAtUri(uri);
    return OCNotifyListOfObservers(handle, obsIdList, numberOfIds, payload,
                                   OC_HIGH_QOS);
}

static OCStackApplicationResult RDPublishCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) ctx;
    (void) handle;
    LOG(LOG_INFO, "response=%p,response->result=%d",
        response, response ? response->result : 0);
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
    OCResourceHandle *hs = new OCResourceHandle[nr];
    uint8_t nhs = 0;
    for (uint8_t i = 0; i < nr; ++i)
    {
        OCResourceHandle h = OCGetResourceHandle(i);
        if (OCGetResourceProperties(h) & OC_DISCOVERABLE)
        {
            hs[nhs++] = h;
        }
    }
    OCCallbackData cbData;
    cbData.cb = RDPublishCB;
    cbData.context = NULL;
    cbData.cd = NULL;
    result = OCRDPublish(NULL, gRD.c_str(), CT_DEFAULT, hs, nhs, &cbData, OC_HIGH_QOS);
    delete[] hs;
    return result;
}
