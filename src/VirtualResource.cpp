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

#include "VirtualResource.h"

#include "Payload.h"
#include "Plugin.h"
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <qcc/StringUtil.h>
#include "Signature.h"
#include "ocpayload.h"
#include <algorithm>

enum
{
    NONE = 0,
    READ = (1 << 0),
    READWRITE = (1 << 1),
};

VirtualResource *VirtualResource::Create(ajn::BusAttachment *bus,
        const char *name, ajn::SessionId sessionId, const char *path,
        const char *ajSoftwareVersion)
{
    VirtualResource *resource = new VirtualResource(bus, name, sessionId, path, ajSoftwareVersion);
    OCStackResult result = resource->Create();
    if (result != OC_STACK_OK)
    {
        delete resource;
        resource = NULL;
    }
    return resource;
}

VirtualResource::VirtualResource(ajn::BusAttachment *bus,
                                 const char *name, ajn::SessionId sessionId, const char *path,
                                 const char *ajSoftwareVersion)
    : ajn::ProxyBusObject(*bus, name, path, sessionId)
    , m_bus(bus)
    , m_ajSoftwareVersion(ajSoftwareVersion)
{
    LOG(LOG_INFO, "[%p] bus=%p,name=%s,sessionId=%d,path=%s,ajSoftwareVersion=%s",
        this, bus, name, sessionId, path, ajSoftwareVersion);
}

VirtualResource::~VirtualResource()
{
    LOG(LOG_INFO, "[%p] name=%s,path=%s", this,
        GetUniqueName().c_str(), GetPath().c_str());

    DestroyResource(GetPath().c_str());
}

OCStackResult VirtualResource::Create()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const ajn::InterfaceDescription *iface = m_bus->GetInterface(
                ::ajn::org::allseen::Introspectable::InterfaceName);
    assert(iface);
    AddInterface(*iface);
    const ajn::InterfaceDescription::Member *member = iface->GetMember("IntrospectWithDescription");
    assert(member);
    ajn::MsgArg arg("s", "");
    QStatus status = MethodCallAsync(*member,
                                     this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::IntrospectCB),
                                     &arg, 1, NULL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "IntrospectWithDescription - %s", QCC_StatusText(status));
        return OC_STACK_ERROR;
    }
    return OC_STACK_OK;
}

