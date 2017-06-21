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

#include "VirtualBusObject.h"

#include "Log.h"
#include "Name.h"
#include "Payload.h"
#include "Plugin.h"
#include "VirtualBusAttachment.h"
#include "ocpayload.h"
#include "ocstack.h"
#include <algorithm>
#include <assert.h>

struct VirtualBusObject::ObserveContext
{
public:
    ObserveContext(VirtualBusObject *obj, std::string iface)
        : m_obj(obj), m_iface(iface), m_handle(NULL), m_result(OC_STACK_KEEP_TRANSACTION) { }
    static void Deleter(void *ctx)
    {
        LOG(LOG_INFO, "[%p]", ctx);
        ObserveContext *context = reinterpret_cast<ObserveContext *>(ctx);
        {
            std::lock_guard<std::mutex> lock(context->m_obj->m_mutex);
            context->m_obj->m_observes.erase(context);
            context->m_obj->m_cond.notify_one();
        }
        delete context;
    }
    VirtualBusObject *m_obj;
    std::string m_iface;
    OCDoHandle m_handle;
    OCStackApplicationResult m_result;
};

struct VirtualBusObject::DoResourceContext
{
public:
    DoResourceContext(VirtualBusObject *obj, VirtualBusObject::DoResourceHandler cb, void *context,
            ajn::Message &msg)
        : m_obj(obj), m_cb(cb), m_context(context), m_msg(msg), m_handle(NULL) { }
    VirtualBusObject *m_obj;
    VirtualBusObject::DoResourceHandler m_cb;
    void *m_context;
    ajn::Message m_msg;
    OCDoHandle m_handle;
};

VirtualBusObject::VirtualBusObject(VirtualBusAttachment *bus, Resource &resource)
    : ajn::BusObject(resource.m_uri.c_str()), m_bus(bus), m_pending(0)
{
    LOG(LOG_INFO, "[%p] bus=%p,uri=%s", this, bus, resource.m_uri.c_str());
    m_resources.push_back(resource);
}

VirtualBusObject::VirtualBusObject(VirtualBusAttachment *bus, const char *path, Resource &resource)
    : ajn::BusObject(path), m_bus(bus), m_pending(0)
{
    LOG(LOG_INFO, "[%p] bus=%p,path=%s", this, bus, path);
    m_resources.push_back(resource);
}

VirtualBusObject::~VirtualBusObject()
{
    LOG(LOG_INFO, "[%p]", this);

    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_pending > 0 || !m_observes.empty())
    {
        m_cond.wait(lock);
    }
}

void VirtualBusObject::Stop()
{
    std::vector<OCDoHandle> handles;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (ObserveContext *context : m_observes)
        {
            context->m_result = OC_STACK_DELETE_TRANSACTION;
            handles.push_back(context->m_handle);
        }
    }
    for (OCDoHandle handle : handles)
    {
        OCStackResult result = OCCancel(handle, OC_LOW_QOS, NULL, 0);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCCancel - %d", result);
        }
    }
}

void VirtualBusObject::AddResource(Resource &resource)
{
    LOG(LOG_INFO, "[%p] uri=%s", this, resource.m_uri.c_str());
    m_resources.push_back(resource);
}

QStatus VirtualBusObject::AddInterface(const char *ifaceName, bool createEmptyInterface)
{
    LOG(LOG_INFO, "[%p] ifaceName=%s,createEmptyInterface=%d", this, ifaceName,
            createEmptyInterface);

    QStatus status = ER_BUS_NO_SUCH_INTERFACE;
    const ajn::InterfaceDescription *iface = m_bus->GetInterface(ifaceName);
    if (!iface && createEmptyInterface)
    {
        ajn::InterfaceDescription *newIface;
        m_bus->CreateInterface(ifaceName, newIface, ajn::AJ_IFC_SECURITY_INHERIT);
        newIface->Activate();
        iface = m_bus->GetInterface(ifaceName);
    }
    if (iface)
    {
        status = ajn::BusObject::AddInterface(*iface, ajn::BusObject::ANNOUNCED);
        if ((status != ER_OK) && (status != ER_BUS_IFACE_ALREADY_EXISTS))
        {
            LOG(LOG_ERR, "AddInterface - %s", QCC_StatusText(status));
        }
    }
    return status;
}

