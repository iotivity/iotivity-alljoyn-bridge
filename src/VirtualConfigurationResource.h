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

#ifndef _VIRTUALCONFIGURATIONRESOURCE_H
#define _VIRTUALCONFIGURATIONRESOURCE_H

#include "VirtualResource.h"

class VirtualConfigurationResource : public VirtualResource
{
    public:
        static VirtualConfigurationResource *Create(ajn::BusAttachment *bus, const char *name,
                ajn::SessionId sessionId, const char *ajSoftwareVersion,
                CreateCB createCb, void *createContext);
        virtual ~VirtualConfigurationResource();
        void SetAboutData(ajn::AboutData *aboutData);

    private:
        OCResourceHandle m_deviceConfigurationHandle;
        OCResourceHandle m_platformConfigurationHandle;
        OCResourceHandle m_maintenanceHandle;
        ajn::AboutData m_aboutData;
        bool m_fr;
        bool m_rb;

        VirtualConfigurationResource(ajn::BusAttachment *bus, const char *name,
                ajn::SessionId sessionId, const char *ajSoftwareVersion,
                CreateCB createCb, void *createContext);

        OCStackResult Create();
        void IntrospectCB(QStatus status, ProxyBusObject *obj, void *context);
        void GetConfigurationsCB(ajn::Message &msg, void *ctx);
        struct MethodCallContext;
        void UpdateConfigurationsCB(ajn::Message &msg, void *ctx);
        static OCEntityHandlerResult ConfigurationHandlerCB(OCEntityHandlerFlag flag,
                OCEntityHandlerRequest *request,
                void *context);
        static OCEntityHandlerResult MaintenanceHandlerCB(OCEntityHandlerFlag flag,
                OCEntityHandlerRequest *request,
                void *context);
};

#endif
