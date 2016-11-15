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

#ifndef _VIRTUALRESOURCE_H
#define _VIRTUALRESOURCE_h

#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include "octypes.h"
#include <mutex>
#include <vector>

class VirtualResource : public ajn::ProxyBusObject
    , private ajn::ProxyBusObject::Listener
    , private ajn::BusAttachment::AddMatchAsyncCB
    , private ajn::BusAttachment::RemoveMatchAsyncCB
{
    public:
        static VirtualResource *Create(ajn::BusAttachment *bus,
                                       const char *name, ajn::SessionId sessionId, const char *path,
                                       const char *ajSoftwareVersion);
        virtual ~VirtualResource();

    private:
        std::mutex m_mutex;
        ajn::BusAttachment *m_bus;
        std::string m_ajSoftwareVersion;
        std::map<std::string, uint8_t> m_rts;
        std::map<std::string, std::vector<OCObservationId>> m_observers;
        std::map<OCObservationId, std::string> m_matchRules;

        VirtualResource(ajn::BusAttachment *bus,
                        const char *name, ajn::SessionId sessionId, const char *path,
                        const char *ajSoftwareVersion);

        OCStackResult Create();
        void IntrospectCB(ajn::Message &msg, void *ctx);
        void SignalCB(const ajn::InterfaceDescription::Member *member, const char *path, ajn::Message &msg);
        void MethodReturnCB(ajn::Message &msg, void *context);
        struct SetContext;
        QStatus Set(SetContext *context);
        void SetCB(ajn::Message &msg, void *context);
        struct GetAllInvalidatedContext;
        void GetAllInvalidatedCB(ajn::Message &msg, void *ctx);
        virtual void AddMatchCB(QStatus status, void *ctx);
        virtual void RemoveMatchCB(QStatus status, void *ctx);
        OCRepPayload *CreatePayload();
        static OCEntityHandlerResult EntityHandlerCB(OCEntityHandlerFlag flag,
                OCEntityHandlerRequest *request,
                void *context);
};

#endif
