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

#include "ocstack.h"
#include <alljoyn/AllJoynStd.h>
#include "Plugin.h"
#include "Presence.h"
#include "VirtualBusAttachment.h"
#include "VirtualBusObject.h"
#include "VirtualDevice.h"
#include "VirtualResource.h"
#include <algorithm>
#include <deque>
#include <iterator>

static bool TranslateResourceType(const char *type)
{
    return !(strcmp(type, OC_RSRVD_RESOURCE_TYPE_PLATFORM) == 0 ||
             strcmp(type, OC_RSRVD_RESOURCE_TYPE_DEVICE) == 0);
}

Bridge::Bridge(Protocol protocols)
    : m_protocols(protocols), m_bus(NULL), m_discoverHandle(NULL), m_discoverNextTick(0)
{
    m_bus = new ajn::BusAttachment("Bridge", true);
}

Bridge::~Bridge()
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        for (Presence *presence : m_presence)
        {
            delete presence;
        }
        m_presence.clear();
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
    delete m_bus;
}

/* Called with m_mutex held. */
void Bridge::Destroy(const char *id)
{
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

    OCResourceHandle handle = OCGetResourceHandleAtUri(OC_RSRVD_DEVICE_URI);
    if (!handle)
    {
        LOG(LOG_ERR, "OCGetResourceHandleAtUri(" OC_RSRVD_DEVICE_URI ") failed");
        return false;
    }
    OCStackResult result = OCBindResourceTypeToResource(handle, "oic.d.bridge");
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "OCBindResourceTypeToResouurce() - %d", result);
        return false;
    }

    if (m_protocols & AJ)
    {
        m_bus->RegisterAboutListener(*this);
        m_bus->RegisterBusListener(*this);
    }
    return true;
}

bool Bridge::Stop()
{
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
        if (!m_bus->IsStarted())
        {
            QStatus status = m_bus->Start();
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "Start - %s", QCC_StatusText(status));
            }
        }
        bool wasConnected = m_bus->IsConnected();
        if (m_bus->IsStarted() && !m_bus->IsConnected())
        {
            if (m_bus->Connect() == ER_OK)
            {
                LOG(LOG_INFO, "[%p] Connected", this);
            }
        }
        if (!wasConnected && m_bus->IsConnected())
        {
            QStatus status = m_bus->WhoImplements(NULL, 0);
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "WhoImplements - %s", QCC_StatusText(status));
            }
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
            OCStackResult result = DoResource(&m_discoverHandle, OC_REST_DISCOVER, OC_RSRVD_WELL_KNOWN_URI, NULL, 0,
                                              &cbData);
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
    (void) aboutDataArg;
    LOG(LOG_INFO, "[%p] name=%s,version=%u,port=%u", this, name, version, port);

    m_mutex.lock();
    QStatus status;
    AnnouncedContext *context;
    ajn::SessionOpts opts;

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
                VirtualResource *resource = VirtualResource::Create(m_bus,
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
            m_virtualDevices.push_back(device);
            size_t numPaths = objectDescription.GetPaths(NULL, 0);
            const char **paths = new const char *[numPaths];
            objectDescription.GetPaths(paths, numPaths);
            for (size_t i = 0; i < numPaths; ++i)
            {
                VirtualResource *resource = VirtualResource::Create(m_bus,
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
}

struct DiscoverContext
{
    Bridge *m_bridge;
    OCPresence *m_presence;
    VirtualBusAttachment *m_bus;
    std::deque<std::string> m_uris;
    VirtualBusObject *m_obj;
    DiscoverContext(Bridge *bridge) : m_bridge(bridge), m_presence(NULL), m_bus(NULL),
        m_obj(NULL) { }
    ~DiscoverContext() { delete m_obj; delete m_bus; delete m_presence; }
};

OCStackApplicationResult Bridge::DiscoverCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    DiscoverContext *context = NULL;
    OCCallbackData cbData;
    OCDiscoveryPayload *payload;
    if (!response || !response->payload || response->result != OC_STACK_OK)
    {
        goto exit;
    }
    payload = (OCDiscoveryPayload *) response->payload;
    if (!strcmp(payload->sid, GetServerInstanceIDString()))
    {
        /* Ignore responses from self */
        goto exit;
    }

    /* Check if we've seen this response before */
    for (std::vector<VirtualBusAttachment *>::iterator it = thiz->m_virtualBusAttachments.begin();
         it != thiz->m_virtualBusAttachments.end(); ++it)
    {
        if ((*it)->GetDi() == payload->sid)
        {
            goto exit;
        }
    }

    context = new DiscoverContext(thiz);
    context->m_presence = new OCPresence(&response->devAddr, payload->sid);
    if (!context->m_presence)
    {
        goto exit;
    }
    context->m_bus = VirtualBusAttachment::Create(payload->sid);
    if (!context->m_bus)
    {
        goto exit;
    }
    for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
         resource = resource->next)
    {
        for (OCStringLL *type = resource->types; type; type = type->next)
        {
            if (!TranslateResourceType(type->value))
            {
                continue;
            }
            std::string uri = resource->uri + std::string("?rt=") + type->value;
            context->m_uris.push_back(uri);
        }
    }
    cbData.cb = Bridge::GetPlatformCB;
    cbData.context = context;
    cbData.cd = NULL;
    result = DoResource(NULL, OC_REST_GET, OC_RSRVD_PLATFORM_URI, &response->devAddr, NULL, &cbData);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
    }
