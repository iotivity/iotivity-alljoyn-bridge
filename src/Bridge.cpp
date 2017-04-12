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

#include "Bridge.h"

#include "Introspection.h"
#include "Name.h"
#include "Payload.h"
#include "Plugin.h"
#include "Presence.h"
#include "Resource.h"
#include "Security.h"
#include "VirtualBusAttachment.h"
#include "VirtualBusObject.h"
#include "VirtualConfigBusObject.h"
#include "VirtualConfigurationResource.h"
#include "VirtualDevice.h"
#include "VirtualResource.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include <alljoyn/AllJoynStd.h>
#include <algorithm>
#include <deque>
#include <iterator>
#include <math.h>
#include <sstream>
#include <thread>

#if __WITH_DTLS__
#define SECURE_MODE_DEFAULT true
#else
#define SECURE_MODE_DEFAULT false
#endif

static bool TranslateResourceType(const char *type)
{
    return !(strcmp(type, OC_RSRVD_RESOURCE_TYPE_DEVICE) == 0 ||
             strcmp(type, OC_RSRVD_RESOURCE_TYPE_INTROSPECTION) == 0 ||
             strcmp(type, OC_RSRVD_RESOURCE_TYPE_PLATFORM) == 0 ||
             strcmp(type, OC_RSRVD_RESOURCE_TYPE_RD) == 0 ||
             strcmp(type, OC_RSRVD_RESOURCE_TYPE_RDPUBLISH) == 0 ||
             strcmp(type, OC_RSRVD_RESOURCE_TYPE_RES) == 0 ||
             strcmp(type, "oic.d.bridge") == 0 ||
             strcmp(type, "oic.r.doxm") == 0 ||
             strcmp(type, "oic.r.pstat") == 0 ||
             strcmp(type, "oic.r.securemode") == 0);
}

std::vector<OCDevAddr> GetDevAddrs(OCDevAddr origin, const char *di, OCResourcePayload *resource)
{
    if (resource->secure)
    {
        origin.flags = (OCTransportFlags) (origin.flags | OC_FLAG_SECURE);
        if (origin.adapter == OC_ADAPTER_IP)
        {
            origin.port = resource->port;
        }
#ifdef TCP_ADAPTER
        else if (origin.adapter == OC_ADAPTER_TCP)
        {
            origin.port = resource->tcpPort;
        }
#endif
    }
    std::vector<OCDevAddr> addrs;
    if (!resource->eps)
    {
        addrs.push_back(origin);
    }
    for (const OCEndpointPayload *ep = resource->eps; ep; ep = ep->next)
    {
        OCDevAddr addr;
        if (!strcmp(ep->tps, "coap") || !strcmp(ep->tps, "coaps"))
        {
            addr.adapter = OC_ADAPTER_IP;
        }
        else if (!strcmp(ep->tps, "coap+tcp") || !strcmp(ep->tps, "coaps+tcp"))
        {
            addr.adapter = OC_ADAPTER_TCP;
        }
        addr.flags = ep->family;
        addr.port = ep->port;
        strncpy(addr.addr, ep->addr, MAX_ADDR_STR_SIZE);
        addr.ifindex = 0;
        addr.routeData[0] = '\0';
        strncpy(addr.remoteId, di, MAX_IDENTITY_SIZE);
        addrs.push_back(addr);
    }
    return addrs;
}

struct Resource
{
    std::string m_uri;
    std::vector<std::string> m_ifs;
    std::vector<std::string> m_rts;
    bool m_isObservable;
    std::vector<OCDevAddr> m_addrs;
    Resource(OCDevAddr origin, const char *di, OCResourcePayload *resource)
        : m_uri(resource->uri), m_isObservable(resource->bitmap & OC_OBSERVABLE)
    {
        for (OCStringLL *interface = resource->interfaces; interface; interface = interface->next)
        {
            m_ifs.push_back(interface->value);
        }
        for (OCStringLL *type = resource->types; type; type = type->next)
        {
            m_rts.push_back(type->value);
        }
        m_addrs = GetDevAddrs(origin, di, resource);
    }
    bool IsSecure()
    {
        for (auto &addr : m_addrs)
        {
            if (addr.flags & OC_FLAG_SECURE)
            {
                return true;
            }
        }
        return false;
    }
};

struct Device
{
    std::string m_di;
    std::vector<Resource> m_resources;
    Device(OCDevAddr origin, OCDiscoveryPayload *payload)
        : m_di(payload->sid)
    {
        for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
             resource = resource->next)
        {
            m_resources.push_back(Resource(origin, payload->sid, resource));
        }
    }
    Resource *GetResourceUri(const char *uri)
    {
        for (auto &resource : m_resources)
        {
            if (resource.m_uri == uri)
            {
                return &resource;
            }
        }
        return NULL;
    }
    Resource *GetResourceType(const char *rt)
    {
        for (auto &resource : m_resources)
        {
            if (std::find(resource.m_rts.begin(), resource.m_rts.end(), rt) !=
                    resource.m_rts.end())
            {
                return &resource;
            }
        }
        return NULL;
    }
    bool IsVirtual()
    {
        Resource *resource = GetResourceUri(OC_RSRVD_DEVICE_URI);
        if (resource)
        {
            return std::find(resource->m_rts.begin(), resource->m_rts.end(), "oic.d.virtual") !=
                    resource->m_rts.end();
        }
        return false;
    }
};

struct Bridge::DiscoverContext
{
    Bridge *m_bridge;
    Device m_device;
    VirtualBusAttachment *m_bus;
    OCRepPayload *m_paths;
    OCRepPayload *m_definitions;
    DiscoverContext(Bridge *bridge, OCDevAddr origin, OCDiscoveryPayload *payload)
        : m_bridge(bridge), m_device(origin, payload), m_bus(NULL), m_paths(NULL),
          m_definitions(NULL) { }
    ~DiscoverContext() { OCRepPayloadDestroy(m_paths); OCRepPayloadDestroy(m_definitions);
        delete m_bus; }
    std::vector<OCDevAddr> GetDevAddrs(const char *uri)
    {
        Resource *resource = m_device.GetResourceUri(uri);
        if (resource)
        {
            return resource->m_addrs;
        }
        return std::vector<OCDevAddr>();
    }

    struct Iterator
    {
        DiscoverContext *m_context;
        std::vector<Resource>::iterator m_r;
        std::vector<std::string>::iterator m_rt;
        Iterator() { }
        Iterator(DiscoverContext *context, bool isBegin = true)
            : m_context(context)
        {
            if (isBegin)
            {
                Iterate(isBegin);
            }
            else
            {
                m_r = m_context->m_device.m_resources.end();
                m_rt = m_context->m_device.m_resources.back().m_rts.end();
            }
        }
        void Iterate(bool isBegin)
        {
            if (!isBegin)
            {
                goto next;
            }
            m_r = m_context->m_device.m_resources.begin();
            while (m_r != m_context->m_device.m_resources.end())
            {
                if (!m_context->m_bridge->m_secureMode || m_r->IsSecure())
                {
                    m_rt = m_r->m_rts.begin();
                    while (m_rt != m_r->m_rts.end())
                    {
                        if (TranslateResourceType(m_rt->c_str()))
                        {
                            return;
                        }
                    next:
                        ++m_rt;
                    }
                }
                ++m_r;
            }
        }
        std::string GetUri()
        {
            std::string uri = m_r->m_uri;
            if (m_r->m_rts.size() > 1)
            {
                uri += "?rt=" + *m_rt;
            }
            return uri;
        }
        std::vector<OCDevAddr> GetDevAddrs()
        {
            return m_r->m_addrs;
        }
        Resource& GetResource()
        {
            return *m_r;
        }
        std::string GetResourceType()
        {
            return *m_rt;
        }
        Iterator& operator++()
        {
            Iterate(false);
            return *this;
        }
        bool operator!=(const Iterator &rhs)
        {
            return (m_r != rhs.m_r) && (m_rt != rhs.m_rt);
        }
    };
    Iterator Begin() { return Iterator(this, true); }
    Iterator End() { return Iterator(this, false); }
    Iterator m_it;
};

Bridge::Bridge(const char *name, Protocol protocols)
    : m_execCb(NULL), m_sessionLostCb(NULL), m_protocols(protocols), m_sender(NULL),
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(SECURE_MODE_DEFAULT),
      m_rdPublishTask(NULL), m_pending(0)
{
    m_bus = new ajn::BusAttachment(name, true);
    m_ajState = CREATED;
    m_ajSecurity = new AllJoynSecurity(m_bus, AllJoynSecurity::CONSUMER);
    m_ocSecurity = new OCSecurity();
}

Bridge::Bridge(const char *name, const char *sender)
    : m_execCb(NULL), m_sessionLostCb(NULL), m_protocols(AJ), m_sender(sender),
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(SECURE_MODE_DEFAULT),
      m_rdPublishTask(NULL), m_pending(0)
{
    m_bus = new ajn::BusAttachment(name, true);
    m_ajState = CREATED;
    m_ajSecurity = new AllJoynSecurity(m_bus, AllJoynSecurity::CONSUMER);
    m_ocSecurity = new OCSecurity();
}

Bridge::~Bridge()
{
    LOG(LOG_INFO, "[%p]", this);

    {
        std::unique_lock<std::mutex> lock(m_mutex);
        while (m_pending > 0)
        {
            m_cond.wait(lock);
        }
        for (Presence *presence : m_presence)
        {
            delete presence;
        }
        m_presence.clear();
        for (auto &dc : m_discovered)
        {
            DiscoverContext *discoverContext = dc.second;
            delete discoverContext;
        }
        m_discovered.clear();
        for (VirtualBusAttachment *busAttachment : m_virtualBusAttachments)
        {
            delete busAttachment;
        }
        m_virtualBusAttachments.clear();
        for (VirtualResource *resource : m_virtualResources)
        {
            delete resource;
        }
        m_virtualResources.clear();
        for (VirtualDevice *device : m_virtualDevices)
        {
            delete device;
        }
        m_virtualDevices.clear();
    }
    delete m_ocSecurity;
    delete m_ajSecurity;
    delete m_bus;
}

