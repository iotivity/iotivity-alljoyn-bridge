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

#ifndef _UNITTEST_H
#define _UNITTEST_H

#include "octypes.h"
#include "Resource.h"
#include "VirtualResource.h"
#include <gtest/gtest.h>

struct LocalizedString
{
    const char *language;
    const char *value;
};

struct DiscoverContext
{
    const char *m_uri;
    Device *m_device;
    Resource *m_resource;
    DiscoverContext(const char *uri = NULL) : m_uri(uri), m_device(NULL), m_resource(NULL) { }
    ~DiscoverContext() { delete m_device; }
};

class AJOCSetUp : public testing::Test
{
protected:
    virtual ~AJOCSetUp() { }
    virtual void SetUp();
    virtual void TearDown();
    void Wait(long waitMs);
};

/* TODO fold addition of context to Callback and change in Wait units back into gtest_helper.h */
class Callback
{
public:
    Callback(OCClientResponseHandler cb, void *context = NULL);
    OCStackResult Wait(long waitMs);
    operator OCCallbackData *() { return &m_cbData; }
private:
    OCClientResponseHandler m_cb;
    void *m_context;
    bool m_called;
    OCCallbackData m_cbData;
    static OCStackApplicationResult handler(void *ctx, OCDoHandle handle, OCClientResponse *clientResponse);
};

class ResourceCallback
{
public:
    OCClientResponse *m_response;
    ResourceCallback();
    virtual ~ResourceCallback();
    OCStackResult Wait(long waitMs);
    operator OCCallbackData *() { return &m_cbData; }
protected:
    bool m_called;
    virtual OCStackApplicationResult Handler(OCDoHandle handle, OCClientResponse *response);
private:
    OCCallbackData m_cbData;
    static OCStackApplicationResult handler(void *ctx, OCDoHandle handle,
            OCClientResponse *response);
};

class ObserveCallback : public ResourceCallback
{
public:
    virtual ~ObserveCallback() { }
    void Reset();
protected:
    virtual OCStackApplicationResult Handler(OCDoHandle handle, OCClientResponse *response);
};

class CreateCallback
{
public:
    CreateCallback();
    OCStackResult Wait(long waitMs);
    operator VirtualResource::CreateCB () { return cb; }
private:
    bool m_called;
    static void cb(void *ctx);
};

class MethodCall : public ajn::MessageReceiver
{
public:
    MethodCall(ajn::BusAttachment *bus, ajn::ProxyBusObject *proxyObj);
    QStatus Call(const char *iface, const char *method, const ajn::MsgArg *args, size_t numArgs);
    QStatus Wait(long waitMs);
    ajn::Message &Reply() { return m_reply; }
private:
    ajn::ProxyBusObject *m_proxyObj;
    ajn::Message m_reply;
    bool m_called;
    void ReplyHandler(ajn::Message &reply, void *context);
};

OCStackApplicationResult Discover(void *ctx, OCDoHandle handle, OCClientResponse *response);

OCStackResult ParseJsonPayload(OCRepPayload** outPayload, const char* payload);

#define A_SIZEOF(a) ((size_t) (sizeof(a) / sizeof(a[0])))

class Row
{
public:
    Row(ajn::MsgArg arg, int64_t i);
    Row(ajn::MsgArg arg, double d);
    Row(ajn::MsgArg arg, bool b);
    Row(ajn::MsgArg arg, const char *str);
    Row(ajn::MsgArg arg, size_t len, const uint8_t *bytes);
    Row(ajn::MsgArg arg, const OCRepPayload *payload);
    Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const int64_t *iArray);
    Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const double *dArray);
    Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const bool *bArray);
    Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const char **strArray);
    Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const uint8_t *bytes);
    Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const OCRepPayload *payload);
    Row(ajn::MsgArg arg, uint8_t y);
    Row(ajn::MsgArg arg, uint64_t t);
    ajn::MsgArg m_arg;
    OCRepPayloadValue m_value;
};

class FromDBus : public ::testing::TestWithParam<Row> { };
class FromOC : public ::testing::TestWithParam<Row> { };

#endif /* _UNITTEST_H */
