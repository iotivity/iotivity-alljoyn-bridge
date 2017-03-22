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

#include "cacommon.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include <alljoyn/AllJoynStd.h>
#include "Security.h"
#include "Name.h"
#include "Plugin.h"
#include "Presence.h"
#include "Resource.h"
#include "VirtualBusAttachment.h"
#include "VirtualBusObject.h"
#include "VirtualConfigBusObject.h"
#include "VirtualConfigurationResource.h"
#include "VirtualDevice.h"
#include "VirtualResource.h"
#include <algorithm>
#include <deque>
#include <iterator>

static void GetDevAddr(OCDevAddr *addr, const OCDevAddr *srcAddr, const char *di,
        const OCEndpointPayload *eps)
{
    const OCEndpointPayload *ep = eps;
    if (ep)
    {
        if (!strcmp(ep->tps, "coap") || !strcmp(ep->tps, "coaps"))
        {
            addr->adapter = OC_ADAPTER_IP;
        }
        else if (!strcmp(ep->tps, "coap+tcp") || !strcmp(ep->tps, "coaps+tcp"))
        {
            addr->adapter = OC_ADAPTER_TCP;
        }
        addr->flags = ep->family;
        addr->port = ep->port;
        strncpy(addr->addr, ep->addr, MAX_ADDR_STR_SIZE);
        addr->ifindex = 0;
        addr->routeData[0] = '\0';
        strncpy(addr->remoteId, di, MAX_IDENTITY_SIZE);
    }
    else
    {
        *addr = *srcAddr;
    }
}

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

struct Bridge::DiscoverContext
{
    Bridge *m_bridge;
    std::string m_di;
    bool m_isVirtual;
    VirtualBusAttachment *m_bus;
    struct Resource
    {
        Resource(const OCDevAddr *srcAddr, const char *di, const OCEndpointPayload *eps,
                uint8_t bitmap, bool hasMultipleRts)
            : m_bitmap(bitmap), m_hasMultipleRts(hasMultipleRts)
        {
            GetDevAddr(&m_devAddr, srcAddr, di, eps);
        }
        OCDevAddr m_devAddr;
        uint8_t m_bitmap;
        bool m_hasMultipleRts;
    };
    std::map<std::string, Resource> m_resources;
    VirtualBusObject *m_obj;
    DiscoverContext(Bridge *bridge, const char *di, bool isVirtual)
        : m_bridge(bridge), m_di(di), m_isVirtual(isVirtual), m_bus(NULL), m_obj(NULL) { }
    ~DiscoverContext() { delete m_obj; delete m_bus; }
};

Bridge::Bridge(const char *name, Protocol protocols)
    : m_execCb(NULL), m_sessionLostCb(NULL), m_protocols(protocols), m_sender(NULL),
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(false), m_rdPublishTask(NULL)
{
    m_bus = new ajn::BusAttachment(name, true);
    m_ajState = CREATED;
    m_ajSecurity = new AllJoynSecurity(m_bus, AllJoynSecurity::CONSUMER);
    m_ocSecurity = new OCSecurity();
}

Bridge::Bridge(const char *name, const char *sender)
    : m_execCb(NULL), m_sessionLostCb(NULL), m_protocols(AJ), m_sender(sender),
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(false), m_rdPublishTask(NULL)
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
        std::lock_guard<std::mutex> lock(m_mutex);
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
        if (context->m_di == id)
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
            OCStackResult result = DoResource(&m_discoverHandle, OC_REST_DISCOVER,
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
        AnnouncedContext(VirtualDevice *device, const char *name, const ajn::MsgArg &objectDescriptionArg)
            : m_device(device), m_name(name), m_objectDescriptionArg(objectDescriptionArg), m_aboutObj(NULL) { }
        ~AnnouncedContext() { delete m_aboutObj; }
        VirtualDevice *m_device;
        std::string m_name;
        ajn::MsgArg m_objectDescriptionArg;
        ajn::ProxyBusObject *m_aboutObj;
};