/* Called with m_mutex held. */
void Bridge::Destroy(const char *id)
{
    std::map<OCDoHandle, DiscoverContext *>::iterator dc = m_discovered.begin();
    while (dc != m_discovered.end())
    {
        DiscoverContext *context = dc->second;
        if (context->m_device.m_di == id)
        {
            delete context;
            dc = m_discovered.erase(dc);
        }
        else
        {
            ++dc;
        }
    }
    std::vector<VirtualBusAttachment *>::iterator vba = m_virtualBusAttachments.begin();
    while (vba != m_virtualBusAttachments.end())
    {
        VirtualBusAttachment *busAttachment = *vba;
        if (busAttachment->GetDi() == id)
        {
            delete busAttachment;
            vba = m_virtualBusAttachments.erase(vba);
        }
        else
        {
            ++vba;
        }
    }
    std::vector<VirtualResource *>::iterator vr = m_virtualResources.begin();
    while (vr != m_virtualResources.end())
    {
        VirtualResource *resource = *vr;
        if (resource->GetUniqueName() == id)
        {
            delete resource;
            vr = m_virtualResources.erase(vr);
        }
        else
        {
            ++vr;
        }
    }
    std::vector<VirtualDevice *>::iterator vd = m_virtualDevices.begin();
    while (vd != m_virtualDevices.end())
    {
        VirtualDevice *device = *vd;
        if (device->GetName() == id)
        {
            delete device;
            vd = m_virtualDevices.erase(vd);
        }
        else
        {
            ++vd;
        }
    }
    std::vector<Presence *>::iterator p = m_presence.begin();
    while (p != m_presence.end())
    {
        Presence *presence = *p;
        if (presence->GetId() == id)
        {
            delete presence;
            p = m_presence.erase(p);
        }
        else
        {
            ++p;
        }
    }
}

bool Bridge::Start()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    if (!m_ocSecurity->Init())
    {
        return false;
    }
    if (!m_sender)
    {
        OCResourceHandle handle = OCGetResourceHandleAtUri(OC_RSRVD_DEVICE_URI);
        if (!handle)
        {
            LOG(LOG_ERR, "OCGetResourceHandleAtUri(" OC_RSRVD_DEVICE_URI ") failed");
            return false;
        }
        OCStackResult result = OCBindResourceTypeToResource(handle, "oic.d.bridge");
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCBindResourceTypeToResource() - %d", result);
            return false;
        }
        handle = OCGetResourceHandleAtUri(OC_RSRVD_WELL_KNOWN_URI);
        if (!handle)
        {
            LOG(LOG_ERR, "OCGetResourceHandleAtUri(" OC_RSRVD_DEVICE_URI ") failed");
            return false;
        }
        result = OCSetResourceProperties(handle, OC_DISCOVERABLE | OC_OBSERVABLE);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCSetResourceProperties() - %d", result);
            return false;
        }
        result = CreateResource("/securemode", OC_RSRVD_RESOURCE_TYPE_SECURE_MODE,
                                OC_RSRVD_INTERFACE_READ_WRITE,
                                Bridge::EntityHandlerCB, this,
                                OC_DISCOVERABLE | OC_OBSERVABLE);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "CreateResource() - %d", result);
            return false;
        }
    }
    LOG(LOG_INFO, "di=%s", OCGetServerInstanceIDString());

    if (m_protocols & AJ)
    {
        m_bus->RegisterAboutListener(*this);
        m_bus->RegisterBusListener(*this);
        QStatus status = m_bus->Start();
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "Start - %s", QCC_StatusText(status));
            return false;
        }
        m_ajState = STARTED;
    }
    return true;
}

bool Bridge::Stop()
{
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (VirtualBusAttachment *busAttachment : m_virtualBusAttachments)
    {
        busAttachment->Stop();
    }
    if (m_discoverHandle)
    {
        OCStackResult result = Cancel(m_discoverHandle, OC_LOW_QOS);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "Cancel() - %d", result);
        }
        m_discoverHandle = NULL;
    }
    return true;
}

bool Bridge::Process()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_protocols & AJ)
    {
        QStatus status;
        switch (m_ajState)
        {
            case CREATED:
                LOG(LOG_ERR, "[%p] Bridge not started", this);
                break;
            case STARTED:
                status = m_bus->Connect();
                if (status != ER_OK)
                {
                    LOG(LOG_INFO, "[%p] Connect() - %s", this, QCC_StatusText(status));
                    break;
                }
                m_ajState = CONNECTED;
                /* FALLTHROUGH */
            case CONNECTED:
                if (!m_ajSecurity->IsClaimed())
                {
                    m_ajSecurity->SetClaimable();
                }
                m_ajState = CLAIMABLE;
                /* FALLTHROUGH */
            case CLAIMABLE:
                if (m_ajSecurity->IsClaimed())
                {
                    WhoImplements();
                    m_ajState = RUNNING;
                }
                break;
            case RUNNING:
                break;
        }
    }
    if (m_protocols & OC)
    {
        if (time(NULL) >= m_discoverNextTick)
        {
            if (m_discoverHandle)
            {
                OCStackResult result = Cancel(m_discoverHandle, OC_LOW_QOS);
                if (result != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "Cancel() - %d", result);
                }
                m_discoverHandle = NULL;
            }
            OCCallbackData cbData;
            cbData.cb = Bridge::DiscoverCB;
            cbData.context = this;
            cbData.cd = NULL;
            OCHeaderOption options[1];
            size_t numOptions = 0;
            uint16_t format = COAP_MEDIATYPE_APPLICATION_VND_OCF_CBOR; // TODO retry with CBOR
            OCSetHeaderOption(options, &numOptions, CA_OPTION_ACCEPT, &format, sizeof(format));
            OCStackResult result = ::DoResource(&m_discoverHandle, OC_REST_DISCOVER,
                    OC_RSRVD_WELL_KNOWN_URI, NULL, 0, &cbData, options, numOptions);
            if (result != OC_STACK_OK)
            {
                LOG(LOG_ERR, "DoResource(OC_REST_DISCOVER) - %d", result);
            }
            m_discoverNextTick = time(NULL) + DISCOVER_PERIOD_SECS;
        }
    }
    std::vector<std::string> absent;
    for (Presence *presence : m_presence)
    {
        if (!presence->IsPresent())
        {
            absent.push_back(presence->GetId());
        }
    }
    for (std::string &id : absent)
    {
        LOG(LOG_INFO, "[%p] %s absent", this, id.c_str());
        Destroy(id.c_str());
    }
    std::list<Task *>::iterator task = m_tasks.begin();
    while (task != m_tasks.end())
    {
        if (time(NULL) >= (*task)->m_tick)
        {
            (*task)->Run(this);
            delete (*task);
            task = m_tasks.erase(task);
        }
        else
        {
            ++task;
        }
    }
    return true;
}

void Bridge::BusDisconnected()
{
    LOG(LOG_INFO, "[%p]", this);
    std::lock_guard<std::mutex> lock(m_mutex);
    std::set<std::string> ids;
    for (VirtualResource *resource : m_virtualResources)
    {
        ids.insert(resource->GetUniqueName());
    }
    for (VirtualDevice *device : m_virtualDevices)
    {
        ids.insert(device->GetName());
    }
    for (const std::string &id : ids)
    {
        LOG(LOG_INFO, "[%p] %s absent", this, id.c_str());
        Destroy(id.c_str());
    }
    m_ajState = STARTED;
}

void Bridge::WhoImplements()
{
    std::string matchRule =
        "type='signal',interface='org.alljoyn.About',member='Announce',sessionless='t'";
    if (m_sender)
    {
        matchRule += ",sender='" + std::string(m_sender) + "'";
    }
    QStatus status = m_bus->AddMatch(matchRule.c_str());
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "AddMatch - %s", QCC_StatusText(status));
    }
}

struct AnnouncedContext
{
public:
    AnnouncedContext(VirtualDevice *device, const char *name,
            const ajn::MsgArg &objectDescriptionArg, const ajn::MsgArg &aboutDataArg)
        : m_device(device), m_name(name), m_objectDescriptionArg(objectDescriptionArg),
          m_aboutDataArg(aboutDataArg), m_sessionId(0), m_aboutObj(NULL) { }
    ~AnnouncedContext() { delete m_aboutObj; }
    ajn::BusAttachment *m_bus;
    VirtualDevice *m_device;
    std::string m_name;
    ajn::MsgArg m_objectDescriptionArg;
    ajn::MsgArg m_aboutDataArg;
    ajn::SessionId m_sessionId;
    ajn::ProxyBusObject *m_aboutObj;
};

void Bridge::Announced(const char *name, uint16_t version, ajn::SessionPort port,
                       const ajn::MsgArg &objectDescriptionArg, const ajn::MsgArg &aboutDataArg)
{
    QStatus status;
    AnnouncedContext *context;
    ajn::SessionOpts opts;

    LOG(LOG_INFO, "[%p] name=%s,version=%u,port=%u", this, name, version, port);

    m_mutex.lock();
    /* Ignore Announce from self */
    for (VirtualBusAttachment *busAttachment : m_virtualBusAttachments)
    {
        if (busAttachment->GetUniqueName() == name)
        {
            m_mutex.unlock();
            return;
        }
    }

    /* Check if we've seen this Announce before */
    VirtualDevice *device = NULL;
    for (std::vector<VirtualDevice *>::iterator it = m_virtualDevices.begin();
         it != m_virtualDevices.end(); ++it)
    {
        if ((*it)->GetName() == name)
        {
            device = *it;
            break;
        }
    }

    context = new AnnouncedContext(device, name, objectDescriptionArg, aboutDataArg);
    if (device)
    {
        LOG(LOG_INFO, "[%p] Received updated Announce", this);
        m_mutex.unlock();
        ajn::SessionOpts opts;
        JoinSessionCB(ER_OK, device->GetSessionId(), opts, context);
    }
    else
    {
        status = m_bus->JoinSessionAsync(name, port, this, opts, this, context);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "JoinSessionAsync - %s", QCC_StatusText(status));
            delete context;
        }
        m_mutex.unlock();
    }
}

void Bridge::JoinSessionCB(QStatus status, ajn::SessionId sessionId, const ajn::SessionOpts &opts,
                           void *ctx)
{
    (void) opts;
    LOG(LOG_INFO, "[%p] status=%s,sessionId=%d,ctx=%p", this, QCC_StatusText(status), sessionId,
            ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    AnnouncedContext *context = reinterpret_cast<AnnouncedContext *>(ctx);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "JoinSessionCB - %s", QCC_StatusText(status));
        delete context;
    }
    else
    {
        context->m_sessionId = sessionId;

        /* BusAttachment::SecureConnectionAsync is not really usable (no means to pass context). */
        ++m_pending;
        std::thread(Bridge::SecureConnection, this, context->m_name.c_str(), ctx).detach();
    }
}

void Bridge::SecureConnection(Bridge *thiz, const char *name, void *ctx)
{
    QStatus status = thiz->m_bus->SecureConnection(name);
    thiz->SecureConnectionCB(status, ctx);
    {
        std::lock_guard<std::mutex> lock(thiz->m_mutex);
        --thiz->m_pending;
        thiz->m_cond.notify_one();
    }
}

