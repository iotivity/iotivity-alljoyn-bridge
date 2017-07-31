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

#include "DeviceConfigurationResource.h"
#include "Hash.h"
#include "Interfaces.h"
#include "Introspection.h"
#include "Log.h"
#include "Name.h"
#include "Payload.h"
#include "PlatformConfigurationResource.h"
#include "Plugin.h"
#include "Presence.h"
#include "Resource.h"
#include "SecureModeResource.h"
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
#include <assert.h>
#include <deque>
#include <iterator>
#include <sstream>
#include <thread>

#if __WITH_DTLS__
#define SECURE_MODE_DEFAULT true
#else
#define SECURE_MODE_DEFAULT false
#endif

struct Bridge::DiscoverContext
{
    Bridge *m_bridge;
    Device m_device;
    VirtualBusAttachment *m_bus;
    OCRepPayload *m_paths;
    OCRepPayload *m_definitions;
    std::vector<Resource>::iterator m_rit;
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

    /* Each iteration returns a translatable resource and resource type pair. */
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
                if (!m_context->m_bridge->m_secureMode->GetSecureMode() || m_r->IsSecure())
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
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(NULL),
      m_rdPublishTask(NULL), m_pending(0)
{
    m_bus = new ajn::BusAttachment(name, true);
    m_ajState = CREATED;
    m_ajSecurity = new AllJoynSecurity(m_bus, AllJoynSecurity::CONSUMER, this);
    m_ocSecurity = new OCSecurity();
    m_secureMode = new SecureModeResource(m_mutex, SECURE_MODE_DEFAULT);
}

Bridge::Bridge(const char *name, const char *sender)
    : m_execCb(NULL), m_sessionLostCb(NULL), m_protocols(AJ), m_sender(sender),
      m_discoverHandle(NULL), m_discoverNextTick(0), m_secureMode(NULL),
      m_rdPublishTask(NULL), m_pending(0)
{
    m_bus = new ajn::BusAttachment(name, true);
    m_ajState = CREATED;
    m_ajSecurity = new AllJoynSecurity(m_bus, AllJoynSecurity::CONSUMER, this);
    m_ocSecurity = new OCSecurity();
    m_secureMode = new SecureModeResource(m_mutex, SECURE_MODE_DEFAULT);
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

void Bridge::SetSecureMode(bool secureMode)
{
    m_secureMode->SetSecureMode(secureMode);
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
        OCStackResult result;
        OCResourceHandle handle;
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

        result = OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SPEC_VERSION, OC_SPEC_VERSION);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCSetPropertyValue() - %d", result);
            return false;
        }
        result = OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DATA_MODEL_VERSION,
                OC_DATA_MODEL_VERSION);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCSetPropertyValue() - %d", result);
            return false;
        }
        handle = OCGetResourceHandleAtUri(OC_RSRVD_DEVICE_URI);
        if (!handle)
        {
            LOG(LOG_ERR, "OCGetResourceHandleAtUri(" OC_RSRVD_DEVICE_URI ") failed");
            return false;
        }
        result = OCBindResourceTypeToResource(handle, "oic.d.bridge");
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCBindResourceTypeToResource() - %d", result);
            return false;
        }

        result = OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_NAME,
                m_manufacturerName.c_str());
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCSetPropertyValue() - %d", result);
            return false;
        }

        result = m_secureMode->Create();
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "SecureModeResource::Create() - %d", result);
            return false;
        }
        SetIntrospectionData(NULL, NULL, "TITLE", "VERSION");
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
        OCStackResult result = OCCancel(m_discoverHandle, OC_LOW_QOS, NULL, 0);
        if (result != OC_STACK_OK)
        {
            LOG(LOG_ERR, "OCCancel() - %d", result);
        }
        m_discoverHandle = NULL;
    }
    return true;
}

void Bridge::ResetSecurity()
{
    LOG(LOG_INFO, "[%p]", this);

    if (m_ocSecurity)
    {
        m_ocSecurity->Reset();
    }
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
                OCStackResult result = OCCancel(m_discoverHandle, OC_LOW_QOS, NULL, 0);
                if (result != OC_STACK_OK)
                {
                    LOG(LOG_ERR, "OCCancel() - %d", result);
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
    QStatus status = m_bus->AddMatchAsync(matchRule.c_str(), this);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "[%p] AddMatchAsync - %s", this, QCC_StatusText(status));
    }
}

