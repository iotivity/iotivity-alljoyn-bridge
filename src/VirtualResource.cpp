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

#include "Bridge.h"
#include "Name.h"
#include "Payload.h"
#include "Plugin.h"
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include <qcc/StringUtil.h>
#include "Signature.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include <algorithm>
#include <assert.h>

enum
{
    NONE = 0,
    READ = (1 << 0),
    READWRITE = (1 << 1),
};

static bool TranslateInterface(const char *ifaceName)
{
    return !(strstr(ifaceName, "org.freedesktop.DBus") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.About") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.Bus") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.Security") == ifaceName ||
             strstr(ifaceName, "org.allseen.Introspectable") == ifaceName);
}

static std::string GetPropName(const ajn::InterfaceDescription::Member *member, std::string argName)
{
    return GetResourceTypeName(member->iface, member->name) + argName;
}

static std::string GetPropName(const ajn::InterfaceDescription *iface, std::string memberName)
{
    return GetResourceTypeName(iface, memberName);
}

VirtualResource *VirtualResource::Create(Bridge *bridge, ajn::BusAttachment *bus,
        const char *name, ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion)
{
    VirtualResource *resource = new VirtualResource(bridge, bus, name, sessionId, path,
            ajSoftwareVersion);
    OCStackResult result = resource->Create();
    if (result != OC_STACK_OK)
    {
        delete resource;
        resource = NULL;
    }
    return resource;
}

VirtualResource::VirtualResource(Bridge *bridge, ajn::BusAttachment *bus, const char *name,
        ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion)
    : ajn::ProxyBusObject(*bus, name, path, sessionId)
    , m_bridge(bridge)
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

    OCStackResult result;
    {
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
            if (!TranslateInterface(ifaceName))
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
                std::string rt = GetResourceTypeName(ifaces[i], value);
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
                std::string rt = GetResourceTypeName(ifaces[i], members[j]->name);
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
            if (!numProps && !numMembers)
            {
                std::string rt = GetResourceTypeName(ifaceName);
                m_rts[rt] |= READ;
            }
        }
        delete[] ifaces;
        if (m_rts.empty())
        {
            LOG(LOG_INFO, "No translatable interfaces");
            return;
        }

        const ajn::InterfaceDescription *iface = m_bus->GetInterface(
            ::ajn::org::freedesktop::DBus::Properties::InterfaceName);
        assert(iface);
        const ajn::InterfaceDescription::Member *signal = iface->GetSignal("PropertiesChanged");
        assert(signal);
        m_bus->RegisterSignalHandler(this,
                static_cast<ajn::MessageReceiver::SignalHandler>(&VirtualResource::SignalCB),
                signal, GetPath().c_str());

        std::map<std::string, uint8_t>::iterator rt = m_rts.begin();
        result = CreateResource(GetPath().c_str(), rt->first.c_str(),
                (access & READ) ?  OC_RSRVD_INTERFACE_READ : OC_RSRVD_INTERFACE_READ_WRITE,
                VirtualResource::EntityHandlerCB, this,
                OC_DISCOVERABLE | OC_OBSERVABLE);
        for (; (rt != m_rts.end()) && (result == OC_STACK_OK); ++rt)
        {
            result = AddResourceType(GetPath().c_str(), rt->first.c_str());
        }
        if ((access & (READ | READWRITE)) == (READ | READWRITE))
        {
            result = ::AddInterface(GetPath().c_str(), OC_RSRVD_INTERFACE_READ_WRITE);
        }
        if (result == OC_STACK_OK)
        {
            LOG(LOG_INFO, "[%p] Created VirtualResource uri=%s",
                    this, GetPath().c_str());
        }
        else
        {
            LOG(LOG_ERR, "[%p] Create VirtualResource - %d", this, result);
        }
    }
    if (result == OC_STACK_OK)
    {
        m_bridge->RDPublish();
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
    if (query.find(OC_RSRVD_RESOURCE_TYPE) != query.end())
    {
        rt = query[OC_RSRVD_RESOURCE_TYPE];
    }
    return rt;
}

