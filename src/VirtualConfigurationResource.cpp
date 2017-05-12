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

#include "VirtualConfigurationResource.h"

#include "Bridge.h"
#include "Log.h"
#include "Name.h"
#include "Payload.h"
#include "Plugin.h"
#include "Resource.h"
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <qcc/StringUtil.h>
#include "Signature.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include <algorithm>
#include <assert.h>

#define OC_RSRVD_CONFIGURATION_URI                   "/oic/con"
#define OC_RSRVD_RESOURCE_TYPE_CONFIGURATION         "oic.wk.con"

#define OC_RSRVD_MAINTENANCE_URI                     "/oic/mnt"
#define OC_RSRVD_RESOURCE_TYPE_MAINTENANCE           "oic.wk.mnt"

static const struct { const char *aj; const char *oc; } keys[] =
{
    { "AppName", "n" },
    { "org.openconnectivity.loc", "loc" },
    { "org.openconnectivity.locn", "locn" },
    { "org.openconnectivity.c", "c" },
    { "org.openconnectivity.r", "r" },
    { "DefaultLanguage", "dl" }
};

VirtualResource *VirtualConfigurationResource::Create(Bridge *bridge, ajn::BusAttachment *bus,
        const char *name, ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion)
{
    VirtualConfigurationResource *resource = new VirtualConfigurationResource(bridge, bus, name,
            sessionId, path, ajSoftwareVersion);
    OCStackResult result = resource->Create();
    if (result != OC_STACK_OK)
    {
        delete resource;
        resource = NULL;
    }
    return resource;
}

VirtualConfigurationResource::VirtualConfigurationResource(Bridge *bridge, ajn::BusAttachment *bus,
        const char *name, ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion)
    : VirtualResource(bridge, bus, name, sessionId, path, ajSoftwareVersion), m_fr(false),
      m_rb(false)
{
    LOG(LOG_INFO, "[%p] bus=%p,name=%s,sessionId=%d,path=%s,ajSoftwareVersion=%s",
        this, bus, name, sessionId, path, ajSoftwareVersion);
}

VirtualConfigurationResource::~VirtualConfigurationResource()
{
    LOG(LOG_INFO, "[%p] name=%s,path=%s", this,
        GetUniqueName().c_str(), GetPath().c_str());

    DestroyResource(GetPath().c_str());
}

OCStackResult VirtualConfigurationResource::Create()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    QStatus status = IntrospectRemoteObjectAsync(
                         this, static_cast<ajn::ProxyBusObject::Listener::IntrospectCB>
                         (&VirtualConfigurationResource::IntrospectCB), NULL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "IntrospectRemoteObjectAsync - %s", QCC_StatusText(status));
        return OC_STACK_ERROR;
    }
    return OC_STACK_OK;
}

void VirtualConfigurationResource::IntrospectCB(QStatus status, ProxyBusObject *obj, void *ctx)
{
    (void) obj;
    (void) ctx;
    LOG(LOG_INFO, "[%p]", this);

    OCStackResult result;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "IntrospectCB - %s", QCC_StatusText(status));
            return;
        }

        result = ::CreateResource(OC_RSRVD_CONFIGURATION_URI,
                OC_RSRVD_RESOURCE_TYPE_CONFIGURATION,
                OC_RSRVD_INTERFACE_READ_WRITE,
                VirtualConfigurationResource::ConfigurationHandlerCB, this,
                OC_DISCOVERABLE | OC_OBSERVABLE);
        if (result == OC_STACK_OK)
        {
            LOG(LOG_INFO, "[%p] Created VirtualConfigurationResource uri=%s", this,
                    OC_RSRVD_CONFIGURATION_URI);
        }
        result = ::CreateResource(OC_RSRVD_MAINTENANCE_URI, OC_RSRVD_RESOURCE_TYPE_MAINTENANCE,
                OC_RSRVD_INTERFACE_READ_WRITE,
                VirtualConfigurationResource::MaintenanceHandlerCB, this,
                OC_DISCOVERABLE | OC_OBSERVABLE);
        if (result == OC_STACK_OK)
        {
            LOG(LOG_INFO, "[%p] Created VirtualConfigurationResource uri=%s", this,
                    OC_RSRVD_MAINTENANCE_URI);
        }
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "[%p] Create VirtualConfigurationResource - %d", this, result);
        }
    }
    if (result == OC_STACK_OK)
    {
        m_bridge->RDPublish();
    }
}

