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
#include "Introspection.h"
#include "Log.h"
#include "Name.h"
#include "Payload.h"
#include "Plugin.h"
#include "Resource.h"
#include <alljoyn/AllJoynStd.h>
#include <alljoyn/BusAttachment.h>
#include "Signature.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include <algorithm>
#include <assert.h>

VirtualResource *VirtualResource::Create(ajn::BusAttachment *bus, const char *name,
        ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion,
        CreateCB createCb, void *createContext)
{
    VirtualResource *resource = new VirtualResource(bus, name, sessionId, path, ajSoftwareVersion,
            createCb, createContext);
    OCStackResult result = resource->Create();
    if (result != OC_STACK_OK)
    {
        delete resource;
        resource = NULL;
    }
    return resource;
}

VirtualResource::VirtualResource(ajn::BusAttachment *bus, const char *name,
        ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion,
        CreateCB createCb, void *createContext)
    : ajn::ProxyBusObject(*bus, name, path, sessionId), m_bus(bus), m_createCb(createCb),
    m_createContext(createContext), m_ajSoftwareVersion(ajSoftwareVersion)
{
    LOG(LOG_INFO, "[%p] bus=%p,name=%s,sessionId=%d,path=%s,ajSoftwareVersion=%s", this, bus, name,
            sessionId, path, ajSoftwareVersion);
}

VirtualResource::~VirtualResource()
{
    LOG(LOG_INFO, "[%p] name=%s,path=%s", this, GetUniqueName().c_str(), GetPath().c_str());

    for (auto &handle : m_handles)
    {
        OCDeleteResource(handle);
    }
}

OCStackResult VirtualResource::Create()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    const ajn::InterfaceDescription *iface = m_bus->GetInterface(
        ::ajn::org::freedesktop::DBus::Introspectable::InterfaceName);
    assert(iface);
    AddInterface(*iface);
    const ajn::InterfaceDescription::Member *member = iface->GetMember("Introspect");
    assert(member);
    QStatus status = MethodCallAsync(*member, this,
            static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::IntrospectCB), NULL, 0,
            NULL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "Introspect - %s", QCC_StatusText(status));
        return OC_STACK_ERROR;
    }
    return OC_STACK_OK;
}