static uint8_t GetAccess(std::map<std::string, std::string> &query,
                         uint8_t accessFlags)
{
    uint8_t access = NONE;
    if (query.find(OC_RSRVD_INTERFACE) != query.end())
    {
        if (query[OC_RSRVD_INTERFACE] == OC_RSRVD_INTERFACE_DEFAULT)
        {
            access = READWRITE;
        }
        else if (query[OC_RSRVD_INTERFACE] == OC_RSRVD_INTERFACE_READ_WRITE)
        {
            access = READWRITE;
        }
        else if (query[OC_RSRVD_INTERFACE] == OC_RSRVD_INTERFACE_READ)
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
            qcc::String propName = GetPropName(iface, key);
            success = ToOCPayload(payload, propName.c_str(), entry->v_dictEntry.val->v_variant.val,
                                  signature.c_str());
        }
    }
    return success;
}

static std::string NextArgName(const char *&argNames, size_t i)
{
    std::string name;
    const char *argName = argNames;
    if (*argNames)
    {
        while (*argNames && *argNames != ',')
        {
            ++argNames;
        }
        if (argNames > argName)
        {
            name = std::string(argName, argNames - argName);
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


struct VirtualResource::GetAllBaselineContext
{
    std::string m_ajSoftwareVersion;
    const ajn::InterfaceDescription **m_ifaces;
    size_t m_numIfaces;
    size_t m_iface;
    OCRepPayload *m_payload;
    OCEntityHandlerResponse *m_response;
    GetAllBaselineContext(std::string ajSoftwareVersion, const ajn::InterfaceDescription **ifaces,
                          size_t numIfaces,
                          OCRepPayload *payload, OCEntityHandlerRequest *request)
        : m_ajSoftwareVersion(ajSoftwareVersion), m_ifaces(ifaces), m_numIfaces(numIfaces), m_iface(0),
          m_payload(payload), m_response(NULL)
    {
        m_response = (OCEntityHandlerResponse *) calloc(1, sizeof(OCEntityHandlerResponse));
        m_response->requestHandle = request->requestHandle;
        m_response->resourceHandle = request->resource;
    }
    ~GetAllBaselineContext()
    {
        delete[] m_ifaces;
        OCRepPayloadDestroy(m_payload);
        free(m_response);
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

OCStackResult VirtualResource::SetMemberPayload(OCRepPayload *payload,
        const char *ifaceName, const char *memberName)
{
    const ajn::InterfaceDescription *iface = m_bus->GetInterface(ifaceName);
    if (!iface)
    {
        return OC_STACK_ERROR;
    }
    const ajn::InterfaceDescription::Member *member = iface->GetMember(memberName);
    if (!member)
    {
        return OC_STACK_ERROR;
    }
    qcc::String signature = member->signature + member->returnSignature;
    size_t numArgs = CountCompleteTypes(signature.c_str());
    const char *argNames = member->argNames.c_str();
    std::string propName = GetPropName(member, "validity");
    OCRepPayloadSetPropBool(payload, propName.c_str(), false);
    for (size_t i = 0; i < numArgs; ++i)
    {
        propName = GetPropName(member, NextArgName(argNames, i));
        OCRepPayloadSetNull(payload, propName.c_str());
    }
    return OC_STACK_OK;
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
        LOG(LOG_INFO, "Unsupported interface requested - %s", queryMap[OC_RSRVD_INTERFACE].c_str());
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
                if (queryMap[OC_RSRVD_INTERFACE] == OC_RSRVD_INTERFACE_DEFAULT)
                {
                    size_t numIfaces = resource->GetInterfaces(NULL, 0);
                    const ajn::InterfaceDescription **ifaces = new const ajn::InterfaceDescription*[numIfaces];
                    resource->GetInterfaces(ifaces, numIfaces);
                    OCRepPayload *payload = resource->CreatePayload();
                    GetAllBaselineContext *context = new GetAllBaselineContext(resource->m_ajSoftwareVersion, ifaces,
                            numIfaces,
                            payload, request);
                    QStatus status = resource->GetAllBaseline(context);
                    if (status == ER_OK)
                    {
                        result = OC_EH_OK;
                    }
                    else
                    {
                        LOG(LOG_ERR, "GetAllBaseline - %s", QCC_StatusText(status));
                        delete context;
                        result = OC_EH_ERROR;
                    }
                }
                else if (memberName == "const" || memberName == "true" || memberName == "false"
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
                    if (resource->SetMemberPayload(payload, ifaceName.c_str(), memberName.c_str()) != OC_STACK_OK)
                    {
                        result = OC_EH_ERROR;
                        break;
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
                    qcc::String propName = GetPropName(member, "validity");
                    OCRepPayload *payload = (OCRepPayload *) request->payload;
                    for (OCRepPayloadValue *value = payload->values; value; value = value->next)
                    {
                        if (propName == value->name && (value->type != OCREP_PROP_BOOL || !value->b))
                        {
                            success = false;
                            break;
                        }
                    }
                    const char *signature = member->signature.c_str();
                    const char *argSignature = signature;
                    const char *argNames = member->argNames.c_str();
                    for (size_t i = 0; success && i < numArgs; ++i)
                    {
                        ParseCompleteType(signature);
                        qcc::String sig(argSignature, signature - argSignature);
                        argSignature = signature;
                        std::string argName = NextArgName(argNames, i);
                        if (resource->m_ajSoftwareVersion >= "v16.10.00")
                        {
                            member->GetArgAnnotation(argName.c_str(), "org.alljoyn.Bus.Type.Name", sig);
                        }
                        qcc::String propName = GetPropName(member, argName);
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
                std::string propName = GetPropName(context->m_member, "validity");
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
                    std::string argName = NextArgName(argNames, numInArgs + i);
                    if (context->m_ajSoftwareVersion >= "v16.10.00")
                    {
                        context->m_member->GetArgAnnotation(argName.c_str(), "org.alljoyn.Bus.Type.Name", sig);
                    }
                    propName = GetPropName(context->m_member, argName);
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

    std::string valueName = context->m_value->name;
    std::string propName = GetMember(valueName);
    size_t numArgs = 3;
    ajn::MsgArg args[3];
    args[0].Set("s", context->m_iface->GetName());
    args[1].Set("s", propName.c_str());
    const ajn::InterfaceDescription::Property *property = context->m_iface->GetProperty(
                propName.c_str());
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
        std::string rt = GetResourceTypeName(msg->GetInterface(), msg->GetMemberName());
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
            std::string propName = GetPropName(member, "validity");
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
                std::string argName = NextArgName(argNames, i);
                if (m_ajSoftwareVersion >= "v16.10.00")
                {
                    member->GetArgAnnotation(argName.c_str(), "org.alljoyn.Bus.Type.Name", sig);
                }
                propName = GetPropName(member, argName);
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

/* Called with m_mutex held. */
QStatus VirtualResource::GetAllBaseline(GetAllBaselineContext *context)
{
    LOG(LOG_INFO, "[%p] context=%p",
        this, context);

    for (; (context->m_iface < context->m_numIfaces)
         && (context->m_response->ehResult == OC_EH_OK); ++context->m_iface)
    {
        const char *ifaceName = context->m_ifaces[context->m_iface]->GetName();
        if (!TranslateInterface(ifaceName))
        {
            continue;
        }
        size_t numProps = context->m_ifaces[context->m_iface]->GetProperties(NULL, 0);
        if (numProps)
        {
            ajn::MsgArg arg("s", ifaceName);
            QStatus status = MethodCallAsync(::ajn::org::freedesktop::DBus::Properties::InterfaceName, "GetAll",
                                             this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::GetAllBaselineCB),
                                             &arg, 1, context);
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                context->m_response->ehResult = OC_EH_ERROR;
            }
            break;
        }
    }
    if ((context->m_response->ehResult != OC_EH_OK) || (context->m_iface == context->m_numIfaces))
    {
        if (context->m_response->ehResult == OC_EH_OK)
        {
            for (size_t i = 0; (i < context->m_numIfaces) && (context->m_response->ehResult == OC_EH_OK); ++i)
            {
                const char *ifaceName = context->m_ifaces[i]->GetName();
                if (!TranslateInterface(ifaceName))
                {
                    continue;
                }
                size_t numMembers = context->m_ifaces[i]->GetMembers(NULL, 0);
                const ajn::InterfaceDescription::Member **members = new const
                ajn::InterfaceDescription::Member*[numMembers];
                context->m_ifaces[i]->GetMembers(members, numMembers);
                for (size_t j = 0; (j < numMembers) && (context->m_response->ehResult == OC_EH_OK); ++j)
                {
                    if (SetMemberPayload(context->m_payload, ifaceName, members[j]->name.c_str()) != OC_STACK_OK)
                    {
                        context->m_response->ehResult = OC_EH_ERROR;
                    }
                }
                delete[] members;
            }
        }
        /* Common properties */
        const char **rts = NULL;
        const char **ifs = NULL;
        if (context->m_response->ehResult == OC_EH_OK)
        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = {0, 0, 0};
            uint8_t nrts = 0;
            OCStackResult result = OCGetNumberOfResourceTypes(context->m_response->resourceHandle, &nrts);
            if (result != OC_STACK_OK)
            {
                context->m_response->ehResult = OC_EH_ERROR;
                goto exit;
            }
            dim[0] = nrts;
            rts = (const char **)OICMalloc(sizeof(const char *) * nrts);
            if (rts == NULL)
            {
                context->m_response->ehResult = OC_EH_ERROR;
                goto exit;
            }
            for (size_t i = 0; i < nrts; ++i)
            {
                rts[i] = OCGetResourceTypeName(context->m_response->resourceHandle, i);
            }
            if (!OCRepPayloadSetStringArray(context->m_payload, OC_RSRVD_RESOURCE_TYPE, (const char **)rts,
                                            dim))
            {
                context->m_response->ehResult = OC_EH_ERROR;
                goto exit;
            }
            uint8_t nifs = 0;
            result = OCGetNumberOfResourceInterfaces(context->m_response->resourceHandle, &nifs);
            if (result != OC_STACK_OK)
            {
                context->m_response->ehResult = OC_EH_ERROR;
                goto exit;
            }
            dim[0] = nifs;
            ifs = (const char **)OICMalloc(sizeof(const char *) * nifs);
            if (ifs == NULL)
            {
                context->m_response->ehResult = OC_EH_ERROR;
                goto exit;
            }
            for (size_t i = 0; i < nifs; ++i)
            {
                ifs[i] = OCGetResourceInterfaceName(context->m_response->resourceHandle, i);
            }
            if (!OCRepPayloadSetStringArray(context->m_payload, OC_RSRVD_INTERFACE, (const char **)ifs, dim))
            {
                context->m_response->ehResult = OC_EH_ERROR;
                goto exit;
            }
        }
exit:
        if (context->m_response->ehResult == OC_EH_OK)
        {
            context->m_response->payload = reinterpret_cast<OCPayload *>(context->m_payload);
        }
        OCStackResult doResult = DoResponse(context->m_response);
        if (doResult != OC_STACK_OK)
        {
            LOG(LOG_ERR, "DoResponse - %d", doResult);
        }
        OICFree(ifs);
        OICFree(rts);
        delete context;
    }
    return ER_OK;
}

void VirtualResource::GetAllBaselineCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    GetAllBaselineContext *context = reinterpret_cast<GetAllBaselineContext *>(ctx);
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            {
                const ajn::InterfaceDescription *iface = context->m_ifaces[context->m_iface];
                const ajn::MsgArg *dict = msg->GetArg(0);
                bool success = true;
                size_t numEntries = dict->v_array.GetNumElements();
                for (size_t i = 0; success && i < numEntries; ++i)
                {
                    const ajn::MsgArg *entry = &dict->v_array.GetElements()[i];
                    const char *key = entry->v_dictEntry.key->v_string.str;
                    const ajn::InterfaceDescription::Property *property = iface->GetProperty(key);
                    if (property)
                    {
                        /*
                         * Annotations prior to v16.10.00 are not guaranteed to
                         * appear in the order they were specified, so are
                         * unreliable.
                         */
                        qcc::String signature = property->signature;
                        if (context->m_ajSoftwareVersion >= "v16.10.00")
                        {
                            property->GetAnnotation("org.alljoyn.Bus.Type.Name", signature);
                        }
                        qcc::String propName = GetPropName(iface, key);
                        success = ToOCPayload(context->m_payload, propName.c_str(), entry->v_dictEntry.val->v_variant.val,
                                              signature.c_str());
                    }
                }
                if (success)
                {
                    context->m_response->ehResult = OC_EH_OK;
                }
                else
                {
                    context->m_response->ehResult = OC_EH_ERROR;
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
    ++context->m_iface;
    QStatus status = GetAllBaseline(context);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "GetAllBaseline - %s", QCC_StatusText(status));
        delete context;
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
