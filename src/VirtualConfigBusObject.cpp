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

#include "VirtualConfigBusObject.h"

#include "AboutData.h"
#include "DeviceConfigurationResource.h"
#include "Interfaces.h"
#include "Log.h"
#include "Payload.h"
#include "PlatformConfigurationResource.h"
#include "Plugin.h"
#include "Signature.h"
#include "VirtualBusAttachment.h"
#include "ocpayload.h"
#include "oic_malloc.h"
#include <assert.h>

VirtualConfigBusObject::VirtualConfigBusObject(VirtualBusAttachment *bus, Resource &resource)
    : VirtualBusObject(bus, "/Config", resource)
{
    LOG(LOG_INFO, "[%p] bus=%p,uri=%s", this, bus, resource.m_uri.c_str());
    QStatus status;
    (void)(status); /* Unused in release build */
    status = m_bus->CreateInterfacesFromXml(ajn::org::alljoyn::Config::InterfaceXml);
    assert(status == ER_OK);
    m_iface = m_bus->GetInterface("org.alljoyn.Config");
    assert(m_iface);
    ajn::BusObject::AddInterface(*m_iface, ajn::BusObject::ANNOUNCED);
    const MethodEntry methodEntries[] =
    {
        { m_iface->GetMember("FactoryReset"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::FactoryReset) },
        { m_iface->GetMember("GetConfigurations"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::GetConfigurations) },
        { m_iface->GetMember("ResetConfigurations"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::NotImplemented) },
        { m_iface->GetMember("Restart"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::Restart) },
        { m_iface->GetMember("SetPasscode"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::NotImplemented) },
        { m_iface->GetMember("UpdateConfigurations"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::UpdateConfigurations) },
    };
    AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
}

VirtualConfigBusObject::~VirtualConfigBusObject()
{
    LOG(LOG_INFO, "[%p]", this);
}

void VirtualConfigBusObject::GetProp(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) member;
    if (!strcmp(msg->GetArg(0)->v_string.str, "org.alljoyn.Config"))
    {
        if (!strcmp(msg->GetArg(1)->v_string.str, "Version"))
        {
            ajn::MsgArg arg("v", new ajn::MsgArg("q", 1));
            arg.SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
            MethodReply(msg, &arg, 1);
        }
        else
        {
            MethodReply(msg, ER_BUS_NO_SUCH_PROPERTY);
        }
    }
    else
    {
        MethodReply(msg, ER_BUS_NO_SUCH_INTERFACE);
    }
}

void VirtualConfigBusObject::SetProp(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) member;
    MethodReply(msg, ER_BUS_PROPERTY_ACCESS_DENIED);
}

void VirtualConfigBusObject::GetAllProps(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) member;
    if (!strcmp(msg->GetArg(0)->v_string.str, "org.alljoyn.Config"))
    {
        ajn::MsgArg entry;
        entry.typeId = ajn::ALLJOYN_DICT_ENTRY;
        entry.v_dictEntry.key = new ajn::MsgArg("s", "Version");
        entry.v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("q", 1));
        entry.SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
        ajn::MsgArg dict;
        dict.typeId = ajn::ALLJOYN_ARRAY;
        dict.v_array.SetElements("{sv}", 1, &entry);
        MethodReply(msg, &dict, 1);
    }
    else
    {
        MethodReply(msg, ER_BUS_NO_SUCH_INTERFACE);
    }
}

struct ConfigurationsContext
{
    std::string m_lang;
    AboutData *m_configData;
    std::vector<Resource>::iterator m_resource;
    ConfigurationsContext(const char *lang) : m_lang(lang), m_configData(NULL) { }
    ~ConfigurationsContext() { delete m_configData; }
};

void VirtualConfigBusObject::GetConfigurations(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p", this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Resource>::iterator resource;
    ConfigurationsContext *context = new ConfigurationsContext(msg->GetArg(0)->v_string.str);
    if (!context)
    {
        goto error;
    }
    context->m_configData = new AboutData();
    if (!context->m_configData)
    {
        goto error;
    }
    /*
     * Four cases need to be considered to handle this properly:
     * 1. only oic.wk.con
     * 2. only oic.wk.con.p
     * 3. oic.wk.con and oic.wk.con.p at same href
     * 4. oic.wk.con and oic.wk.con.p at different hrefs
     *
     * The first three cases reduce to one GET (case #3 will return the union of both oic.wk.con and
     * oic.wk.con.p properties).  The fourth case requires two GETs.
     */
    resource = FindResourceFromType(m_resources, OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION);
    if (resource == m_resources.end())
    {
        resource = FindResourceFromType(m_resources, OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION);
    }
    if (resource == m_resources.end())
    {
        goto error;
    }
    context->m_resource = resource;
    DoResource(OC_REST_GET, resource->m_uri, resource->m_addrs, NULL, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::GetConfigurationsCB),
            context);
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
    delete context;
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::GetConfigurationsCB(ajn::Message &msg, OCRepPayload *payload, void *ctx)
{
    LOG(LOG_INFO, "[%p]", this);

    std::vector<Resource>::iterator platform;
    ConfigurationsContext *context = (ConfigurationsContext *) ctx;
    QStatus status = context->m_configData->Set(payload);
    if (status != ER_OK)
    {
        goto error;
    }
    platform = FindResourceFromType(m_resources, OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION);
    if (platform != m_resources.end() && platform != context->m_resource)
    {
        context->m_resource = platform;
        DoResource(OC_REST_GET, context->m_resource->m_uri, context->m_resource->m_addrs, NULL, msg,
                static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::GetConfigurationsCB),
                context);
    }
    else
    {
        ajn::MsgArg arg;
        status = context->m_configData->GetConfigData(&arg, context->m_lang.c_str());
        if (status != ER_OK)
        {
            goto error;
        }
        status = MethodReply(msg, &arg, 1);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
        }
        delete context;
    }
    return;