void VirtualResource::IntrospectCB(ajn::Message &msg, void *ctx)
{
    (void) ctx;
    LOG(LOG_INFO, "[%p]",
        this);

    std::lock_guard<std::mutex> lock(m_mutex);
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                QStatus status = ParseXml(msg->GetArg(0)->v_string.str);
                if (status != ER_OK)
                {
                    LOG(LOG_ERR, "ParseXml - %s", QCC_StatusText(status));
                    return;
                }
                break;
            }
        case ajn::MESSAGE_ERROR:
            {
                qcc::String errorMsg;
                const char *errorName = msg->GetErrorName(&errorMsg);
                LOG(LOG_ERR, "[%p] IntrospectCB %s %s", this, errorName, errorMsg.c_str());
                return;
            }
        default:
            assert(0);
            break;
    }

    uint8_t access = 0;
    size_t numIfaces = GetInterfaces(NULL, 0);
    const ajn::InterfaceDescription **ifaces = new const ajn::InterfaceDescription*[numIfaces];
    GetInterfaces(ifaces, numIfaces);
    for (size_t i = 0; i < numIfaces; ++i)
    {
        const char *ifaceName = ifaces[i]->GetName();
        LOG(LOG_INFO, "%s ifaceName=%s", GetPath().c_str(), ifaceName);
        if (strstr(ifaceName, "org.freedesktop.DBus") == ifaceName ||
            strstr(ifaceName, "org.alljoyn.Bus") == ifaceName ||
            strstr(ifaceName, "org.alljoyn.Security") == ifaceName ||
            strstr(ifaceName, "org.allseen.Introspectable") == ifaceName)
        {
            continue;
        }
        size_t numProps = ifaces[i]->GetProperties(NULL, 0);
        const ajn::InterfaceDescription::Property **props = new const
        ajn::InterfaceDescription::Property*[numProps];
        ifaces[i]->GetProperties(props, numProps);
        for (size_t j = 0; j < numProps; ++j)
        {
            qcc::String value = (props[j]->name == "Version") ? "const" : "false";
            props[j]->GetAnnotation(::ajn::org::freedesktop::DBus::AnnotateEmitsChanged, value);
            std::string rt = std::string("x.") + ifaces[i]->GetName() + "." + value;
            switch (props[j]->access)
            {
                case ajn::PROP_ACCESS_RW:
                case ajn::PROP_ACCESS_WRITE:
                    m_rts[rt] |= READWRITE;
                    break;
                case ajn::PROP_ACCESS_READ:
                    m_rts[rt] |= READ;
                    break;
            }
            access |= m_rts[rt];
        }
        delete[] props;
        size_t numMembers = ifaces[i]->GetMembers(NULL, 0);
        const ajn::InterfaceDescription::Member **members = new const
        ajn::InterfaceDescription::Member*[numMembers];
        ifaces[i]->GetMembers(members, numMembers);
        for (size_t j = 0; j < numMembers; ++j)
        {
            std::string rt = std::string("x.") + ifaces[i]->GetName() + "." + members[j]->name.c_str();
            if (members[j]->memberType == ajn::MESSAGE_METHOD_CALL)
            {
                m_rts[rt] |= READWRITE;
                access |= m_rts[rt];
            }
            else if (members[j]->memberType == ajn::MESSAGE_SIGNAL)
            {
                m_rts[rt] |= READ;
                access |= m_rts[rt];
                m_bus->RegisterSignalHandler(this,
                                             static_cast<ajn::MessageReceiver::SignalHandler>(&VirtualResource::SignalCB),
                                             members[j], GetPath().c_str());
            }
        }
        delete[] members;
        size_t numAnnotations = ifaces[i]->GetAnnotations();
        qcc::String *names = new qcc::String[numAnnotations];
        qcc::String *values = new qcc::String[numAnnotations];
        ifaces[i]->GetAnnotations(names, values, numAnnotations);
        for (size_t j = 0; j < numAnnotations; ++j)
        {
            if (names[j].find("org.alljoyn.Bus.Struct.") == 0)
            {
                size_t pos = sizeof("org.alljoyn.Bus.Struct.") - 1;
                size_t dot = names[j].find(".", pos);
                if (dot == qcc::String::npos)
                {
                    continue;
                }
                qcc::String structName = "[" + names[j].substr(pos, dot - pos) + "]";
                pos = dot + sizeof(".Field.") - 1;
                dot = names[j].find(".", pos);
                if (dot == qcc::String::npos)
                {
                    continue;
                }
                qcc::String fieldName = names[j].substr(pos, dot - pos);
                Types::m_structs[structName].push_back(Types::Field(fieldName, values[j]));
            }
        }
        delete[] names;
        delete[] values;
    }
    delete[] ifaces;

    const ajn::InterfaceDescription *iface = m_bus->GetInterface(
                ::ajn::org::freedesktop::DBus::Properties::InterfaceName);
    assert(iface);
    const ajn::InterfaceDescription::Member *signal = iface->GetSignal("PropertiesChanged");
    assert(signal);
    m_bus->RegisterSignalHandler(this,
                                 static_cast<ajn::MessageReceiver::SignalHandler>(&VirtualResource::SignalCB),
                                 signal, GetPath().c_str());

    std::map<std::string, uint8_t>::iterator rt = m_rts.begin();
    OCStackResult result = CreateResource(GetPath().c_str(), rt->first.c_str(),
                                          (access & READ) ?  "oic.if.r" : "oic.if.rw",
                                          VirtualResource::EntityHandlerCB, this,
                                          OC_DISCOVERABLE | OC_OBSERVABLE);
    for (; (rt != m_rts.end()) && (result == OC_STACK_OK); ++rt)
    {
        result = AddResourceType(GetPath().c_str(), rt->first.c_str());
    }
    if ((access & (READ | READWRITE)) == (READ | READWRITE))
    {
        result = ::AddInterface(GetPath().c_str(), "oic.if.rw");
    }
    if (result == OC_STACK_OK)
    {
        LOG(LOG_INFO, "[%p] Created VirtualResource uri=%s",
            this, GetPath().c_str());
    }
}