void Bridge::Announced(const char *name, uint16_t version, ajn::SessionPort port,
                       const ajn::MsgArg &objectDescriptionArg, const ajn::MsgArg &aboutDataArg)
{
    QStatus status;
    AnnouncedContext *context;
    ajn::SessionOpts opts;

    ajn::AboutData aboutData(aboutDataArg);
    OCUUIdentity piid;
    GetPiid(&piid, &aboutData);
    char piidStr[UUID_STRING_SIZE];
    OCConvertUuidToString(piid.id, piidStr);

    LOG(LOG_INFO, "[%p] name=%s,version=%u,port=%u,piid=%s", this, name, version, port, piidStr);

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

    if (!m_sender)
    {
        bool isVirtual = ajn::AboutObjectDescription(objectDescriptionArg).HasInterface("oic.d.virtual");
        switch (GetSeenState(piidStr))
        {
        case NOT_SEEN:
            m_execCb(piidStr, name, isVirtual);
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
                m_tasks.push_back(new AnnouncedTask(time(NULL) + 10, name, piidStr, isVirtual));
            }
            break;
        }
        m_mutex.unlock();
        return;
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

    context = new AnnouncedContext(device, name, objectDescriptionArg);
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
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    AnnouncedContext *context = reinterpret_cast<AnnouncedContext *>(ctx);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "JoinSessionCB - %s", QCC_StatusText(status));
    }
    else
    {
        context->m_aboutObj = new ajn::ProxyBusObject(*m_bus, context->m_name.c_str(), "/About", sessionId);
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
        delete context;
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
            VirtualDevice *device = new VirtualDevice(msg->GetSender(), msg->GetSessionId());
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
    }
    delete context;
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

OCStackApplicationResult Bridge::DiscoverCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    if (!response || response->result != OC_STACK_OK)
    {
        return OC_STACK_KEEP_TRANSACTION;
    }
    OCDiscoveryPayload *payload;
    for (payload = (OCDiscoveryPayload *) response->payload; payload; payload = payload->next)
    {
        bool isVirtual = false;
        DiscoverContext *context = NULL;
        OCDevAddr devAddr;
        OCCallbackData cbData;
        OCDoHandle cbHandle = 0;
        std::map<std::string, DiscoverContext::Resource> resources;

        if (!strcmp(payload->sid, GetServerInstanceIDString()))
        {
            /* Ignore responses from self */
            goto next;
        }

        /* Update presence status */
        for (std::vector<Presence *>::iterator it = thiz->m_presence.begin();
             it != thiz->m_presence.end(); ++it)
        {
            if ((*it)->GetId() == payload->sid)
            {
                (*it)->Seen();
            }
        }

        /* Check if we've seen this response before */
        for (std::vector<VirtualBusAttachment *>::iterator it = thiz->m_virtualBusAttachments.begin();
             it != thiz->m_virtualBusAttachments.end(); ++it)
        {
            if ((*it)->GetDi() == payload->sid)
            {
                goto next;
            }
        }
        for (std::map<OCDoHandle, DiscoverContext *>::iterator it = thiz->m_discovered.begin();
             it != thiz->m_discovered.end(); ++it)
        {
            if (it->second->m_di == payload->sid)
            {
                goto next;
            }
        }

        /* Check if this is a virtual device */
        for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
             resource = resource->next)
        {
            for (OCStringLL *type = resource->types; type; type = type->next)
            {
                if (!strcmp(type->value, "oic.d.virtual"))
                {
                    isVirtual = true;
                    break;
                }
            }
        }

        for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
             resource = resource->next)
        {
            bool hasMultipleRts = resource->types && resource->types->next;
            for (OCStringLL *type = resource->types; type; type = type->next)
            {
                if (!strcmp(resource->uri, OC_RSRVD_DEVICE_URI))
                {
                    GetDevAddr(&devAddr, &response->devAddr, payload->sid, resource->eps);
                }
                if (!TranslateResourceType(type->value))
                {
                    continue;
                }
                std::string uri = resource->uri;
                uri += std::string("?rt=") + type->value;
                resources.insert(std::pair<std::string, DiscoverContext::Resource>(uri,
                                DiscoverContext::Resource(&response->devAddr, payload->sid, resource->eps, resource->bitmap, hasMultipleRts)));
            }
        }
        if (resources.empty())
        {
            continue;
        }
        resources.insert(std::pair<std::string, DiscoverContext::Resource>("/oic/d?rt=oic.d.virtual",
                        DiscoverContext::Resource(&response->devAddr, payload->sid, NULL, OC_DISCOVERABLE, true)));

        context = new DiscoverContext(thiz, payload->sid, isVirtual);
        context->m_resources = resources;

        cbData.cb = Bridge::GetDeviceCB;
        cbData.context = thiz;
        cbData.cd = NULL;
        result = DoResource(&cbHandle, OC_REST_GET, OC_RSRVD_DEVICE_URI, &devAddr, NULL, &cbData,
                NULL, 0);
        if (result == OC_STACK_OK)
        {
            thiz->m_discovered[cbHandle] = context;
        }
        else
        {
            LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
        }
