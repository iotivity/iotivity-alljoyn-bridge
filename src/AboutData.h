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

#ifndef _ABOUTDATA_H
#define _ABOUTDATA_H

#include <alljoyn/AboutData.h>
#include "ocpayload.h"

class AboutData : public ajn::AboutData
{
public:
    AboutData(const char *defaultLanguage = NULL);
    QStatus Set(const char *rt, OCRepPayload *payload);
    bool IsValid();
private:
    char *m_n;
    void SetVendorFields(OCRepPayload *payload);
};

#endif