void VirtualBusObject::Observe()
{
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (auto &parent : m_resources)
    {
        Observe(parent);
        for (auto &child : parent.m_resources)
        {
            Observe(child);
        }
    }
}

/* Called with m_mutex held. */
void VirtualBusObject::Observe(Resource &resource)
{
    if (!resource.m_isObservable)
    {
        return;
    }
    for (auto &rt : resource.m_rts)
    {
        std::string uri = resource.m_uri;
        if (resource.m_rts.size() > 1)
        {
            uri += std::string("?rt=") + rt;
        }
        ObserveContext *context = new ObserveContext(this, ToAJName(rt));
        OCCallbackData cbData;
        cbData.cb = VirtualBusObject::ObserveCB;
        cbData.context = context;
        cbData.cd = ObserveContext::Deleter;
        LOG(LOG_INFO, "[%p] Observe uri=%s", this, uri.c_str());
        OCStackResult result = ::DoResource(&context->m_handle, OC_REST_OBSERVE, uri.c_str(),
                resource.m_addrs, NULL, &cbData, NULL, 0);
        if (result == OC_STACK_OK)
        {
            m_observes.insert(context);
        }
        else
        {
            LOG(LOG_ERR, "DoResource - %d", result);
        }
    }
}

void VirtualBusObject::CancelObserve()
{
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (ObserveContext *context : m_observes)
    {
        context->m_result = OC_STACK_DELETE_TRANSACTION;
        OCStackResult result = OCCancel(context->m_handle, OC_HIGH_QOS, NULL, 0);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCCancel - %d", result);
        }
    }
}

OCStackApplicationResult VirtualBusObject::ObserveCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    ObserveContext *context = reinterpret_cast<ObserveContext *>(ctx);
    LOG(LOG_INFO, "[%p] ctx=%p,handle=%p,response=%p,{payload=%p,result=%d}", context->m_obj, ctx,
            handle, response, response ? response->payload : 0, response ? response->result : 0);

    std::lock_guard<std::mutex> lock(context->m_obj->m_mutex);
    if (response && response->result == OC_STACK_OK && response->payload)
    {
        OCRepPayloadValue value;
        memset(&value, 0, sizeof(value));
        value.type = OCREP_PROP_OBJECT;
        value.obj = (OCRepPayload *) response->payload;
        ajn::MsgArg args[3];
        args[0].Set("s", context->m_iface.c_str());
        std::string valueType = std::string("[") + context->m_iface + ".Properties" + "]";
        ToAJMsgArg(&args[1], "a{sv}", &value, valueType.c_str());
        args[2].Set("as", 0, NULL);
        const ajn::InterfaceDescription *iface = context->m_obj->m_bus->GetInterface(
                    ajn::org::freedesktop::DBus::Properties::InterfaceName);
        assert(iface);
        const ajn::InterfaceDescription::Member *member = iface->GetMember("PropertiesChanged");
        assert(member);
        QStatus status = context->m_obj->Signal(NULL, ajn::SESSION_ID_ALL_HOSTED,
                                                *member,
                                                args, 3);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "Signal - %s", QCC_StatusText(status));
        }
    }
    return context->m_result;
}