void Bridge::AddMatchCB(QStatus status, void *ctx)
{
    (void) ctx;
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "[%p] AddMatchCB - %s", this, QCC_StatusText(status));
    }
}

struct Bridge::AnnouncedContext
{
public:
    AnnouncedContext(Bridge *bridge, VirtualDevice *device, const char *name, ajn::SessionPort port,
            const ajn::MsgArg &objectDescriptionArg, const ajn::MsgArg &aboutDataArg)
        : m_bridge(bridge), m_device(device), m_name(name), m_port(port),
          m_objectDescriptionArg(objectDescriptionArg), m_aboutData(aboutDataArg), m_sessionId(0),
          m_aboutObj(NULL) { }
    ~AnnouncedContext() { delete m_aboutObj; }
    Bridge *m_bridge;
    ajn::BusAttachment *m_bus;
    VirtualDevice *m_device;
    std::string m_name;
    ajn::SessionPort m_port;
    ajn::MsgArg m_objectDescriptionArg;
    ajn::AboutData m_aboutData;
    std::vector<std::string> m_langs;
    std::vector<std::string>::iterator m_lang;
    ajn::SessionId m_sessionId;
    ajn::ProxyBusObject *m_aboutObj;
};

void Bridge::Announced(const char *name, uint16_t version, ajn::SessionPort port,
                       const ajn::MsgArg &objectDescriptionArg, const ajn::MsgArg &aboutDataArg)
{
    QStatus status;
    AnnouncedContext *context;
    ajn::SessionOpts opts;

    LOG(LOG_INFO, "[%p] name=%s,version=%u,port=%u,objectDescriptionArg=%s,aboutDataArg=%s", this,
            name, version, port, objectDescriptionArg.ToString().c_str(),
            aboutDataArg.ToString().c_str());

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

    context = new AnnouncedContext(this, device, name, port, objectDescriptionArg, aboutDataArg);
    if (device)
    {
        LOG(LOG_INFO, "[%p] Received updated Announce", this);
        m_mutex.unlock();
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

    if (m_secureMode->GetSecureMode() && (status != ER_OK))
    {
        LOG(LOG_ERR, "SecureConnectionCB - %s", QCC_StatusText(status));
        m_insecureAnnounced.insert(context);
        status = m_bus->LeaveSessionAsync(context->m_sessionId, &m_insecureLeaveSessionCB, context);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "LeaveSessionAsync - %s", QCC_StatusText(status));
            delete context;
        }
        return;
    }

    if (!m_sender)
    {
        qcc::String peerGuid;
        m_bus->GetPeerGUID(context->m_name.c_str(), peerGuid);
        char piid[UUID_STRING_SIZE];
        GetProtocolIndependentId(piid, &context->m_aboutData,
                peerGuid.empty() ? NULL : peerGuid.c_str());

        bool isVirtual = ajn::AboutObjectDescription(context->m_objectDescriptionArg)
                .HasInterface("oic.d.virtual");

        switch (GetSeenState(piid))
        {
            case NOT_SEEN:
                if (isVirtual)
                {
                    /* Delay creating virtual resources from a virtual Announce */
                    LOG(LOG_INFO, "[%p] Delaying creation of virtual resources from a virtual device",
                            this);
                    m_tasks.push_back(new AnnouncedTask(time(NULL) + 10, context->m_name.c_str(),
                            piid, isVirtual));
                }
                else
                {
                    m_execCb(piid, context->m_name.c_str(), isVirtual);
                }
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
                    DestroyPiid(piid);
                    m_execCb(piid, context->m_name.c_str(), isVirtual);
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
                &arg, 1, context);
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
            LOG(LOG_ERR, "LeaveSessionAsync - %s", QCC_StatusText(status));
            delete context;
        }
    }
}

static bool ComparePath(const char *a, const char *b)
{
    return (strcmp(a, b) < 0);
}