struct VirtualConfigurationResource::MethodCallContext
{
    OCEntityHandlerResponse *m_response;
    OCRepPayload *m_payload;
    std::map<std::string, std::string>::iterator m_appName;
    std::map<std::string, std::string> m_appNames;
    MethodCallContext(OCEntityHandlerRequest *request)
        : m_response(NULL), m_payload(::CreatePayload(request->resource, request->query))
    {
        m_response = (OCEntityHandlerResponse *) calloc(1, sizeof(OCEntityHandlerResponse));
        m_response->requestHandle = request->requestHandle;
        m_response->resourceHandle = request->resource;
    }
    ~MethodCallContext()
    {
        free(m_response);
    }
};

OCEntityHandlerResult VirtualConfigurationResource::ConfigurationHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p",
        ctx, flag, request, ctx);

    VirtualConfigurationResource *resource = reinterpret_cast<VirtualConfigurationResource *>(ctx);
    std::lock_guard<std::mutex> lock(resource->m_mutex);

    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                MethodCallContext *context = new MethodCallContext(request);
                ajn::MsgArg lang("s", "");
                const ajn::InterfaceDescription *iface = resource->m_bus->GetInterface("org.alljoyn.Config");
                assert(iface);
                const ajn::InterfaceDescription::Member *member = iface->GetMember("GetConfigurations");
                assert(member);
                QStatus status = resource->MethodCallAsync(*member,
                                 resource, static_cast<ajn::MessageReceiver::ReplyHandler>
                                 (&VirtualConfigurationResource::GetSupportedLanguagesCB),
                                 &lang, 1, context);
                if (status == ER_OK)
                {
                    result = OC_EH_OK;
                }
                else
                {
                    LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                    delete context;
                    result = OC_EH_ERROR;
                }
                break;
            }
        case OC_REST_POST:
            {
                if (!request->payload || request->payload->type != PAYLOAD_TYPE_REPRESENTATION)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                MethodCallContext *context = new MethodCallContext(request);
                bool success = true;
                ajn::MsgArg entries[sizeof(keys) / sizeof(keys[0])];
                ajn::MsgArg *entry = entries;
                OCRepPayload *payload = (OCRepPayload *) request->payload;
                for (OCRepPayloadValue *v = payload->values; success && v; v = v->next)
                {
                    for (size_t i = 0; i < sizeof(keys) / sizeof(keys[0]); ++i)
                    {
                        if (!strcmp(v->name, keys[i].oc))
                        {
                            OCRepPayloadValue k;
                            memset(&k, 0, sizeof(k));
                            k.type = OCREP_PROP_STRING;
                            k.str = (char *) keys[i].aj;
                            entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
                            entry->v_dictEntry.key = new ajn::MsgArg();
                            entry->v_dictEntry.val = new ajn::MsgArg();
                            entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                            success = ToAJMsgArg(entry->v_dictEntry.key, "s", &k);
                            if (success)
                            {
                                success = ToAJMsgArg(entry->v_dictEntry.val, "v", v);
                            }
                            if (success)
                            {
                                /* Copy the value into the response payload */
                                ToOCPayload(context->m_payload, keys[i].oc, entry->v_dictEntry.val, "v");
                            }
                            ++entry;
                            break;
                        }
                    }
                }
                size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
                OCRepPayload **objArray = NULL;
                if (OCRepPayloadGetPropObjectArray(payload, "ln", &objArray, dim))
                {
                    size_t dimTotal = calcDimTotal(dim);
                    for (size_t i = 0; i < dimTotal; ++i)
                    {
                        char *language;
                        char *value;
                        if (OCRepPayloadGetPropString(objArray[i], "language", &language) &&
                            OCRepPayloadGetPropString(objArray[i], "value", &value))
                        {
                            context->m_appNames[language] = value;
                            OICFree(language);
                            OICFree(value);
                        }
                    }
                    /* Copy the value into the response payload */
                    OCRepPayloadSetPropObjectArrayAsOwner(context->m_payload, "ln", objArray, dim);
                }
                context->m_appName = context->m_appNames.begin();
                if (success)
                {
                    if (entry - entries)
                    {
                        ajn::MsgArg args[2];
                        args[0].Set("s", ""); /* languageTag */
                        args[1].Set("a{sv}", entry - entries, entries);
                        const ajn::InterfaceDescription *iface = resource->m_bus->GetInterface("org.alljoyn.Config");
                        assert(iface);
                        const ajn::InterfaceDescription::Member *member = iface->GetMember("UpdateConfigurations");
                        assert(member);
                        QStatus status = resource->MethodCallAsync(*member,
                                         resource, static_cast<ajn::MessageReceiver::ReplyHandler>
                                         (&VirtualConfigurationResource::UpdateConfigurationsCB),
                                         args, 2, context);
                        success = (status == ER_OK);
                        if (status != ER_OK)
                        {
                            LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                        }
                    }
                    else if (!context->m_appNames.empty())
                    {
                        QStatus status = resource->UpdateAppNames(context);
                        success = (status == ER_OK);
                    }
                    else
                    {
                        success = false;
                    }
                }
                if (success)
                {
                    result = OC_EH_OK;
                }
                else
                {
                    delete context;
                    result = OC_EH_ERROR;
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

void VirtualConfigurationResource::GetSupportedLanguagesCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    MethodCallContext *context = reinterpret_cast<MethodCallContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                const ajn::MsgArg *dict = msg->GetArg(0);
                size_t numLangs = 0;
                ajn::MsgArg *langs = NULL;
                dict->GetElement("{sas}", "SupportedLanguages", &numLangs, &langs);
                for (size_t i = 0; i < numLangs; ++i)
                {
                    const char *lang;
                    langs[i].Get("s", &lang);
                    m_appNames[lang] = "";
                }
                const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.alljoyn.Config");
                assert(iface);
                const ajn::InterfaceDescription::Member *member = iface->GetMember("GetConfigurations");
                assert(member);
                QStatus status;
                if (!m_appNames.empty())
                {
                    context->m_appName = m_appNames.begin();
                    ajn::MsgArg lang("s", context->m_appName->first.c_str());
                    status = MethodCallAsync(*member,
                                             this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualConfigurationResource::GetAppNameCB),
                                             &lang, 1, context);
                }
                else
                {
                    ajn::MsgArg lang("s", "");
                    status = MethodCallAsync(*member,
                                             this, static_cast<ajn::MessageReceiver::ReplyHandler>
                                             (&VirtualConfigurationResource::GetConfigurationsCB),
                                             &lang, 1, context);
                }
                if (status == ER_OK)
                {
                    break;
                }
                /* FALLTHROUGH */
            }
        case ajn::MESSAGE_ERROR:
            OCRepPayloadDestroy(context->m_payload);
            context->m_payload = NULL;
            context->m_response->ehResult = OC_EH_ERROR;
            context->m_response->payload = reinterpret_cast<OCPayload *>(context->m_payload);
            result = DoResponse(context->m_response);
            if (result != OC_STACK_OK)
            {
                LOG(LOG_ERR, "DoResponse - %d", result);
            }
            delete context;
            break;
        default:
            assert(0);
            break;
    }
}

