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

#ifndef _VIRTUALDEVICE_H
#define _VIRTUALDEVICE_H

#include <inttypes.h>
#include <alljoyn/AboutData.h>
#include <alljoyn/AboutObjectDescription.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/Session.h>
#include <mutex>
#include <string>

class VirtualDevice
{
    public:
        VirtualDevice(ajn::BusAttachment *bus, const char *name, ajn::SessionId sessionId);
        ~VirtualDevice();

        std::string GetName() const { return m_name; }
        ajn::SessionId GetSessionId() const { return m_sessionId; }
        void SetInfo(ajn::AboutObjectDescription &objectDescription, ajn::AboutData &aboutData);
        void StartPresence();

    private:
        std::mutex m_mutex;
        ajn::BusAttachment *m_bus;
        std::string m_name;
        ajn::SessionId m_sessionId;
        ajn::AboutData m_aboutData;

        void SetPlatformAndDeviceInfo(ajn::AboutObjectDescription &objectDescription,
                                      ajn::AboutData &aboutData);
};

#endif