VirtualResource *Bridge::CreateVirtualResource(ajn::BusAttachment *bus, const char *name,
        ajn::SessionId sessionId, const char *path, const char *ajSoftwareVersion,
        ajn::AboutData *aboutData)
{
    if (!strcmp(path, "/Config"))
    {
        VirtualConfigurationResource *resource = VirtualConfigurationResource::Create(bus, name,
                sessionId, path, ajSoftwareVersion, RDPublish, this);
        resource->SetAboutData(aboutData);
        return resource;
    }
    else
    {
        return VirtualResource::Create(bus, name, sessionId, path, ajSoftwareVersion, RDPublish,
                this);
    }
}

void Bridge::GetAboutDataCB(ajn::Message &msg, void *ctx)
{
    LOG(LOG_INFO, "[%p]", this);

    std::lock_guard<std::mutex> lock(m_mutex);
    AnnouncedContext *context = reinterpret_cast<AnnouncedContext *>(ctx);
    if (msg->GetType() == ajn::MESSAGE_METHOD_RET)
    {
        ajn::AboutData aboutData(*msg->GetArg(0));
        if (context->m_langs.empty())
        {
            context->m_aboutData = aboutData;

            char *lang = NULL;
            aboutData.GetDefaultLanguage(&lang);
            context->m_langs.push_back(lang);
            size_t numLangs = aboutData.GetSupportedLanguages();
            const char **langs = new const char *[numLangs];
            aboutData.GetSupportedLanguages(langs, numLangs);
            for (size_t i = 0; i < numLangs; ++i)
            {
                if (strcmp(lang, langs[i]))
                {
                    context->m_langs.push_back(langs[i]);
                }
            }
            delete[] langs;
            context->m_lang = context->m_langs.begin();

            char *ajSoftwareVersion = NULL;
            QStatus status = aboutData.GetAJSoftwareVersion(&ajSoftwareVersion);
            LOG(LOG_INFO, "[%p] %s AJSoftwareVersion=%s", this, QCC_StatusText(status),
                    ajSoftwareVersion ? ajSoftwareVersion : "unknown");
            m_ajSoftwareVersion = ajSoftwareVersion;
        }
        else
        {
            const char *lang = context->m_lang->c_str();
            context->m_aboutData.CreatefromMsgArg(*msg->GetArg(0), lang);
        }
        if (++context->m_lang != context->m_langs.end())
        {
            ajn::MsgArg arg("s", context->m_lang->c_str());
            QStatus status = context->m_aboutObj->MethodCallAsync(
                ::ajn::org::alljoyn::About::InterfaceName, "GetAboutData",
                this, static_cast<ajn::MessageReceiver::ReplyHandler>(&Bridge::GetAboutDataCB),
                &arg, 1, context);
            if (status != ER_OK)
            {
                LOG(LOG_ERR, "MethodCallAsync - %s", QCC_StatusText(status));
                delete context;
            }
        }
        else
        {
            if (!context->m_device)
            {
                context->m_device = new VirtualDevice(m_bus, msg->GetSender(), msg->GetSessionId());
                m_virtualDevices.push_back(context->m_device);
                Presence *presence = new AllJoynPresence(m_bus, context->m_name);
                m_presence.push_back(presence);
            }

            ajn::AboutObjectDescription objectDescription(context->m_objectDescriptionArg);
            context->m_device->SetProperties(&objectDescription, &context->m_aboutData);

            size_t n = objectDescription.GetPaths(NULL, 0);
            const char **pa = new const char *[n];
            objectDescription.GetPaths(pa, n);
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
            std::set_difference(pb.begin(), pb.end(), pa, pa + n,
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
            std::set_difference(pa, pa + n, pb.begin(), pb.end(),
                    std::inserter(add, add.begin()), ComparePath);
            for (size_t i = 0; i < add.size(); ++i)
            {
                VirtualResource *resource = CreateVirtualResource(m_bus, context->m_name.c_str(),
                        msg->GetSessionId(), add[i], m_ajSoftwareVersion.c_str(),
                        &context->m_aboutData);
                if (resource)
                {
                    m_virtualResources.push_back(resource);
                }
            }
            delete[] pa;
            delete context;
        }
    }
    else if (msg->GetType() == ajn::MESSAGE_ERROR)
    {
        qcc::String message;
        const char *name = msg->GetErrorName(&message);
        LOG(LOG_ERR, "%s: %s", name, message.c_str());
        QStatus status = m_bus->LeaveSessionAsync(context->m_sessionId, this, context);
        if (status != ER_OK)
        {
            delete context;
        }
    }
}

void Bridge::InsecureLeaveSessionCB::LeaveSessionCB(QStatus status, void *ctx)
{
    LOG(LOG_INFO, "[%p] status=%s,ctx=%p", this, QCC_StatusText(status), ctx);
    AnnouncedContext *context = reinterpret_cast<AnnouncedContext *>(ctx);
    ajn::SessionId sessionId = context->m_sessionId;
    /* context belongs to m_insecureAnnounced, so don't delete it here */
    context->m_bridge->SessionLost(sessionId, ALLJOYN_SESSIONLOST_REASON_OTHER);
}

void Bridge::State(const char* busName, const qcc::KeyInfoNISTP256& publicKeyInfo,
        ajn::PermissionConfigurator::ApplicationState state)
{
    (void) publicKeyInfo;
    LOG(LOG_INFO, "[%p] busName=%s,state=%d", this, busName, state);

    std::lock_guard<std::mutex> lock(m_mutex);
    if (state != ajn::PermissionConfigurator::CLAIMED)
    {
        return;
    }
    auto it = std::find_if(m_insecureAnnounced.begin(), m_insecureAnnounced.end(),
            [busName](AnnouncedContext *ctx) -> bool {return ctx->m_name == busName;});
    if (it != m_insecureAnnounced.end())
    {
        LOG(LOG_INFO, "[%p] Found %s", this, busName);
        AnnouncedContext *context = *it;
        m_insecureAnnounced.erase(it);
        ajn::SessionOpts opts;
        QStatus status = m_bus->JoinSessionAsync(context->m_name.c_str(), context->m_port,
                this, opts, this, context);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "JoinSessionAsync - %s", QCC_StatusText(status));
            delete context;
        }
    }
    else
    {
        LOG(LOG_INFO, "[%p] Did not find %s", this, busName);
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
    return !strcmp(payload->sid, OCGetServerInstanceIDString());
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
        if (m_secureMode->GetSecureMode() && !IsSecure(resource))
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
            if (isVirtual)
            {
                /* Delay creating virtual objects from a virtual device */
                LOG(LOG_INFO, "[%p] Delaying creation of virtual objects from a virtual device",
                        thiz);
                thiz->m_tasks.push_back(new DiscoverTask(time(NULL) + 10, piid, payload, context));
                context = NULL;
                goto exit;
            }
            else
            {
                context->m_bus = VirtualBusAttachment::Create(context->m_device.m_di.c_str(), piid,
                        isVirtual);
            }
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
                thiz->DestroyPiid(piid);
                context->m_bus = VirtualBusAttachment::Create(context->m_device.m_di.c_str(), piid,
                        isVirtual);
            }
            break;
    }
    if (!context->m_bus)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(payload);
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
    OCStackResult result = OC_STACK_OK;
    DiscoverContext *context;
    OCRepPayload *payload;
    Resource *resource;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context || !payload)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(payload);

    resource = context->m_device.GetResourceType(OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION);
    if (resource)
    {
        result = thiz->ContinueDiscovery(context, resource->m_uri.c_str(), resource->m_addrs,
                Bridge::GetDeviceConfigurationCB);
    }
    else
    {
        result = thiz->GetPlatformConfiguration(context);
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

OCStackApplicationResult Bridge::GetDeviceConfigurationCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_OK;
    DiscoverContext *context;
    OCRepPayload *payload;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context || !payload)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(payload);

    result = thiz->GetPlatformConfiguration(context);
    if (result == OC_STACK_OK)
    {
        context = NULL;
    }