void Bridge::SecureConnectionCB(QStatus status, void *ctx)
{
    LOG(LOG_INFO, "[%p] status=%s,ctx=%p", this, QCC_StatusText(status), ctx);

    std::lock_guard<std::mutex> lock(m_mutex);
    AnnouncedContext *context = reinterpret_cast<AnnouncedContext *>(ctx);
    if (m_secureMode && (status != ER_OK))
    {
        LOG(LOG_ERR, "SecureConnectionCB - %s", QCC_StatusText(status));
    }
    else if (!m_sender)
    {
        qcc::String peerGuid;
        m_bus->GetPeerGUID(context->m_name.c_str(), peerGuid);
        ajn::AboutData aboutData(context->m_aboutDataArg);
        OCUUIdentity piid;
        GetPiid(&piid, peerGuid.c_str(), &aboutData);
        char piidStr[UUID_STRING_SIZE];
        OCConvertUuidToString(piid.id, piidStr);

        bool isVirtual = ajn::AboutObjectDescription(context->m_objectDescriptionArg)
                .HasInterface("oic.d.virtual");

        switch (GetSeenState(piidStr))
        {
            case NOT_SEEN:
                m_execCb(piidStr, context->m_name.c_str(), isVirtual);
                break;
            case SEEN_NATIVE:
                /* Do nothing */
                break;
            case SEEN_VIRTUAL:
                if (isVirtual)
                {
                    /* Do nothing */
                }
                else
                {
                    /* Delay creating virtual resources from a virtual Announce */
                    LOG(LOG_INFO, "[%p] Delaying creation of virtual resources from a virtual device",
                            this);
                    m_tasks.push_back(new AnnouncedTask(time(NULL) + 10, context->m_name.c_str(),
                            piidStr, isVirtual));
                }
                break;
        }
        /* Force a leave of the session */
        status = ER_FAIL;
    }
    else
    {
        context->m_aboutObj = new ajn::ProxyBusObject(*m_bus, context->m_name.c_str(), "/About",
                context->m_sessionId);
        context->m_aboutObj->AddInterface(::ajn::org::alljoyn::About::InterfaceName);
        ajn::MsgArg arg("s", "");
        status = context->m_aboutObj->MethodCallAsync(::ajn::org::alljoyn::About::InterfaceName,
                 "GetAboutData",
                 this, static_cast<ajn::MessageReceiver::ReplyHandler>(&Bridge::GetAboutDataCB),
                 &arg, 1,
                 context);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
        }
    }
    if (status != ER_OK)
    {
        status = m_bus->LeaveSessionAsync(context->m_sessionId, this, context);
        if (status != ER_OK)
        {
            delete context;
        }
    }
}

static bool ComparePath(const char *a, const char *b)
{
    return (strcmp(a, b) < 0);
}

VirtualResource *Bridge::CreateVirtualResource(ajn::BusAttachment *bus,
        const char *name, ajn::SessionId sessionId, const char *path,
        const char *ajSoftwareVersion)
{
    if (!strcmp(path, "/Config"))
    {
        return VirtualConfigurationResource::Create(this, bus, name, sessionId, path,
                ajSoftwareVersion);
    }
    else
    {
        return VirtualResource::Create(this, bus, name, sessionId, path, ajSoftwareVersion);
    }
}

void Bridge::GetAboutDataCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    AnnouncedContext *context = reinterpret_cast<AnnouncedContext *>(ctx);
    if (msg->GetType() == ajn::MESSAGE_METHOD_RET)
    {
        ajn::AboutObjectDescription objectDescription(context->m_objectDescriptionArg);
        ajn::AboutData aboutData(*msg->GetArg(0));
        char *ajSoftwareVersion = NULL;
        QStatus status = aboutData.GetAJSoftwareVersion(&ajSoftwareVersion);
        LOG(LOG_INFO, "[%p] %s AJSoftwareVersion=%s", this, QCC_StatusText(status),
            ajSoftwareVersion ? ajSoftwareVersion : "unknown");
        m_ajSoftwareVersion = ajSoftwareVersion;
        if (context->m_device)
        {
            context->m_device->SetInfo(objectDescription, aboutData);
            OCResourceHandle handle = OCGetResourceHandleAtUri(OC_RSRVD_DEVICE_URI);
            if (!handle)
            {
                LOG(LOG_ERR, "OCGetResourceHandleAtUri(" OC_RSRVD_DEVICE_URI ") failed");
            }
            OCStackResult result = OCBindResourceTypeToResource(handle, "oic.d.virtual");
            if (result != OC_STACK_OK)
            {
                LOG(LOG_ERR, "OCBindResourceTypeToResource() - %d", result);
            }
            size_t n = objectDescription.GetPaths(NULL, 0);
            const char **pa = new const char *[n];
            objectDescription.GetPaths(pa, n);
            std::sort(pa, pa + n, ComparePath);
            std::vector<const char *> pb;
            for (VirtualResource *resource : m_virtualResources)
            {
                if (resource->GetUniqueName() == context->m_name.c_str())
                {
                    pb.push_back(resource->GetPath().c_str());
                }
            }
            std::sort(pb.begin(), pb.end(), ComparePath);
            std::vector<const char *> remove;
            std::set_difference(pb.begin(), pb.end(),
                                pa, pa + n,
                                std::inserter(remove, remove.begin()), ComparePath);
            for (size_t i = 0; i < remove.size(); ++i)
            {
                for (std::vector<VirtualResource *>::iterator vr = m_virtualResources.begin();
                     vr != m_virtualResources.end(); ++vr)
                {
                    VirtualResource *resource = *vr;
                    if (resource->GetUniqueName() == context->m_name.c_str() &&
                        resource->GetPath() == remove[i])
                    {
                        delete resource;
                        m_virtualResources.erase(vr);
                        break;
                    }
                }
            }
            std::vector<const char *> add;
            std::set_difference(pa, pa + n,
                                pb.begin(), pb.end(),
                                std::inserter(add, add.begin()), ComparePath);
            for (size_t i = 0; i < add.size(); ++i)
            {
                VirtualResource *resource = CreateVirtualResource(m_bus,
                                            context->m_name.c_str(), msg->GetSessionId(), add[i],
                                            ajSoftwareVersion);
                if (resource)
                {
                    m_virtualResources.push_back(resource);
                }
            }
        }
        else
        {
            Presence *presence = new AllJoynPresence(m_bus, context->m_name);
            m_presence.push_back(presence);
            VirtualDevice *device = new VirtualDevice(m_bus, msg->GetSender(), msg->GetSessionId());
            device->SetInfo(objectDescription, aboutData);
            OCResourceHandle handle = OCGetResourceHandleAtUri(OC_RSRVD_DEVICE_URI);
            if (!handle)
            {
                LOG(LOG_ERR, "OCGetResourceHandleAtUri(" OC_RSRVD_DEVICE_URI ") failed");
            }
            OCStackResult result = OCBindResourceTypeToResource(handle, "oic.d.virtual");
            if (result != OC_STACK_OK)
            {
                LOG(LOG_ERR, "OCBindResourceTypeToResource() - %d", result);
            }
            m_virtualDevices.push_back(device);
            size_t numPaths = objectDescription.GetPaths(NULL, 0);
            const char **paths = new const char *[numPaths];
            objectDescription.GetPaths(paths, numPaths);
            for (size_t i = 0; i < numPaths; ++i)
            {
                VirtualResource *resource = CreateVirtualResource(m_bus,
                                            context->m_name.c_str(), msg->GetSessionId(), paths[i],
                                            ajSoftwareVersion);
                if (resource)
                {
                    m_virtualResources.push_back(resource);
                }
            }
            delete[] paths;
            device->StartPresence();
        }
        delete context;
    }
    else if (msg->GetType() == ajn::MESSAGE_ERROR)
    {
        QStatus status = m_bus->LeaveSessionAsync(context->m_sessionId, this, context);
        if (status != ER_OK)
        {
            delete context;
        }
    }
}

void Bridge::LeaveSessionCB(QStatus status, void *ctx)
{
    LOG(LOG_INFO, "[%p] status=%s,ctx=%p", this, QCC_StatusText(status), ctx);
    AnnouncedContext *context = reinterpret_cast<AnnouncedContext *>(ctx);
    ajn::SessionId sessionId = context->m_sessionId;
    delete context;
    SessionLost(sessionId, ALLJOYN_SESSIONLOST_REASON_OTHER);
}

void Bridge::SessionLost(ajn::SessionId sessionId, ajn::SessionListener::SessionLostReason reason)
{
    LOG(LOG_INFO, "[%p] sessionId=%d,reason=%d", this, sessionId, reason);

    std::lock_guard<std::mutex> lock(m_mutex);
    for (VirtualDevice *device : m_virtualDevices)
    {
        if (device->GetSessionId() == sessionId)
        {
            Destroy(device->GetName().c_str());
            break;
        }
    }

    if (m_sessionLostCb)
    {
        m_sessionLostCb();
    }
}

void Bridge::UpdatePresenceStatus(const OCDiscoveryPayload *payload)
{
    for (Presence *p : m_presence)
    {
        if (p->GetId() == payload->sid)
        {
            p->Seen();
        }
    }
}

bool Bridge::IsSelf(const OCDiscoveryPayload *payload)
{
    return !strcmp(payload->sid, GetServerInstanceIDString());
}

bool Bridge::HasSeenBefore(const OCDiscoveryPayload *payload)
{
    for (VirtualBusAttachment *b : m_virtualBusAttachments)
    {
        if (b->GetDi() == payload->sid)
        {
            return true;
        }
    }
    for (auto &d : m_discovered)
    {
        if (d.second->m_device.m_di == payload->sid)
        {
            return true;
        }
    }
    return false;
}

bool Bridge::IsSecure(const OCResourcePayload *resource)
{
    if (resource->secure)
    {
        return true;
    }
    for (OCEndpointPayload *ep = resource->eps; ep; ep = ep->next)
    {
        if (ep->family & OC_SECURE)
        {
            return true;
        }
    }
    return false;
}

bool Bridge::HasTranslatableResource(OCDiscoveryPayload *payload)
{
    for (OCResourcePayload *resource = payload->resources; resource; resource = resource->next)
    {
        if (m_secureMode && !IsSecure(resource))
        {
            continue;
        }
        for (OCStringLL *type = resource->types; type; type = type->next)
        {
            if (TranslateResourceType(type->value))
            {
                return true;
            }
        }
    }
    return false;
}

OCStackResult Bridge::DoResource(OCDoHandle *handle, OCMethod method, const char *uri,
        const std::vector<OCDevAddr> &addrs, OCClientResponseHandler cb)
{
    OCCallbackData cbData;
    cbData.cb = cb;
    cbData.context = this;
    cbData.cd = NULL;
    OCStackResult result = ::DoResource(handle, method, uri, addrs, NULL, &cbData, NULL, 0);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "[%p] DoResource(method=%d,uri=%s) - %d", this, method, uri, result);
    }
    return result;
}

OCStackResult Bridge::DoResource(OCDoHandle *handle, OCMethod method, const char *uri,
        OCDevAddr *addr, OCClientResponseHandler cb)
{
    std::vector<OCDevAddr> addrs = { *addr };
    return DoResource(handle, method, uri, addrs, cb);
}

