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

#include "SecureModeResource.h"
#include "ocpayload.h"
#include "ocstack.h"

/*
 * 1.1 Secure Mode
 */

class SecureMode : public AJOCSetUp
{
public:
    virtual ~SecureMode() { }
    virtual void SetUp()
    {
        AJOCSetUp::SetUp();
        m_secureMode = new SecureModeResource(m_mutex, true);
        EXPECT_EQ(OC_STACK_OK, m_secureMode->Create());
    }
    virtual void TearDown()
    {
        delete m_secureMode;
        AJOCSetUp::TearDown();
    }
    DiscoverContext *DiscoverResource()
    {
        DiscoverContext *context = new DiscoverContext(OC_RSRVD_SECURE_MODE_URI);
        Callback discoverCB(Discover, context);
        EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "/oic/res", NULL, 0,
                CT_DEFAULT, OC_HIGH_QOS, discoverCB, NULL, 0));
        EXPECT_EQ(OC_STACK_OK, discoverCB.Wait(1000));
        return context;
    }
private:
    std::mutex m_mutex;
    SecureModeResource *m_secureMode;
};

TEST_F(SecureMode, WhenTrueAnyBridgedServerThatCannotBeCommunicatedWithSecurelyShallNotHaveACorrespondingVirtualOCFServer)
{
    FAIL();
}

TEST_F(SecureMode, WhenTrueAnyBridgedClientThatCannotBeCommunicatedWithSecurelyShallNotHaveACorrespondingVirtualOCFClient)
{
    FAIL();
}

TEST_F(SecureMode, WhenFalseAnyBridgedServerCanHaveACorrespondingVirtualOCFServer)
{
    FAIL();
}

TEST_F(SecureMode, WhenFalseAnyBridgedClientCanHaveACorrespondingVirtualOCFClient)
{
    FAIL();
}

static void VerifyBaselinePayload(OCRepPayload *payload, bool secureMode)
{
    EXPECT_STREQ("oic.r.securemode", payload->types->value);
    EXPECT_TRUE(payload->types->next == NULL);
    EXPECT_STREQ("oic.if.baseline", payload->interfaces->value);
    EXPECT_STREQ("oic.if.rw", payload->interfaces->next->value);
    EXPECT_TRUE(payload->interfaces->next->next == NULL);
    bool mode;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "secureMode", &mode));
    EXPECT_EQ(secureMode, mode);
}

static void VerifyPayload(OCRepPayload *payload, bool secureMode)
{
    EXPECT_TRUE(payload->types == NULL);
    EXPECT_TRUE(payload->interfaces == NULL);
    bool mode;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "secureMode", &mode));
    EXPECT_EQ(secureMode, mode);
}

TEST_F(SecureMode, GetBaseline)
{
    DiscoverContext *context = DiscoverResource();

    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "/securemode?if=oic.if.baseline",
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    VerifyBaselinePayload(payload, true);

    delete context;
}

TEST_F(SecureMode, GetRW)
{
    DiscoverContext *context = DiscoverResource();

    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "/securemode?if=oic.if.rw",
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    VerifyPayload(payload, true);

    delete context;
}

TEST_F(SecureMode, Get)
{
    DiscoverContext *context = DiscoverResource();

    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, OC_RSRVD_SECURE_MODE_URI,
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    VerifyPayload(payload, true);

    delete context;
}

static void Post(const char *uri, const OCDevAddr *addr, ResourceCallback *cb)
{
    OCRepPayload *request = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropBool(request, "secureMode", false));
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri, addr, (OCPayload *) request,
            CT_DEFAULT, OC_HIGH_QOS, *cb, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, cb->Wait(1000));
}

TEST_F(SecureMode, PostBaseline)
{
    DiscoverContext *context = DiscoverResource();

    ResourceCallback postCB;
    Post("/securemode?if=oic.if.baseline", &context->m_resource->m_addrs[0], &postCB);

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);
    EXPECT_TRUE(postCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, postCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) postCB.m_response->payload;
    VerifyBaselinePayload(payload, false);

    delete context;
}

TEST_F(SecureMode, PostRW)
{
    DiscoverContext *context = DiscoverResource();

    ResourceCallback postCB;
    Post("/securemode?if=oic.if.rw", &context->m_resource->m_addrs[0], &postCB);

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);
    EXPECT_TRUE(postCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, postCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) postCB.m_response->payload;
    VerifyPayload(payload, false);

    delete context;
}

TEST_F(SecureMode, Post)
{
    DiscoverContext *context = DiscoverResource();

    ResourceCallback postCB;
    Post(OC_RSRVD_SECURE_MODE_URI, &context->m_resource->m_addrs[0], &postCB);

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);
    EXPECT_TRUE(postCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, postCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) postCB.m_response->payload;
    VerifyPayload(payload, false);

    delete context;
}

TEST_F(SecureMode, Observe)
{
    DiscoverContext *context = DiscoverResource();

    ObserveCallback observeCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_OBSERVE, OC_RSRVD_SECURE_MODE_URI,
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, observeCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    observeCB.Reset();
    ResourceCallback postCB;
    Post(OC_RSRVD_SECURE_MODE_URI, &context->m_resource->m_addrs[0], &postCB);
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) observeCB.m_response->payload;
    VerifyPayload(payload, false);

    delete context;
}
