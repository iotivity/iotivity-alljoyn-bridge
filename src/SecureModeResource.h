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

#ifndef _SECUREMODERESOURCE_H
#define _SECUREMODERESOURCE_H

#include "octypes.h"
#include <mutex>

/** To represent secure mode resource type.*/
#define OC_RSRVD_RESOURCE_TYPE_SECURE_MODE "oic.r.securemode"

#define OC_RSRVD_SECURE_MODE_URI "/securemode"

class SecureModeResource
{
public:
    SecureModeResource(std::mutex &mutex, bool secureMode);
    ~SecureModeResource();
    OCStackResult Create();
    bool GetSecureMode() const { return m_secureMode; }
    void SetSecureMode(bool secureMode) { m_secureMode = secureMode; }

private:
    std::mutex &m_mutex;
    bool m_secureMode;
    OCResourceHandle m_handle;

    OCRepPayload *GetSecureMode(OCEntityHandlerRequest *request);
    bool PostSecureMode(OCEntityHandlerRequest *request, bool &hasChanged);
    static OCEntityHandlerResult EntityHandlerCB(OCEntityHandlerFlag flag,
            OCEntityHandlerRequest *request, void *ctx);
};

#endif
