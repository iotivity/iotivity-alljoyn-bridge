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

#include "VirtualDevice.h"

#include "Bridge.h"
#include "DeviceResource.h"
#include "Hash.h"
#include "Log.h"
#include "PlatformResource.h"
#include "Plugin.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include "ocstack.h"

VirtualDevice::VirtualDevice(ajn::BusAttachment *bus, const char *name, ajn::SessionId sessionId)
    : m_bus(bus), m_name(name), m_sessionId(sessionId)
{
    LOG(LOG_INFO, "[%p] name=%s,sessionId=%d",
        this, name, sessionId);
}

VirtualDevice::~VirtualDevice()
{
    LOG(LOG_INFO, "[%p]",
        this);

    OCStackResult result = StopPresence();
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "StopPresence - %d", result);
    }
}

void VirtualDevice::SetProperties(ajn::AboutObjectDescription &objectDescription,
        ajn::AboutData &aboutData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aboutData = aboutData;
    qcc::String peerGuid;
    m_bus->GetPeerGUID(m_name.c_str(), peerGuid);
    SetDeviceProperties(m_bus, &objectDescription, &aboutData,
            peerGuid.empty() ? NULL : peerGuid.c_str());
    SetPlatformProperties(&aboutData);
}

void VirtualDevice::StartPresence()
{
    OCStackResult result = ::StartPresence();
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "StartPresence - %d", result);
    }
}