void VirtualConfigurationResource::GetAppNameCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    MethodCallContext *context = reinterpret_cast<MethodCallContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                const ajn::MsgArg *dict = msg->GetArg(0);
                const char *appName;
                QStatus status = dict->GetElement("{ss}", "AppName", &appName);
                if (status == ER_OK)
                {
                    context->m_appName->second = appName;
                    if (++context->m_appName != m_appNames.end())
                    {
                        ajn::MsgArg lang("s", context->m_appName->first.c_str());
                        const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.alljoyn.Config");
                        assert(iface);
                        const ajn::InterfaceDescription::Member *member = iface->GetMember("GetConfigurations");
                        assert(member);
                        status = MethodCallAsync(*member,
                                                 this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualConfigurationResource::GetAppNameCB),
                                                 &lang, 1, context);
                    }
                    else
                    {
                        ajn::MsgArg lang("s", "");
                        const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.alljoyn.Config");
                        assert(iface);
                        const ajn::InterfaceDescription::Member *member = iface->GetMember("GetConfigurations");
                        assert(member);
                        status = MethodCallAsync(*member,
                                                 this, static_cast<ajn::MessageReceiver::ReplyHandler>
                                                 (&VirtualConfigurationResource::GetConfigurationsCB),
                                                 &lang, 1, context);
                    }
                }
                if (status == ER_OK)
                {
                    break;
                }
                /* FALLTHROUGH */
            }
        case ajn::MESSAGE_ERROR:
            OCRepPayloadDestroy(context->m_payload);
            context->m_payload = NULL;
            context->m_response->ehResult = OC_EH_ERROR;
            context->m_response->payload = reinterpret_cast<OCPayload *>(context->m_payload);
            result = DoResponse(context->m_response);
            if (result != OC_STACK_OK)
            {
                LOG(LOG_ERR, "DoResponse - %d", result);
            }
            delete context;
            break;
        default:
            assert(0);
            break;
    }
}

