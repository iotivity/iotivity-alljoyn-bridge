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

#include "UnitTest.h"

#include "cJSON.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "oic_time.h"
#include <alljoyn/Init.h>
#include <thread>

Callback::Callback(OCClientResponseHandler cb, void *context)
    : m_cb(cb), m_context(context), m_called(false)
{
    m_cbData.cb = &Callback::handler;
    m_cbData.cd = NULL;
    m_cbData.context = this;
}

OCStackResult Callback::Wait(long waitMs)
{
    uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
    while (!m_called)
    {
        uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
        long elapsed = (long)(currTime - startTime);
        if (elapsed > waitMs)
        {
            break;
        }
        OCProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return m_called ? OC_STACK_OK : OC_STACK_TIMEOUT;
}

OCStackApplicationResult Callback::handler(void *ctx, OCDoHandle handle,
        OCClientResponse *clientResponse)
{
    Callback *callback = (Callback *) ctx;
    OCStackApplicationResult result = callback->m_cb(callback->m_context, handle, clientResponse);
    callback->m_called = true;
    return result;
}

ResourceCallback::ResourceCallback()
    : m_response(NULL), m_called(false)
{
    m_cbData.cb = &ResourceCallback::handler;
    m_cbData.cd = NULL;
    m_cbData.context = this;
}

ResourceCallback::~ResourceCallback()
{
    if (m_response)
    {
        OICFree((void *) m_response->resourceUri);
        OCPayloadDestroy(m_response->payload);
    }
}

OCStackResult ResourceCallback::Wait(long waitMs)
{
    uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
    while (!m_called)
    {
        uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
        long elapsed = (long)(currTime - startTime);
        if (elapsed > waitMs)
        {
            break;
        }
        OCProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return m_called ? OC_STACK_OK : OC_STACK_TIMEOUT;
}

OCStackApplicationResult ResourceCallback::Handler(OCDoHandle handle, OCClientResponse *response)
{
    (void) handle;
    m_response = (OCClientResponse *) OICCalloc(1, sizeof(OCClientResponse));
    EXPECT_TRUE(m_response != NULL);
    memcpy(m_response, response, sizeof(OCClientResponse));
    m_response->addr = &m_response->devAddr;
    m_response->resourceUri = OICStrdup(response->resourceUri);
    if (response->payload)
    {
        switch (response->payload->type)
        {
            case PAYLOAD_TYPE_REPRESENTATION:
                m_response->payload = (OCPayload *) OCRepPayloadClone(
                    (OCRepPayload *) response->payload);
                break;
            case PAYLOAD_TYPE_DIAGNOSTIC:
                m_response->payload = (OCPayload *) OCDiagnosticPayloadCreate(
                    ((OCDiagnosticPayload *) response->payload)->message);
                break;
            default:
                m_response->payload = NULL;
                break;
        }
    }
    m_called = true;
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult ResourceCallback::handler(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    ResourceCallback *callback = (ResourceCallback *) ctx;
    return callback->Handler(handle, response);
}

void ObserveCallback::Reset()
{
    m_called = false;
}

OCStackApplicationResult ObserveCallback::Handler(OCDoHandle handle, OCClientResponse *response)
{
    (void) handle;
    m_response = (OCClientResponse *) OICCalloc(1, sizeof(OCClientResponse));
    EXPECT_TRUE(m_response != NULL);
    memcpy(m_response, response, sizeof(OCClientResponse));
    m_response->addr = &m_response->devAddr;
    m_response->resourceUri = OICStrdup(response->resourceUri);
    if (response->payload)
    {
        EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
        m_response->payload = (OCPayload *) OCRepPayloadClone((OCRepPayload *) response->payload);
    }
    m_called = true;
    return OC_STACK_KEEP_TRANSACTION;
}

CreateCallback::CreateCallback() : m_called(false)
{
}

OCStackResult CreateCallback::Wait(long waitMs)
{
    uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
    while (!m_called)
    {
        uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
        long elapsed = (long)(currTime - startTime);
        if (elapsed > waitMs)
        {
            break;
        }
        OCProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return m_called ? OC_STACK_OK : OC_STACK_TIMEOUT;
}

void CreateCallback::cb(void *ctx)
{
    CreateCallback *callback = (CreateCallback *) ctx;
    callback->m_called = true;
}

void AJOCSetUp::SetUp()
{
    EXPECT_EQ(OC_STACK_OK, OCInit2(OC_SERVER, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS, OC_ADAPTER_IP));
    EXPECT_EQ(ER_OK, AllJoynInit());
    EXPECT_EQ(ER_OK, AllJoynRouterInit());
}

void AJOCSetUp::TearDown()
{
    EXPECT_EQ(ER_OK, AllJoynShutdown());
    EXPECT_EQ(OC_STACK_OK, OCStop());
    /* This will fail if a test starts the stack and did not stop it */
    EXPECT_EQ(OC_STACK_ERROR, OCProcess());
}

void AJOCSetUp::Wait(long waitMs)
{
    uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
    for (;;)
    {
        uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
        long elapsed = (long)(currTime - startTime);
        if (elapsed > waitMs)
        {
            break;
        }
        OCProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

OCStackApplicationResult Discover(void *ctx, OCDoHandle handle, OCClientResponse *response)
{
    DiscoverContext *context = (DiscoverContext *) ctx;
    (void) handle;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_DISCOVERY, response->payload->type);
    OCDiscoveryPayload *payload = (OCDiscoveryPayload *) response->payload;
    context->m_device = new Device(response->devAddr, payload);
    context->m_resource = context->m_uri ? context->m_device->GetResourceUri(context->m_uri) : NULL;
    return OC_STACK_DELETE_TRANSACTION;
}

/* Only single-dimension homogenous arrays are supported for now */
static int JsonArrayType(cJSON *json, size_t dim[MAX_REP_ARRAY_DEPTH])
{
    dim[0] = 0;
    cJSON *element = json->child;
    if (!element)
    {
        return cJSON_NULL;
    }
    int type = element->type;
    while (element)
    {
        if (element->type != type)
        {
            return 0;
        }
        ++dim[0];
        element = element->next;
    }
    return type;
}

static OCStackResult ParseJsonItem(OCRepPayload *outPayload, cJSON *json)
{
    bool success = true;
    while (success && json)
    {
        switch (json->type)
        {
            case cJSON_False:
                success = OCRepPayloadSetPropBool(outPayload, json->string, false);
                break;
            case cJSON_True:
                success = OCRepPayloadSetPropBool(outPayload, json->string, true);
                break;
            case cJSON_NULL:
                success = false;
                break;
            case cJSON_Number:
                success = OCRepPayloadSetPropDouble(outPayload, json->string, json->valuedouble);
                break;
            case cJSON_String:
                success = OCRepPayloadSetPropString(outPayload, json->string, json->valuestring);
                break;
            case cJSON_Array:
                {
                    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
                    int type = JsonArrayType(json, dim);
                    size_t dimTotal = calcDimTotal(dim);
                    switch (type)
                    {
                        case cJSON_Number:
                            {
                                double *array = (double *) OICCalloc(dimTotal, sizeof(double));
                                if (!array)
                                {
                                    success = false;
                                    break;
                                }
                                cJSON *element = json->child;
                                for (size_t i = 0; i < dimTotal; ++i)
                                {
                                    array[i] = element->valuedouble;
                                    element = element->next;
                                }
                                success = OCRepPayloadSetDoubleArrayAsOwner(outPayload,
                                        json->string, array, dim);
                                if (!success)
                                {
                                    OICFree(array);
                                }
                            }
                            break;
                        case cJSON_String:
                            {
                                char **array = (char **) OICCalloc(dimTotal, sizeof(char*));
                                if (!array)
                                {
                                    success = false;
                                    break;
                                }
                                cJSON *element = json->child;
                                for (size_t i = 0; i < dimTotal; ++i)
                                {
                                    array[i] = OICStrdup(element->valuestring);
                                    if (!array[i])
                                    {
                                        success = false;
                                        break;
                                    }
                                    element = element->next;
                                }
                                if (success)
                                {
                                    success = OCRepPayloadSetStringArrayAsOwner(outPayload,
                                            json->string, array, dim);
                                }
                                if (!success)
                                {
                                    for (size_t i = 0; i < dimTotal; ++i)
                                    {
                                        OICFree(array[i]);
                                    }
                                    OICFree(array);
                                }
                            }
                            break;
                        case cJSON_Object:
                            {
                                OCRepPayload **array = (OCRepPayload **) OICCalloc(dimTotal,
                                        sizeof(OCRepPayload*));
                                if (!array)
                                {
                                    success = false;
                                    break;
                                }
                                cJSON *element = json->child;
                                for (size_t i = 0; i < dimTotal; ++i)
                                {
                                    array[i] = OCRepPayloadCreate();
                                    if (!array[i])
                                    {
                                        success = false;
                                        break;
                                    }
                                    OCStackResult result = ParseJsonItem(array[i], element->child);
                                    if (result != OC_STACK_OK)
                                    {
                                        success = false;
                                        break;
                                    }
                                    element = element->next;
                                }
                                if (success)
                                {
                                    success = OCRepPayloadSetPropObjectArrayAsOwner(outPayload,
                                            json->string, array, dim);
                                }
                                if (!success)
                                {
                                    for (size_t i = 0; i < dimTotal; ++i)
                                    {
                                        OCRepPayloadDestroy(array[i]);
                                    }
                                    OICFree(array);
                                }
                            }
                            break;
                        default:
                            /* Only number, string, and object arrays are supported for now */
                            assert(0);
                            success = false;
                            break;
                    }
                }
                break;
            case cJSON_Object:
                {
                    OCRepPayload *objPayload = OCRepPayloadCreate();
                    if (!objPayload)
                    {
                        success = false;
                        break;
                    }
                    cJSON *obj = json->child;
                    OCStackResult result = ParseJsonItem(objPayload, obj);
                    if (result != OC_STACK_OK)
                    {
                        success = false;
                        break;
                    }
                    success = OCRepPayloadSetPropObjectAsOwner(outPayload, json->string, objPayload);
                }
                break;
        }
        json = json->next;
    }
    return success ? OC_STACK_OK : OC_STACK_ERROR;
}

OCStackResult ParseJsonPayload(OCRepPayload** outPayload, const char* payload)
{
    OCStackResult result = OC_STACK_INVALID_PARAM;
    cJSON *json;

    *outPayload = NULL;
    json = cJSON_Parse(payload);
    if (!json)
    {
        goto exit;
    }
    if (json->type != cJSON_Object)
    {
        goto exit;
    }
    *outPayload = OCRepPayloadCreate();
    if (!*outPayload)
    {
        goto exit;
    }
    result = ParseJsonItem(*outPayload, json->child);
exit:
    if (json)
    {
        cJSON_Delete(json);
    }
    if (result != OC_STACK_OK)
    {
        OCRepPayloadDestroy(*outPayload);
        *outPayload = NULL;
    }
    return result;
}

MethodCall::MethodCall(ajn::BusAttachment *bus, ajn::ProxyBusObject *proxyObj)
    : m_proxyObj(proxyObj), m_reply(*bus), m_called(false)
{
}

QStatus MethodCall::Call(const char *iface, const char *method, const ajn::MsgArg *args,
        size_t numArgs)
{
    return m_proxyObj->MethodCallAsync(iface, method, this,
            static_cast<MessageReceiver::ReplyHandler>(&MethodCall::ReplyHandler), args,
            numArgs);
}

QStatus MethodCall::Wait(long waitMs)
{
    uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
    while (!m_called)
    {
        uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
        long elapsed = (long)(currTime - startTime);
        if (elapsed > waitMs)
        {
            break;
        }
        OCProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    if (!m_called)
    {
        return ER_TIMEOUT;
    }
    if (m_reply->GetType() != ajn::MESSAGE_METHOD_RET)
    {
        return ER_BUS_REPLY_IS_ERROR_MESSAGE;
    }
    return ER_OK;
}

void MethodCall::ReplyHandler(ajn::Message &reply, void *context)
{
    (void) context;
    m_reply = reply;
    m_called = true;
}
