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
#include "ocpayload.h"
#include "octypes.h"
#include <list>
#include <mutex>
#include <vector>
#include <set>

class AllJoynSecurity;
class OCSecurity;
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
        Bridge(const char *name, Protocol protocols);
        Bridge(const char *name, const char *sender);
        ~Bridge();

        typedef void (*ExecCB)(const char *piid, const char *sender, bool isVirtual);
        typedef void (*KillCB)(const char *piid);
        typedef enum { NOT_SEEN = 0, SEEN_NATIVE, SEEN_VIRTUAL } SeenState;
        typedef SeenState (*GetSeenStateCB)(const char *piid);
        void SetProcessCB(ExecCB execCb, KillCB killCb, GetSeenStateCB seenStateCb)
                { m_execCb = execCb; m_killCb = killCb; m_seenStateCb = seenStateCb; }
        typedef void (*SessionLostCB)();
        void SetSessionLostCB(SessionLostCB cb) { m_sessionLostCb = cb; }

        bool Start();
        bool Stop();
        bool Process();

        /* Used internally */
        void RDPublish();

    private:
        struct DiscoverContext;
        struct Task
        {
            time_t m_tick;
            Task(time_t tick) : m_tick(tick) { }
            virtual ~Task() { }
            virtual void Run(Bridge *thiz) = 0;
        };
        struct AnnouncedTask : public Task {
            std::string m_name;
            std::string m_piid;
            bool m_isVirtual;
            AnnouncedTask(time_t tick, const char *name, const char *piid, bool isVirtual)
                : Task(tick), m_name(name), m_piid(piid), m_isVirtual(isVirtual) { }
            virtual ~AnnouncedTask() { }
            virtual void Run(Bridge *thiz);
        };
        struct DiscoverTask : public Task {
            std::string m_piid;
            OCRepPayload *m_payload;
            OCDevAddr m_devAddr;
            DiscoverContext *m_context;
            DiscoverTask(time_t tick, const char *piid, OCRepPayload *payload,
                    OCDevAddr *devAddr, DiscoverContext *context) : Task(tick), m_piid(piid),
                    m_payload(OCRepPayloadClone(payload)), m_devAddr(*devAddr),
                    m_context(context) { }
            virtual ~DiscoverTask() { OCRepPayloadDestroy(m_payload); }
            virtual void Run(Bridge *thiz);
        };
        struct RDPublishTask : public Task {
            RDPublishTask(time_t tick) : Task(tick) { }
            virtual ~RDPublishTask() { }
            virtual void Run(Bridge *thiz);
        };

        static const time_t DISCOVER_PERIOD_SECS = 5;

        ExecCB m_execCb;
        GetSeenStateCB m_seenStateCb;
        KillCB m_killCb;
        SessionLostCB m_sessionLostCb;

        std::mutex m_mutex;
        Protocol m_protocols;
        enum { CREATED, STARTED, CONNECTED, CLAIMABLE, RUNNING } m_ajState;
        const char *m_sender;
        ajn::BusAttachment *m_bus;
        AllJoynSecurity *m_ajSecurity;
        OCSecurity *m_ocSecurity;
        OCDoHandle m_discoverHandle;
        time_t m_discoverNextTick;
        std::vector<Presence *> m_presence;
        std::vector<VirtualDevice *> m_virtualDevices;
        std::vector<VirtualResource *> m_virtualResources;
        std::vector<VirtualBusAttachment *> m_virtualBusAttachments;
        std::map<OCDoHandle, DiscoverContext *> m_discovered;
        bool m_secureMode;
        std::list<Task*> m_tasks;
        RDPublishTask *m_rdPublishTask;

        void WhoImplements();
        void Destroy(const char *id);
        virtual void BusDisconnected();
        virtual void Announced(const char *name, uint16_t version, ajn::SessionPort port,
                               const ajn::MsgArg &objectDescriptionArg, const ajn::MsgArg &aboutDataArg);
        virtual void JoinSessionCB(QStatus status, ajn::SessionId sessionId, const ajn::SessionOpts &opts,
                                   void *context);
        void GetAboutDataCB(ajn::Message &msg, void *ctx);
        virtual void SessionLost(ajn::SessionId sessionId, ajn::SessionListener::SessionLostReason reason);
        VirtualResource *CreateVirtualResource(ajn::BusAttachment *bus,
                                               const char *name, ajn::SessionId sessionId, const char *path,
                                               const char *ajSoftwareVersion);

        OCRepPayload *GetSecureMode(OCEntityHandlerRequest *request);
        bool PostSecureMode(OCEntityHandlerRequest *request, bool &hasChanged);
        static OCEntityHandlerResult EntityHandlerCB(OCEntityHandlerFlag flag,
                OCEntityHandlerRequest *request,
                void *context);
        static OCStackApplicationResult DiscoverCB(void *context, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult GetPlatformCB(void *context, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult GetDeviceCB(void *context, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult GetCB(void *ctx, OCDoHandle handle,
                                              OCClientResponse *response);
        OCStackResult CreateInterface(DiscoverContext *context, OCClientResponse *response);
        OCStackApplicationResult Get(void *ctx, OCDoHandle handle, OCClientResponse *response);

        SeenState GetSeenState(const char *piid);
        void DestroyPiid(const char *piid);
};

#endif