void VirtualConfigurationResource::GetConfigurationsCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    MethodCallContext *context = reinterpret_cast<MethodCallContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                bool success = true;
                const ajn::MsgArg *dict = msg->GetArg(0);
                for (size_t i = 0; success && (i < sizeof(keys) / sizeof(keys[0])); ++i)
                {
                    char *value;
                    if (dict->GetElement("{ss}", keys[i].aj, &value) == ER_OK)
                    {
                        success = success && OCRepPayloadSetPropString(context->m_payload, keys[i].oc, value);
                    }
                }
                if (!m_appNames.empty())
                {
                    size_t dim[MAX_REP_ARRAY_DEPTH] = { m_appNames.size(), 0, 0 };
                    OCRepPayload **objArray = (OCRepPayload **) OICCalloc(calcDimTotal(dim), sizeof(OCRepPayload *));
                    if (!objArray)
                    {
                        success = false;
                    }
                    else
                    {
                        size_t i = 0;
                        for (std::map<std::string, std::string>::iterator it = m_appNames.begin(); success
                             && it != m_appNames.end(); ++it)
                        {
                            OCRepPayload *value = OCRepPayloadCreate();
                            if (!value)
                            {
                                success = false;
                                break;
                            }
                            success = OCRepPayloadSetPropString(value, "language", it->first.c_str()) &&
                                      OCRepPayloadSetPropString(value, "value", it->second.c_str());
                            objArray[i++] = value;
                        }
                    }
                    if (success)
                    {
                        success = OCRepPayloadSetPropObjectArrayAsOwner(context->m_payload, "ln", objArray, dim);
                    }
                    else
                    {
                        OICFree(objArray);
                    }
                }
                if (success)
                {
                    context->m_response->ehResult = OC_EH_OK;
                }
                else
                {
                    context->m_response->ehResult = OC_EH_ERROR;
                    OCRepPayloadDestroy(context->m_payload);
                    context->m_payload = NULL;
                }
                break;
            }
        case ajn::MESSAGE_ERROR:
            context->m_response->ehResult = OC_EH_ERROR;
            break;
        default:
            assert(0);
            break;
    }
    context->m_response->payload = reinterpret_cast<OCPayload *>(context->m_payload);
    result = DoResponse(context->m_response);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResponse - %d", result);
        OCRepPayloadDestroy(context->m_payload);
    }
    delete context;
}