void Bridge::GetContextAndRepPayload(OCDoHandle handle, OCClientResponse *response,
        DiscoverContext **context, OCRepPayload **payload)
{
    std::map<OCDoHandle, DiscoverContext *>::iterator it = m_discovered.find(handle);
    if (it != m_discovered.end())
    {
        *context = it->second;
    }
    else
    {
        *context = NULL;
    }

    if (response && response->result == OC_STACK_OK &&
            response->payload && response->payload->type == PAYLOAD_TYPE_REPRESENTATION)
    {
        *payload = (OCRepPayload *) response->payload;
    }
    else
    {
        LOG(LOG_INFO, "[%p] Missing %s or unexpected payload type: %d", this, response->resourceUri,
                (response && response->payload) ? response->payload->type : PAYLOAD_TYPE_INVALID);
        *payload = NULL;
    }
}

OCStackResult Bridge::ContinueDiscovery(DiscoverContext *context, const char *uri,
        const std::vector<OCDevAddr> &addrs, OCClientResponseHandler cb)
{
    OCDoHandle cbHandle;
    OCStackResult result = DoResource(&cbHandle, OC_REST_GET, uri, addrs, cb);
    if (result == OC_STACK_OK)
    {
        LOG(LOG_INFO, "Get(%s)", uri);
        m_discovered[cbHandle] = context;
    }
    return result;
}

OCStackResult Bridge::ContinueDiscovery(DiscoverContext *context, const char *uri,
        OCDevAddr *addr, OCClientResponseHandler cb)
{
    OCDoHandle cbHandle;
    OCStackResult result = DoResource(&cbHandle, OC_REST_GET, uri, addr, cb);
    if (result == OC_STACK_OK)
    {
        LOG(LOG_INFO, "Get(%s)", uri);
        m_discovered[cbHandle] = context;
    }
    return result;
}

OCStackApplicationResult Bridge::DiscoverCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    if (!response || response->result != OC_STACK_OK)
    {
        goto exit;
    }
    OCDiscoveryPayload *payload;
    for (payload = (OCDiscoveryPayload *) response->payload; payload; payload = payload->next)
    {
        DiscoverContext *context = NULL;
        std::vector<OCDevAddr> addrs;
        OCStackResult result;
        thiz->UpdatePresenceStatus(payload);
        if (thiz->IsSelf(payload) || thiz->HasSeenBefore(payload) ||
                !thiz->HasTranslatableResource(payload))
        {
            goto next;
        }
        context = new DiscoverContext(thiz, response->devAddr, payload);
        if (!context)
        {
            goto next;
        }
        result = thiz->ContinueDiscovery(context, OC_RSRVD_DEVICE_URI,
                context->GetDevAddrs(OC_RSRVD_DEVICE_URI), Bridge::GetDeviceCB);
        if (result == OC_STACK_OK)
        {
            context = NULL;
        }
    next:
        delete context;
    }

exit:
    return OC_STACK_KEEP_TRANSACTION;
}

OCStackApplicationResult Bridge::GetDeviceCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result;
    bool isVirtual;
    char *piid = NULL;
    DiscoverContext *context;
    OCRepPayload *payload;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context || !payload)
    {
        goto exit;
    }
    OCRepPayloadGetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, &piid);
    isVirtual = context->m_device.IsVirtual();
    switch (thiz->GetSeenState(piid))
    {
        case NOT_SEEN:
            context->m_bus = VirtualBusAttachment::Create(context->m_device.m_di.c_str(), piid,
                    isVirtual);
            break;
        case SEEN_NATIVE:
            /* Do nothing */
            goto exit;
        case SEEN_VIRTUAL:
            if (isVirtual)
            {
                /* Do nothing */
            }
            else
            {
                /* Delay creating virtual objects from a virtual device */
                LOG(LOG_INFO, "[%p] Delaying creation of virtual objects from a virtual device",
                        thiz);
                thiz->m_tasks.push_back(new DiscoverTask(time(NULL) + 10, piid, payload, context));
                context = NULL;
            }
            goto exit;
    }
    if (!context->m_bus)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(OC_RSRVD_DEVICE_URI, payload);
    result = thiz->ContinueDiscovery(context, OC_RSRVD_PLATFORM_URI,
            context->GetDevAddrs(OC_RSRVD_PLATFORM_URI), Bridge::GetPlatformCB);
    if (result == OC_STACK_OK)
    {
        context = NULL;
    }

exit:
    OICFree(piid);
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetPlatformCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    Resource *resource;
    DiscoverContext *context;
    OCRepPayload *payload;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context || !payload)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(OC_RSRVD_PLATFORM_URI, payload);
    resource = context->m_device.GetResourceType(OC_RSRVD_RESOURCE_TYPE_INTROSPECTION);
    OCStackResult result;
    if (resource)
    {
        result = thiz->ContinueDiscovery(context, resource->m_uri.c_str(),
                resource->m_addrs, Bridge::GetIntrospectionCB);
    }
    else
    {
        context->m_paths = OCRepPayloadCreate();
        context->m_definitions = OCRepPayloadCreate();
        if (!context->m_paths || !context->m_definitions)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        context->m_it = context->Begin();
        result = thiz->ContinueDiscovery(context, context->m_it.GetUri().c_str(),
                context->m_it.GetDevAddrs(), Bridge::GetCB);
    }
    if (result == OC_STACK_OK)
    {
        context = NULL;
    }

exit:
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetIntrospectionCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    char *url = NULL;
    char *protocol = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
    size_t dimTotal;
    OCRepPayload **urlInfo = NULL;
    DiscoverContext *context;
    OCRepPayload *payload;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context)
    {
        goto exit;
    }
    if (!payload)
    {
        goto exit;
    }
    if (!OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_INTROSPECTION_URL_INFO, &urlInfo, dim))
    {
        goto exit;
    }
    dimTotal = calcDimTotal(dim);
    for (size_t i = 0; i < dimTotal; ++i)
    {
        if (!OCRepPayloadGetPropString(urlInfo[i], OC_RSRVD_INTROSPECTION_PROTOCOL, &protocol) ||
                !OCRepPayloadGetPropString(urlInfo[i], OC_RSRVD_INTROSPECTION_URL, &url))
        {
            LOG(LOG_INFO, "[%p] Failed to get mandatory protocol or url properties", thiz);
            goto exit;
        }
        if (!strcmp(protocol, "coap") || !strcmp(protocol, "coaps")
#ifdef TCP_ADAPTER
                || !strcmp(protocol, "coap+tcp") || !strcmp(protocol, "coaps+tcp")
#endif
           )
        {
            LOG(LOG_INFO, "[%p] protocol=%s,url=%s", thiz, protocol, url);
            OCStackResult result = thiz->ContinueDiscovery(context, url, &response->devAddr,
                    Bridge::GetIntrospectionDataCB);
            if (result == OC_STACK_OK)
            {
                context = NULL;
                break;
            }
        }
        OICFree(protocol);
        protocol = NULL;
        OICFree(url);
        url = NULL;
    }

exit:
    if (context && (result != OC_STACK_OK))
    {
        context->m_paths = OCRepPayloadCreate();
        context->m_definitions = OCRepPayloadCreate();
        if (!context->m_paths || !context->m_definitions)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        context->m_it = context->Begin();
        result = thiz->ContinueDiscovery(context, context->m_it.GetUri().c_str(),
                context->m_it.GetDevAddrs(), Bridge::GetCB);
        if (result == OC_STACK_OK)
        {
            context = NULL;
        }
    }
    OICFree(url);
    OICFree(protocol);
    if (urlInfo)
    {
        size_t dimTotal = calcDimTotal(dim);
        for(size_t i = 0; i < dimTotal; ++i)
        {
            OCRepPayloadDestroy(urlInfo[i]);
        }
    }
    OICFree(urlInfo);
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetIntrospectionDataCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    char *data = NULL;
    OCPayload *outPayload = NULL;
    DiscoverContext *context;
    OCRepPayload *payload;

    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context)
    {
        goto exit;
    }
    if (!payload)
    {
        goto exit;
    }
    if (!OCRepPayloadGetPropString(payload, OC_RSRVD_INTROSPECTION_DATA_NAME, &data))
    {
        goto exit;
    }
    result = ParsePayload(&outPayload, OC_FORMAT_JSON, PAYLOAD_TYPE_REPRESENTATION,
            (const uint8_t*) data, strlen(data));
    if (result != OC_STACK_OK)
    {
        goto exit;
    }
    OICFree(data);
    data = NULL;

    thiz->ParseIntrospectionPayload(context, (OCRepPayload *) outPayload);

exit:
    if (context && (result != OC_STACK_OK))
    {
        context->m_paths = OCRepPayloadCreate();
        context->m_definitions = OCRepPayloadCreate();
        if (!context->m_paths || !context->m_definitions)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        context->m_it = context->Begin();
        result = thiz->ContinueDiscovery(context, context->m_it.GetUri().c_str(),
                context->m_it.GetDevAddrs(), Bridge::GetCB);
        if (result == OC_STACK_OK)
        {
            context = NULL;
        }
    }
    OCPayloadDestroy(outPayload);
    OICFree(data);
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
}

static bool SetPropertiesSchema(OCRepPayload *parent, OCRepPayload *obj);

static bool SetPropertiesSchema(OCRepPayload *property, OCRepPayloadPropType type,
        OCRepPayload *obj)
{
    OCRepPayload *child = NULL;
    bool success;

    switch (type)
    {
        case OCREP_PROP_NULL:
            success = false;
            break;
        case OCREP_PROP_INT:
            success = OCRepPayloadSetPropString(property, "type", "integer");
            break;
        case OCREP_PROP_DOUBLE:
            success = OCRepPayloadSetPropString(property, "type", "number");
            break;
        case OCREP_PROP_BOOL:
            success = OCRepPayloadSetPropString(property, "type", "boolean");
            break;
        case OCREP_PROP_STRING:
            success = OCRepPayloadSetPropString(property, "type", "string");
            break;
        case OCREP_PROP_BYTE_STRING:
            child = OCRepPayloadCreate();
            success = child &&
                    OCRepPayloadSetPropString(child, "binaryEncoding", "base64") &&
                    OCRepPayloadSetPropObjectAsOwner(property, "media", child) &&
                    OCRepPayloadSetPropString(property, "type", "string");
            if (success)
            {
                child = NULL;
            }
            break;
        case OCREP_PROP_OBJECT:
            child = OCRepPayloadCreate();
            success = child &&
                    SetPropertiesSchema(child, obj) &&
                    OCRepPayloadSetPropObjectAsOwner(property, "properties", child) &&
                    OCRepPayloadSetPropString(property, "type", "object");
            if (success)
            {
                child = NULL;
            }
            break;
        case OCREP_PROP_ARRAY:
            success = false;
            break;
    }
    if (!success)
    {
        goto exit;
    }
    success = true;

exit:
    OCRepPayloadDestroy(child);
    return success;
}

