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
#define _VIRTUALRESOURCE_H

#include "cacommon.h"
#include "octypes.h"
#include <inttypes.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/ProxyBusObject.h>
#include <mutex>
#include <vector>

class Bridge;

class VirtualResource : public ajn::ProxyBusObject
    , protected ajn::ProxyBusObject::Listener
    , private ajn::BusAttachment::AddMatchAsyncCB
    , private ajn::BusAttachment::RemoveMatchAsyncCB
{
    public:
        typedef void (*CreateCB)(void *context);
        static VirtualResource *Create(ajn::BusAttachment *bus, const char *name,
                ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion,
                CreateCB createCb, void *createContext);
        virtual ~VirtualResource();

    protected:
        std::mutex m_mutex;
        ajn::BusAttachment *m_bus;
        CreateCB m_createCb;
        void *m_createContext;

        VirtualResource(ajn::BusAttachment *bus, const char *name, ajn::SessionId sessionId,
                const char *path, const char *ajSoftwareVersion, CreateCB createCb,
                void *createContext);

    private:
        std::string m_ajSoftwareVersion;
        struct ResourceType {
            uint8_t m_access;
            uint8_t m_props;
            ResourceType() : m_access(0), m_props(OC_DISCOVERABLE) { }
        };
        std::map<std::string, ResourceType> m_rts;
        struct Observation {
            OCResourceHandle m_resource;
            std::string m_query;
            Observation() : m_resource(0) { }
            Observation(OCResourceHandle resource, const char *query)
                : m_resource(resource), m_query(query) { }
            bool operator<(const Observation& rhs) const
            {
                return (m_resource != rhs.m_resource) ? (m_resource < rhs.m_resource) :
                        (m_query < rhs.m_query);
            };
        };
        std::map<Observation, std::vector<OCObservationId>> m_observers;
        std::map<OCObservationId, std::string> m_matchRules;
        OCResourceHandle m_handle;

        OCStackResult Create();
        uint8_t GetMethodCallFlags(const char *ifaceName);
        void IntrospectCB(ajn::Message &msg, void *ctx);
        OCStackResult CreateResources();
        OCStackResult CreateResource(OCResourceHandle *handle, std::string path, uint8_t props);
        void SignalCB(const ajn::InterfaceDescription::Member *member, const char *path,
                ajn::Message &msg);
        void MethodReturnCB(ajn::Message &msg, void *context);
        struct SetContext;
        QStatus Set(SetContext *context);
        void SetCB(ajn::Message &msg, void *context);
        struct GetAllInvalidatedContext;
        void GetAllInvalidatedCB(ajn::Message &msg, void *ctx);
        struct GetAllBaselineContext;
        QStatus GetAllBaseline(GetAllBaselineContext *context);
        void GetAllBaselineCB(ajn::Message &msg, void *ctx);
        virtual void AddMatchCB(QStatus status, void *ctx);
        virtual void RemoveMatchCB(QStatus status, void *ctx);
        OCDiagnosticPayload *CreatePayload(ajn::Message &msg, OCEntityHandlerResult *ehResult);
        OCRepPayload *CreatePayload(const char *uri);
        OCStackResult SetMemberPayload(OCRepPayload *payload, const char *ifaceName,
                const char *memberName);
        static OCEntityHandlerResult EntityHandlerCB(OCEntityHandlerFlag flag,
                OCEntityHandlerRequest *request, void *context);
};

#endif