exit:
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackResult Bridge::GetPlatformConfiguration(DiscoverContext *context)
{
    OCStackResult result = OC_STACK_OK;
    Resource *resource;
    resource = context->m_device.GetResourceType(OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION);
    if (resource)
    {
        result = ContinueDiscovery(context, resource->m_uri.c_str(), resource->m_addrs,
                Bridge::GetPlatformConfigurationCB);
    }
    else
    {
        result = GetCollection(context);
    }
    return result;
}

OCStackApplicationResult Bridge::GetPlatformConfigurationCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_OK;
    DiscoverContext *context;
    OCRepPayload *payload;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context)
    {
        goto exit;
    }
    context->m_bus->SetAboutData(payload);

    result = thiz->GetCollection(context);
    if (result == OC_STACK_OK)
    {
        context = NULL;
    }

exit:
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackResult Bridge::GetCollection(DiscoverContext *context)
{
    OCStackResult result = OC_STACK_OK;
    for (context->m_rit = context->m_device.m_resources.begin();
         context->m_rit != context->m_device.m_resources.end(); ++context->m_rit)
    {
        Resource &r = *context->m_rit;
        if (HasResourceType(r.m_rts, "oic.r.alljoynobject"))
        {
            result = ContinueDiscovery(context, r.m_uri.c_str(), r.m_addrs, Bridge::GetCollectionCB);
            if (result == OC_STACK_OK)
            {
                context = NULL;
            }
            goto exit;
        }
    }
    result = GetIntrospection(context);
    if (result == OC_STACK_OK)
    {
        context = NULL;
    }

exit:
    return result;
}