static bool SetPropertiesSchema(OCRepPayload *parent, OCRepPayload *obj)
{
    OCRepPayload *property = NULL;
    OCRepPayload *child = NULL;
    bool success;

    for (OCRepPayloadValue *value = obj->values; value; value = value->next)
    {
        property = OCRepPayloadCreate();
        if (!property)
        {
            LOG(LOG_ERR, "Failed to create payload");
            success = false;
            goto exit;
        }
        switch (value->type)
        {
            case OCREP_PROP_NULL:
                success = false;
                break;
            case OCREP_PROP_INT:
                success = OCRepPayloadSetPropString(property, "type", "integer");
                break;
            case OCREP_PROP_DOUBLE:
                success = OCRepPayloadSetPropString(property, "type", "number");
                break;
            case OCREP_PROP_BOOL:
                success = OCRepPayloadSetPropString(property, "type", "boolean");
                break;
            case OCREP_PROP_STRING:
                success = OCRepPayloadSetPropString(property, "type", "string");
                break;
            case OCREP_PROP_BYTE_STRING:
                child = OCRepPayloadCreate();
                success = child &&
                        OCRepPayloadSetPropString(child, "binaryEncoding", "base64") &&
                        OCRepPayloadSetPropObjectAsOwner(property, "media", child) &&
                        OCRepPayloadSetPropString(property, "type", "string");
                if (success)
                {
                    child = NULL;
                }
                break;
            case OCREP_PROP_OBJECT:
                child = OCRepPayloadCreate();
                success = child &&
                        SetPropertiesSchema(child, value->obj) &&
                        OCRepPayloadSetPropObjectAsOwner(property, "properties", child) &&
                        OCRepPayloadSetPropString(property, "type", "object");
                if (success)
                {
                    child = NULL;
                }
                break;
            case OCREP_PROP_ARRAY:
                success = true;
                parent = property;
                for (size_t i = 0; success && (i < MAX_REP_ARRAY_DEPTH) && value->arr.dimensions[i];
                     ++i)
                {
                    child = OCRepPayloadCreate();
                    success = child &&
                            OCRepPayloadSetPropObjectAsOwner(parent, "items", child) &&
                            OCRepPayloadSetPropString(parent, "type", "array");
                    if (success)
                    {
                        parent = child;
                        child = NULL;
                    }
                }
                if (success)
                {
                    success = SetPropertiesSchema(parent, value->arr.type, value->arr.objArray[0]);
                }
                break;
        }
        if (!success)
        {
            goto exit;
        }

        if (!OCRepPayloadSetPropObjectAsOwner(parent, value->name, property))
        {
            success = false;
            goto exit;
        }
        property = NULL;
    }
    success = true;

exit:
    OCRepPayloadDestroy(child);
    OCRepPayloadDestroy(property);
    return success;
}

OCStackApplicationResult Bridge::GetCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    DiscoverContext *context;
    OCRepPayload *payload;
    bool found;
    OCRepPayload *definition = NULL;
    OCRepPayload *properties = NULL;
    OCRepPayload *rt = NULL;
    size_t rtsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    size_t dimTotal;
    char **rts = NULL;
    OCRepPayload *itf = NULL;
    OCRepPayload *items = NULL;
    size_t itfsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    char **itfs = NULL;
    OCRepPayload *path = NULL;
    OCRepPayload *method = NULL;
    size_t parametersDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **parameters = NULL;
    OCRepPayload *responses = NULL;
    OCRepPayload *code = NULL;
    OCRepPayload *schema = NULL;
    size_t oneOfDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **oneOf = NULL;
    std::string ref;
    OCRepPayload *outPayload = NULL;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context)
    {
        goto exit;
    }

    found = false;
    for (OCRepPayloadValue *d = context->m_definitions->values; d; d = d->next)
    {
        if (context->m_it.GetResourceType() == d->name)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        definition = OCRepPayloadCreate();
        if (!definition)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropString(definition, "type", "object"))
        {
            goto exit;
        }
        properties = OCRepPayloadCreate();
        if (!properties)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        rt = OCRepPayloadCreate();
        if (!rt)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropBool(rt, "readOnly", true) ||
                !OCRepPayloadSetPropString(rt, "type", "array"))
        {
            goto exit;
        }
        rtsDim[0] = 1;
        dimTotal = calcDimTotal(rtsDim);
        rts = (char**) OICCalloc(dimTotal, sizeof(char*));
        if (!rts)
        {
            LOG(LOG_ERR, "Failed to allocate string array");
            goto exit;
        }
        rts[0] = OICStrdup(context->m_it.GetResourceType().c_str());
        if (!OCRepPayloadSetStringArrayAsOwner(rt, "default", rts, rtsDim))
        {
            goto exit;
        }
        rts = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(properties, "rt", rt))
        {
            goto exit;
        }
        rt = NULL;
        itf = OCRepPayloadCreate();
        if (!itf)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropBool(itf, "readOnly", true) ||
                !OCRepPayloadSetPropString(itf, "type", "array"))
        {
            goto exit;
        }
        items = OCRepPayloadCreate();
        if (!items)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropString(items, "type", "string"))
        {
            goto exit;
        }
        /* The definition of a resource type has the union of all possible interfaces listed */
        std::set<std::string> ifSet;
        for (DiscoverContext::Iterator it = context->Begin(); it != context->End(); ++it)
        {
            Resource &r = it.GetResource();
            if (std::find(r.m_rts.begin(), r.m_rts.end(), context->m_it.GetResourceType()) !=
                    r.m_rts.end())
            {
                ifSet.insert(r.m_ifs.begin(), r.m_ifs.end());
            }
        }
        itfsDim[0] = ifSet.size();
        dimTotal = calcDimTotal(itfsDim);
        itfs = (char **) OICCalloc(dimTotal, sizeof(char *));
        if (!itfs)
        {
            LOG(LOG_ERR, "Failed to allocate string array");
            goto exit;
        }
        auto ifIt = ifSet.begin();
        for (size_t i = 0; i < dimTotal; ++i, ++ifIt)
        {
            itfs[i] = OICStrdup(ifIt->c_str());
        }
        if (!OCRepPayloadSetStringArrayAsOwner(items, "enum", itfs, itfsDim))
        {
            goto exit;
        }
        itfs = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(itf, "items", items))
        {
            goto exit;
        }
        items = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(properties, "if", itf))
        {
            goto exit;
        }
        itf = NULL;
        if (!SetPropertiesSchema(properties, payload))
        {
            goto exit;
        }
        if (!OCRepPayloadSetPropObjectAsOwner(definition, "properties", properties))
        {
            goto exit;
        }
        properties = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(context->m_definitions,
                context->m_it.GetResourceType().c_str(), definition))
        {
            goto exit;
        }
        definition = NULL;
    }

    found = false;
    for (OCRepPayloadValue *p = context->m_paths->values; p; p = p->next)
    {
        if (context->m_it.GetResource().m_uri == p->name)
        {
            found = true;
            break;
        }
    }
    if (!found)
    {
        path = OCRepPayloadCreate();
        if (!path)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        /* oic.if.baseline is mandatory and it supports post */
        method = OCRepPayloadCreate();
        if (!method)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        parametersDim[0] = 2;
        dimTotal = calcDimTotal(parametersDim);
        parameters = (OCRepPayload **) OICCalloc(dimTotal, sizeof(OCRepPayload*));
        if (!parameters)
        {
            LOG(LOG_ERR, "Failed to allocate object array");
            goto exit;
        }
        parameters[0] = OCRepPayloadCreate();
        if (!parameters[0])
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropString(parameters[0], "name", "if") ||
                !OCRepPayloadSetPropString(parameters[0], "in", "query") ||
                !OCRepPayloadSetPropString(parameters[0], "type", "string"))
        {
            goto exit;
        }
        itfs = (char **) OICCalloc(context->m_it.GetResource().m_ifs.size(), sizeof(char *));
        if (!itfs)
        {
            LOG(LOG_ERR, "Failed to allocate string array");
            goto exit;
        }
        itfsDim[0] = 0;
        for (size_t i = 0; i < context->m_it.GetResource().m_ifs.size(); ++i)
        {
            /* Filter out read-only interfaces from post method */
            std::string &itf = context->m_it.GetResource().m_ifs[i];
            if (itf == "oic.if.ll" || itf == "oic.if.r" || itf == "oic.if.s")
            {
                continue;
            }
            itfs[itfsDim[0]++] = OICStrdup(itf.c_str());
        }
        if (!OCRepPayloadSetStringArrayAsOwner(parameters[0], "enum", itfs, itfsDim))
        {
            goto exit;
        }
        itfs = NULL;
        parameters[1] = OCRepPayloadCreate();
        if (!parameters[1])
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropString(parameters[1], "name", "body") ||
                !OCRepPayloadSetPropString(parameters[1], "in", "body"))
        {
            goto exit;
        }
        schema = OCRepPayloadCreate();
        if (!schema)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        oneOfDim[0] = context->m_it.GetResource().m_rts.size();
        dimTotal = calcDimTotal(oneOfDim);
        oneOf = (OCRepPayload **) OICCalloc(dimTotal, sizeof(OCRepPayload*));
        if (!oneOf)
        {
            LOG(LOG_ERR, "Failed to allocate object array");
            goto exit;
        }
        for (size_t i = 0; i < dimTotal; ++i)
        {
            oneOf[i] = OCRepPayloadCreate();
            if (!oneOf[i])
            {
                LOG(LOG_ERR, "Failed to create payload");
                goto exit;
            }
            ref = std::string("#/definitions/") + context->m_it.GetResource().m_rts[i];
            if (!OCRepPayloadSetPropString(oneOf[i], "$ref", ref.c_str()))
            {
                goto exit;
            }
        }
        if (!OCRepPayloadSetPropObjectArrayAsOwner(schema, "oneOf", oneOf, oneOfDim))
        {
            goto exit;
        }
        oneOf = NULL;
        /* schema will be re-used in "responses" (so no ...AsOwner here) */
        if (!OCRepPayloadSetPropObject(parameters[1], "schema", schema))
        {
            goto exit;
        }
        /* parameters will be re-used in "get" (so no ...AsOwner here) */
        if (!OCRepPayloadSetPropObjectArray(method, "parameters", (const OCRepPayload **)parameters,
                parametersDim))
        {
            goto exit;
        }
        responses = OCRepPayloadCreate();
        if (!responses)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        code = OCRepPayloadCreate();
        if (!code)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropString(code, "description", ""))
        {
            goto exit;
        }
        if (!OCRepPayloadSetPropObjectAsOwner(code, "schema", schema))
        {
            goto exit;
        }
        schema = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(responses, "200", code))
        {
            goto exit;
        }
        code = NULL;
        /* responses will be re-used in "get" (so no ...AsOwner here) */
        if (!OCRepPayloadSetPropObject(method, "responses", responses))
        {
            goto exit;
        }
        if (!OCRepPayloadSetPropObjectAsOwner(path, "post", method))
        {
            goto exit;
        }
        method = NULL;
        method = OCRepPayloadCreate();
        if (!method)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        itfs = (char **) OICCalloc(context->m_it.GetResource().m_ifs.size(), sizeof(char *));
        if (!itfs)
        {
            LOG(LOG_ERR, "Failed to allocate string array");
            goto exit;
        }
        itfsDim[0] = 0;
        for (size_t i = 0; i < context->m_it.GetResource().m_ifs.size(); ++i)
        {
            /* All interfaces support get method */
            itfs[itfsDim[0]++] = OICStrdup(context->m_it.GetResource().m_ifs[i].c_str());
        }
        if (!OCRepPayloadSetStringArrayAsOwner(parameters[0], "enum", itfs, itfsDim))
        {
            goto exit;
        }
        itfs = NULL;
        parametersDim[0] = 1; /* only use "if" parameter */
        if (!OCRepPayloadSetPropObjectArrayAsOwner(method, "parameters", parameters, parametersDim))
        {
            goto exit;
        }
        parameters = NULL;
        if (!OCRepPayloadSetPropObject(method, "responses", responses))
        {
            goto exit;
        }
        responses = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(path, "get", method))
        {
            goto exit;
        }
        method = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(context->m_paths,
                context->m_it.GetResource().m_uri.c_str(), path))
        {
            goto exit;
        }
        path = NULL;
    }

    if (++context->m_it != context->End())
    {
        OCStackResult result = thiz->ContinueDiscovery(context, context->m_it.GetUri().c_str(),
                context->m_it.GetDevAddrs(), Bridge::GetCB);
        if (result == OC_STACK_OK)
        {
            context = NULL;
        }
    }
    else
    {
        outPayload = OCRepPayloadCreate();
        if (!outPayload)
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto exit;
        }
        if (!OCRepPayloadSetPropObjectAsOwner(outPayload, "paths", context->m_paths))
        {
            goto exit;
        }
        context->m_paths = NULL;
        if (!OCRepPayloadSetPropObjectAsOwner(outPayload, "definitions", context->m_definitions))
        {
            goto exit;
        }
        context->m_definitions = NULL;
        thiz->ParseIntrospectionPayload(context, outPayload);
    }