void VirtualResource::IntrospectCB(ajn::Message &msg, void *ctx)
{
    (void) ctx;
    LOG(LOG_INFO, "[%p]", this);

    OCStackResult result;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        switch (msg->GetType())
        {
            case ajn::MESSAGE_METHOD_RET:
                {
                    LOG(LOG_INFO, "[%p] %s", this, msg->GetArg(0)->ToString().c_str());
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
        result = CreateResources();
    }
    if (result == OC_STACK_OK)
    {
        m_createCb(m_createContext);
    }
}

OCStackResult VirtualResource::CreateResource(std::string path, uint8_t props)
{
    std::map<std::string, ResourceType>::iterator rt;
    /* Determine interfaces supported by matching resource types */
    uint8_t access = NONE;
    for (rt = m_rts.begin(); rt != m_rts.end(); ++rt)
    {
        if (rt->second.m_props != props)
        {
            continue;
        }
        access |= rt->second.m_access;
    }
    /* Add the resource */
    for (rt = m_rts.begin(); rt != m_rts.end(); ++rt)
    {
        if (rt->second.m_props != props)
        {
            continue;
        }
        break;
    }
    if (rt == m_rts.end())
    {
        return OC_STACK_OK;
    }
    OCResourceHandle handle;
    OCStackResult result = ::CreateResource(&handle, path.c_str(), rt->first.c_str(),
            (access & READ) ? OC_RSRVD_INTERFACE_READ : OC_RSRVD_INTERFACE_READ_WRITE,
            VirtualResource::EntityHandlerCB, this, props);
    for (++rt; (rt != m_rts.end()) && (result == OC_STACK_OK); ++rt)
    {
        if (rt->second.m_props != props)
        {
            continue;
        }
        result = OCBindResourceTypeToResource(handle, rt->first.c_str());
    }
    if ((access & (READ | READWRITE)) == (READ | READWRITE))
    {
        result = OCBindResourceInterfaceToResource(handle, OC_RSRVD_INTERFACE_READ_WRITE);
    }
    if (result == OC_STACK_OK)
    {
        LOG(LOG_INFO, "[%p] Created VirtualResource uri=%s", this, path.c_str());
        m_handles.push_back(handle);
    }
    else
    {
        LOG(LOG_ERR, "[%p] Create VirtualResource - %d", this, result);
    }
    return result;
}

OCStackResult VirtualResource::CreateResources()
{
    OCStackResult result;
    size_t numIfaces = GetInterfaces(NULL, 0);
    const ajn::InterfaceDescription **ifaces = new const ajn::InterfaceDescription*[numIfaces];
    GetInterfaces(ifaces, numIfaces);
    for (size_t i = 0; i < numIfaces; ++i)
    {
        const char *ifaceName = ifaces[i]->GetName();
        if (!TranslateInterface(ifaceName))
        {
            continue;
        }
        uint8_t secure = 0;
        ajn::InterfaceSecurityPolicy secPolicy = ifaces[i]->GetSecurityPolicy();
        if (IsSecure() || (secPolicy == ajn::AJ_IFC_SECURITY_REQUIRED) ||
                (IsSecure() && (secPolicy != ajn::AJ_IFC_SECURITY_OFF)))
        {
            secure = OC_SECURE;
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
            if (value == "true" || value == "invalidates")
            {
                m_rts[rt].m_props |= (OC_OBSERVABLE | secure);
            }
            else
            {
                m_rts[rt].m_props |= secure;
            }
            switch (props[j]->access)
            {
                case ajn::PROP_ACCESS_RW:
                case ajn::PROP_ACCESS_WRITE:
                    m_rts[rt].m_access |= READWRITE;
                    break;
                case ajn::PROP_ACCESS_READ:
                    m_rts[rt].m_access |= READ;
                    break;
            }
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
                m_rts[rt].m_access |= READWRITE;
                m_rts[rt].m_props |= secure;
            }
            else if (members[j]->memberType == ajn::MESSAGE_SIGNAL)
            {
                m_rts[rt].m_access |= READ;
                m_rts[rt].m_props |= (OC_OBSERVABLE | secure);
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
            m_rts[rt].m_access |= READ;
            m_rts[rt].m_props |= secure;
        }
    }
    delete[] ifaces;
    if (m_rts.empty())
    {
        LOG(LOG_INFO, "No translatable interfaces");
        return OC_STACK_NO_RESOURCE;
    }

    const ajn::InterfaceDescription *iface = m_bus->GetInterface(
        ::ajn::org::freedesktop::DBus::Properties::InterfaceName);
    assert(iface);
    const ajn::InterfaceDescription::Member *signal = iface->GetSignal("PropertiesChanged");
    assert(signal);
    m_bus->RegisterSignalHandler(this,
            static_cast<ajn::MessageReceiver::SignalHandler>(&VirtualResource::SignalCB),
            signal, GetPath().c_str());

    /* First see if we need more than one resource to translate this object */
    std::map<std::string, ResourceType>::iterator rt = m_rts.begin();
    uint8_t props = rt->second.m_props;
    for (; rt != m_rts.end(); ++rt)
    {
        if (rt->second.m_props != props)
        {
            /* Need multiple resources */
            break;
        }
    }
    /* Next create the resource(s) */
    if (rt == m_rts.end())
    {
        result = CreateResource(GetPath(), OC_DISCOVERABLE | props);
    }
    else
    {
#ifdef __WITH_DTLS__
        static const uint8_t ps[] = { OC_DISCOVERABLE,
                                      OC_DISCOVERABLE | OC_OBSERVABLE,
                                      OC_DISCOVERABLE | OC_SECURE,
                                      OC_DISCOVERABLE | OC_OBSERVABLE | OC_SECURE,
                                    };
#else
        static const uint8_t ps[] = { OC_DISCOVERABLE,
                                      OC_DISCOVERABLE | OC_OBSERVABLE,
                                    };
#endif
        for (size_t i = 0; i < (sizeof(ps) / sizeof(ps[0])); ++i)
        {
            std::string path = GetPath() + "/" + std::to_string(ps[i]);
            result = CreateResource(path, ps[i]);
        }
        OCResourceHandle handle;
        result = ::CreateResource(&handle, GetPath().c_str(), "oic.r.alljoynobject", OC_RSRVD_INTERFACE_LL,
                VirtualResource::CollectionHandlerCB, this, OC_DISCOVERABLE | OC_OBSERVABLE);
        if (result == OC_STACK_OK)
        {
            result = OCBindResourceTypeToResource(handle, OC_RSRVD_RESOURCE_TYPE_COLLECTION);
        }
        if (result == OC_STACK_OK)
        {
            LOG(LOG_INFO, "[%p] Created VirtualResource uri=%s", this, GetPath().c_str());
            m_handles.push_back(handle);
        }
        else
        {
            LOG(LOG_ERR, "[%p] Create VirtualResource - %d", this, result);
        }
    }
    return result;
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

OCDiagnosticPayload *VirtualResource::CreatePayload(ajn::Message &msg,
        OCEntityHandlerResult *ehResult)
{
    *ehResult = OC_EH_ERROR;
    OCDiagnosticPayload *payload = NULL;
    qcc::String description;
    const char *name = msg->GetErrorName(&description);
    if (name)
    {
        static const std::string ErrorPrefix = "org.openconnectivity.Error.";
        std::string message = name;
        if (message.compare(0, ErrorPrefix.size(), ErrorPrefix) == 0)
        {
            message = message.substr(ErrorPrefix.size());
            char *endptr = NULL;
            unsigned long code = strtoul(message.c_str(), &endptr, 10);
            if (*endptr == '\0')
            {
                *ehResult = (OCEntityHandlerResult) code;
            }
        }
        message = message + ": " + description;
        payload = OCDiagnosticPayloadCreate(message.c_str());
    }
    return payload;
}

OCRepPayload *VirtualResource::CreatePayload(const char *uri)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    if (payload)
    {
        OCRepPayloadSetUri(payload, uri);
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
    std::string rt = GetResourceType(queryMap, OCGetResourceTypeName(request->resource, 0));
    uint8_t access = GetAccess(queryMap, resource->m_rts[rt].m_access);
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
            Observation key(request->resource, request->query);
            std::vector<OCObservationId>::iterator it = std::find(resource->m_observers[key].begin(),
                    resource->m_observers[key].end(), request->obsInfo.obsId);
            if (it == resource->m_observers[key].end())
            {
                LOG(LOG_INFO, "[%p] Register observer rt=%s %d", resource, rt.c_str(), request->obsInfo.obsId);
                resource->m_observers[key].push_back(request->obsInfo.obsId);
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
            for (std::map<Observation, std::vector<OCObservationId>>::iterator it =
                     resource->m_observers.begin(); it != resource->m_observers.end(); ++it)
            {
                for (std::vector<OCObservationId>::iterator jt = it->second.begin(); jt != it->second.end(); ++jt)
                {
                    if (*jt == request->obsInfo.obsId)
                    {
                        LOG(LOG_INFO, "[%p] Deregister observer %s %d", resource, it->first.m_query.c_str(),
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
    const char *uri = OCGetResourceUri(request->resource);
    switch (request->method)
    {
        case OC_REST_GET:
            {
                std::string ifaceName = ::GetInterface(rt);
                std::string memberName = GetMember(rt);
                if (queryMap[OC_RSRVD_INTERFACE] == OC_RSRVD_INTERFACE_DEFAULT)
                {
                    size_t numIfaces = resource->GetInterfaces(NULL, 0);
                    const ajn::InterfaceDescription **ifaces =
                            new const ajn::InterfaceDescription*[numIfaces];
                    resource->GetInterfaces(ifaces, numIfaces);
                    OCRepPayload *payload = resource->CreatePayload(uri);
                    GetAllBaselineContext *context =
                            new GetAllBaselineContext(resource->m_ajSoftwareVersion, ifaces,
                                    numIfaces, payload, request);
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
                    QStatus status = resource->MethodCallAsync(*member, resource,
                            static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::MethodReturnCB),
                            &arg, 1, context, DefaultCallTimeout, resource->GetMethodCallFlags(ifaceName.c_str()));
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
                    OCRepPayload *payload = resource->CreatePayload(uri);
                    if (resource->SetMemberPayload(payload, ifaceName.c_str(), memberName.c_str()) != OC_STACK_OK)
                    {
                        result = OC_EH_ERROR;
                        break;
                    }
                    result = OC_EH_OK;
                    response.ehResult = result;
                    response.payload = reinterpret_cast<OCPayload *>(payload);
                    OCStackResult doResult = OCDoResponse(&response);
                    if (doResult != OC_STACK_OK)
                    {
                        LOG(LOG_ERR, "OCDoResponse - %d", doResult);
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
                    if (payload)
                    {
                        for (OCRepPayloadValue *value = payload->values; value; value = value->next)
                        {
                            if (propName == value->name && (value->type != OCREP_PROP_BOOL || !value->b))
                            {
                                success = false;
                                break;
                            }
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
                        assert(payload);
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
    const char *uri = OCGetResourceUri(context->m_response->resourceHandle);
    OCStackResult result = OC_STACK_ERROR;
    OCPayload *payload = NULL;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            payload = (OCPayload*) CreatePayload(uri);
            bool success;
            if (!strcmp(context->m_member->iface->GetName(),
                        ajn::org::freedesktop::DBus::Properties::InterfaceName) &&
                !strcmp(context->m_member->name.c_str(), "GetAll"))
            {
                const ajn::InterfaceDescription *iface =
                        m_bus->GetInterface(::GetInterface(context->m_rt).c_str());
                assert(iface);
                success = ToFilteredOCPayload((OCRepPayload *) payload,
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
                OCRepPayloadSetPropBool((OCRepPayload *) payload, propName.c_str(), true);
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
                        context->m_member->GetArgAnnotation(argName.c_str(),
                                "org.alljoyn.Bus.Type.Name", sig);
                    }
                    propName = GetPropName(context->m_member, argName);
                    success = ToOCPayload((OCRepPayload *) payload, propName.c_str(), &outArgs[i],
                            sig.c_str());
                }
            }
            if (success)
            {
                context->m_response->ehResult = OC_EH_OK;
            }
            else
            {
                context->m_response->ehResult = OC_EH_ERROR;
                OCPayloadDestroy(payload);
                payload = NULL;
            }
            break;
        case ajn::MESSAGE_ERROR:
            payload = (OCPayload*) CreatePayload(msg, &context->m_response->ehResult);
            break;
        default:
            assert(0);
            break;
    }
    context->m_response->payload = payload;
    result = OCDoResponse(context->m_response);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "OCDoResponse - %d", result);
        OCPayloadDestroy(payload);
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
    return MethodCallAsync(::ajn::org::freedesktop::DBus::Properties::InterfaceName, "Set", this,
            static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::SetCB), args, numArgs,
            context, DefaultCallTimeout, GetMethodCallFlags(context->m_iface->GetName()));
}

void VirtualResource::SetCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p] ctx=%p",
        this, ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    SetContext *context = reinterpret_cast<SetContext *>(ctx);
    const char *uri = OCGetResourceUri(context->m_response->resourceHandle);
    OCStackResult result = OC_STACK_ERROR;
    OCRepPayload *payload = NULL;
    switch (msg->GetType())
    {
        case ajn::MESSAGE_METHOD_RET:
            context->m_value = context->m_value->next;
            if (!context->m_value)
            {
                payload = CreatePayload(uri);
                context->m_response->ehResult = OC_EH_OK;
                context->m_response->payload = reinterpret_cast<OCPayload *>(payload);
                result = OCDoResponse(context->m_response);
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
            context->m_response->payload = (OCPayload *) CreatePayload(msg,
                    &context->m_response->ehResult);
            result = OCDoResponse(context->m_response);
            delete context;
            break;
        default:
            assert(0);
            break;
    }
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "OCDoResponse - %d", result);
        OCRepPayloadDestroy(payload);
    }
}

void VirtualResource::SignalCB(const ajn::InterfaceDescription::Member *member, const char *path,
                               ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p,path=%s", this, member, path);

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
            QStatus status = MethodCallAsync(::ajn::org::freedesktop::DBus::Properties::InterfaceName,
                    "GetAll", this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::GetAllInvalidatedCB),
                    msg->GetArg(0), 1, NULL, DefaultCallTimeout, GetMethodCallFlags(msg->GetArg(0)->v_string.str));
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
            }
        }
        else
        {
            for (std::map<Observation, std::vector<OCObservationId>>::iterator it = m_observers.begin();
                 it != m_observers.end(); ++it)
            {
                const char *uri = OCGetResourceUri(it->first.m_resource);
                OCRepPayload *payload = CreatePayload(uri);
                std::map<std::string, std::string> queryMap = ParseQuery(it->first.m_query.c_str());
                std::string rt = GetResourceType(queryMap,
                        OCGetResourceTypeName(it->first.m_resource, 0));
                uint8_t access = GetAccess(queryMap, m_rts[rt].m_access);
                const ajn::InterfaceDescription *iface = m_bus->GetInterface(::GetInterface(rt).c_str());
                assert(iface);
                bool success = ToFilteredOCPayload(payload, m_ajSoftwareVersion, iface,
                        GetMember(rt).c_str(), access, msg->GetArg(1));
                if (success && payload->values)
                {
                    OCStackResult result = OCNotifyListOfObservers(it->first.m_resource,
                            &it->second[0], it->second.size(), payload, OC_HIGH_QOS);
                    if (result == OC_STACK_OK)
                    {
                        LOG(LOG_INFO, "[%p] Notify observers rt=%s", this, it->first.m_query.c_str());
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
    else
    {
        std::string rt = GetResourceTypeName(msg->GetInterface(), msg->GetMemberName());
        for (std::map<Observation, std::vector<OCObservationId>>::iterator it = m_observers.begin();
             it != m_observers.end(); ++it)
        {
            std::map<std::string, std::string> queryMap = ParseQuery(it->first.m_query.c_str());
            if (rt != GetResourceType(queryMap, OCGetResourceTypeName(it->first.m_resource, 0)))
            {
                continue;
            }
            const char *uri = OCGetResourceUri(it->first.m_resource);
            const ajn::InterfaceDescription *iface = GetInterface(msg->GetInterface());
            assert(iface);
            const ajn::InterfaceDescription::Member *member = iface->GetMember(msg->GetMemberName());
            assert(member);
            const char *argNames = member->argNames.c_str();
            size_t numArgs;
            const ajn::MsgArg *args;
            msg->GetArgs(numArgs, args);
            std::string propName = GetPropName(member, "validity");
            OCRepPayload *payload = CreatePayload(uri);
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
                OCStackResult result = OCNotifyListOfObservers(it->first.m_resource, &it->second[0],
                        it->second.size(), payload, OC_HIGH_QOS);
                if (result == OC_STACK_OK)
                {
                    LOG(LOG_INFO, "[%p] Notify observers rt=%s", this, it->first.m_query.c_str());
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
    for (std::map<Observation, std::vector<OCObservationId>>::iterator it = m_observers.begin();
         it != m_observers.end(); ++it)
    {
        const char *uri = OCGetResourceUri(it->first.m_resource);
        OCRepPayload *payload = CreatePayload(uri);
        std::map<std::string, std::string> queryMap = ParseQuery(it->first.m_query.c_str());
        std::string rt = GetResourceType(queryMap,
                OCGetResourceTypeName(it->first.m_resource, 0));
        uint8_t access = GetAccess(queryMap, m_rts[rt].m_access);
        const ajn::InterfaceDescription *iface = m_bus->GetInterface(::GetInterface(rt).c_str());
        assert(iface);
        bool success = ToFilteredOCPayload(payload, m_ajSoftwareVersion, iface,
                GetMember(rt).c_str(), access, msg->GetArg(0));
        if (success && payload->values)
        {
            OCStackResult result = OCNotifyListOfObservers(it->first.m_resource, &it->second[0],
                    it->second.size(), payload, OC_HIGH_QOS);
            if (result == OC_STACK_OK)
            {
                LOG(LOG_INFO, "[%p] Notify observers rt=%s", this, it->first.m_query.c_str());
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
            QStatus status = MethodCallAsync(::ajn::org::freedesktop::DBus::Properties::InterfaceName,
                    "GetAll", this, static_cast<ajn::MessageReceiver::ReplyHandler>(&VirtualResource::GetAllBaselineCB),
                    &arg, 1, context, DefaultCallTimeout, GetMethodCallFlags(ifaceName));
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
            bool observable = OCGetResourceProperties(context->m_response->resourceHandle) &
                    OC_OBSERVABLE;
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
                    if ((observable && (members[j]->memberType == ajn::MESSAGE_SIGNAL)) ||
                            (!observable && (members[j]->memberType == ajn::MESSAGE_METHOD_CALL)))
                    {
                        if (SetMemberPayload(context->m_payload, ifaceName, members[j]->name.c_str()) != OC_STACK_OK)
                        {
                            context->m_response->ehResult = OC_EH_ERROR;
                        }
                    }
                }
                delete[] members;
            }
        }
        /* Common properties */
        if (context->m_response->ehResult == OC_EH_OK)
        {
            if (!SetResourceTypes(context->m_payload, context->m_response->resourceHandle) ||
                    !SetInterfaces(context->m_payload, context->m_response->resourceHandle))
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
        OCStackResult doResult = OCDoResponse(context->m_response);
        if (doResult != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCDoResponse - %d", doResult);
        }
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
                if (OCGetResourceProperties(context->m_response->resourceHandle) & OC_OBSERVABLE)
                {
                    success = ToFilteredOCPayload(context->m_payload, context->m_ajSoftwareVersion,
                            iface, "true", READ, dict) &&
                            ToFilteredOCPayload(context->m_payload, context->m_ajSoftwareVersion,
                                    iface, "invalidates", READ, dict);
                }
                else
                {
                    success = ToFilteredOCPayload(context->m_payload, context->m_ajSoftwareVersion,
                            iface, "const", READ, dict) &&
                            ToFilteredOCPayload(context->m_payload, context->m_ajSoftwareVersion,
                                    iface, "false", READ, dict);
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
            context->m_response->payload = (OCPayload *) CreatePayload(msg,
                    &context->m_response->ehResult);
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

uint8_t VirtualResource::GetMethodCallFlags(const char *ifaceName)
{
    uint8_t flags = 0;
    const ajn::InterfaceDescription *iface = m_bus->GetInterface(ifaceName);
    if (iface)
    {
        ajn::InterfaceSecurityPolicy security = iface->GetSecurityPolicy();
        if ((security == ajn::AJ_IFC_SECURITY_REQUIRED) ||
                (IsSecure() && (security != ajn::AJ_IFC_SECURITY_OFF)))
        {
            flags |= ajn::ALLJOYN_FLAG_ENCRYPTED;
        }
    }
    return flags;
}

OCEntityHandlerResult VirtualResource::CollectionHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p", ctx, flag, request, ctx);

    VirtualResource *resource = reinterpret_cast<VirtualResource *>(ctx);
    std::lock_guard<std::mutex> lock(resource->m_mutex);

    OCEntityHandlerResult result = OC_EH_OK;
    OCRepPayload *payload = NULL;
    OCRepPayload **links = NULL;
    size_t linksDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload *link = NULL;
    char **rtArray = NULL;
    size_t rtDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    char **itfArray = NULL;
    size_t itfDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                payload = ::CreatePayload(request->resource, request->query);
                if (!payload)
                {
                    result = OC_EH_ERROR;
                    goto exit;
                }
                std::vector<OCResourceHandle> handles;
                for (size_t i = 0; i < resource->m_handles.size(); ++i)
                {
                    if (resource->GetPath() == OCGetResourceUri(resource->m_handles[i]))
                    {
                        continue;
                    }
                    handles.push_back(resource->m_handles[i]);
                }
                if (!SetLinks(payload, &handles[0], handles.size()))
                {
                    result = OC_EH_ERROR;
                    goto exit;
                }
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(payload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "OCDoResponse - %d", doResult);
                    result = OC_EH_ERROR;
                    goto exit;
                }
                payload = NULL;
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }

exit:
    if (itfArray)
    {
        for (size_t i = 0; i < itfDim[0]; ++i)
        {
            OICFree(itfArray[i]);
        }
        OICFree(itfArray);
    }
    if (rtArray)
    {
        for (size_t i = 0; i < rtDim[0]; ++i)
        {
            OICFree(rtArray[i]);
        }
        OICFree(rtArray);
    }
    OCRepPayloadDestroy(link);
    if (links)
    {
        for (size_t i = 0; i < linksDim[0]; ++i)
        {
            OCRepPayloadDestroy(links[i]);
        }
        OICFree(links);
    }
    OCRepPayloadDestroy(payload);
    return result;
}
