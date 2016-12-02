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
#include "Plugin.h"
#include "VirtualBusObject.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include <assert.h>

static void ToAppId(const char *di, uint8_t *appId)
{
    memset(appId, 0, 16);
    for (size_t i = 0; (i < 16) && *di; ++i)
    {
        if (*di == '-') ++di;
        for (size_t j = 0; (j < 2) && *di; ++j, ++di)
        {
            if ('0' <= *di && *di <= '9')
            {
                appId[i] = (appId[i] << 4) | (*di - '0');
            }
            else if ('a' <= *di && *di <= 'f')
            {
                appId[i] = (appId[i] << 4) | (*di - 'a' + 10);
            }
            else if ('A' <= *di && *di <= 'F')
            {
                appId[i] = (appId[i] << 4) | (*di - 'A' + 10);
            }
        }
    }
}

VirtualBusAttachment *VirtualBusAttachment::Create(const char *di)
{
    QStatus status;
    ajn::SessionOpts opts;
    VirtualBusAttachment *busAttachment = new VirtualBusAttachment(di);
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

VirtualBusAttachment::VirtualBusAttachment(const char *di)
    : ajn::BusAttachment(di), m_di(di), m_numSessions(0), m_aboutObj(NULL)
{
    LOG(LOG_INFO, "[%p] di=%s",
        this, di);

    m_aboutData.SetDefaultLanguage("");
    m_aboutData.SetDescription("");
    m_aboutData.SetSoftwareVersion("");

    /*
     * Default mandatory values necessary for Announce to succeed when
     * /oic/d or /oic/p not present.
     */
    m_aboutData.SetDeviceId("");
    m_aboutData.SetManufacturer("");
    m_aboutData.SetModelNumber("");
    m_aboutData.SetDateOfManufacture("");
    m_aboutData.SetHardwareVersion("");
    m_aboutData.SetSupportUrl("");
    uint8_t appId[16];
    ToAppId(di, appId);
    m_aboutData.SetAppId(appId, 16);
    m_aboutData.SetAppName("");
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
    LOG(LOG_INFO, "[%p]",
        this);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (!payload)
    {
        return;
    }
    void *value = NULL;
    if (OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_ID, &value) == OC_STACK_OK && value)
    {
        uint8_t appId[16];
        ToAppId((char *)value, appId);
        m_aboutData.SetAppId(appId, 16);
        OICFree(value);
        value = NULL;
    }
    if (OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, &value) == OC_STACK_OK && value)
    {
        m_aboutData.SetAppName((char *)value);
        OICFree(value);
        value = NULL;
    }
    if (OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, &value) == OC_STACK_OK && value)
    {
        m_aboutData.SetDeviceId((char *)value);
        OICFree(value);
        value = NULL;
    }
    if (OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_NAME, &value) == OC_STACK_OK && value)
    {
        m_aboutData.SetManufacturer((char *)value);
        OICFree(value);
        value = NULL;
    }
    if (OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MODEL_NUM, &value) == OC_STACK_OK && value)
    {
        m_aboutData.SetModelNumber((char *)value);
        OICFree(value);
        value = NULL;
    }
    if (OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, &value) == OC_STACK_OK && value)
    {
        m_aboutData.SetDateOfManufacture((char *)value);
        OICFree(value);
        value = NULL;
    }
    if (OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, &value) == OC_STACK_OK && value)
    {
        m_aboutData.SetHardwareVersion((char *)value);
        OICFree(value);
        value = NULL;
    }
    if (OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, &value) == OC_STACK_OK && value)
    {
        m_aboutData.SetSupportUrl((char *)value);
        OICFree(value);
        value = NULL;
    }
}

static void CreateSignature(char *sig, OCRepPayloadValue *value)
{
    switch (value->type)
    {
        case OCREP_PROP_NULL:
            assert(0); /* Explicitly not supported */
            break;
        case OCREP_PROP_INT:
            strcpy(sig, "i");
            break;
        case OCREP_PROP_DOUBLE:
            strcpy(sig, "d");
            break;
        case OCREP_PROP_BOOL:
            strcpy(sig, "b");
            break;
        case OCREP_PROP_STRING:
            strcpy(sig, "s");
            break;
        case OCREP_PROP_BYTE_STRING:
            strcpy(sig, "ay");
            break;
        case OCREP_PROP_OBJECT:
            strcpy(sig, "a{sv}");
            break;
        case OCREP_PROP_ARRAY:
            for (size_t i = 0; i < MAX_REP_ARRAY_DEPTH; ++i)
            {
                if (value->arr.dimensions[i])
                {
                    *sig++ = 'a';
                }
            }
            switch (value->arr.type)
            {
                case OCREP_PROP_NULL:
                case OCREP_PROP_INT:
                case OCREP_PROP_DOUBLE:
                case OCREP_PROP_BOOL:
                case OCREP_PROP_STRING:
                case OCREP_PROP_BYTE_STRING:
                case OCREP_PROP_OBJECT:
                    {
                        OCRepPayloadValue valueArr;
                        valueArr.type = value->arr.type;
                        CreateSignature(sig, &valueArr);
                        break;
                    }
                case OCREP_PROP_ARRAY:
                    assert(0); /* Not supported - dimensions provide for arrays of arrays */
                    break;
            }
            break;
    }
}

const ajn::InterfaceDescription *VirtualBusAttachment::CreateInterface(const char *ifaceName,
        OCPayload *payload)
{
    LOG(LOG_INFO, "[%p] ifaceName=%s,payload=%p",
        this, ifaceName, payload);

    std::lock_guard<std::mutex> lock(m_mutex);
    ajn::InterfaceDescription *iface;
    QStatus status = ajn::BusAttachment::CreateInterface(ifaceName, iface,
                     ajn::AJ_IFC_SECURITY_INHERIT);
    if (status == ER_BUS_IFACE_ALREADY_EXISTS)
    {
        return ajn::BusAttachment::GetInterface(ifaceName);
    }
    else if (status != ER_OK)
    {
        LOG(LOG_ERR, "CreateInterface - %s", QCC_StatusText(status));
        return NULL;
    }
    if (payload->type != PAYLOAD_TYPE_REPRESENTATION)
    {
        LOG(LOG_INFO, "Ignoring non-representation payload");
        return NULL;
    }
    for (OCRepPayloadValue *value = ((OCRepPayload *) payload)->values; value; value = value->next)
    {
        char sig[] = "aaaa{sv}";
        CreateSignature(sig, value);
        status = iface->AddProperty(value->name, sig, ajn::PROP_ACCESS_RW);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "AddProperty - %s", QCC_StatusText(status));
            return NULL;
        }
        status = iface->AddPropertyAnnotation(value->name,
                                              ajn::org::freedesktop::DBus::AnnotateEmitsChanged, "true");
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "AddPropertyAnnotation - %s", QCC_StatusText(status));
            return NULL;
        }
    }
    iface->Activate();
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

QStatus VirtualBusAttachment::Announce()
{
    LOG(LOG_INFO, "[%p]",
        this);

    std::lock_guard<std::mutex> lock(m_mutex);
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

