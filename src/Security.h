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

#ifndef _SECURITY_H
#define _SECURITY_H

#include <alljoyn/AboutObj.h>
#include <alljoyn/AuthListener.h>
#include <alljoyn/PermissionConfigurationListener.h>
#include <inttypes.h>
#include <stddef.h>

class AllJoynSecurity : public ajn::DefaultECDHEAuthListener,
    public ajn::PermissionConfigurationListener
{
    public:
        typedef enum
        {
            CONSUMER,
            PRODUCER
        } Role;
        AllJoynSecurity(ajn::BusAttachment *bus, Role role);
        virtual ~AllJoynSecurity() { }

        bool IsClaimed();
        QStatus SetClaimable();

    private:
        ajn::BusAttachment *m_bus;
        Role m_role;

        virtual void SecurityViolation(QStatus status, const ajn::Message &msg);
        virtual void AuthenticationComplete(const char *authMechanism, const char *peerName,
                bool success);
        virtual QStatus FactoryReset();
        virtual void PolicyChanged();
        virtual void StartManagement();
        virtual void EndManagement();
};

class OCSecurity
{
    public:
        bool Init();

    private:
        static void DisplayPinCB(char *pin, size_t pinSize, void *context);
        static void ClosePinDisplayCB();
};

#endif