OCStackApplicationResult Bridge::GetCollectionCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    OCStackResult result = OC_STACK_OK;
    DiscoverContext *context;
    OCRepPayload *payload;
    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context || !payload)
    {
        goto exit;
    }
    if (!context->m_device.SetCollectionLinks(context->m_rit->m_uri, payload))
    {
        goto exit;
    }
    for (++context->m_rit; context->m_rit != context->m_device.m_resources.end(); ++context->m_rit)
    {
        Resource &r = *context->m_rit;
        if (HasResourceType(r.m_rts, "oic.r.alljoynobject"))
        {
            result = thiz->ContinueDiscovery(context, r.m_uri.c_str(), r.m_addrs,
                    Bridge::GetCollectionCB);
            if (result == OC_STACK_OK)
            {
                context = NULL;
            }
            goto exit;
        }
    }
    result = thiz->GetIntrospection(context);
    if (result == OC_STACK_OK)
    {
        context = NULL;
    }

exit:
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
}

OCStackResult Bridge::GetIntrospection(DiscoverContext *context)
{
    OCStackResult result = OC_STACK_OK;
    Resource *resource;
    resource = context->m_device.GetResourceType(OC_RSRVD_RESOURCE_TYPE_INTROSPECTION);
    if (resource)
    {
        result = ContinueDiscovery(context, resource->m_uri.c_str(),
                resource->m_addrs, Bridge::GetIntrospectionCB);
    }
    else
    {
        LOG(LOG_INFO, "[%p] Missing introspection resource", this);
        context->m_paths = OCRepPayloadCreate();
        context->m_definitions = OCRepPayloadCreate();
        if (!context->m_paths || !context->m_definitions)
        {
            LOG(LOG_ERR, "Failed to create payload");
            return result;
        }
        context->m_it = context->Begin();
        result = ContinueDiscovery(context, context->m_it.GetUri().c_str(),
                context->m_it.GetDevAddrs(), Bridge::GetCB);
    }
    return result;
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
    if (!context || !payload)
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
    DiscoverContext *context;
    OCRepPayload *payload;

    thiz->GetContextAndRepPayload(handle, response, &context, &payload);
    if (!context || !payload)
    {
        goto exit;
    }
    if (thiz->ParseIntrospectionPayload(context, payload))
    {
        result = OC_STACK_OK;
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
    delete context;
    thiz->m_discovered.erase(handle);
    return OC_STACK_DELETE_TRANSACTION;
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
    OCRepPayload *path = NULL;
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
        std::string resourceType = context->m_it.GetResourceType();
        /* The definition of a resource type has the union of all possible interfaces listed */
        std::set<std::string> ifSet;
        for (DiscoverContext::Iterator it = context->Begin(); it != context->End(); ++it)
        {
            Resource &r = it.GetResource();
            if (HasResourceType(r.m_rts, resourceType))
            {
                ifSet.insert(r.m_ifs.begin(), r.m_ifs.end());
            }
        }
        std::vector<std::string> interfaces(ifSet.begin(), ifSet.end());
        definition = IntrospectDefinition(payload, resourceType, interfaces);
        if (!OCRepPayloadSetPropObjectAsOwner(context->m_definitions, resourceType.c_str(),
                definition))
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
        path = IntrospectPath(context->m_it.GetResource().m_rts, context->m_it.GetResource().m_ifs);
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
    OCRepPayloadDestroy(path);
    OCRepPayloadDestroy(definition);
    thiz->m_discovered.erase(handle);
    delete context;
    return OC_STACK_DELETE_TRANSACTION;
}

bool Bridge::ParseIntrospectionPayload(DiscoverContext *context, OCRepPayload *payload)
{
    OCPresence *presence = NULL;
    bool success = ::ParseIntrospectionPayload(&context->m_device, context->m_bus, payload);
    if (success)
    {
        QStatus status;
        presence = new OCPresence(context->m_device.m_di.c_str(), DISCOVER_PERIOD_SECS);
        if (!presence)
        {
            LOG(LOG_ERR, "new OCPresence() failed");
            goto exit;
        }
        m_presence.push_back(presence);
        presence = NULL; /* presence now belongs to this */
        status = context->m_bus->Announce();
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "Announce() failed - %s", QCC_StatusText(status));
            goto exit;
        }
        m_virtualBusAttachments.push_back(context->m_bus);
        context->m_bus = NULL; /* context->m_bus now belongs to this */
    }