next:
        if (result != OC_STACK_OK)
        {
            thiz->m_discovered.erase(handle);
            delete context;
            context = NULL;
        }
    }
    return OC_STACK_KEEP_TRANSACTION;
}

OCStackApplicationResult Bridge::GetDeviceCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    DiscoverContext *context = NULL;
    OCStackResult result = OC_STACK_ERROR;
    OCCallbackData cbData;
    OCDoHandle cbHandle = 0;
    OCRepPayload *payload;
    char *piid = NULL;
    std::map<OCDoHandle, DiscoverContext *>::iterator it = thiz->m_discovered.find(handle);
    if (it == thiz->m_discovered.end())
    {
        goto exit;
    }
    context = it->second;
    if (!response)
    {
        goto exit;
    }
    if (response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_INFO, "[%p] Missing /oic/d", thiz);
        goto exit;
    }
    if (response->payload && response->payload->type != PAYLOAD_TYPE_REPRESENTATION)
    {
        LOG(LOG_INFO, "[%p] Unexpected /oic/d payload type: %d", thiz,
            response->payload->type);
        goto exit;
    }
    payload = (OCRepPayload *) response->payload;
    OCRepPayloadGetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, &piid);
    switch (thiz->GetSeenState(piid))
    {
        case NOT_SEEN:
            context->m_bus = VirtualBusAttachment::Create(context->m_di.c_str(), piid,
                    context->m_isVirtual);
            break;
        case SEEN_NATIVE:
            /* Do nothing */
            goto exit;
        case SEEN_VIRTUAL:
            if (context->m_isVirtual)
            {
                /* Do nothing */
            }
            else
            {
                /* Delay creating virtual objects from a virtual device */
                LOG(LOG_INFO, "[%p] Delaying creation of virtual objects from a virtual device",
                        thiz);
                thiz->m_tasks.push_back(new DiscoverTask(time(NULL) + 10, piid, payload, &response->devAddr, context));
                result = OC_STACK_OK;
            }
            goto exit;
    }
    if (!context->m_bus)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(OC_RSRVD_DEVICE_URI, payload);
    cbData.cb = Bridge::GetPlatformCB;
    cbData.context = thiz;
    cbData.cd = NULL;
    result = DoResource(&cbHandle, OC_REST_GET, OC_RSRVD_PLATFORM_URI, &response->devAddr, NULL,
            &cbData, NULL, 0);
    if (result == OC_STACK_OK)
    {
        thiz->m_discovered[cbHandle] = context;
    }
    else
    {
        LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
    }
