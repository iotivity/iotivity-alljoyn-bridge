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

#ifndef _VIRTUALCONFIGBUSOBJECT_H
#define _VIRTUALCONFIGBUSOBJECT_H

#include "VirtualBusObject.h"

class VirtualConfigBusObject : public VirtualBusObject
{
    public:
        VirtualConfigBusObject(ajn::BusAttachment *bus, Resource &resource);
        virtual ~VirtualConfigBusObject();

    private:
        const ajn::InterfaceDescription *m_iface;

        virtual void GetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        virtual void SetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        virtual void GetAllProps(const ajn::InterfaceDescription::Member *member,
                ajn::Message &msg);
        void FactoryReset(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        void GetConfigurations(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        void ResetConfigurations(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        void Restart(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        void SetPasscode(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        void UpdateConfigurations(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);

        void GetConfigurationsCB(ajn::Message &msg, OCRepPayload *payload, void *context);
        void UpdateConfigurationsCB(ajn::Message &msg, OCRepPayload *payload, void *context);
        void FactoryResetCB(ajn::Message &msg, OCRepPayload *payload, void *context);
        void RestartCB(ajn::Message &msg, OCRepPayload *payload, void *context);
};

#endif