exit:
    if (result != OC_STACK_OK)
    {
        delete context;
    }
    return OC_STACK_KEEP_TRANSACTION;
}

OCStackApplicationResult Bridge::GetPlatformCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    DiscoverContext *context = reinterpret_cast<DiscoverContext *>(ctx);
    LOG(LOG_INFO, "[%p]", context->m_bridge);

    std::lock_guard<std::mutex> lock(context->m_bridge->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    OCCallbackData cbData;
    OCRepPayload *payload;
    if (!response)
    {
        goto exit;
    }
    if (response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_INFO, "[%p] Missing /oic/p", context->m_bridge);
    }
    if (response->payload->type != PAYLOAD_TYPE_REPRESENTATION)
    {
        LOG(LOG_INFO, "[%p] Unexpected /oic/p payload type: %d", context->m_bridge, response->payload->type);
    }
    payload = (OCRepPayload *) response->payload;
    context->m_bus->SetAboutData(OC_RSRVD_PLATFORM_URI, payload);
    cbData.cb = Bridge::GetDeviceCB;
    cbData.context = context;
    cbData.cd = NULL;
    result = DoResource(NULL, OC_REST_GET, OC_RSRVD_DEVICE_URI, &response->devAddr, NULL, &cbData);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
    }
exit:
    if (result != OC_STACK_OK)
    {
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetDeviceCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    DiscoverContext *context = reinterpret_cast<DiscoverContext *>(ctx);
    LOG(LOG_INFO, "[%p]", context->m_bridge);

    std::lock_guard<std::mutex> lock(context->m_bridge->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    OCRepPayload *payload;
    OCCallbackData cbData;
    std::string uri;
    std::string path;
    if (!response)
    {
        goto exit;
    }
    if (response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_INFO, "[%p] Missing /oic/d", context->m_bridge);
    }
    if (response->payload->type != PAYLOAD_TYPE_REPRESENTATION)
    {
        LOG(LOG_INFO, "[%p] Unexpected /oic/d payload type: %d", context->m_bridge, response->payload->type);
    }
    payload = (OCRepPayload *) response->payload;
    context->m_bus->SetAboutData(OC_RSRVD_DEVICE_URI, payload);
    if (context->m_uris.empty())
    {
        goto exit;
    }
    uri = context->m_uris.front();
    path = uri.substr(0, uri.find("?"));
    context->m_obj = new VirtualBusObject(context->m_bus, path.c_str(), &response->devAddr);

    cbData.cb = Bridge::GetCB;
    cbData.context = context;
    cbData.cd = NULL;
    result = DoResource(NULL, OC_REST_GET, uri.c_str(), &response->devAddr, NULL, &cbData);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
        goto exit;
    }
    return OC_STACK_DELETE_TRANSACTION;
exit:
    if (result != OC_STACK_OK)
    {
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    DiscoverContext *context = reinterpret_cast<DiscoverContext *>(ctx);
    LOG(LOG_INFO, "[%p]", context->m_bridge);

    std::lock_guard<std::mutex> lock(context->m_bridge->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    std::string rt;
    std::string uri;
    std::string path;
    const ajn::InterfaceDescription *iface;
    if (!response || response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_ERR, "GetCB (%s) response=%p {payload=%p,result=%d}", context->m_uris.front().c_str(),
            response, response ? response->payload : 0, response ? response->result : 0);
        goto next;
    }
    uri = context->m_uris.front();
    rt = uri.substr(uri.find("rt=") + 3);
    iface = context->m_bus->CreateInterface(rt.c_str(), response->payload);
    if (!iface)
    {
        goto next;
    }
    context->m_obj->AddInterface(iface);

next:
    context->m_uris.pop_front();
    if (!context->m_uris.empty())
    {
        uri = context->m_uris.front();
        if (uri.find(context->m_obj->GetPath()) == std::string::npos)
        {
            QStatus status = context->m_bus->RegisterBusObject(context->m_obj);
            if (status != ER_OK)
            {
                delete context->m_obj;
                context->m_obj = NULL;
            }
            path = uri.substr(0, uri.find("?"));
            context->m_obj = new VirtualBusObject(context->m_bus, path.c_str(), &response->devAddr);
        }
        OCCallbackData cbData;
        cbData.cb = Bridge::GetCB;
        cbData.context = context;
        cbData.cd = NULL;
        result = DoResource(NULL, OC_REST_GET, uri.c_str(), &response->devAddr, NULL, &cbData);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
            goto exit;
        }
    }
    else
    {
        /* Done */
        QStatus status = context->m_bus->RegisterBusObject(context->m_obj);
        if (status != ER_OK)
        {
            goto exit;
        }
        context->m_obj = NULL; /* context->m_obj now belongs to context->m_bus */
        status = context->m_bus->Announce();
        if (status != ER_OK)
        {
            goto exit;
        }
        context->m_bridge->m_virtualBusAttachments.push_back(context->m_bus);
        context->m_bus = NULL; /* context->m_bus now belongs to context->m_bridge */
        context->m_bridge->m_presence.push_back(context->m_presence);
        context->m_presence = NULL; /* context->m_presence now belongs to context->m_bridge */
    }
exit:
    if (result != OC_STACK_OK)
    {
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}