exit:
    thiz->m_discovered.erase(handle);
    OICFree(piid);
    if (result != OC_STACK_OK)
    {
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetPlatformCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    std::map<OCDoHandle, DiscoverContext *>::iterator it;
    DiscoverContext *context = NULL;
    OCStackResult result = OC_STACK_ERROR;
    OCRepPayload *payload;
    OCCallbackData cbData;
    it = thiz->m_discovered.find(handle);
    if (it == thiz->m_discovered.end())
    {
        goto exit;
    }
    context = it->second;
    if (!response)
    {
        goto exit;
    }
    if (response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_INFO, "[%p] Missing /oic/p", thiz);
    }
    if (response->payload && response->payload->type != PAYLOAD_TYPE_REPRESENTATION)
    {
        LOG(LOG_INFO, "[%p] Unexpected /oic/p payload type: %d", thiz,
            response->payload->type);
        goto exit;
    }
    payload = (OCRepPayload *) response->payload;
    context->m_bus->SetAboutData(OC_RSRVD_PLATFORM_URI, payload);
    return thiz->Get(ctx, handle, response);
exit:
    thiz->m_discovered.erase(handle);
    if (result != OC_STACK_OK)
    {
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetCB(void *ctx, OCDoHandle handle,
                                       OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    std::map<OCDoHandle, DiscoverContext *>::iterator it;
    DiscoverContext *context = NULL;
    it = thiz->m_discovered.find(handle);
    if (it == thiz->m_discovered.end())
    {
        goto exit;
    }
    context = it->second;
    if (!response || response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_ERR, "GetCB (%s) response=%p {payload=%p,result=%d}",
            context->m_resources.begin()->first.c_str(),
            response, response ? response->payload : 0, response ? response->result : 0);
    }
    else
    {
        thiz->CreateInterface(context, response);
    }
    context->m_resources.erase(context->m_resources.begin());
    return thiz->Get(ctx, handle, response);
exit:
    thiz->m_discovered.erase(handle);
    delete context;
    return OC_STACK_DELETE_TRANSACTION;
}

/* Called with m_mutex held. */
OCStackResult Bridge::CreateInterface(DiscoverContext *context, OCClientResponse *response)
{
    bool isObservable = context->m_resources.begin()->second.m_bitmap & OC_OBSERVABLE;
    std::string uri = context->m_resources.begin()->first;
    std::string rt = uri.substr(uri.find("rt=") + 3);
    const ajn::InterfaceDescription *iface = context->m_bus->CreateInterface(ToAJName(rt).c_str(),
            isObservable, response->payload);
    if (!iface)
    {
        return OC_STACK_ERROR;
    }
    context->m_obj->AddInterface(iface);
    return OC_STACK_OK;
}

