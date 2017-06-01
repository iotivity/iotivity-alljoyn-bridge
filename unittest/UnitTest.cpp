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
    /*
     * TODO AllJoyn has a bug where AllJoynShutdown() does not clear all the state needed to
     * call AllJoynInit() again (an assert fires: alljoyn_core/src/XmlRulesValidator.cc:98:
     * static void ajn::XmlRulesValidator::MethodsValidator::Init(): Assertion `nullptr ==
     * s_actionsMap' failed.)
     */
    //EXPECT_EQ(ER_OK, AllJoynShutdown());
    EXPECT_EQ(OC_STACK_OK, OCStop());
}

OCStackApplicationResult Discover(void *ctx, OCDoHandle handle, OCClientResponse *response)
{
    DiscoverContext *context = (DiscoverContext *) ctx;
    (void) handle;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_DISCOVERY, response->payload->type);
    for (OCDiscoveryPayload *payload = (OCDiscoveryPayload *) response->payload; payload;
         payload = payload->next)
    {
        for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
             resource = resource->next)
        {
            if (!strcmp(resource->uri, context->m_uri))
            {
                context->m_resource = new Resource(response->devAddr, payload->sid, resource);
            }
        }
    }
    return OC_STACK_DELETE_TRANSACTION;
}