exit:
    OCRepPayloadDestroy(outPayload);
    if (oneOf)
    {
        dimTotal = calcDimTotal(oneOfDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OCRepPayloadDestroy(oneOf[i]);
        }
        OICFree(oneOf);
    }
    OCRepPayloadDestroy(schema);
    OCRepPayloadDestroy(code);
    if (parameters)
    {
        dimTotal = calcDimTotal(parametersDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OCRepPayloadDestroy(parameters[i]);
        }
        OICFree(parameters);
    }
    OCRepPayloadDestroy(responses);
    OCRepPayloadDestroy(method);
    OCRepPayloadDestroy(path);
    OCRepPayloadDestroy(items);
    OCRepPayloadDestroy(itf);
    if (itfs)
    {
        dimTotal = calcDimTotal(itfsDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(itfs[i]);
        }
        OICFree(itfs);
    }
    if (rts)
    {
        dimTotal = calcDimTotal(rtsDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(rts[i]);
        }
        OICFree(rts);
    }
    OCRepPayloadDestroy(rt);
    OCRepPayloadDestroy(properties);
    OCRepPayloadDestroy(definition);
    thiz->m_discovered.erase(handle);
    delete context;
    return OC_STACK_DELETE_TRANSACTION;
}

typedef std::pair<std::string, std::string> Annotation;
typedef std::vector<Annotation> Annotations;

static const char *GetRefDefinition(const char *ref)
{
    static const char prefix[] = "#/definitions/";
    if (!strncmp(ref, prefix, sizeof(prefix) - 1))
    {
        return &ref[sizeof(prefix) - 1];
    }
    return NULL;
}

static const std::string EnumPrefix = "org.alljoyn.Bus.Enum.";
static const std::string DictPrefix = "org.alljoyn.Bus.Dict.";
static const std::string StructPrefix = "org.alljoyn.Bus.Struct.";

/* Returns a pair<D-Bus signature, org.alljoyn.Bus.Type.Name> */
static std::pair<std::string, std::string> GetSignature(OCRepPayload *obj,
        std::map<std::string, Annotations> &annotations)
{
    OCRepPayload **anyOf = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
    char *str = NULL;
    const char *ref = NULL;
    std::pair<std::string, std::string> sig;
    if (OCRepPayloadGetPropObjectArray(obj, "anyOf", &anyOf, dim))
    {
        sig.first = "v";
    }
    else if (OCRepPayloadGetPropString(obj, "$ref", &str) &&
            (ref = GetRefDefinition(str)))
    {
        Annotations &as = annotations[ref];
        if (!as.empty())
        {
            std::string &aName = as[0].first;
            if (aName.compare(0, EnumPrefix.size(), EnumPrefix) == 0)
            {
                sig.first = "x";
            }
            else if (aName.compare(0, DictPrefix.size(), DictPrefix) == 0)
            {
                sig.first = "a{" + as[0].second + as[1].second + "}";
            }
            else if (aName.compare(0, StructPrefix.size(), StructPrefix) == 0)
            {
                sig.first = "(";
                for (Annotation a : as)
                {
                    sig.first += a.second;
                }
                sig.first += ")";
            }
            sig.second = std::string("[") + ref + "]";
        }
    }
    else if (OCRepPayloadGetPropString(obj, "type", &str))
    {
        if (!strcmp(str, "boolean"))
        {
            sig.first = "b";
        }
        else if (!strcmp(str, "integer"))
        {
            double min = MIN_SAFE_INTEGER;
            double max = MAX_SAFE_INTEGER;
            OCRepPayloadGetPropDouble(obj, "minimum", &min);
            OCRepPayloadGetPropDouble(obj, "maximum", &max);
            if (min >= 0 && max <= UINT8_MAX)
            {
                sig.first = "y";
            }
            else if (min >= 0 && max <= UINT16_MAX)
            {
                sig.first = "q";
            }
            else if (min >= INT16_MIN && max <= INT16_MAX)
            {
                sig.first = "n";
            }
            else if (min >= 0 && max <= UINT32_MAX)
            {
                sig.first = "u";
            }
            else if (min >= INT32_MIN && max <= INT32_MAX)
            {
                sig.first = "i";
            }
            else if (min >= 0)
            {
                sig.first = "t";
            }
            else
            {
                sig.first = "x";
            }
        }
        else if (!strcmp(str, "number"))
        {
            sig.first = "d";
        }
        else if (!strcmp(str, "string"))
        {
            char *format = NULL;
            OCRepPayload *media = NULL;
            char *encoding = NULL;
            OCRepPayloadGetPropString(obj, "format", &format);
            OCRepPayloadGetPropObject(obj, "media", &media);
            if (format && !strcmp(format, "uint64"))
            {
                sig.first = "t";
            }
            else if (format && !strcmp(format, "int64"))
            {
                sig.first = "x";
            }
            else if (media && OCRepPayloadGetPropString(media, "binaryEncoding", &encoding) &&
                    !strcmp(encoding, "base64"))
            {
                sig.first = "ay";
            }
            else
            {
                sig.first = "s";
            }
            OICFree(encoding);
            OCRepPayloadDestroy(media);
            OICFree(format);
        }
        else if (!strcmp(str, "object"))
        {
            sig.first = "a{sv}";
        }
        else if (!strcmp(str, "array"))
        {
            OCRepPayload *items = NULL;
            if (OCRepPayloadGetPropObject(obj, "items", &items))
            {
                std::pair<std::string, std::string> itemSig = GetSignature(items, annotations);
                sig.first = "a" + itemSig.first;
                if (!itemSig.second.empty())
                {
                    sig.second = "a" + itemSig.second;
                }
                OCRepPayloadDestroy(items);
            }
            else
            {
                sig.first = "av";
            }
        }
        else
        {
            LOG(LOG_INFO, "Unhandled type %s", str);
        }
    }
    else
    {
        LOG(LOG_INFO, "Missing \"anyOf\", \"$ref\", or \"type\" property");
    }
    size_t dimTotal = calcDimTotal(dim);
    for (size_t i = 0; i < dimTotal; ++i)
    {
        OCRepPayloadDestroy(anyOf[i]);
    }
    OICFree(anyOf);
    OICFree(str);
    return sig;
}

static void AddAnnotations(ajn::InterfaceDescription *iface, std::string propertyName,
        OCRepPayload *obj, std::map<std::string, Annotations> &annotations)
{
    char *str = NULL;
    const char *ref = NULL;
    if (OCRepPayloadGetPropString(obj, "$ref", &str) &&
            (ref = GetRefDefinition(str)))
    {
        for (Annotation &a : annotations[ref])
        {
            iface->AddAnnotation(a.first, a.second);
        }
    }
    else if (OCRepPayloadGetPropString(obj, "type", &str))
    {
        if (!strcmp(str, "array"))
        {
            OCRepPayload *items = NULL;
            if (OCRepPayloadGetPropObject(obj, "items", &items))
            {
                AddAnnotations(iface, propertyName, items, annotations);
                OCRepPayloadDestroy(items);
            }
        }
    }
    OICFree(str);
}

static void AddProperty(ajn::InterfaceDescription *iface, OCRepPayloadValue *property,
        bool isObservable, std::map<std::string, Annotations> &annotations)
{
    std::pair<std::string, std::string> sig = GetSignature(property->obj, annotations);
    if (sig.first.empty())
    {
        LOG(LOG_INFO, "%s property unknown type, skipping", property->name);
        return;
    }
    uint8_t access = ajn::PROP_ACCESS_RW;
    bool readOnly;
    if (OCRepPayloadGetPropBool(property->obj, "readOnly", &readOnly) && readOnly)
    {
        access = ajn::PROP_ACCESS_READ;
    }
    std::string ajPropName = ToAJName(property->name);
    iface->AddProperty(ajPropName.c_str(), sig.first.c_str(), access);
    if (!sig.second.empty())
    {
        iface->AddPropertyAnnotation(ajPropName, "org.alljoyn.Bus.Type.Name", sig.second);
    }
    AddAnnotations(iface, ajPropName, property->obj, annotations);
    double d;
    if (OCRepPayloadGetPropDouble(property->obj, "default", &d) &&
            (floor(d) == d))
    {
        iface->AddPropertyAnnotation(ajPropName, "org.alljoyn.Bus.Type.Default",
                (d > 0) ? std::to_string((uint64_t) d) : std::to_string((int64_t) d));
    }
    if (OCRepPayloadGetPropDouble(property->obj, "maximum", &d) &&
            (floor(d) == d))
    {
        iface->AddPropertyAnnotation(ajPropName, "org.alljoyn.Bus.Type.Max",
                (d > 0) ? std::to_string((uint64_t) d) : std::to_string((int64_t) d));
    }
    if (OCRepPayloadGetPropDouble(property->obj, "minimum", &d) &&
            (floor(d) == d))
    {
        iface->AddPropertyAnnotation(ajPropName, "org.alljoyn.Bus.Type.Min",
                (d > 0) ? std::to_string((uint64_t) d) : std::to_string((int64_t) d));
    }
    if (isObservable)
    {
        /* "false" is the default value */
        iface->AddPropertyAnnotation(ajPropName, ajn::org::freedesktop::DBus::AnnotateEmitsChanged,
                "true");
    }
}

