//******************************************************************
//
// Copyright 2016 Intel Corporation All Rights Reserved.
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

#ifndef _VIRTUALBUSATTACHMENT_H
#define _VIRTUALBUSATTACHMENT_H

#include <inttypes.h>
#include <alljoyn/AboutData.h>
#include <alljoyn/AboutObj.h>
#include <alljoyn/BusAttachment.h>
#include "octypes.h"
#include <mutex>
#include <vector>

class VirtualBusObject;

class VirtualBusAttachment : public ajn::BusAttachment
    , private ajn::SessionPortListener
    , private ajn::SessionListener
{
    public:
        static VirtualBusAttachment *Create(const char *di);
        virtual ~VirtualBusAttachment();
        std::string GetDi() { return m_di; }
        void SetAboutData(OCRepPayload *payload);
        const ajn::InterfaceDescription *CreateInterface(const char *ifaceName, OCPayload *payload);
        QStatus RegisterBusObject(VirtualBusObject *busObject);
        QStatus Announce();
        void Stop();

    private:
        std::string m_di;
        std::mutex m_mutex;
        ajn::AboutData m_aboutData;
        ajn::SessionPort m_port;
        uint32_t m_numSessions;
        std::vector<VirtualBusObject *> m_virtualBusObjects;
        ajn::AboutObj *m_aboutObj;

        VirtualBusAttachment(const char *di);
        virtual bool AcceptSessionJoiner(ajn::SessionPort port, const char *name,
                                         const ajn::SessionOpts &opts);
        virtual void SessionJoined(ajn::SessionPort port, ajn::SessionId id, const char *name);
        virtual void SessionLost(ajn::SessionId id, ajn::SessionListener::SessionLostReason reason);
};

#endif