exit:
    delete presence;
    return success;
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
    context->m_bus->SetAboutData(m_payload);
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

void Bridge::RDPublish(void *ctx)
{
    Bridge *thiz = reinterpret_cast<Bridge *>(ctx);
    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    if (thiz->m_rdPublishTask)
    {
        /* Delay the pending publication to give time for multiple resources to be created. */
        thiz->m_rdPublishTask->m_tick = time(NULL) + 1;
    }
    else
    {
        thiz->m_rdPublishTask = new RDPublishTask(time(NULL) + 1);
        thiz->m_tasks.push_back(thiz->m_rdPublishTask);
    }
}

/* Called with m_mutex held. */
void Bridge::SetIntrospectionData(ajn::BusAttachment *bus, const char *ajSoftwareVersion,
        const char *title, const char *version)
{
    LOG(LOG_INFO, "[%p]", this);

    OCPersistentStorage *ps = OCGetPersistentStorageHandler();
    assert(ps);
    size_t curSize = 1024;
    uint8_t *out = NULL;
    CborError err;
    FILE *fp = NULL;
    size_t ret;
    for (;;)
    {
        out = (uint8_t *)OICCalloc(1, curSize);
        if (!out)
        {
            LOG(LOG_ERR, "Failed to allocate introspection data buffer");
            goto exit;
        }
        err = Introspect(bus, ajSoftwareVersion, title, version, out, &curSize);
        if (err != CborErrorOutOfMemory)
        {
            break;
        }
        OICFree(out);
    }
    fp = ps->open(OC_INTROSPECTION_FILE_NAME, "wb");
    if (!fp)
    {
        LOG(LOG_ERR, "open failed");
        goto exit;
    }
    ret = ps->write(out, 1, curSize, fp);
    if (ret != curSize)
    {
        LOG(LOG_ERR, "write failed");
        goto exit;
    }
exit:
    if (fp)
    {
        ps->close(fp);
    }
    OICFree(out);
}

/* Called with m_mutex held. */
void Bridge::RDPublishTask::Run(Bridge *thiz)
{
    LOG(LOG_INFO, "[%p] thiz=%p", this, thiz);

    thiz->SetIntrospectionData(thiz->m_bus, thiz->m_ajSoftwareVersion.c_str(), "TITLE", "VERSION");
    ::RDPublish();
    thiz->m_rdPublishTask = NULL;
}
