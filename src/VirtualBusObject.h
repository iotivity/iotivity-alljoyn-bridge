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

#ifndef _VIRTUALBUSOBJECT_H
#define _VIRTUALBUSOBJECT_H

#include <inttypes.h>
#include <alljoyn/BusObject.h>
#include "octypes.h"
#include <condition_variable>
#include <mutex>
#include <set>
#include <vector>

class VirtualBusObject : public ajn::BusObject
{
    public:
        VirtualBusObject(ajn::BusAttachment *bus, const char *uri, OCDevAddr *devAddr);
        virtual ~VirtualBusObject();
        QStatus AddInterface(const ajn::InterfaceDescription *iface);
        void Observe();
        void CancelObserve();
        void Stop();

    private:
        typedef void (VirtualBusObject::*DoResourceHandler)(ajn::Message &msg, OCRepPayload *payload);
        class DoResourceContext;
        class ObserveContext;

        std::mutex m_mutex;
        std::condition_variable m_cond;
        ajn::BusAttachment *m_bus;
        OCDevAddr m_devAddr;
        std::vector<const ajn::InterfaceDescription *> m_ifaces;
        std::set<ObserveContext *> m_observes;
        size_t m_pending;

        virtual void GetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        virtual void SetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        virtual void GetAllProps(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        void GetPropCB(ajn::Message &msg, OCRepPayload *payload);
        void SetPropCB(ajn::Message &msg, OCRepPayload *payload);
        void GetAllPropsCB(ajn::Message &msg, OCRepPayload *payload);
        void DoResource(OCMethod method, const char *uri, OCRepPayload *payload, ajn::Message &msg,
                        DoResourceHandler cb);
        static OCStackApplicationResult DoResourceCB(void *ctx, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult ObserveCB(void *ctx, OCDoHandle handle, OCClientResponse *response);
};

#endif