void VirtualBusObject::GetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p", this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Resource>::iterator resource;
    std::string uri;
    const char *ifaceName = msg->GetArg(0)->v_string.str;
    if (m_resources.size() > 1)
    {
        goto error;
    }
    resource = FindResourceFromUri(m_resources, GetPath());
    if (resource == m_resources.end())
    {
        goto error;
    }
    if (!resource->m_resources.empty())
    {
        resource = FindResourceFromType(resource->m_resources, ToOCName(ifaceName));
        if (resource == resource->m_resources.end())
        {
            goto error;
        }
    }
    uri = resource->m_uri;
    if (resource->m_rts.size() > 1)
    {
        uri += qcc::String("?rt=") + msg->GetArg(0)->v_string.str;
    }
    DoResource(OC_REST_GET, uri, resource->m_addrs, NULL, msg, &VirtualBusObject::GetPropCB);
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualBusObject::GetPropCB(ajn::Message &msg, OCRepPayload *payload, void *ctx)
{
    (void) ctx;
    LOG(LOG_INFO, "[%p]", this);

    const char *ifaceName = msg->GetArg(0)->v_string.str;
    const char *propName = msg->GetArg(1)->v_string.str;
    const ajn::InterfaceDescription *iface = m_bus->GetInterface(ifaceName);
    if (!iface)
    {
        MethodReply(msg, ER_BUS_NO_SUCH_INTERFACE);
        return;
    }
    const ajn::InterfaceDescription::Property *prop = iface->GetProperty(propName);
    if (!prop)
    {
        MethodReply(msg, ER_BUS_NO_SUCH_PROPERTY);
        return;
    }

    ajn::MsgArg arg;
    for (OCRepPayloadValue *value = payload->values; value; value = value->next)
    {
        if (!strcmp(value->name, propName))
        {
            qcc::String signature = prop->signature;
            prop->GetAnnotation("org.alljoyn.Bus.Type.Name", signature);
            ToAJMsgArg(&arg, "v", value, signature.c_str());
            break;
        }
    }
    QStatus status = MethodReply(msg, &arg, 1);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

void VirtualBusObject::SetProp(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p", this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Resource>::iterator resource;
    std::string uri;
    OCRepPayload *payload;
    const char *ifaceName = msg->GetArg(0)->v_string.str;
    const char *propName = msg->GetArg(1)->v_string.str;
    const ajn::MsgArg *arg = msg->GetArg(2);
    qcc::String signature;
    const ajn::InterfaceDescription *iface = m_bus->GetInterface(ifaceName);
    if (!iface)
    {
        MethodReply(msg, ER_BUS_NO_SUCH_INTERFACE);
        return;
    }
    const ajn::InterfaceDescription::Property *prop = iface->GetProperty(propName);
    if (!prop)
    {
        MethodReply(msg, ER_BUS_NO_SUCH_PROPERTY);
        return;
    }
    if (m_resources.size() > 1)
    {
        goto error;
    }
    resource = FindResourceFromUri(m_resources, GetPath());
    if (resource == m_resources.end())
    {
        goto error;
    }
    if (!resource->m_resources.empty())
    {
        resource = FindResourceFromType(resource->m_resources, ToOCName(ifaceName));
        if (resource == resource->m_resources.end())
        {
            goto error;
        }
    }
    uri = resource->m_uri;
    payload = OCRepPayloadCreate();
    signature = prop->signature;
    prop->GetAnnotation("org.alljoyn.Bus.Type.Name", signature);
    ToOCPayload(payload, propName, GetPropType(prop, arg->v_variant.val), arg->v_variant.val,
            signature.c_str());
    DoResource(OC_REST_POST, uri, resource->m_addrs, payload, msg, &VirtualBusObject::SetPropCB);
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualBusObject::SetPropCB(ajn::Message &msg, OCRepPayload *payload, void *ctx)
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

void VirtualBusObject::GetAllProps(const ajn::InterfaceDescription::Member *member,
                                   ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p", this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    std::vector<Resource>::iterator resource;
    std::string uri;
    const char *ifaceName = msg->GetArg(0)->v_string.str;
    if (m_resources.size() > 1)
    {
        goto error;
    }
    resource = FindResourceFromUri(m_resources, GetPath());
    if (resource == m_resources.end())
    {
        goto error;
    }
    if (!resource->m_resources.empty())
    {
        resource = FindResourceFromType(resource->m_resources, ToOCName(ifaceName));
        if (resource == resource->m_resources.end())
        {
            goto error;
        }
    }
    uri = resource->m_uri;
    if (resource->m_rts.size() > 1)
    {
        uri += qcc::String("?rt=") + msg->GetArg(0)->v_string.str;
    }
    DoResource(OC_REST_GET, uri, resource->m_addrs, NULL, msg, &VirtualBusObject::GetAllPropsCB);
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualBusObject::GetAllPropsCB(ajn::Message &msg, OCRepPayload *payload, void *ctx)
{
    (void) ctx;
    LOG(LOG_INFO, "[%p]", this);

    const char *ifaceName = msg->GetArg(0)->v_string.str;
    OCRepPayloadValue value;
    memset(&value, 0, sizeof(value));
    value.type = OCREP_PROP_OBJECT;
    value.obj = payload;
    ajn::MsgArg arg;
    std::string valueType = std::string("[") + ifaceName + ".Properties" + "]";
    ToAJMsgArg(&arg, "a{sv}", &value, valueType.c_str());
    QStatus status = MethodReply(msg, &arg, 1);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* This must be called with m_mutex held. */
void VirtualBusObject::DoResource(OCMethod method, std::string uri, std::vector<OCDevAddr> addrs,
        OCRepPayload *payload, ajn::Message &msg, DoResourceHandler cb, void *ctx)
{
    LOG(LOG_INFO, "[%p] method=%d,uri=%s,payload=%p", this, method, uri.c_str(), payload);

    DoResourceContext *context = new DoResourceContext(this, cb, ctx, msg);
    OCCallbackData cbData;
    cbData.cb = VirtualBusObject::DoResourceCB;
    cbData.context = context;
    cbData.cd = NULL;
    OCStackResult result = ::DoResource(&context->m_handle, method, uri.c_str(), addrs,
            (OCPayload *) payload, &cbData, NULL, 0);
    if (result == OC_STACK_OK)
    {
        ++m_pending;
    }
    else
    {
        LOG(LOG_ERR, "DoResource - %d", result);
        delete context;
        QStatus status = MethodReply(msg, ER_FAIL);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
        }
    }
}

OCStackApplicationResult VirtualBusObject::DoResourceCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    DoResourceContext *context = reinterpret_cast<DoResourceContext *>(ctx);
    LOG(LOG_INFO, "[%p] ctx=%p,handle=%p,response=%p,{payload=%p,result=%d}", context->m_obj, ctx,
            handle, response, response ? response->payload : 0, response ? response->result : 0);

    std::lock_guard<std::mutex> lock(context->m_obj->m_mutex);
    if (response && (response->result > OC_STACK_RESOURCE_CHANGED))
    {
        QStatus status;
        const char *description;
        OCDiagnosticPayload *payload = (OCDiagnosticPayload *) response->payload;
        if (payload && (response->payload->type == PAYLOAD_TYPE_DIAGNOSTIC) &&
                IsValidErrorName(payload->message, &description) && (*description == ':'))
        {
            std::string name(payload->message, description - payload->message);
            ++description;
            while (isblank(*description))
            {
                ++description;
            }
            status = context->m_obj->MethodReply(context->m_msg, name.c_str(), description);
        }
        else
        {
            int code = 0;
            switch (response->result)
            {
                case OC_STACK_INVALID_QUERY:
                    code = 400;
                    break;
                case OC_STACK_UNAUTHORIZED_REQ:
                    code = 401;
                    break;
                case OC_STACK_INVALID_OPTION:
                    code = 402;
                    break;
                case OC_STACK_FORBIDDEN_REQ:
                    code = 403;
                    break;
                case OC_STACK_NO_RESOURCE:
                    code = 404;
                    break;
                case OC_STACK_TOO_LARGE_REQ:
                    code = 413;
                    break;
                case OC_STACK_INTERNAL_SERVER_ERROR:
                    code = 500;
                    break;
                case OC_STACK_COMM_ERROR:
                    code = 504;
                    break;
                case OC_STACK_GATEWAY_TIMEOUT:
                    code = 504;
                    break;
                default:
                    break;
            }
            if (code)
            {
                std::string name("org.openconnectivity.Error.");
                name = name + std::to_string(code);
                status = context->m_obj->MethodReply(context->m_msg, name.c_str());
            }
            else
            {
                status = context->m_obj->MethodReply(context->m_msg, ER_FAIL);
            }
        }
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
        }
    }
    else if (!response || !response->payload)
    {
        QStatus status = context->m_obj->MethodReply(context->m_msg, ER_FAIL);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
        }
    }
    else
    {
        OCRepPayload *payload = (OCRepPayload *) response->payload;
        (context->m_obj->*(context->m_cb))(context->m_msg, payload, context->m_context);
    }
    --context->m_obj->m_pending;
    context->m_obj->m_cond.notify_one();
    delete context;
    return OC_STACK_DELETE_TRANSACTION;
}
