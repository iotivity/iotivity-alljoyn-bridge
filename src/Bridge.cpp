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

#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include <alljoyn/AllJoynStd.h>
#include "BridgeSecurity.h"
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
    std::string m_host;
    VirtualBusAttachment *m_bus;
    struct Resource
    {
        Resource(std::string uri, uint8_t bitmap, bool hasMultipleRts)
            : m_uri(uri), m_bitmap(bitmap), m_hasMultipleRts(hasMultipleRts) { }
        std::string m_uri;
        uint8_t m_bitmap;
        bool m_hasMultipleRts;
    };
    std::deque<Resource> m_resources;
    VirtualBusObject *m_obj;
    DiscoverContext(Bridge *bridge, const char *di) : m_bridge(bridge), m_di(di), m_bus(NULL),
        m_obj(NULL) { }
    ~DiscoverContext() { delete m_obj; delete m_bus; }
};

Bridge::Bridge(const char *name, Protocol protocols)
    : m_announcedCb(NULL), m_sessionLostCb(NULL),
      m_protocols(protocols), m_sender(NULL), m_bus(NULL), m_authListener(NULL),
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(false)
{
    m_bus = new ajn::BusAttachment(name, true);
}

Bridge::Bridge(const char *name, const char *uuid, const char *sender)
    : m_announcedCb(NULL), m_sessionLostCb(NULL),
      m_protocols(AJ), m_sender(sender), m_bus(NULL), m_authListener(NULL),
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(false)
{
    std::string nm = std::string(name) + uuid;
    m_bus = new ajn::BusAttachment(nm.c_str(), true);
    m_authListener = new BusAuthListener();
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
        for (DiscoverContext *discoverContext : m_discovered)
        {
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
        if (!m_bus->IsStarted())
        {
            QStatus status = m_bus->Start();
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "Start - %s", QCC_StatusText(status));
            }
            status = m_bus->EnablePeerSecurity("ALLJOYN_ECDHE_ECDSA ALLJOYN_ECDHE_NULL ALLJOYN_ECDHE_PSK ALLJOYN_ECDHE_SPEKE "
                                               "ALLJOYN_SRP_KEYX ALLJOYN_SRP_LOGON "
                                               "GSSAPI",
                                               m_authListener); // TODO NULL, true, permissionConfigurationListener);
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "EnablePeerSecurity - %s", QCC_StatusText(status));
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
            OCStackResult result = DoResource(&m_discoverHandle, OC_REST_DISCOVER, OC_RSRVD_WELL_KNOWN_URI,
                                              NULL, 0,
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
    QStatus status;
    AnnouncedContext *context;
    ajn::SessionOpts opts;

    ajn::AboutData aboutData(aboutDataArg);
    char *deviceId;
    aboutData.GetDeviceId(&deviceId);
    uint8_t *appId;
    size_t n;
    aboutData.GetAppId(&appId, &n);
    OCUUIdentity id;
    DeriveUniqueId(&id, deviceId, appId, n);
    char piid[UUID_IDENTITY_SIZE * 2 + 5];
    snprintf(piid, UUID_IDENTITY_SIZE * 2 + 5,
             "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
             id.id[0], id.id[1], id.id[2], id.id[3], id.id[4], id.id[5], id.id[6], id.id[7],
             id.id[8], id.id[9], id.id[10], id.id[11], id.id[12], id.id[13], id.id[14], id.id[15]);

    LOG(LOG_INFO, "[%p] name=%s,version=%u,port=%u,piid=%s", this, name, version, port, piid);

    ajn::MsgArg *value;
    bool isVirtual;
    if (aboutData.GetField("com.intel.Virtual", value) == ER_OK &&
        value->Get("b", &isVirtual) == ER_OK &&
        isVirtual)
    {
        LOG(LOG_INFO, "[%p] Ignoring virtual application", this);
        return;
    }

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
        m_mutex.unlock();
        if (m_announcedCb)
        {
            m_announcedCb(piid, name);
        }
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
        return VirtualConfigurationResource::Create(bus, name, sessionId, path, ajSoftwareVersion);
    }
    else
    {
        return VirtualResource::Create(bus, name, sessionId, path, ajSoftwareVersion);
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
    if (!thiz->m_whitelistAddr.empty() && (thiz->m_whitelistAddr != response->devAddr.addr))
    {
        LOG(LOG_INFO, "[%p] Ignoring %s:%d", thiz, response->devAddr.addr, response->devAddr.port);
        return OC_STACK_KEEP_TRANSACTION;
    }
    OCDiscoveryPayload *payload;
    for (payload = (OCDiscoveryPayload *) response->payload; payload; payload = payload->next)
    {
        DiscoverContext *context = NULL;
        OCCallbackData cbData;
        char targetUri[MAX_URI_LENGTH] = { 0 };
        std::deque<DiscoverContext::Resource> resources;

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
        for (std::set<DiscoverContext *>::iterator it = thiz->m_discovered.begin();
             it != thiz->m_discovered.end(); ++it)
        {
            if ((*it)->m_di == payload->sid)
            {
                goto next;
            }
        }

        for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
             resource = resource->next)
        {
            bool hasMultipleRts = resource->types && resource->types->next;
            for (OCStringLL *type = resource->types; type; type = type->next)
            {
                if (!TranslateResourceType(type->value))
                {
                    continue;
                }
                std::string uri = resource->uri;
                uri += std::string("?rt=") + type->value;
                resources.push_back(DiscoverContext::Resource(uri, resource->bitmap, hasMultipleRts));
            }
        }
        if (resources.empty())
        {
            continue;
        }

        context = new DiscoverContext(thiz, payload->sid);
        if (payload->baseURI)
        {
            context->m_host = payload->baseURI;
        }
        else
        {
            char host[MAX_URI_LENGTH] = { 0 };
            snprintf(host, MAX_URI_LENGTH, "%s:%d", response->devAddr.addr, response->devAddr.port);
            context->m_host = host;
        }
        context->m_resources = resources;
        thiz->m_discovered.insert(context);

        snprintf(targetUri, MAX_URI_LENGTH, "%s%s", context->m_host.c_str(), OC_RSRVD_DEVICE_URI);
        cbData.cb = Bridge::GetDeviceCB;
        cbData.context = context;
        cbData.cd = NULL;
        result = DoResource(NULL, OC_REST_GET, targetUri, NULL, NULL, &cbData);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
        }