static std::map<std::string, std::string> ParseQuery(const char *query)
{
    std::map<std::string, std::string> queryMap;
    if (query)
    {
        std::string queryStr = query;
        std::string::size_type beg, end = 0;
        beg = 0;
        while (end != std::string::npos)
        {
            std::string key, value;
            end = queryStr.find('=', beg);
            if (end == std::string::npos)
            {
                key = queryStr.substr(beg);
            }
            else
            {
                key = queryStr.substr(beg, end - beg);
                beg = end + 1;
                end = queryStr.find_first_of("&;", beg);
                if (end == std::string::npos)
                {
                    value = queryStr.substr(beg);
                }
                else
                {
                    value = queryStr.substr(beg, end - beg);
                    beg = end + 1;
                }
            }
            queryMap[key] = value;
        }
    }
    return queryMap;
}

static std::string GetResourceType(std::map<std::string, std::string> &query,
                                   std::string defaultResourceType)
{
    std::string rt = defaultResourceType;
    if (query.find("rt") != query.end())
    {
        rt = query["rt"];
    }
    return rt;
}

static uint8_t GetAccess(std::map<std::string, std::string> &query,
                         uint8_t accessFlags)
{
    uint8_t access = NONE;
    if (query.find("if") != query.end())
    {
        if (query["if"] == "oic.if.rw")
        {
            access = READWRITE;
        }
        else if (query["if"] == "oic.if.r")
        {
            access = READ;
        }
    }
    else
    {
        if (accessFlags & READ)
        {
            access = READ;
        }
        else if (accessFlags & READWRITE)
        {
            access = READWRITE;
        }
    }
    return access;
}

static std::string GetInterface(std::string &rt)
{
    std::string::size_type b, e;
    b = rt.find('.') + 1;
    e = rt.rfind('.');
    return rt.substr(b, e - b);
}

static std::string GetMember(std::string &rt)
{
    return rt.substr(rt.rfind('.') + 1);
}

/* Filter properties based on resource type and interface requested. */
static bool ToFilteredOCPayload(OCRepPayload *payload,
                                const std::string ajSoftwareVersion,
                                const ajn::InterfaceDescription *iface,
                                const char *emitsChangedValue, uint8_t access,
                                const ajn::MsgArg *dict)
{
    bool success = true;
    size_t numEntries = dict->v_array.GetNumElements();
    for (size_t i = 0; success && i < numEntries; ++i)
    {
        const ajn::MsgArg *entry = &dict->v_array.GetElements()[i];
        const char *key = entry->v_dictEntry.key->v_string.str;
        const ajn::InterfaceDescription::Property *property = iface->GetProperty(key);
        if (property)
        {
            if ((access == READWRITE) &&
                (property->access == ajn::PROP_ACCESS_READ))
            {
                continue;
            }
            qcc::String emitsChanged = (property->name == "Version") ? "const" : "false";
            property->GetAnnotation(::ajn::org::freedesktop::DBus::AnnotateEmitsChanged, emitsChanged);
            if (strcmp(emitsChangedValue, emitsChanged.c_str()))
            {
                continue;
            }
            /*
             * Annotations prior to v16.10.00 are not guaranteed to
             * appear in the order they were specified, so are
             * unreliable.
             */
            qcc::String signature = property->signature;
            if (ajSoftwareVersion >= "v16.10.00")
            {
                property->GetAnnotation("org.alljoyn.Bus.Type.Name", signature);
            }
            success = ToOCPayload(payload, key, entry->v_dictEntry.val->v_variant.val, signature.c_str());
        }
    }
    return success;
}

static qcc::String NextArgName(const char *&argNames, size_t i)
{
    qcc::String name;
    const char *argName = argNames;
    if (*argNames)
    {
        while (*argNames && *argNames != ',')
        {
            ++argNames;
        }
        if (argNames > argName)
        {
            name = qcc::String(argName, argNames - argName);
        }
        else
        {
            name = "arg" + qcc::U32ToString(i);
        }
        if (*argNames == ',')
        {
            ++argNames;
        }
    }
    else
    {
        name = "arg" + qcc::U32ToString(i);
    }
    return name;
}