error:
    status = MethodReply(msg, status);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
    delete context;
}

void VirtualConfigBusObject::UpdateConfigurations(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p", this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    const char *lang;
    OCRepPayload *payload = NULL;
    std::vector<Resource>::iterator device;
    std::vector<Resource>::iterator platform;
    ConfigurationsContext *context = new ConfigurationsContext(msg->GetArg(0)->v_string.str);
    if (!context)
    {
        goto error;
    }
    lang = msg->GetArg(0)->v_string.str;
    if (!strcmp(lang, ""))
    {
        lang = m_bus->GetDefaultLanguage();
    }
    context->m_configData = new AboutData(msg->GetArg(1), lang);
    payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    device = FindResourceFromType(m_resources, OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION);
    platform = FindResourceFromType(m_resources, OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION);
    if (device != m_resources.end())
    {
        result = SetDeviceConfigurationProperties(payload, context->m_configData);
        if (result != OC_STACK_OK)
        {
            goto error;
        }
        context->m_resource = device;
    }
    if (platform != m_resources.end() && (device == m_resources.end() || platform == device ||
            !payload->values))
    {
        result = SetPlatformConfigurationProperties(payload, context->m_configData);
        if (result != OC_STACK_OK)
        {
            goto error;
        }
        context->m_resource = platform;
    }
    DoResource(OC_REST_POST, context->m_resource->m_uri, context->m_resource->m_addrs, payload, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::UpdateConfigurationsCB),
            context);
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
    OCRepPayloadDestroy(payload);
    delete context;
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::UpdateConfigurationsCB(ajn::Message &msg, OCRepPayload *responsePayload,
        void *ctx)
{
    (void) responsePayload;
    LOG(LOG_INFO, "[%p]", this);

    OCRepPayload *payload = NULL;
    std::vector<Resource>::iterator platform;
    ConfigurationsContext *context = (ConfigurationsContext *) ctx;
    platform = FindResourceFromType(m_resources, OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION);
    if (platform != m_resources.end() && platform != context->m_resource)
    {
        OCStackResult result;
        context->m_resource = platform;
        payload = OCRepPayloadCreate();
        if (!payload)
        {
            goto error;
        }
        result = SetPlatformConfigurationProperties(payload, context->m_configData);
        if (result != OC_STACK_OK)
        {
            goto error;
        }
        DoResource(OC_REST_POST, context->m_resource->m_uri, context->m_resource->m_addrs, payload, msg,
                static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::UpdateConfigurationsCB),
                context);
    }
    else
    {
        QStatus status = MethodReply(msg);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
        }
        delete context;
    }
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
    OCRepPayloadDestroy(payload);
    delete context;
}

void VirtualConfigBusObject::FactoryReset(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) msg;
    LOG(LOG_INFO, "[%p] member=%p", this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto resource = FindResourceFromType(m_resources, "oic.wk.mnt");
    if (resource == m_resources.end())
    {
        goto error;
    }
    OCRepPayload *payload;
    payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    OCRepPayloadSetPropBool(payload, "fr", true);
    DoResource(OC_REST_POST, resource->m_uri, resource->m_addrs, payload, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::FactoryResetCB));
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::FactoryResetCB(ajn::Message &msg, OCRepPayload *payload, void *ctx)
{
    (void) payload;
    (void) ctx;
    LOG(LOG_INFO, "[%p]", this);

    QStatus status = MethodReply(msg);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

void VirtualConfigBusObject::Restart(const ajn::InterfaceDescription::Member *member,
                                     ajn::Message &msg)
{
    (void) msg;
    LOG(LOG_INFO, "[%p] member=%p", this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    auto resource = FindResourceFromType(m_resources, "oic.wk.mnt");
    if (resource == m_resources.end())
    {
        goto error;
    }
    OCRepPayload *payload;
    payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    OCRepPayloadSetPropBool(payload, "rb", true);
    DoResource(OC_REST_POST, resource->m_uri, resource->m_addrs, payload, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::RestartCB));
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::RestartCB(ajn::Message &msg, OCRepPayload *payload, void *ctx)
{
    (void) payload;
    (void) ctx;
    LOG(LOG_INFO, "[%p]", this);

    QStatus status = MethodReply(msg);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

void VirtualConfigBusObject::NotImplemented(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) msg;
    LOG(LOG_INFO, "[%p] member=%p", this, member);
    MethodReply(msg, ER_NOT_IMPLEMENTED);
}
