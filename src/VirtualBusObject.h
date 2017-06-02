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

#include "Resource.h"
#include "octypes.h"
#include <inttypes.h>
#include <alljoyn/BusObject.h>
#include <condition_variable>
#include <mutex>
#include <set>
#include <vector>

class VirtualBusAttachment;

class VirtualBusObject : public ajn::BusObject
{
    public:
        VirtualBusObject(VirtualBusAttachment *bus, Resource &resource);
        VirtualBusObject(VirtualBusAttachment *bus, const char *path, Resource &resource);
        virtual ~VirtualBusObject();
        void AddResource(Resource &resource);
        QStatus AddInterface(const char *ifaceName, bool createEmptyInterface = false);
        virtual void Observe();
        virtual void CancelObserve();
        virtual void Stop();

    protected:
        typedef void (VirtualBusObject::*DoResourceHandler)(ajn::Message &msg,
                OCRepPayload *payload, void *context);

        std::mutex m_mutex;
        VirtualBusAttachment *m_bus;
        std::vector<Resource> m_resources;

        void Observe(Resource &resource);
        virtual void GetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        virtual void SetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg);
        virtual void GetAllProps(const ajn::InterfaceDescription::Member *member,
                ajn::Message &msg);
        void DoResource(OCMethod method, std::string uri, std::vector<OCDevAddr> addrs,
                OCRepPayload *payload, ajn::Message &msg, DoResourceHandler cb,
                void *context = NULL);

    private:
        class DoResourceContext;
        class ObserveContext;

        std::condition_variable m_cond;
        std::set<ObserveContext *> m_observes;
        size_t m_pending;

        void GetPropCB(ajn::Message &msg, OCRepPayload *payload, void *context);
        void SetPropCB(ajn::Message &msg, OCRepPayload *payload, void *context);
        void GetAllPropsCB(ajn::Message &msg, OCRepPayload *payload, void *context);
        static OCStackApplicationResult DoResourceCB(void *ctx, OCDoHandle handle,
                OCClientResponse *response);
        static OCStackApplicationResult ObserveCB(void *ctx, OCDoHandle handle,
                OCClientResponse *response);
};

#endif