struct MethodCallContext
{
    std::string m_ajSoftwareVersion;
    std::string m_rt;
    uint8_t m_access;
    const ajn::InterfaceDescription::Member *m_member;
    OCEntityHandlerResponse *m_response;
    MethodCallContext(std::string ajSoftwareVersion, std::string &rt, uint8_t access,
                      const ajn::InterfaceDescription::Member *member,
                      OCEntityHandlerRequest *request)
        : m_ajSoftwareVersion(ajSoftwareVersion), m_rt(rt), m_access(access), m_member(member),
          m_response(NULL)
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

struct VirtualResource::SetContext
{
    std::string m_ajSoftwareVersion;
    OCRepPayload *m_payload;
    OCRepPayloadValue *m_value;
    const ajn::InterfaceDescription *m_iface;
    OCEntityHandlerResponse *m_response;
    SetContext(std::string ajSoftwareVersion, OCEntityHandlerRequest *request,
               const ajn::InterfaceDescription *iface)
        : m_ajSoftwareVersion(ajSoftwareVersion),
          m_payload(OCRepPayloadClone((OCRepPayload *) request->payload)), m_value(m_payload->values),
          m_iface(iface), m_response(NULL)
    {
        m_response = (OCEntityHandlerResponse *) calloc(1, sizeof(OCEntityHandlerResponse));
        m_response->requestHandle = request->requestHandle;
        m_response->resourceHandle = request->resource;
    }
    ~SetContext()
    {
        free(m_response);
        OCPayloadDestroy((OCPayload *) m_payload);
    }
};

OCRepPayload *VirtualResource::CreatePayload()
{
    OCRepPayload *payload = OCRepPayloadCreate();
    if (payload)
    {
        OCRepPayloadSetUri(payload, GetPath().c_str());
    }
    return payload;
}

OCEntityHandlerResult VirtualResource::EntityHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p",
        ctx, flag, request, ctx);

    VirtualResource *resource = reinterpret_cast<VirtualResource *>(ctx);
    std::lock_guard<std::mutex> lock(resource->m_mutex);
    std::map<std::string, std::string> queryMap = ParseQuery(request->query);
    std::string rt = GetResourceType(queryMap, resource->m_rts.begin()->first);
    uint8_t access = GetAccess(queryMap, resource->m_rts[rt]);
    if (!access)
    {
        LOG(LOG_INFO, "Unsupported interface requested - %s", queryMap["if"].c_str());
        return OC_EH_ERROR;
    }
    if (flag & OC_OBSERVE_FLAG)
    {
        if (request->obsInfo.action == OC_OBSERVE_REGISTER)
        {
            std::string ifaceName = ::GetInterface(rt);
            const ajn::InterfaceDescription *iface = resource->GetInterface(ifaceName.c_str());
            if (!iface)
            {
                LOG(LOG_INFO, "[%p] Observe invalid iface %s", ifaceName.c_str());
                return OC_EH_ERROR;
            }
            std::vector<OCObservationId>::iterator it = std::find(resource->m_observers[request->query].begin(),
                    resource->m_observers[request->query].end(), request->obsInfo.obsId);
            if (it == resource->m_observers[request->query].end())
            {
                LOG(LOG_INFO, "[%p] Register observer rt=%s %d", resource, rt.c_str(), request->obsInfo.obsId);
                resource->m_observers[request->query].push_back(request->obsInfo.obsId);
            }
            /* Add match rule for sessionless signal */
            std::string memberName = GetMember(rt);
            const ajn::InterfaceDescription::Member *signal = iface->GetSignal(memberName.c_str());
            if (signal && signal->isSessionlessSignal)
            {
                std::string rule = "type='signal',sender='" + std::string(resource->GetUniqueName().c_str()) +
                                   "',interface='" +
                                   ifaceName + "',member='" + memberName + "',sessionless='t'";
                QStatus status = resource->m_bus->AddMatchAsync(rule.c_str(), resource);
                if (status == ER_OK)
                {
                    resource->m_matchRules[request->obsInfo.obsId] = rule;
                }
                else
                {
                    LOG(LOG_ERR, "AddMatchAsync - %s", QCC_StatusText(status));
                }
            }
        }
        else if (request->obsInfo.action == OC_OBSERVE_DEREGISTER)
        {
            for (std::map<std::string, std::vector<OCObservationId>>::iterator it =
                     resource->m_observers.begin(); it != resource->m_observers.end(); ++it)
            {
                for (std::vector<OCObservationId>::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
                {
                    if (*jt == request->obsInfo.obsId)
                    {
                        LOG(LOG_INFO, "[%p] Deregister observer %s %d", resource, it->first.c_str(),
                            request->obsInfo.obsId);
                        if (!resource->m_matchRules[request->obsInfo.obsId].empty())
                        {
                            QStatus status = resource->m_bus->RemoveMatchAsync(
                                                 resource->m_matchRules[request->obsInfo.obsId].c_str(), resource);
                            if (status != ER_OK)
                            {
                                LOG(LOG_ERR, "RemoveMatchAsync - %s", QCC_StatusText(status));
                            }
                        }
                        it->second.erase(jt);
                        goto handleRequest;
                    }
                }
            }
        }
    }
handleRequest:
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                std::string ifaceName = ::GetInterface(rt);
                std::string memberName = GetMember(rt);
                if (memberName == "const" || memberName == "true" || memberName == "false"
                    || memberName == "invalidates")
                {
                    ajn::MsgArg arg("s", ifaceName.c_str());
                    const ajn::InterfaceDescription *iface = resource->m_bus->GetInterface(
                                ::ajn::org::freedesktop::DBus::Properties::InterfaceName);
                    assert(iface);
                    const ajn::InterfaceDescription::Member *member = iface->GetMember("GetAll");
                    assert(member);
                    MethodCallContext *context = new MethodCallContext(resource->m_ajSoftwareVersion, rt, access,
                            member, request);
                    QStatus status = resource->MethodCallAsync(*member,
                                     resource, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::MethodReturnCB),
                                     &arg, 1, context);
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
                }
                else
                {
                    OCEntityHandlerResponse response;
                    memset(&response, 0, sizeof(response));
                    response.requestHandle = request->requestHandle;
                    response.resourceHandle = request->resource;
                    OCRepPayload *payload = resource->CreatePayload();
                    const ajn::InterfaceDescription *iface = resource->m_bus->GetInterface(ifaceName.c_str());
                    if (!iface)
                    {
                        result = OC_EH_ERROR;
                        break;
                    }
                    const ajn::InterfaceDescription::Member *member = iface->GetMember(memberName.c_str());
                    if (!member)
                    {
                        result = OC_EH_ERROR;
                        break;
                    }
                    qcc::String signature = member->signature + member->returnSignature;
                    size_t numArgs = CountCompleteTypes(signature.c_str());
                    const char *argNames = member->argNames.c_str();
                    qcc::String propName = member->name + "-validity";
                    OCRepPayloadSetPropBool(payload, propName.c_str(), false);
                    for (size_t i = 0; i < numArgs; ++i)
                    {
                        propName = member->name + "-" + NextArgName(argNames, i);
                        OCRepPayloadSetNull(payload, propName.c_str());
                    }
                    result = OC_EH_OK;
                    response.ehResult = result;
                    response.payload = reinterpret_cast<OCPayload *>(payload);
                    OCStackResult doResult = DoResponse(&response);
                    if (doResult != OC_STACK_OK)
                    {
                        LOG(LOG_ERR, "DoResponse - %d", doResult);
                        OCRepPayloadDestroy(payload);
                    }
                }
                break;
            }
        case OC_REST_POST:
            {
                if ((access & READWRITE) == 0)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                std::string ifaceName = ::GetInterface(rt);
                std::string memberName = GetMember(rt);
                const ajn::InterfaceDescription *iface = resource->GetInterface(ifaceName.c_str());
                if (!iface)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                if (memberName == "const")
                {
                    result = OC_EH_METHOD_NOT_ALLOWED;
                }
                else if (memberName == "true" || memberName == "false" || memberName == "invalidates")
                {
                    if (!request->payload || request->payload->type != PAYLOAD_TYPE_REPRESENTATION)
                    {
                        result = OC_EH_ERROR;
                        break;
                    }
                    OCRepPayload *payload = (OCRepPayload *) request->payload;
                    if (!payload->values)
                    {
                        result = OC_EH_ERROR;
                        break;
                    }
                    SetContext *context = new SetContext(resource->m_ajSoftwareVersion, request, iface);
                    QStatus status = resource->Set(context);
                    if (status == ER_OK)
                    {
                        result = OC_EH_OK;
                    }
                    else
                    {
                        LOG(LOG_ERR, "Set - %s", QCC_StatusText(status));
                        delete context;
                        result = OC_EH_ERROR;
                    }
                }
                else
                {
                    const ajn::InterfaceDescription::Member *member = iface->GetMethod(memberName.c_str());
                    if (!member)
                    {
                        result = OC_EH_ERROR;
                        break;
                    }
                    bool success = true;
                    size_t numArgs = CountCompleteTypes(member->signature.c_str());
                    ajn::MsgArg *args = NULL;
                    if (numArgs)
                    {
                        args = new ajn::MsgArg[numArgs];
                        if (!request->payload || request->payload->type != PAYLOAD_TYPE_REPRESENTATION)
                        {
                            result = OC_EH_ERROR;
                            break;
                        }
                    }
                    const char *signature = member->signature.c_str();
                    const char *argSignature = signature;
                    const char *argNames = member->argNames.c_str();
                    for (size_t i = 0; i < numArgs; ++i)
                    {
                        ParseCompleteType(signature);
                        qcc::String sig(argSignature, signature - argSignature);
                        argSignature = signature;
                        qcc::String argName = NextArgName(argNames, i);
                        if (resource->m_ajSoftwareVersion >= "v16.10.00")
                        {
                            member->GetArgAnnotation(argName.c_str(), "org.alljoyn.Bus.Type.Name", sig);
                        }
                        qcc::String propName = member->name + "-" + argName;
                        OCRepPayload *payload = (OCRepPayload *) request->payload;
                        for (OCRepPayloadValue *value = payload->values; value; value = value->next)
                        {
                            if (propName == value->name)
                            {
                                success = ToAJMsgArg(&args[i], sig.c_str(), value);
                                break;
                            }
                        }
                    }
                    if (success)
                    {
                        MethodCallContext *context = new MethodCallContext(resource->m_ajSoftwareVersion, rt, access,
                                member, request);
                        QStatus status = resource->MethodCallAsync(*member,
                                         resource, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::MethodReturnCB),
                                         args, numArgs, context);
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
                    }
                    else
                    {
                        result = OC_EH_ERROR;
                    }
                    delete[] args;
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

void VirtualResource::MethodReturnCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    MethodCallContext *context = reinterpret_cast<MethodCallContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    OCRepPayload *payload = NULL;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            payload = CreatePayload();
            bool success;
            if (!strcmp(context->m_member->iface->GetName(),
                        ajn::org::freedesktop::DBus::Properties::InterfaceName) &&
                !strcmp(context->m_member->name.c_str(), "GetAll"))
            {
                const ajn::InterfaceDescription *iface = m_bus->GetInterface(::GetInterface(context->m_rt).c_str());
                assert(iface);
                success = ToFilteredOCPayload(payload,
                                              m_ajSoftwareVersion, iface,
                                              GetMember(context->m_rt).c_str(), context->m_access,
                                              msg->GetArg(0));
            }
            else
            {
                size_t numInArgs = CountCompleteTypes(context->m_member->signature.c_str());
                size_t numOutArgs;
                const ajn::MsgArg *outArgs;
                msg->GetArgs(numOutArgs, outArgs);
                qcc::String propName = context->m_member->name + "-validity";
                OCRepPayloadSetPropBool(payload, propName.c_str(), true);
                const char *argNames = context->m_member->argNames.c_str();
                for (size_t i = 0; i < numInArgs; ++i)
                {
                    NextArgName(argNames, i);
                }
                success = true;
                const char *signature = context->m_member->returnSignature.c_str();
                const char *argSignature = signature;
                for (size_t i = 0; success && i < numOutArgs; ++i)
                {
                    ParseCompleteType(signature);
                    qcc::String sig(argSignature, signature - argSignature);
                    argSignature = signature;
                    qcc::String argName = NextArgName(argNames, numInArgs + i);
                    if (context->m_ajSoftwareVersion >= "v16.10.00")
                    {
                        context->m_member->GetArgAnnotation(argName.c_str(), "org.alljoyn.Bus.Type.Name", sig);
                    }
                    propName = context->m_member->name + "-" + argName;
                    success = ToOCPayload(payload, propName.c_str(), &outArgs[i], sig.c_str());
                }
            }
            if (success)
            {
                context->m_response->ehResult = OC_EH_OK;
            }
            else
            {
                context->m_response->ehResult = OC_EH_ERROR;
                OCRepPayloadDestroy(payload);
                payload = NULL;
            }
            break;
        case ajn::MESSAGE_ERROR:
            context->m_response->ehResult = OC_EH_ERROR;
            break;
        default:
            assert(0);
            break;
    }
    context->m_response->payload = reinterpret_cast<OCPayload *>(payload);
    result = DoResponse(context->m_response);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResponse - %d", result);
        OCRepPayloadDestroy(payload);
    }
    delete context;
}

