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

#ifndef _BRIDGESECURITY_H
#define _BRIDGESECURITY_H

#include <alljoyn/AuthListener.h>

class BusAuthListener : public ajn::AuthListener
{
public:
    virtual ~BusAuthListener() { }
    virtual QStatus RequestCredentialsAsync(const char* authMechanism, const char* peerName, uint16_t authCount,
                                            const char* userName, uint16_t credMask, void* authContext);
    virtual QStatus VerifyCredentialsAsync(const char* authMechanism, const char* peerName, const Credentials& credentials,
                                           void* authContext);
    virtual void SecurityViolation(QStatus status, const ajn::Message& msg);
    virtual void AuthenticationComplete(const char* authMechanism, const char* peerName, bool success);
};

#endif
