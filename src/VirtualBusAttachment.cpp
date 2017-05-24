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

#include "VirtualBusAttachment.h"

#include "Bridge.h"
#include "Log.h"
#include "Payload.h"
#include "Plugin.h"
#include "Security.h"
#include "Signature.h"
#include "VirtualBusObject.h"
#include "ocstack.h"
#include "ocpayload.h"
#include "oic_malloc.h"
#include <assert.h>

VirtualBusAttachment *VirtualBusAttachment::Create(const char *di, const char *piid, bool isVirtual)
{
    QStatus status;
    ajn::SessionOpts opts;
    VirtualBusAttachment *busAttachment = new VirtualBusAttachment(di, piid, isVirtual);
    {
        std::lock_guard<std::mutex> lock(busAttachment->m_mutex);
        status = busAttachment->Start();
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "Start - %s", QCC_StatusText(status));
            goto exit;
        }
        status = busAttachment->Connect();
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "Connect - %s", QCC_StatusText(status));
            goto exit;
        }
        status = busAttachment->m_ajSecurity->SetClaimable();
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "SetClaimable failed");
            goto exit;
        }
        status = busAttachment->BindSessionPort(busAttachment->m_port, opts, *busAttachment);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "BindSessionPort - %s", QCC_StatusText(status));
            goto exit;
        }
    }
exit:
    if (status != ER_OK)
    {
        delete busAttachment;
        busAttachment = NULL;
    }
    return busAttachment;
}

VirtualBusAttachment::VirtualBusAttachment(const char *di, const char *piid, bool isVirtual)
    : ajn::BusAttachment(di), m_di(di), m_isVirtual(isVirtual), m_aboutData(NULL),
    m_port(ajn::SESSION_PORT_ANY), m_numSessions(0), m_aboutObj(NULL)
{
    LOG(LOG_INFO, "[%p] di=%s,piid=%s,isVirtual=%d",
            this, di, piid, isVirtual);

    if (piid)
    {
        m_piid = piid;
    }

    m_ajSecurity = new AllJoynSecurity(this, AllJoynSecurity::PRODUCER);
}

VirtualBusAttachment::~VirtualBusAttachment()
{
    LOG(LOG_INFO, "[%p]",
        this);

    std::lock_guard<std::mutex> lock(m_mutex);
    delete m_aboutObj;
    for (VirtualBusObject *busObject : m_virtualBusObjects)
    {
        delete busObject;
    }

    delete m_ajSecurity;
}

void VirtualBusAttachment::Stop()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    for (VirtualBusObject *busObject : m_virtualBusObjects)
    {
        busObject->Stop();
    }
}

void VirtualBusAttachment::SetAboutData(OCRepPayload *payload)
{
    LOG(LOG_INFO, "[%p]", this);

    if (!payload)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(m_mutex);
    m_aboutData.Set(payload);
}

ajn::InterfaceDescription *VirtualBusAttachment::CreateInterface(const char* ifaceName)
{
    LOG(LOG_INFO, "[%p] ifaceName=%s", this, ifaceName);

    ajn::InterfaceDescription *iface = NULL;
    ajn::BusAttachment::CreateInterface(ifaceName, iface, ajn::AJ_IFC_SECURITY_INHERIT);
    return iface;
}

QStatus VirtualBusAttachment::RegisterBusObject(VirtualBusObject *busObject)
{
    LOG(LOG_INFO, "[%p] busObject=%p",
        this, busObject);

    std::lock_guard<std::mutex> lock(m_mutex);
    QStatus status = ajn::BusAttachment::RegisterBusObject(*busObject);
    if (status == ER_OK)
    {
        m_virtualBusObjects.push_back(busObject);
    }
    else
    {
        LOG(LOG_ERR, "RegisterBusObject - %s", QCC_StatusText(status));
    }
    return status;
}

VirtualBusObject *VirtualBusAttachment::GetBusObject(const char *path)
{
    for (VirtualBusObject *busObject : m_virtualBusObjects)
    {
        if (!strcmp(busObject->GetPath(), path))
        {
            return busObject;
        }
    }
    return NULL;
}

QStatus VirtualBusAttachment::Announce()
{
    LOG(LOG_INFO, "[%p]",
        this);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!m_aboutData.IsValid())
    {
        return ER_FAIL;
    }
    m_aboutObj = new ajn::AboutObj(*this, ajn::BusObject::ANNOUNCED);
    return m_aboutObj->Announce(m_port, m_aboutData);
}

bool VirtualBusAttachment::AcceptSessionJoiner(ajn::SessionPort port, const char *name,
        const ajn::SessionOpts &opts)
{
    (void) opts;
    LOG(LOG_INFO, "[%p] port=%d,name=%s",
        this, port, name);

    std::lock_guard<std::mutex> lock(m_mutex);
    return true;
}

void VirtualBusAttachment::SessionJoined(ajn::SessionPort port, ajn::SessionId id, const char *name)
{
    LOG(LOG_INFO, "[%p] port=%d,id=%d,name=%s",
        this, port, id, name);

    std::lock_guard<std::mutex> lock(m_mutex);
    QStatus status = SetSessionListener(id, this);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "SetSesssionListener - %s", QCC_StatusText(status));
    }
    if (m_numSessions++ == 0)
    {
        for (VirtualBusObject *busObj : m_virtualBusObjects)
        {
            busObj->Observe();
        }
    }
}

void VirtualBusAttachment::SessionLost(ajn::SessionId id,
                                       ajn::SessionListener::SessionLostReason reason)
{
    LOG(LOG_INFO, "[%p] id=%d,reason=%d",
        this, id, reason);

    std::lock_guard<std::mutex> lock(m_mutex);
    assert(m_numSessions > 0);
    if (--m_numSessions == 0)
    {
        for (VirtualBusObject *busObj : m_virtualBusObjects)
        {
            busObj->CancelObserve();
        }
    }
}