/* Called with m_mutex held. */
QStatus VirtualResource::Set(SetContext *context)
{
    LOG(LOG_INFO, "[%p] context=%p",
        this, context);

    size_t numArgs = 3;
    ajn::MsgArg args[numArgs];
    args[0].Set("s", context->m_iface->GetName());
    args[1].Set("s", context->m_value->name);
    const ajn::InterfaceDescription::Property *property = context->m_iface->GetProperty(
                context->m_value->name);
    if (!property)
    {
        return ER_BUS_NO_SUCH_PROPERTY;
    }
    qcc::String signature = property->signature;
    if (context->m_ajSoftwareVersion >= "v16.10.00")
    {
        property->GetAnnotation("org.alljoyn.Bus.Type.Name", signature);
    }
    ajn::MsgArg value;
    if (!ToAJMsgArg(&value, signature.c_str(), context->m_value))
    {
        return ER_FAIL;
    }
    args[2].Set("v", &value);
    return MethodCallAsync(::ajn::org::freedesktop::DBus::Properties::InterfaceName, "Set",
                           this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::SetCB),
                           args, numArgs, context);
}

void VirtualResource::SetCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    SetContext *context = reinterpret_cast<SetContext *>(ctx);
    OCStackResult result = OC_STACK_ERROR;
    OCRepPayload *payload = NULL;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            context->m_value = context->m_value->next;
            if (!context->m_value)
            {
                payload = CreatePayload();
                context->m_response->ehResult = OC_EH_OK;
                context->m_response->payload = reinterpret_cast<OCPayload *>(payload);
                result = DoResponse(context->m_response);
                delete context;
                break;
            }
            else
            {
                QStatus status = Set(context);
                if (status == ER_OK)
                {
                    break;
                }
                /* FALLTHROUGH */
            }
        case ajn::MESSAGE_ERROR:
            {
                qcc::String errorMsg;
                const char *errorName = msg->GetErrorName(&errorMsg);
                LOG(LOG_INFO, "[%p] SetCB %s %s", this, errorName, errorMsg.c_str());
                context->m_response->ehResult = OC_EH_ERROR;
                context->m_response->payload = reinterpret_cast<OCPayload *>(payload);
                result = DoResponse(context->m_response);
                delete context;
                break;
            }
        default:
            assert(0);
            break;
    }
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResponse - %d", result);
        OCRepPayloadDestroy(payload);
    }
}

