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

class AJOCSetUp : public testing::Test
{
protected:
    virtual void SetUp();
    virtual void TearDown();
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

struct DiscoverContext
{
    const char *m_uri;
    Resource *m_resource;
    DiscoverContext(const char *uri) : m_uri(uri), m_resource(NULL) { }
    ~DiscoverContext() { delete m_resource; }
};

OCStackApplicationResult Discover(void *ctx, OCDoHandle handle, OCClientResponse *response);

struct LocalizedString
{
    const char *language;
    const char *value;
};

struct ExpectedProperties
{
    /* oic.wk.d */
    const char *n;
    const char *piid;
    const char *dmv;
    LocalizedString *ld;
    size_t nld;
    const char *sv;
    LocalizedString *dmn;
    size_t ndmn;
    const char *dmno;
    /* oic.wk.con */
    double loc[2];
    const char *locn;
    const char *c;
    const char *r;
    LocalizedString *ln;
    size_t nln;
    const char *dl;
    /* oic.wk.p */
    const char *pi;
    const char *mnmn;
    const char *mnml;
    const char *mnmo;
    const char *mndt;
    const char *mnpv;
    const char *mnos;
    const char *mnhw;
    const char *mnfv;
    const char *mnsl;
    const char *st;
    const char *vid;
    /* oic.wk.con.p */
    LocalizedString *mnpn;
    size_t nmnpn;
    /* Vendor-specific */
    const char *vendorProperty;
    const char *vendorValue;
};

#endif /* _UNITTEST_H */