static void AddInterface(VirtualBusObject *obj, const char *refValue,
        std::map<std::string, std::string> &ajNames)
{
    const char *def = GetRefDefinition(refValue);
    if (def && ajNames.find(def) != ajNames.end())
    {
        obj->AddInterface(ajNames[def].c_str());
    }
    else
    {
        LOG(LOG_INFO, "Missing definition of %s", def);
    }
}

static void AddInterface(VirtualBusObject *obj, OCRepPayload *schema,
        std::map<std::string, std::string> &ajNames)
{
    if (!schema->values)
    {
        return;
    }
    if (!strcmp(schema->values->name, "$ref") &&
            schema->values->type == OCREP_PROP_STRING)
    {
        AddInterface(obj, schema->values->str, ajNames);
    }
    else if (!strcmp(schema->values->name, "oneOf") &&
            schema->values->type == OCREP_PROP_ARRAY &&
            schema->values->arr.type == OCREP_PROP_OBJECT)
    {
        OCRepPayload **objArray = schema->values->arr.objArray;
        for (size_t j = 0; j < schema->values->arr.dimensions[0]; ++j)
        {
            AddInterface(obj, objArray[j], ajNames);
        }
    }
}

void Bridge::ParseIntrospectionPayload(DiscoverContext *context, OCRepPayload *payload)
{
    OCRepPayload *definitions = NULL;
    OCRepPayload *paths = NULL;
    OCPresence *presence = NULL;
    Resource *resource;
    std::map<std::string, bool> isObservable; /* rt => isObservable */
    std::map<std::string, Annotations> annotations; /* definition => annotations */
    std::map<std::string, std::string> ajNames; /* definition => ifaceName */
    QStatus status;

    /*
     * Figure out which resource types are observable so that the properties can be annotated with
     * the correct EmitsChanged value.  On the OC side it is resources that are observable, not
     * resource types.  So in the worst case where a resource type is used by two resources, one of
     * which is observable and one which is not, we set EmitsChanged to false.
     */
    for (auto &r : context->m_device.m_resources)
    {
        for (auto &rt : r.m_rts)
        {
            if (isObservable.find(rt) == isObservable.end())
            {
                isObservable[rt] = r.m_isObservable;
            }
            else
            {
                isObservable[rt] = isObservable[rt] && r.m_isObservable;
            }
        }
    }

    if (!OCRepPayloadGetPropObject(payload, "definitions", &definitions))
    {
        goto exit;
    }
    /* Look for struct definitions first since they will be needed by resource types */
    for (OCRepPayloadValue *definition = definitions->values; definition;
         definition = definition->next)
    {
        if (definition->type != OCREP_PROP_OBJECT)
        {
            LOG(LOG_INFO, "%s unknown type %d, skipping", definition->name, definition->type);
            continue;
        }
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
        OCRepPayload **oneOf = NULL;
        char *type = NULL;
        if (OCRepPayloadGetPropObjectArray(definition->obj, "oneOf", &oneOf, dim))
        {
            size_t dimTotal = calcDimTotal(dim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                char *enumName = NULL;
                double *enumValue = NULL;
                size_t dimEnumValue[MAX_REP_ARRAY_DEPTH] = { 0 };
                if (OCRepPayloadGetPropString(oneOf[i], "title", &enumName) &&
                        OCRepPayloadGetDoubleArray(oneOf[i], "enum", &enumValue, dimEnumValue) &&
                        (calcDimTotal(dimEnumValue) == 1))
                {
                    std::string Enum = EnumPrefix + definition->name;
                    /* Assume enum value is in range of an int64_t */
                    annotations[definition->name].push_back(Annotation(Enum + ".Value." + enumName,
                            std::to_string((int64_t) enumValue[0])));
                }
                OICFree(enumValue);
                OICFree(enumName);
            }
        }
        else if (OCRepPayloadGetPropString(definition->obj, "type", &type) &&
                !strcmp(type, "object"))
        {
            OCRepPayload *properties = NULL;
            OCRepPayload *rt = NULL;
            if (!OCRepPayloadGetPropObject(definition->obj, "properties", &properties))
            {
                std::string Dict = DictPrefix + definition->name;
                annotations[definition->name].push_back(Annotation(Dict + ".Key.Type", "s"));
                annotations[definition->name].push_back(Annotation(Dict + ".Value.Type", "v"));
            }
            else if (!OCRepPayloadGetPropObject(properties, "rt", &rt))
            {
                for (OCRepPayloadValue *property = properties->values; property;
                     property = property->next)
                {
                    if (property->type != OCREP_PROP_OBJECT)
                    {
                        LOG(LOG_INFO, "%s property unknown type %d, skipping", property->name,
                                property->type);
                        continue;
                    }
                    std::string unused;
                    std::string Struct = StructPrefix + definition->name;
                    annotations[definition->name].push_back(Annotation(Struct + ".Field." +
                            property->name + ".Type", GetSignature(property->obj, annotations).first));
                }
            }
            OCRepPayloadDestroy(rt);
            OCRepPayloadDestroy(properties);
        }
        else
        {
            LOG(LOG_INFO, "%s unsupported \"type\" value \"%s\", skipping", definition->name, type);
        }
        OICFree(type);
        if (oneOf)
        {
            size_t dimTotal = calcDimTotal(dim);
            for(size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(oneOf[i]);
            }
        }
        OICFree(oneOf);
    }
    /* Look for resource types next */
    for (OCRepPayloadValue *definition = definitions->values; definition;
         definition = definition->next)
    {
        if (definition->type != OCREP_PROP_OBJECT)
        {
            LOG(LOG_INFO, "%s unknown type %d, skipping", definition->name, definition->type);
            continue;
        }
        OCRepPayload *properties = NULL;
        OCRepPayload *rt = NULL;
        char **rts = NULL;
        size_t rtsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
        std::string ifaceName;
        ajn::InterfaceDescription *iface;
        if (!OCRepPayloadGetPropObject(definition->obj, "properties", &properties))
        {
            LOG(LOG_INFO, "%s missing \"properties\", skipping", definition->name);
            goto next_iface;
        }
        if (!OCRepPayloadGetPropObject(properties, "rt", &rt))
        {
            LOG(LOG_INFO, "%s missing \"rt\" property, skipping", definition->name);
            goto next_iface;
        }
        if (!OCRepPayloadGetStringArray(rt, "default", &rts, rtsDim) || !rtsDim[0])
        {
            LOG(LOG_INFO, "%s missing or empty \"default\" property, skipping", definition->name);
            goto next_iface;
        }
        ifaceName = ToAJName(rts[0]);
        ajNames[definition->name] = ifaceName;
        iface = context->m_bus->CreateInterface(ifaceName.c_str());
        if (!iface)
        {
            LOG(LOG_ERR, "CreateInterface %s failed", ifaceName.c_str());
            goto next_iface;
        }
        LOG(LOG_INFO, "Created interface %s", ifaceName.c_str());
        if (strstr(ifaceName.c_str(), "oic.d.") == ifaceName.c_str())
        {
            /* Device types are translated as empty interfaces */
            iface->Activate();
            goto next_iface;
        }
        for (OCRepPayloadValue *property = properties->values; property; property = property->next)
        {
            if (property->type != OCREP_PROP_OBJECT)
            {
                LOG(LOG_INFO, "%s property unknown type %d, skipping", property->name,
                        property->type);
                continue;
            }
            if (!strcmp(property->name, "rt"))
            {
                /* "rt" property is special-cased above */
                continue;
            }
            else if (!strcmp(property->name, "if") ||
                    !strcmp(property->name, "p") ||
                    !strcmp(property->name, "n") ||
                    !strcmp(property->name, "id"))
            {
                /* Ignore baseline properties */
                continue;
            }
            AddProperty(iface, property, isObservable[rts[0]], annotations);
        }
        iface->Activate();
    next_iface:
        size_t dimTotal = calcDimTotal(rtsDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(rts[i]);
        }
        OICFree(rts);
        rts = NULL;
        OCRepPayloadDestroy(rt);
        rt = NULL;
        OCRepPayloadDestroy(properties);
        properties = NULL;
    }
    OCRepPayloadDestroy(definitions);
    definitions = NULL;

    if (!OCRepPayloadGetPropObject(payload, "paths", &paths))
    {
        goto exit;
    }
    for (OCRepPayloadValue *path = paths->values; path; path = path->next)
    {
        if (path->type != OCREP_PROP_OBJECT)
        {
            LOG(LOG_INFO, "%s path unknown type %d, skipping", path->name, path->type);
            continue;
        }
        if (!strcmp(path->name, "/oic/d") || !strcmp(path->name, "/oic/p"))
        {
            /* These resources are not translated on-the-fly */
        }
        else if (!strcmp(path->name, "/oic/con") || !strcmp(path->name, "/oic/mnt"))
        {
            VirtualBusObject *obj = context->m_bus->GetBusObject("/Config");
            if (!obj)
            {
                obj = new VirtualConfigBusObject(context->m_bus, context->GetDevAddrs(path->name));
                status = context->m_bus->RegisterBusObject(obj);
                if (status != ER_OK)
                {
                    delete obj;
                }
            }
        }
        else
        {
            VirtualBusObject *obj = new VirtualBusObject(context->m_bus, path->name,
                    context->GetDevAddrs(path->name));
            for (OCRepPayloadValue *method = path->obj->values; method; method = method->next)
            {
                if (method->type != OCREP_PROP_OBJECT)
                {
                    LOG(LOG_INFO, "%s method unknown type %d, skipping", method->name,
                            method->type);
                    continue;
                }
                for (OCRepPayloadValue *value = method->obj->values; value; value = value->next)
                {
                    if (!strcmp(value->name, "parameters"))
                    {
                        if (value->type != OCREP_PROP_ARRAY || value->arr.type != OCREP_PROP_OBJECT)
                        {
                            LOG(LOG_INFO, "%s unknown type %d, skipping", value->name,
                                    value->type);
                            continue;
                        }
                        for (size_t i = 0; i < value->arr.dimensions[0]; ++i)
                        {
                            OCRepPayload *schema = NULL;
                            if (OCRepPayloadGetPropObject(value->arr.objArray[i], "schema",
                                    &schema))
                            {
                                AddInterface(obj, schema, ajNames);
                            }
                            OCRepPayloadDestroy(schema);
                            schema = NULL;
                        }
                    }
                    else if (!strcmp(value->name, "responses"))
                    {
                        if (value->type != OCREP_PROP_OBJECT)
                        {
                            LOG(LOG_INFO, "%s unknown type %d, skipping", value->name,
                                    value->type);
                            continue;
                        }
                        for (OCRepPayloadValue *code = value->obj->values; code; code = code->next)
                        {
                            if (code->type != OCREP_PROP_OBJECT)
                            {
                                LOG(LOG_INFO, "%s code unknown type %d, skipping", code->name,
                                        code->type);
                                continue;
                            }
                            for (OCRepPayloadValue *codeValue = code->obj->values; codeValue;
                                 codeValue = codeValue->next)
                            {
                                if (!strcmp(codeValue->name, "schema") &&
                                        codeValue->type == OCREP_PROP_OBJECT)
                                {
                                    AddInterface(obj, codeValue->obj, ajNames);
                                }
                            }
                        }
                    }
                }
            }
            status = context->m_bus->RegisterBusObject(obj);
            if (status != ER_OK)
            {
                delete obj;
            }
        }
    }
    OCRepPayloadDestroy(paths);
    paths = NULL;

    /* Done */
    resource = context->m_device.GetResourceUri(OC_RSRVD_DEVICE_URI);
    if (resource)
    {
        VirtualBusObject *obj = new VirtualBusObject(context->m_bus, OC_RSRVD_DEVICE_URI,
                resource->m_addrs);
        for (auto &rt : resource->m_rts)
        {
            if (TranslateResourceType(rt.c_str()))
            {
                obj->AddInterface(ToAJName(rt).c_str(), true);
            }
        }
        obj->AddInterface(ToAJName("oic.d.virtual").c_str(), true);
        status = context->m_bus->RegisterBusObject(obj);
        if (status != ER_OK)
        {
            delete obj;
        }
    }
    presence = new OCPresence(context->m_device.m_di.c_str(), DISCOVER_PERIOD_SECS);
    if (!presence)
    {
        LOG(LOG_ERR, "new OCPresence() failed");
        goto exit;
    }
    m_presence.push_back(presence);
    status = context->m_bus->Announce();
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "Announce() failed - %s", QCC_StatusText(status));
        goto exit;
    }
    m_virtualBusAttachments.push_back(context->m_bus);
    context->m_bus = NULL; /* context->m_bus now belongs to thiz */

