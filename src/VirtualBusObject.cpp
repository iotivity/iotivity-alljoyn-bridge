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

#include "Name.h"
#include "Payload.h"
#include "Plugin.h"
#include <alljoyn/BusAttachment.h>
#include "ocpayload.h"
#include <algorithm>
#include <assert.h>

struct VirtualBusObject::ObserveContext
{
    public:
        ObserveContext(VirtualBusObject *obj, const char *iface)
            : m_obj(obj), m_iface(iface), m_handle(NULL), m_result(OC_STACK_KEEP_TRANSACTION) { }
        static void Deleter(void *ctx)
        {
            LOG(LOG_INFO, "[%p]", ctx);
            ObserveContext *context = reinterpret_cast<ObserveContext *>(ctx);
            {
                std::lock_guard<std::mutex> lock(context->m_obj->m_mutex);
                context->m_obj->m_observes.erase(context);
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
        DoResourceContext(VirtualBusObject *obj, VirtualBusObject::DoResourceHandler cb, ajn::Message &msg)
            : m_obj(obj), m_cb(cb), m_msg(msg), m_handle(NULL) { }
        VirtualBusObject *m_obj;
        VirtualBusObject::DoResourceHandler m_cb;
        ajn::Message m_msg;
        OCDoHandle m_handle;
};

VirtualBusObject::VirtualBusObject(ajn::BusAttachment *bus, const char *uri,
        const std::vector<OCDevAddr> &devAddrs)
    : ajn::BusObject(uri), m_bus(bus), m_devAddrs(devAddrs), m_pending(0)
{
    LOG(LOG_INFO, "[%p] bus=%p,uri=%s", this, bus, uri);
}

VirtualBusObject::~VirtualBusObject()
{
    LOG(LOG_INFO, "[%p]", this);

    std::unique_lock<std::mutex> lock(m_mutex);
    while (m_pending > 0 && !m_observes.empty())
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
        OCStackResult result = Cancel(handle, OC_LOW_QOS);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "Cancel - %d", result);
        }
    }
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
        if (std::find(m_ifaces.begin(), m_ifaces.end(), iface) == m_ifaces.end())
        {
            status = ajn::BusObject::AddInterface(*iface, ajn::BusObject::ANNOUNCED);
            if (status == ER_OK)
            {
                m_ifaces.push_back(iface);
            }
            else
            {
                LOG(LOG_ERR, "AddInterface - %s", QCC_StatusText(status));
            }
        }
    }
    return status;
}

void VirtualBusObject::Observe()
{
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    bool multipleRts = m_ifaces.size() > 1;
    for (const ajn::InterfaceDescription *iface : m_ifaces)
    {
        qcc::String uri = GetPath();
        if (multipleRts)
        {
            uri += qcc::String("?rt=") + iface->GetName();
        }
        ObserveContext *context = new ObserveContext(this, iface->GetName());
        OCCallbackData cbData;
        cbData.cb = VirtualBusObject::ObserveCB;
        cbData.context = context;
        cbData.cd = ObserveContext::Deleter;
        OCStackResult result = ::DoResource(&context->m_handle, OC_REST_OBSERVE, uri.c_str(),
                m_devAddrs, NULL, &cbData, NULL, 0);
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
        OCStackResult result = Cancel(context->m_handle, OC_HIGH_QOS);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "Cancel - %d", result);
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
        ToAJMsgArg(&args[1], "a{sv}", &value);
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
    bool multipleRts = m_ifaces.size() > 1;
    qcc::String uri = GetPath();
    if (multipleRts)
    {
        uri += qcc::String("?rt=") + msg->GetArg(0)->v_string.str;
    }
    DoResource(OC_REST_GET, uri.c_str(), NULL, msg, &VirtualBusObject::GetPropCB);
}

/* Called with m_mutex held. */
void VirtualBusObject::GetPropCB(ajn::Message &msg, OCRepPayload *payload)
{
    LOG(LOG_INFO, "[%p]", this);

    ajn::MsgArg arg;
    for (OCRepPayloadValue *value = payload->values; value; value = value->next)
    {
        if (!strcmp(value->name, msg->GetArg(1)->v_string.str))
        {
            ToAJMsgArg(&arg, "v", value);
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
    qcc::String uri = GetPath();
    OCRepPayload *payload = OCRepPayloadCreate();
    const char *name = msg->GetArg(1)->v_string.str;
    const ajn::MsgArg *arg = msg->GetArg(2);
    ToOCPayload(payload, name, arg, arg->Signature().c_str());
    DoResource(OC_REST_POST, uri.c_str(), payload, msg, &VirtualBusObject::SetPropCB);
}

/* Called with m_mutex held. */
void VirtualBusObject::SetPropCB(ajn::Message &msg, OCRepPayload *payload)
{
    (void) payload;
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
    bool multipleRts = m_ifaces.size() > 1;
    qcc::String uri = GetPath();
    if (multipleRts)
    {
        uri += qcc::String("?rt=") + msg->GetArg(0)->v_string.str;
    }
    DoResource(OC_REST_GET, uri.c_str(), NULL, msg, &VirtualBusObject::GetAllPropsCB);
}

/* Called with m_mutex held. */
void VirtualBusObject::GetAllPropsCB(ajn::Message &msg, OCRepPayload *payload)
{
    LOG(LOG_INFO, "[%p]", this);

    OCRepPayloadValue value;
    memset(&value, 0, sizeof(value));
    value.type = OCREP_PROP_OBJECT;
    value.obj = payload;
    ajn::MsgArg arg;
    ToAJMsgArg(&arg, "a{sv}", &value);
    QStatus status = MethodReply(msg, &arg, 1);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* This must be called with m_mutex held. */
void VirtualBusObject::DoResource(OCMethod method, const char *uri, OCRepPayload *payload,
                                  ajn::Message &msg, DoResourceHandler cb)
{
    LOG(LOG_INFO, "[%p] method=%d,uri=%s,payload=%p", this, method, uri, payload);

    DoResourceContext *context = new DoResourceContext(this, cb, msg);
    OCCallbackData cbData;
    cbData.cb = VirtualBusObject::DoResourceCB;
    cbData.context = context;
    cbData.cd = NULL;
    OCStackResult result = ::DoResource(&context->m_handle, method, uri, m_devAddrs,
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
        (context->m_obj->*(context->m_cb))(context->m_msg, payload);
    }
    --context->m_obj->m_pending;
    context->m_obj->m_cond.notify_one();
    delete context;
    return OC_STACK_DELETE_TRANSACTION;
}