next:
        if (result != OC_STACK_OK)
        {
            thiz->m_discovered.erase(context);
            delete context;
            context = NULL;
        }
    }
    return OC_STACK_KEEP_TRANSACTION;
}

OCStackApplicationResult Bridge::GetDeviceCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    DiscoverContext *context = reinterpret_cast<DiscoverContext *>(ctx);
    Bridge *thiz = context->m_bridge;
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    char targetUri[MAX_URI_LENGTH] = { 0 };
    OCCallbackData cbData;
    OCRepPayload *payload;
    char *value = NULL;
    if (!response)
    {
        goto exit;
    }
    if (response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_INFO, "[%p] Missing /oic/d", thiz);
    }
    if (response->payload && response->payload->type != PAYLOAD_TYPE_REPRESENTATION)
    {
        LOG(LOG_INFO, "[%p] Unexpected /oic/d payload type: %d", thiz,
            response->payload->type);
        goto exit;
    }
    payload = (OCRepPayload *) response->payload;
    if (OCRepPayloadGetPropString(payload, "x.com.intel.virtual", &value))
    {
        LOG(LOG_INFO, "[%p] Ignoring virtual resource", thiz);
        OICFree(value);
        value = NULL;
        goto exit;
    }
    OCRepPayloadGetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, &value);
    context->m_bus = VirtualBusAttachment::Create(context->m_di.c_str(), value, thiz->m_isGoldenUnit);
    OICFree(value);
    value = NULL;
    if (!context->m_bus)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(OC_RSRVD_DEVICE_URI, payload);
    snprintf(targetUri, MAX_URI_LENGTH, "%s%s", context->m_host.c_str(), OC_RSRVD_PLATFORM_URI);
    cbData.cb = Bridge::GetPlatformCB;
    cbData.context = context;
    cbData.cd = NULL;
    result = DoResource(NULL, OC_REST_GET, targetUri, NULL, NULL, &cbData);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResource(OC_REST_GET) - %d", result);
    }