exit:
    OCRepPayloadDestroy(paths);
    OCRepPayloadDestroy(definitions);
}

/* Called with m_mutex held. */
OCRepPayload *Bridge::GetSecureMode(OCEntityHandlerRequest *request)
{
    OCRepPayload *payload = CreatePayload(request->resource, request->query);
    if (!OCRepPayloadSetPropBool(payload, "secureMode", m_secureMode))
    {
        OCRepPayloadDestroy(payload);
        payload = NULL;
    }
    return payload;
}

/* Called with m_mutex held. */
bool Bridge::PostSecureMode(OCEntityHandlerRequest *request, bool &hasChanged)
{
    OCRepPayload *payload = (OCRepPayload *) request->payload;
    bool secureMode;
    if (!OCRepPayloadGetPropBool(payload, "secureMode", &secureMode))
    {
        return false;
    }
    hasChanged = (m_secureMode != secureMode);
    m_secureMode = secureMode;
    return true;
}

OCEntityHandlerResult Bridge::EntityHandlerCB(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    LOG(LOG_INFO, "[%p] flag=%x,request=%p,ctx=%p",
        ctx, flag, request, ctx);

    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    thiz->m_mutex.lock();
    bool hasChanged = false;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = thiz->GetSecureMode(request);
                if (!payload)
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
                break;
            }
        case OC_REST_POST:
            {
                if (!request->payload || request->payload->type != PAYLOAD_TYPE_REPRESENTATION)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                if (!thiz->PostSecureMode(request, hasChanged))
                {
                    result = OC_EH_ERROR;
                    break;
                }
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *outPayload = thiz->GetSecureMode(request);
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = DoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "DoResponse - %d", doResult);
                    OCRepPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    thiz->m_mutex.unlock();
    if (hasChanged)
    {
        OCNotifyAllObservers(request->resource, OC_NA_QOS);
    }
    return result;
}

static const char *SeenStateText[] = { "NOT_SEEN", "SEEN_NATIVE", "SEEN_VIRTUAL" };

/* Called with m_mutex held. */
Bridge::SeenState Bridge::GetSeenState(const char *piid)
{
    SeenState state = NOT_SEEN;

    /* Check what we've seen on the AJ side. */
    if (piid && (NOT_SEEN == state))
    {
        state = m_seenStateCb(piid);
    }

    /* Check what we've seen on the OC side. */
    if (piid && (NOT_SEEN == state))
    {
        VirtualBusAttachment *bus = NULL;
        for (auto &dc : m_discovered)
        {
            DiscoverContext *discoverContext = dc.second;
            if (discoverContext->m_bus &&
                    (discoverContext->m_bus->GetProtocolIndependentId() == piid))
            {
                bus = discoverContext->m_bus;
                break;
            }
        }
        if (!bus)
        {
            for (VirtualBusAttachment *busAttachment : m_virtualBusAttachments)
            {
                if (busAttachment->GetProtocolIndependentId() == piid)
                {
                    bus = busAttachment;
                    break;
                }
            }
        }
        if (bus)
        {
            state = bus->IsVirtual() ? SEEN_VIRTUAL : SEEN_NATIVE;
        }
    }

    LOG(LOG_INFO, "piid=%s,state=%s", piid, SeenStateText[state]);
    return state;
}

/* Called with m_mutex held. */
void Bridge::AnnouncedTask::Run(Bridge *thiz)
{
    switch (thiz->GetSeenState(m_piid.c_str()))
    {
        case NOT_SEEN:
            thiz->m_execCb(m_piid.c_str(), m_name.c_str(), m_isVirtual);
            break;
        case SEEN_NATIVE:
            /* Do nothing */
            break;
        case SEEN_VIRTUAL:
            if (m_isVirtual)
            {
                /* Do nothing */
            }
            else
            {
                thiz->DestroyPiid(m_piid.c_str());
                thiz->m_execCb(m_piid.c_str(), m_name.c_str(), m_isVirtual);
            }
            break;
    }
}

/* Called with m_mutex held. */
void Bridge::DiscoverTask::Run(Bridge *thiz)
{
    OCStackResult result = OC_STACK_ERROR;
    DiscoverContext *context = NULL;
    bool isVirtual;
    for (std::map<OCDoHandle, DiscoverContext *>::iterator it = thiz->m_discovered.begin();
         it != thiz->m_discovered.end(); ++it)
    {
        if (it->second == m_context)
        {
            context = it->second;
        }
    }
    if (!context)
    {
        goto exit;
    }

    isVirtual = context->m_device.IsVirtual();
    switch (thiz->GetSeenState(m_piid.c_str()))
    {
        case NOT_SEEN:
            context->m_bus = VirtualBusAttachment::Create(context->m_device.m_di.c_str(),
                    m_piid.c_str(), isVirtual);
            break;
        case SEEN_NATIVE:
            /* Do nothing */
            goto exit;
        case SEEN_VIRTUAL:
            if (isVirtual)
            {
                /* Do nothing */
            }
            else
            {
                thiz->DestroyPiid(m_piid.c_str());
                context->m_bus = VirtualBusAttachment::Create(context->m_device.m_di.c_str(),
                        m_piid.c_str(), isVirtual);
            }
            break;
    }
    if (!context->m_bus)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(OC_RSRVD_DEVICE_URI, m_payload);
    result = thiz->ContinueDiscovery(context, OC_RSRVD_PLATFORM_URI,
            context->GetDevAddrs(OC_RSRVD_PLATFORM_URI), Bridge::GetPlatformCB);
    if (result == OC_STACK_OK)
    {
        context = NULL;
    }
exit:
    delete context;
}

/* Called with m_mutex held. */
void Bridge::DestroyPiid(const char *piid)
{
    /* Destroy virtual OC devices */
    m_killCb(piid);

    /* Destroy virtual AJ devices */
    std::string di;
    for (auto &dc : m_discovered)
    {
        DiscoverContext *discoverContext = dc.second;
        if (discoverContext->m_bus &&
                (discoverContext->m_bus->GetProtocolIndependentId() == piid))
        {
            di = discoverContext->m_bus->GetDi();
            break;
        }
    }
    if (di.empty())
    {
        for (VirtualBusAttachment *busAttachment : m_virtualBusAttachments)
        {
            if (busAttachment->GetProtocolIndependentId() == piid)
            {
                di = busAttachment->GetDi();
                break;
            }
        }
    }
    if (!di.empty())
    {
        Destroy(di.c_str());
    }
}

void Bridge::RDPublish()
{
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_rdPublishTask)
    {
        /* Delay the pending publication to give time for multiple resources to be created. */
        m_rdPublishTask->m_tick = time(NULL) + 1;
    }
    else
    {
        m_rdPublishTask = new RDPublishTask(time(NULL) + 1);
        m_tasks.push_back(m_rdPublishTask);
    }
}

/* Called with m_mutex held. */
void Bridge::RDPublishTask::Run(Bridge *thiz)
{
    LOG(LOG_INFO, "[%p] thiz=%p", this, thiz);

    /* Also write out current introspection data. */
    OCPersistentStorage *ps = OCGetPersistentStorageHandler();
    assert(ps);
    FILE *fp = NULL;
    size_t ret;
    std::string s;
    std::ostringstream os;
    OCStackResult result = Introspect(os, thiz->m_bus, thiz->m_ajSoftwareVersion.c_str(), "TITLE",
            "VERSION");
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "Introspect() failed - %d", result);
        goto exit;
    }
    s = os.str();
    fp = ps->open(OC_INTROSPECTION_FILE_NAME, "wb");
    if (!fp)
    {
        LOG(LOG_ERR, "open failed");
        goto exit;
    }
    ret = ps->write(s.c_str(), 1, s.size(), fp);
    if (ret != s.size())
    {
        LOG(LOG_ERR, "write failed");
        goto exit;
    }
exit:
    if (fp)
    {
        ps->close(fp);
    }

    ::RDPublish();
    thiz->m_rdPublishTask = NULL;
}

bool GetPiid(OCUUIdentity *piid, const char *peerGuid, ajn::AboutData *aboutData)
{
    if (peerGuid && peerGuid[0])
    {
        return sscanf(peerGuid, "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx"
                "%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx%02hhx",
                &piid->id[0], &piid->id[1], &piid->id[2], &piid->id[3], &piid->id[4], &piid->id[5],
                &piid->id[6], &piid->id[7], &piid->id[8], &piid->id[9], &piid->id[10], &piid->id[11],
                &piid->id[12], &piid->id[13], &piid->id[14], &piid->id[15]) == 16;
    }
    else
    {
        ajn::MsgArg *piidArg = NULL;
        aboutData->GetField("org.openconnectivity.piid", piidArg);
        char *piidStr = NULL;
        if (piidArg && (ER_OK == piidArg->Get("s", &piidStr)))
        {
            return (piidStr && OCConvertStringToUuid(piidStr, piid->id));
        }
        else
        {
            char *deviceId;
            aboutData->GetDeviceId(&deviceId);
            uint8_t *appId;
            size_t n;
            aboutData->GetAppId(&appId, &n);
            DeriveUniqueId(piid, deviceId, appId, n);
            return true;
        }
    }
}
