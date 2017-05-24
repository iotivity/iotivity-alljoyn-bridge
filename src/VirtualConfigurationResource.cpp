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

#include "AboutData.h"
#include "Bridge.h"
#include "DeviceConfigurationResource.h"
#include "Log.h"
#include "Name.h"
#include "Payload.h"
#include "PlatformConfigurationResource.h"
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

#define OC_RSRVD_MAINTENANCE_URI                     "/oic/mnt"
#define OC_RSRVD_RESOURCE_TYPE_MAINTENANCE           "oic.wk.mnt"

VirtualConfigurationResource *VirtualConfigurationResource::Create(ajn::BusAttachment *bus,
        const char *name, ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion,
        CreateCB createCb, void *createContext)
{
    VirtualConfigurationResource *resource = new VirtualConfigurationResource(bus, name, sessionId,
            path, ajSoftwareVersion, createCb, createContext);
    OCStackResult result = resource->Create();
    if (result != OC_STACK_OK)
    {
        delete resource;
        resource = NULL;
    }
    return resource;
}

VirtualConfigurationResource::VirtualConfigurationResource(ajn::BusAttachment *bus,
        const char *name, ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion,
        CreateCB createCb, void *createContext)
    : VirtualResource(bus, name, sessionId, path, ajSoftwareVersion, createCb, createContext),
      m_fr(false), m_rb(false)
{
    LOG(LOG_INFO, "[%p] bus=%p,name=%s,sessionId=%d,path=%s,ajSoftwareVersion=%s", this, bus, name,
            sessionId, path, ajSoftwareVersion);
}

VirtualConfigurationResource::~VirtualConfigurationResource()
{
    LOG(LOG_INFO, "[%p] name=%s,path=%s", this, GetUniqueName().c_str(), GetPath().c_str());

    OCDeleteResource(m_deviceConfigurationHandle);
    OCDeleteResource(m_platformConfigurationHandle);
    OCDeleteResource(m_maintenanceHandle);
}

void VirtualConfigurationResource::SetAboutData(ajn::AboutData *aboutData)
{
    m_aboutData = *aboutData;
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

        result = ::CreateResource(&m_deviceConfigurationHandle, "/con",
                OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION, OC_RSRVD_INTERFACE_READ_WRITE,
                VirtualConfigurationResource::ConfigurationHandlerCB, this,
                OC_DISCOVERABLE | OC_OBSERVABLE);
        if (result == OC_STACK_OK)
        {
            LOG(LOG_INFO, "[%p] Created VirtualConfigurationResource uri=%s", this,
                    OCGetResourceUri(m_deviceConfigurationHandle));
        }
        result = ::CreateResource(&m_platformConfigurationHandle, "/con/p",
                OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION, OC_RSRVD_INTERFACE_READ_WRITE,
                VirtualConfigurationResource::ConfigurationHandlerCB, this,
                OC_DISCOVERABLE | OC_OBSERVABLE);
        if (result == OC_STACK_OK)
        {
            LOG(LOG_INFO, "[%p] Created VirtualConfigurationResource uri=%s", this,
                    OCGetResourceUri(m_platformConfigurationHandle));
        }
        result = ::CreateResource(&m_maintenanceHandle, OC_RSRVD_MAINTENANCE_URI,
                OC_RSRVD_RESOURCE_TYPE_MAINTENANCE, OC_RSRVD_INTERFACE_READ_WRITE,
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
        m_createCb(m_createContext);
    }
}

struct VirtualConfigurationResource::MethodCallContext
{
    OCEntityHandlerResponse *m_response;
    OCRepPayload *m_payload;
    std::vector<std::string> m_langs;
    std::vector<std::string>::iterator m_lang;
    AboutData *m_configData;
    MethodCallContext(OCEntityHandlerRequest *request)
        : m_response(NULL), m_payload(::CreatePayload(request->resource, request->query)),
          m_configData(NULL)
    {
        CreateResponse(request);
    }
    MethodCallContext(OCEntityHandlerRequest *request, OCRepPayload *payload)
        : m_response(NULL), m_payload(OCRepPayloadClone(payload)), m_configData(NULL)
    {
        CreateResponse(request);
    }
    ~MethodCallContext()
    {
        delete m_configData;
        OICFree(m_response);
    }
    void CreateResponse(OCEntityHandlerRequest *request)
    {
        m_response = (OCEntityHandlerResponse *) OICCalloc(1, sizeof(OCEntityHandlerResponse));
        m_response->requestHandle = request->requestHandle;
        m_response->resourceHandle = request->resource;
    }
    void SetLanguages(ajn::AboutData *aboutData)
    {
        size_t numLangs = aboutData->GetSupportedLanguages();
        const char *langs[numLangs];
        aboutData->GetSupportedLanguages(langs, numLangs);
        for (size_t i = 0; i < numLangs; ++i)
        {
            m_langs.push_back(langs[i]);
        }
        m_lang = m_langs.begin();
    }
};