void VirtualResource::SignalCB(const ajn::InterfaceDescription::Member *member, const char *path,
                               ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p,path=%s",
        this, member, path);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_observers.empty())
    {
        LOG(LOG_INFO, "[%p] No observers", this);
        return;
    }
    if (!strcmp(msg->GetInterface(), ajn::org::freedesktop::DBus::Properties::InterfaceName) &&
        !strcmp(msg->GetMemberName(), "PropertiesChanged"))
    {
        if (msg->GetArg(2)->v_array.GetNumElements())
        {
            /* Get the values of the invalidated properties */
            QStatus status = MethodCallAsync(::ajn::org::freedesktop::DBus::Properties::InterfaceName, "GetAll",
                                             this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::GetAllInvalidatedCB),
                                             msg->GetArg(0), 1, NULL);
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
            }
        }
        else
        {
            for (std::map<std::string, std::vector<OCObservationId>>::iterator it = m_observers.begin();
                 it != m_observers.end(); ++it)
            {
                OCRepPayload *payload = CreatePayload();
                std::map<std::string, std::string> queryMap = ParseQuery(it->first.c_str());
                std::string rt = GetResourceType(queryMap, m_rts.begin()->first);
                uint8_t access = GetAccess(queryMap, m_rts[rt]);
                const ajn::InterfaceDescription *iface = m_bus->GetInterface(::GetInterface(rt).c_str());
                assert(iface);
                bool success = ToFilteredOCPayload(payload,
                                                   m_ajSoftwareVersion, iface,
                                                   GetMember(rt).c_str(), access,
                                                   msg->GetArg(1));
                if (success && payload->values)
                {
                    OCStackResult result = NotifyListOfObservers(GetPath().c_str(),
                                           &it->second[0], it->second.size(),
                                           payload);
                    if (result == OC_STACK_OK)
                    {
                        LOG(LOG_INFO, "[%p] Notify observers rt=%s", this, it->first.c_str());
                    }
                    else
                    {
                        LOG(LOG_ERR, "[%p] Notify observers - %d", this, result);
                        OCRepPayloadDestroy(payload);
                    }
                }
            }
        }
    }
    else
    {
        std::string rt = std::string("x.") + msg->GetInterface() + "." + msg->GetMemberName();
        for (std::map<std::string, std::vector<OCObservationId>>::iterator it = m_observers.begin();
             it != m_observers.end(); ++it)
        {
            std::map<std::string, std::string> queryMap = ParseQuery(it->first.c_str());
            if (rt != GetResourceType(queryMap, m_rts.begin()->first))
            {
                continue;
            }
            const ajn::InterfaceDescription *iface = GetInterface(msg->GetInterface());
            assert(iface);
            const ajn::InterfaceDescription::Member *member = iface->GetMember(msg->GetMemberName());
            assert(member);
            const char *argNames = member->argNames.c_str();
            size_t numArgs;
            const ajn::MsgArg *args;
            msg->GetArgs(numArgs, args);
            qcc::String propName = member->name + "-validity";
            OCRepPayload *payload = CreatePayload();
            OCRepPayloadSetPropBool(payload, propName.c_str(), true);
            bool success = true;
            const char *signature = member->signature.c_str();
            const char *argSignature = signature;
            for (size_t i = 0; success && i < numArgs; ++i)
            {
                ParseCompleteType(signature);
                qcc::String sig(argSignature, signature - argSignature);
                argSignature = signature;
                qcc::String argName = NextArgName(argNames, i);
                if (m_ajSoftwareVersion >= "v16.10.00")
                {
                    member->GetArgAnnotation(argName.c_str(), "org.alljoyn.Bus.Type.Name", sig);
                }
                propName = member->name + "-" + argName;
                success = ToOCPayload(payload, propName.c_str(), &args[i], sig.c_str());
            }
            if (success)
            {
                OCStackResult result = NotifyListOfObservers(GetPath().c_str(),
                                       &it->second[0], it->second.size(),
                                       payload);
                if (result == OC_STACK_OK)
                {
                    LOG(LOG_INFO, "[%p] Notify observers rt=%s", this, it->first.c_str());
                }
                else
                {
                    LOG(LOG_ERR, "[%p] Notify observers - %d", this, result);
                    OCRepPayloadDestroy(payload);
                }
            }
            else
            {
                OCRepPayloadDestroy(payload);
            }
        }
    }
}

