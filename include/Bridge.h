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

#ifndef _BRIDGE_H
#define _BRIDGE_H

#include <inttypes.h>
#include <alljoyn/AboutListener.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/SessionListener.h>
#include "octypes.h"
#include <mutex>
#include <vector>
#include <set>

class Presence;
class VirtualBusAttachment;
class VirtualBusObject;
class VirtualDevice;
class VirtualResource;

class Bridge : private ajn::AboutListener
    , private ajn::BusListener
    , private ajn::BusAttachment::JoinSessionAsyncCB
    , private ajn::MessageReceiver
    , private ajn::SessionListener
{
    public:
        enum Protocol
        {
            AJ = (1 << 0),
            OC = (1 << 1),
        };
        Bridge(Protocol protocols);
        ~Bridge();

        bool Start();
        bool Stop();
        bool Process();

    private:
        static const time_t DISCOVER_PERIOD_SECS = 5;

        std::mutex m_mutex;
        Protocol m_protocols;
        ajn::BusAttachment *m_bus;
        OCDoHandle m_discoverHandle;
        time_t m_discoverNextTick;
        std::vector<Presence *> m_presence;
        std::vector<VirtualDevice *> m_virtualDevices;
        std::vector<VirtualResource *> m_virtualResources;
        std::vector<VirtualBusAttachment *> m_virtualBusAttachments;
        struct DiscoverContext;
        std::set<DiscoverContext *> m_discovered;

        void Destroy(const char *id);
        virtual void BusDisconnected();
        virtual void Announced(const char *name, uint16_t version, ajn::SessionPort port,
                               const ajn::MsgArg &objectDescriptionArg, const ajn::MsgArg &aboutDataArg);
        virtual void JoinSessionCB(QStatus status, ajn::SessionId sessionId, const ajn::SessionOpts &opts,
                                   void *context);
        void GetAboutDataCB(ajn::Message &msg, void *ctx);
        virtual void SessionLost(ajn::SessionId sessionId, ajn::SessionListener::SessionLostReason reason);

        static OCStackApplicationResult DiscoverCB(void *context, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult GetPlatformCB(void *context, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult GetDeviceCB(void *context, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult GetCB(void *ctx, OCDoHandle handle,
                OCClientResponse *response);
        OCStackResult CreateInterface(DiscoverContext *context, OCClientResponse *response);
        OCStackApplicationResult GetNext(DiscoverContext *context, OCClientResponse *response);
};

#endif
