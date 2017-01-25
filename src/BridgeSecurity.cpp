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

#include "BridgeSecurity.h"

#include "Plugin.h"

QStatus BusAuthListener::RequestCredentialsAsync(const char* authMechanism, const char* peerName, uint16_t authCount,
                                                 const char* userName, uint16_t credMask, void* authContext)
{
    LOG(LOG_INFO, "[%p] authMechanism=%s,peerName=%s,authCount=%u,userName=%s,credMask=0x%x,authContext=%p",
        this, authMechanism, peerName, authCount, userName, credMask, authContext);

    bool accept = true;
    Credentials credentials;
    if (credMask & CRED_PASSWORD)
    {
        credentials.SetPassword("000000"); // TODO
    }
    // TODO other credMask bits
    QStatus status = RequestCredentialsResponse(authContext, accept, credentials);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "[%p] RequestCredentialsResponse - %s", this, QCC_StatusText(status));
    }
    return ER_OK;
}

QStatus BusAuthListener::VerifyCredentialsAsync(const char* authMechanism, const char* peerName, const Credentials& credentials,
                                                void* authContext)
{
    (void) credentials;
    LOG(LOG_INFO, "[%p] authMechanism=%s,peerName=%s,credentials=,authContext=%p",
        this, authMechanism, peerName, authContext);

    bool accept = true;
    QStatus status = VerifyCredentialsResponse(authContext, accept);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "[%p] VerifyCredentialsResponse - %s", this, QCC_StatusText(status));
    }
    return ER_OK;
}

void BusAuthListener::SecurityViolation(QStatus status, const ajn::Message& msg)
{
    LOG(LOG_INFO, "[%p] status=%s,msg=%s",
        this, QCC_StatusText(status), msg->ToString().c_str());
}

void BusAuthListener::AuthenticationComplete(const char* authMechanism, const char* peerName, bool success)
{
    LOG(LOG_INFO, "[%p] authMechanism=%s,peerName=%s,success=%d",
        this, authMechanism, peerName, success);
}
