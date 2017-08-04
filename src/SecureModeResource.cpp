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

#include "SecureModeResource.h"

#include "Log.h"
#include "Resource.h"
#include "ocpayload.h"
#include "ocstack.h"

SecureModeResource::SecureModeResource(std::mutex &mutex, bool secureMode)
    : m_mutex(mutex), m_secureMode(secureMode), m_handle(NULL)
{
}

SecureModeResource::~SecureModeResource()
{
    OCDeleteResource(m_handle);
}

OCStackResult SecureModeResource::Create()
{
    return CreateResource(&m_handle, OC_RSRVD_SECURE_MODE_URI, OC_RSRVD_RESOURCE_TYPE_SECURE_MODE,
            OC_RSRVD_INTERFACE_READ_WRITE, SecureModeResource::EntityHandlerCB, this,
            OC_DISCOVERABLE | OC_OBSERVABLE);
}

/* Called with m_mutex held. */
OCRepPayload *SecureModeResource::GetSecureMode(OCEntityHandlerRequest *request)
{
    OCRepPayload *payload = CreatePayload(request->resource, request->query);
    if (!OCRepPayloadSetPropBool(payload, "secureMode", m_secureMode))
    {
        OCRepPayloadDestroy(payload);
        payload = NULL;
    }
    return payload;
}

/* Called with m_mutex held. */
bool SecureModeResource::PostSecureMode(OCEntityHandlerRequest *request, bool &hasChanged)
{
    OCRepPayload *payload = (OCRepPayload *) request->payload;
    bool secureMode;
    if (!OCRepPayloadGetPropBool(payload, "secureMode", &secureMode))
    {
        return false;
    }
    hasChanged = (m_secureMode != secureMode);
    m_secureMode = secureMode;
    return true;
}

OCEntityHandlerResult SecureModeResource::EntityHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p", ctx, flag, request, ctx);
    if (!IsValidRequest(request))
    {
        LOG(LOG_INFO, "Invalid request received");
        return OC_EH_BAD_REQ;
    }

    SecureModeResource *thiz = reinterpret_cast<SecureModeResource *>(ctx);
    thiz->m_mutex.lock();
    bool hasChanged = false;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = thiz->GetSecureMode(request);
                if (!payload)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(payload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "OCDoResponse - %d", doResult);
                    OCRepPayloadDestroy(payload);
                }
                break;
            }
        case OC_REST_POST:
            {
                if (!request->payload || request->payload->type != PAYLOAD_TYPE_REPRESENTATION)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                if (!thiz->PostSecureMode(request, hasChanged))
                {
                    result = OC_EH_ERROR;
                    break;
                }
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *outPayload = thiz->GetSecureMode(request);
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "OCDoResponse - %d", doResult);
                    OCRepPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    thiz->m_mutex.unlock();
    if (hasChanged)
    {
        OCNotifyAllObservers(request->resource, OC_NA_QOS);
    }
    return result;
}