exit:
    if (result != OC_STACK_OK)
    {
        thiz->m_discovered.erase(context);
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetPlatformCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    DiscoverContext *context = reinterpret_cast<DiscoverContext *>(ctx);
    Bridge *thiz = context->m_bridge;
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_ERROR;
    OCRepPayload *payload;
    OCCallbackData cbData;
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
    return thiz->Get(context, response);
exit:
    if (result != OC_STACK_OK)
    {
        thiz->m_discovered.erase(context);
        delete context;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackApplicationResult Bridge::GetCB(void *ctx, OCDoHandle handle,
                                       OCClientResponse *response)
{
    (void) handle;
    DiscoverContext *context = reinterpret_cast<DiscoverContext *>(ctx);
    Bridge *thiz = context->m_bridge;
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    if (!response || response->result != OC_STACK_OK || !response->payload)
    {
        LOG(LOG_ERR, "GetCB (%s) response=%p {payload=%p,result=%d}",
            context->m_resources.front().m_uri.c_str(),
            response, response ? response->payload : 0, response ? response->result : 0);
    }
    else
    {
        thiz->CreateInterface(context, response);
    }
    context->m_resources.pop_front();
    return thiz->Get(context, response);
}

OCStackResult Bridge::CreateInterface(DiscoverContext *context, OCClientResponse *response)
{
    bool isObservable = context->m_resources.front().m_bitmap & OC_OBSERVABLE;
    std::string uri = context->m_resources.front().m_uri;
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

OCStackApplicationResult Bridge::Get(DiscoverContext *context, OCClientResponse *response)
{
    (void) response;
    OCStackResult result = OC_STACK_ERROR;

    if (!context->m_resources.empty())
    {
        std::string uri = context->m_resources.front().m_uri;
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
                obj = new VirtualConfigBusObject(context->m_bus, context->m_host.c_str());
                QStatus status = context->m_bus->RegisterBusObject(obj);
                if (status != ER_OK)
                {
                    delete obj;
                }
            }
            context->m_resources.pop_front();
            return Get(context, response);
        }
        if (!context->m_obj)
        {
            context->m_obj = new VirtualBusObject(context->m_bus, path.c_str(), context->m_host.c_str());
        }
        std::string rt = uri.substr(uri.find("rt=") + 3);
        if (rt.find("oic.d.") == 0)
        {
            /* Don't need to issue a GET for device types */
            CreateInterface(context, response);
            context->m_resources.pop_front();
            return Get(context, response);
        }
        else
        {
            if (!context->m_resources.front().m_hasMultipleRts)
            {
                uri = path;
            }
            char targetUri[MAX_URI_LENGTH] = { 0 };
            snprintf(targetUri, MAX_URI_LENGTH, "%s%s", context->m_host.c_str(), uri.c_str());
            OCCallbackData cbData;
            cbData.cb = Bridge::GetCB;
            cbData.context = context;
            cbData.cd = NULL;
            result = DoResource(NULL, OC_REST_GET, targetUri, NULL, NULL, &cbData);
            if (result != OC_STACK_OK)
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
        QStatus status = context->m_bus->RegisterBusObject(context->m_obj);
        if (status != ER_OK)
        {
            delete context->m_obj;
        }
        context->m_obj = NULL; /* context->m_obj now belongs to context->m_bus */
        status = context->m_bus->Announce();
        if (status != ER_OK)
        {
            result = OC_STACK_ERROR;
            goto exit;
        }
        m_virtualBusAttachments.push_back(context->m_bus);
        context->m_bus = NULL; /* context->m_bus now belongs to thiz */
        result = OC_STACK_OK;
        m_discovered.erase(context);
        delete context;
    }
exit:
    if (result != OC_STACK_OK)
    {
        m_discovered.erase(context);
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