void VirtualResource::GetAllInvalidatedCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (std::map<std::string, std::vector<OCObservationId>>::iterator it = m_observers.begin();
         it != m_observers.end(); ++it)
    {
        OCRepPayload *payload = CreatePayload();
        std::map<std::string, std::string> queryMap = ParseQuery(it->first.c_str());
        std::string rt = GetResourceType(queryMap, m_rts.begin()->first);
        uint8_t access = GetAccess(queryMap, m_rts[rt]);
        const ajn::InterfaceDescription *iface = m_bus->GetInterface(::GetInterface(rt).c_str());
        assert(iface);
        bool success = ToFilteredOCPayload(payload,
                                           m_ajSoftwareVersion, iface,
                                           GetMember(rt).c_str(), access,
                                           msg->GetArg(0));
        if (success && payload->values)
        {
            OCStackResult result = NotifyListOfObservers(GetPath().c_str(),
                                   &it->second[0], it->second.size(),
                                   payload);
            if (result == OC_STACK_OK)
            {
                LOG(LOG_INFO, "[%p] Notify observers rt=%s", this, it->first.c_str());
            }
            else
            {
                LOG(LOG_ERR, "[%p] Notify observers - %d", this, result);
                OCRepPayloadDestroy(payload);
            }
        }
        else
        {
            OCRepPayloadDestroy(payload);
        }
    }
}

void VirtualResource::AddMatchCB(QStatus status, void *ctx)
{
    (void) ctx;
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "AddMatchCB - %s", QCC_StatusText(status));
    }
}

void VirtualResource::RemoveMatchCB(QStatus status, void *ctx)
{
    (void) ctx;
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "RemoveMatchCB - %s", QCC_StatusText(status));
    }
}