QStatus VirtualConfigurationResource::UpdateAppNames(MethodCallContext *context)
{
    if (context->m_appName == context->m_appNames.end())
    {
        return ER_OK;
    }
    ajn::MsgArg args[2];
    args[0].Set("s", context->m_appName->first.c_str()); /* languageTag */
    ajn::MsgArg entry;
    entry.typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry.v_dictEntry.key = new ajn::MsgArg("s", "AppName");
    entry.v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s",
                                            context->m_appName->second.c_str()));
    entry.SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    args[1].Set("a{sv}", 1, &entry);
    const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.alljoyn.Config");
    assert(iface);
    const ajn::InterfaceDescription::Member *member = iface->GetMember("UpdateConfigurations");
    assert(member);
    QStatus status = MethodCallAsync(*member,
                                     this, static_cast<ajn::MessageReceiver::ReplyHandler>
                                     (&VirtualConfigurationResource::UpdateConfigurationsCB),
                                     args, 2, context);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
    }
    return status;
}

void VirtualConfigurationResource::UpdateConfigurationsCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    MethodCallContext *context = reinterpret_cast<MethodCallContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                QStatus status = UpdateAppNames(context);
                if (status == ER_OK)
                {
                    context->m_response->ehResult = OC_EH_OK;
                    break;
                }
                /* FALLTHROUGH */
            }
        case ajn::MESSAGE_ERROR:
            OCRepPayloadDestroy(context->m_payload);
            context->m_payload = NULL;
            context->m_response->ehResult = OC_EH_ERROR;
            break;
        default:
            assert(0);
            break;
    }
    if (context->m_appName != context->m_appNames.end() &&
        context->m_response->ehResult == OC_EH_OK)
    {
        ++context->m_appName;
    }
    else
    {
        context->m_response->payload = reinterpret_cast<OCPayload *>(context->m_payload);
        result = DoResponse(context->m_response);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "DoResponse - %d", result);
            OCRepPayloadDestroy(context->m_payload);
        }
        delete context;
    }
}

OCEntityHandlerResult VirtualConfigurationResource::MaintenanceHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p",
        ctx, flag, request, ctx);

    VirtualConfigurationResource *resource = reinterpret_cast<VirtualConfigurationResource *>(ctx);
    std::lock_guard<std::mutex> lock(resource->m_mutex);

    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_POST:
            {
                if (!request->payload || request->payload->type != PAYLOAD_TYPE_REPRESENTATION)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                OCRepPayload *payload = (OCRepPayload *) request->payload;
                bool value;
                if (OCRepPayloadGetPropBool(payload, "fr", &value) && value)
                {
                    const ajn::InterfaceDescription *iface = resource->m_bus->GetInterface("org.alljoyn.Config");
                    assert(iface);
                    const ajn::InterfaceDescription::Member *member = iface->GetMember("FactoryReset");
                    assert(member);
                    QStatus status = resource->MethodCallAsync(*member, NULL, NULL);
                    if (status != ER_OK)
                    {
                        LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                        result = OC_EH_ERROR;
                        break;
                    }
                }
                if (OCRepPayloadGetPropBool(payload, "rb", &value) && value)
                {
                    const ajn::InterfaceDescription *iface = resource->m_bus->GetInterface("org.alljoyn.Config");
                    assert(iface);
                    const ajn::InterfaceDescription::Member *member = iface->GetMember("Restart");
                    assert(member);
                    QStatus status = resource->MethodCallAsync(*member, NULL, NULL);
                    if (status != ER_OK)
                    {
                        LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                        result = OC_EH_ERROR;
                        break;
                    }
                }
                /* FALLTHROUGH */
            }
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = ::CreatePayload(request->resource, request->query);
                if (!payload)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                if (!OCRepPayloadSetPropBool(payload, "fr", resource->m_fr) ||
                    !OCRepPayloadSetPropBool(payload, "rb", resource->m_rb))
                {
                    result = OC_EH_ERROR;
                    OCRepPayloadDestroy(payload);
                    payload = NULL;
                }
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(payload);
                OCStackResult doResult = DoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "DoResponse - %d", doResult);
                    OCRepPayloadDestroy(payload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}