OCEntityHandlerResult VirtualConfigurationResource::ConfigurationHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p", ctx, flag, request, ctx);

    VirtualConfigurationResource *resource = reinterpret_cast<VirtualConfigurationResource *>(ctx);
    std::lock_guard<std::mutex> lock(resource->m_mutex);

    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                MethodCallContext *context = new MethodCallContext(request);
                context->SetLanguages(&resource->m_aboutData);
                const char *lang = "";
                if (context->m_lang != context->m_langs.end())
                {
                    lang = context->m_lang->c_str();
                }
                ajn::MsgArg arg("s", lang);
                QStatus status = resource->MethodCallAsync("org.alljoyn.Config",
                        "GetConfigurations", resource,
                        static_cast<ajn::MessageReceiver::ReplyHandler> (&VirtualConfigurationResource::GetConfigurationsCB),
                        &arg, 1, context);
                if (status == ER_OK)
                {
                    result = OC_EH_OK;
                }
                else
                {
                    LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                    OCRepPayloadDestroy(context->m_payload);
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
                OCRepPayload *payload = (OCRepPayload *) request->payload;
                bool success = true;
                MethodCallContext *context = new MethodCallContext(request, payload);
                if (!context->m_payload)
                {
                    success = false;
                }
                if (success)
                {
                    char *defaultLang = (char *) "";
                    resource->m_aboutData.GetDefaultLanguage(&defaultLang);
                    context->m_configData = new AboutData(defaultLang);
                    if (!context->m_configData)
                    {
                        success = false;
                    }
                }
                if (success)
                {
                    context->m_configData->Set(payload);
                    context->SetLanguages(context->m_configData);
                    const char *lang = "";
                    if (context->m_lang != context->m_langs.end())
                    {
                        lang = context->m_lang->c_str();
                    }
                    ajn::MsgArg args[2];
                    args[0].Set("s", lang);
                    context->m_configData->GetConfigData(&args[1], lang);
                    QStatus status = resource->MethodCallAsync("org.alljoyn.Config",
                            "UpdateConfigurations", resource,
                            static_cast<ajn::MessageReceiver::ReplyHandler> (&VirtualConfigurationResource::UpdateConfigurationsCB),
                            args, 2, context);
                    success = (status == ER_OK);
                    if (status != ER_OK)
                    {
                        LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                    }
                }
                if (success)
                {
                    result = OC_EH_OK;
                }
                else
                {
                    OCRepPayloadDestroy(context->m_payload);
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

void VirtualConfigurationResource::GetConfigurationsCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p", this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    MethodCallContext *context = reinterpret_cast<MethodCallContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                const char *lang = context->m_lang->c_str();
                if (!context->m_configData)
                {
                    context->m_configData = new AboutData(msg->GetArg(0), lang);
                }
                else
                {
                    context->m_configData->CreateFromMsgArg(msg->GetArg(0), lang);
                }
                char *appName = NULL;
                if (context->m_configData->GetAppName(&appName, lang) != ER_OK)
                {
                    m_aboutData.GetAppName(&appName, lang);
                    context->m_configData->SetAppName(appName, lang);
                }
                if (context->m_lang != context->m_langs.end() &&
                        (++context->m_lang != context->m_langs.end()))
                {
                    lang = context->m_lang->c_str();
                    ajn::MsgArg arg("s", lang);
                    QStatus status = MethodCallAsync("org.alljoyn.Config", "GetConfigurations", this,
                            static_cast<ajn::MessageReceiver::ReplyHandler> (&VirtualConfigurationResource::GetConfigurationsCB),
                            &arg, 1, context);
                    if (status == ER_OK)
                    {
                        return;
                    }
                    else
                    {
                        LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                        context->m_response->ehResult = OC_EH_ERROR;
                    }
                }
                else
                {
                    if (context->m_response->resourceHandle == m_deviceConfigurationHandle)
                    {
                        result = SetDeviceConfigurationProperties(context->m_payload,
                                context->m_configData);
                    }
                    else
                    {
                        result = SetPlatformConfigurationProperties(context->m_payload,
                                context->m_configData);
                    }
                    if (result == OC_STACK_OK)
                    {
                        context->m_response->ehResult = OC_EH_OK;
                    }
                    else
                    {
                        context->m_response->ehResult = OC_EH_ERROR;
                    }
                }
                if (context->m_response->ehResult == OC_EH_OK)
                {
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
    context->m_response->payload = reinterpret_cast<OCPayload *>(context->m_payload);
    result = OCDoResponse(context->m_response);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "OCDoResponse - %d", result);
        OCRepPayloadDestroy(context->m_payload);
    }
    delete context;
}

void VirtualConfigurationResource::UpdateConfigurationsCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p", this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    MethodCallContext *context = reinterpret_cast<MethodCallContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                if (context->m_lang != context->m_langs.end() &&
                        (++context->m_lang != context->m_langs.end()))
                {
                    const char *lang = context->m_lang->c_str();
                    ajn::MsgArg args[2];
                    args[0].Set("s", lang);
                    context->m_configData->GetConfigData(&args[1], lang);
                    QStatus status = MethodCallAsync("org.alljoyn.Config", "UpdateConfigurations",
                            this,
                            static_cast<ajn::MessageReceiver::ReplyHandler> (&VirtualConfigurationResource::UpdateConfigurationsCB),
                            args, 2, context);
                    if (status == ER_OK)
                    {
                        return;
                    }
                    else
                    {
                        LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                        context->m_response->ehResult = OC_EH_ERROR;
                    }
                }
                else
                {
                    context->m_response->ehResult = OC_EH_OK;
                }
                if (context->m_response->ehResult == OC_EH_OK)
                {
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
    context->m_response->payload = reinterpret_cast<OCPayload *>(context->m_payload);
    result = OCDoResponse(context->m_response);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "OCDoResponse - %d", result);
        OCRepPayloadDestroy(context->m_payload);
    }
    delete context;
}

OCEntityHandlerResult VirtualConfigurationResource::MaintenanceHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p", ctx, flag, request, ctx);

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
                    QStatus status = resource->MethodCallAsync("org.alljoyn.Config", "FactoryReset",
                            NULL, NULL);
                    if (status != ER_OK)
                    {
                        LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                        result = OC_EH_ERROR;
                        break;
                    }
                }
                if (OCRepPayloadGetPropBool(payload, "rb", &value) && value)
                {
                    QStatus status = resource->MethodCallAsync("org.alljoyn.Config", "Restart",
                            NULL, NULL);
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
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "OCDoResponse - %d", doResult);
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