/* Called with m_mutex held. */
OCStackApplicationResult Bridge::Get(void *ctx, OCDoHandle handle, OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    std::map<OCDoHandle, DiscoverContext *>::iterator it;
    DiscoverContext *context = NULL;
    OCStackResult result = OC_STACK_ERROR;
    it = thiz->m_discovered.find(handle);
    if (it == thiz->m_discovered.end())
    {
        goto exit;
    }
    context = it->second;
    if (!context->m_resources.empty())
    {
        OCDevAddr *devAddr = &context->m_resources.begin()->second.m_devAddr;
        std::string uri = context->m_resources.begin()->first;
        if (context->m_obj && uri.find(context->m_obj->GetPath()) == std::string::npos)
        {
            QStatus status = context->m_bus->RegisterBusObject(context->m_obj);
            if (status != ER_OK)
            {
                delete context->m_obj;
            }
            context->m_obj = NULL;
        }
        std::string path = uri.substr(0, uri.find("?"));
        if (path == "/oic/con" ||
            path == "/oic/mnt")
        {
            VirtualBusObject *obj = context->m_bus->GetBusObject("/Config");
            if (!obj)
            {
                obj = new VirtualConfigBusObject(context->m_bus, devAddr);
                QStatus status = context->m_bus->RegisterBusObject(obj);
                if (status != ER_OK)
                {
                    delete obj;
                }
            }
            context->m_resources.erase(context->m_resources.begin());
            return Get(ctx, handle, response);
        }
        if (!context->m_obj)
        {
            context->m_obj = new VirtualBusObject(context->m_bus, path.c_str(), devAddr);
        }
        std::string rt = uri.substr(uri.find("rt=") + 3);
        if (rt.find("oic.d.") == 0)
        {
            /* Don't need to issue a GET for device types */
            CreateInterface(context, response);
            context->m_resources.erase(context->m_resources.begin());
            return Get(ctx, handle, response);
        }
        else
        {
            if (!context->m_resources.begin()->second.m_hasMultipleRts)
            {
                uri = path;
            }
            OCCallbackData cbData;
            cbData.cb = Bridge::GetCB;
            cbData.context = this;
            cbData.cd = NULL;
            OCDoHandle cbHandle;
            result = DoResource(&cbHandle, OC_REST_GET, uri.c_str(), devAddr, NULL, &cbData,
                    NULL, 0);
            if (result == OC_STACK_OK)
            {
                m_discovered[cbHandle] = context;
            }
            else
            {
                LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
                goto exit;
            }
        }
    }
    else
    {
        /* Done */
        OCPresence *presence = new OCPresence(&response->devAddr, context->m_bus->GetDi().c_str(),
                                              DISCOVER_PERIOD_SECS);
        if (!presence)
        {
            result = OC_STACK_ERROR;
            goto exit;
        }
        m_presence.push_back(presence);
        QStatus status;
        if (context->m_obj)
        {
            status = context->m_bus->RegisterBusObject(context->m_obj);
            if (status != ER_OK)
            {
                delete context->m_obj;
            }
            context->m_obj = NULL; /* context->m_obj now belongs to context->m_bus */
        }
        status = context->m_bus->Announce();
        if (status != ER_OK)
        {
            result = OC_STACK_ERROR;
            goto exit;
        }
        m_virtualBusAttachments.push_back(context->m_bus);
        context->m_bus = NULL; /* context->m_bus now belongs to thiz */
        result = OC_STACK_OK;
        delete context;
    }
exit:
    m_discovered.erase(handle);
    if (result != OC_STACK_OK)
    {
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

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
    OCCallbackData cbData;
    OCDoHandle cbHandle = 0;
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
    switch (thiz->GetSeenState(m_piid.c_str()))
    {
        case NOT_SEEN:
            context->m_bus = VirtualBusAttachment::Create(context->m_di.c_str(), m_piid.c_str(),
                    context->m_isVirtual);
            break;
        case SEEN_NATIVE:
            /* Do nothing */
            goto exit;
        case SEEN_VIRTUAL:
            if (context->m_isVirtual)
            {
                /* Do nothing */
            }
            else
            {
                thiz->DestroyPiid(m_piid.c_str());
                context->m_bus = VirtualBusAttachment::Create(context->m_di.c_str(), m_piid.c_str(),
                        context->m_isVirtual);
            }
            break;
    }
    if (!context->m_bus)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(OC_RSRVD_DEVICE_URI, m_payload);
    cbData.cb = Bridge::GetPlatformCB;
    cbData.context = thiz;
    cbData.cd = NULL;
    result = DoResource(&cbHandle, OC_REST_GET, OC_RSRVD_PLATFORM_URI, &m_devAddr, NULL, &cbData,
            NULL, 0);
    if (result == OC_STACK_OK)
    {
        thiz->m_discovered[cbHandle] = context;
    }
    else
    {
        LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
    }
exit:
    if (result != OC_STACK_OK)
    {
        delete context;
    }
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

    ::RDPublish();
    thiz->m_rdPublishTask = NULL;
}
