//******************************************************************
//
// Copyright 2017 Intel Corporation All Rights Reserved.
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

#include "UnitTest.h"

#include "DeviceConfigurationResource.h"
#include "Interfaces.h"
#include "Introspection.h"
#include "Name.h"
#include "PlatformConfigurationResource.h"
#include "Plugin.h"
#include "VirtualBusAttachment.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_time.h"
#include <thread>

class AboutListener : public ajn::AboutListener
{
public:
    AboutListener(ajn::BusAttachment *bus) : m_bus(bus), m_called(false)
    {
        m_bus->RegisterAboutListener(*this);
        EXPECT_EQ(ER_OK, m_bus->WhoImplements(NULL, 0));
    }
    virtual ~AboutListener()
    {
        m_bus->UnregisterAboutListener(*this);
    }
    OCStackResult Wait(long waitMs)
    {
        uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
        while (!m_called)
        {
            uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
            long elapsed = (long)(currTime - startTime);
            if (elapsed > waitMs)
            {
                break;
            }
            OCProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!m_called)
        {
            return OC_STACK_TIMEOUT;
        }
        return OC_STACK_OK;
    }
    bool HasPath(const char *path) { return m_objectDescription.HasPath(path); }
    bool HasInterface(const char *name) { return m_objectDescription.HasInterface(name); }
    QStatus JoinSession()
    {
        ajn::SessionOpts opts;
        return m_bus->JoinSession(m_name.c_str(), m_sessionPort, NULL, m_sessionId, opts);
    }
    ajn::SessionId SessionId() { return m_sessionId; }
    ajn::ProxyBusObject *CreateProxyBusObject(const char *path)
    {
        ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_name.c_str(), path,
                m_sessionId);
        if (proxyObj)
        {
            EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());
        }
        return proxyObj;
    }
    ajn::AboutData GetAboutData(const char *lang)
    {
        ajn::ProxyBusObject proxyObj(*m_bus, m_name.c_str(), "/About", m_sessionId);
        EXPECT_EQ(ER_OK, proxyObj.AddInterface("org.alljoyn.About"));
        ajn::MsgArg arg("s", lang);
        MethodCall get(m_bus, &proxyObj);
        EXPECT_EQ(ER_OK, get.Call("org.alljoyn.About", "GetAboutData", &arg, 1));
        EXPECT_EQ(ER_OK, get.Wait(1000));
        return ajn::AboutData(*get.Reply()->GetArg(0));
    }
    ajn::MsgArg GetConfigurations(const char *lang)
    {
        ajn::ProxyBusObject proxyObj(*m_bus, m_name.c_str(), "/Config", m_sessionId);
        EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(ajn::org::alljoyn::Config::InterfaceXml));
        EXPECT_EQ(ER_OK, proxyObj.AddInterface("org.alljoyn.Config"));
        ajn::MsgArg arg("s", lang);
        MethodCall get(m_bus, &proxyObj);
        EXPECT_EQ(ER_OK, get.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
        EXPECT_EQ(ER_OK, get.Wait(1000));
        size_t numArgs;
        const ajn::MsgArg *args;
        get.Reply()->GetArgs(numArgs, args);
        EXPECT_EQ(1u, numArgs);
        return args[0];
    }
    void UpdateConfigurations(const char *lang, ajn::MsgArg &arg)
    {
        ajn::ProxyBusObject proxyObj(*m_bus, m_name.c_str(), "/Config", m_sessionId);
        EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(ajn::org::alljoyn::Config::InterfaceXml));
        EXPECT_EQ(ER_OK, proxyObj.AddInterface("org.alljoyn.Config"));
        ajn::MsgArg args[2];
        args[0].Set("s", lang);
        args[1] = arg;
        MethodCall update(m_bus, &proxyObj);
        EXPECT_EQ(ER_OK, update.Call("org.alljoyn.Config", "UpdateConfigurations", args, 2));
        EXPECT_EQ(ER_OK, update.Wait(1000));
    }
private:
    ajn::BusAttachment *m_bus;
    bool m_called;
    std::string m_name;
    ajn::SessionPort m_sessionPort;
    ajn::AboutObjectDescription m_objectDescription;
    ajn::SessionId m_sessionId;
    virtual void Announced(const char* name, uint16_t version, ajn::SessionPort port,
            const ajn::MsgArg& objectDescriptionArg, const ajn::MsgArg& aboutDataArg)
    {
        m_name = name;
        (void) version;
        m_sessionPort = port;
        (void) aboutDataArg;
        EXPECT_EQ(ER_OK, m_objectDescription.CreateFromMsgArg(objectDescriptionArg));
        m_called = true;
    }
};

class GetPropertyCall : public ajn::ProxyBusObject::Listener
{
public:
    GetPropertyCall(ajn::ProxyBusObject *proxyObj)
        : m_proxyObj(proxyObj), m_status(ER_OK), m_called(false) { }
    QStatus Call(const char *iface, const char *property, bool withError = false)
    {
        Reset();
        if (withError)
        {
            return m_proxyObj->GetPropertyAsync(iface, property, this,
                    static_cast<ajn::ProxyBusObject::Listener::GetPropertyAsyncCB>(&GetPropertyCall::GetPropertyAsyncCB),
                    NULL);
        }
        else
        {
            return m_proxyObj->GetPropertyAsync(iface, property, this,
                    static_cast<ajn::ProxyBusObject::Listener::GetPropertyCB>(&GetPropertyCall::GetPropertyCB),
                    NULL);
        }
    }
    void Reset() { m_called = false; }
    QStatus Wait(long waitMs)
    {
        uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
        while (!m_called)
        {
            uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
            long elapsed = (long)(currTime - startTime);
            if (elapsed > waitMs)
            {
                break;
            }
            OCProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!m_called)
        {
            return ER_TIMEOUT;
        }
        return m_status;
    }
    const ajn::MsgArg &Value() { return m_value; }
    const std::string &ErrorName() { return m_errorName; }
    const std::string &ErrorDescription() { return m_errorDescription; }
private:
    ajn::ProxyBusObject *m_proxyObj;
    QStatus m_status;
    ajn::MsgArg m_value;
    std::string m_errorName;
    std::string m_errorDescription;
    bool m_called;
    void GetPropertyCB(QStatus status, ajn::ProxyBusObject* obj, const ajn::MsgArg& value,
            void* context)
    {
        (void) context;
        (void) obj;
        m_status = status;
        m_value = value;
        m_called = true;
    }
    void GetPropertyAsyncCB(QStatus status, ajn::ProxyBusObject *obj, const ajn::MsgArg &value,
            const qcc::String& errorName, const qcc::String& errorDescription, void* context)
    {
        (void) context;
        (void) obj;
        m_status = status;
        m_value = value;
        m_errorName = errorName;
        m_errorDescription = errorDescription;
        m_called = true;
    }
};

class SetPropertyCall : public ajn::ProxyBusObject::Listener
{
public:
    SetPropertyCall(ajn::ProxyBusObject *proxyObj)
        : m_proxyObj(proxyObj), m_status(ER_OK), m_called(false) { }
    QStatus Call(const char *iface, const char *property, ajn::MsgArg &value, bool withError = false)
    {
        Reset();
        if (withError)
        {
            return m_proxyObj->SetPropertyAsync(iface, property, value, this,
                    static_cast<ajn::ProxyBusObject::Listener::SetPropertyAsyncCB>(&SetPropertyCall::SetPropertyAsyncCB),
                    NULL);
        }
        else
        {
            return m_proxyObj->SetPropertyAsync(iface, property, value, this,
                    static_cast<ajn::ProxyBusObject::Listener::SetPropertyCB>(&SetPropertyCall::SetPropertyCB),
                    NULL);
        }
    }
    void Reset() { m_called = false; }
    QStatus Wait(long waitMs)
    {
        uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
        while (!m_called)
        {
            uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
            long elapsed = (long)(currTime - startTime);
            if (elapsed > waitMs)
            {
                break;
            }
            OCProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!m_called)
        {
            return ER_TIMEOUT;
        }
        return m_status;
    }
    const std::string &ErrorName() { return m_errorName; }
    const std::string &ErrorDescription() { return m_errorDescription; }
private:
    ajn::ProxyBusObject *m_proxyObj;
    QStatus m_status;
    std::string m_errorName;
    std::string m_errorDescription;
    bool m_called;
    void SetPropertyCB(QStatus status, ajn::ProxyBusObject *obj, void* context)
    {
        (void) context;
        (void) obj;
        m_status = status;
        m_called = true;
    }
    void SetPropertyAsyncCB(QStatus status, ajn::ProxyBusObject *obj, const qcc::String& errorName,
            const qcc::String& errorDescription, void* context)
    {
        (void) context;
        (void) obj;
        m_status = status;
        m_errorName = errorName;
        m_errorDescription = errorDescription;
        m_called = true;
    }
};

class GetAllPropertiesCall : public ajn::ProxyBusObject::Listener
{
public:
    GetAllPropertiesCall(ajn::ProxyBusObject *proxyObj)
        : m_proxyObj(proxyObj), m_status(ER_OK), m_called(false) { }
    QStatus Call(const char *iface)
    {
        Reset();
        return m_proxyObj->GetAllPropertiesAsync(iface, this,
                static_cast<ajn::ProxyBusObject::Listener::GetAllPropertiesCB>(&GetAllPropertiesCall::GetAllPropertiesCB),
                NULL);
    }
    void Reset() { m_called = false; }
    QStatus Wait(long waitMs)
    {
        uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
        while (!m_called)
        {
            uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
            long elapsed = (long)(currTime - startTime);
            if (elapsed > waitMs)
            {
                break;
            }
            OCProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!m_called)
        {
            return ER_TIMEOUT;
        }
        return m_status;
    }
    const ajn::MsgArg &Values() { return m_values; }
private:
    ajn::ProxyBusObject *m_proxyObj;
    QStatus m_status;
    ajn::MsgArg m_values;
    bool m_called;
    void GetAllPropertiesCB(QStatus status, ajn::ProxyBusObject *obj, const ajn::MsgArg &values,
            void *context)
    {
        (void) context;
        (void) obj;
        m_status = status;
        m_values = values;
        m_called = true;
    }
};

struct BoolProperty
{
    const char *m_name;
    bool m_value;
    BoolProperty(const char *name = "value") : m_name(name), m_value(false) { }
};

struct MaintenanceEntity
{
    bool m_fr;
    bool m_rb;
    MaintenanceEntity() : m_fr(false), m_rb(false) { }
};

struct ConfigurationEntity
{
    bool m_includeOptionalProperties;
    bool m_includeDefaultLanguage;
    std::string m_dl;
    std::map<std::string, std::string> m_ln;
    double m_loc[2];
    std::string m_locn;
    std::string m_c;
    std::string m_r;
    std::string m_orgIoTivityCon;
    std::map<std::string, std::string> m_mnpn;
    ConfigurationEntity()
        : m_includeOptionalProperties(false), m_includeDefaultLanguage(true), m_dl("en"),
          m_locn("locn"), m_c("c"), m_r("r"), m_orgIoTivityCon("con")
    {
        m_ln["en"] = "en-ln";
        m_ln["fr"] = "fr-ln";
        m_loc[0] = -1.0;
        m_loc[1] = 1.0;
        m_mnpn["en"] = "en-mnpn";
        m_mnpn["fr"] = "fr-mnpn";
    }
};

static OCEntityHandlerResult BoolEntityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    (void) flag;
    BoolProperty *property = (BoolProperty *) ctx;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = CreatePayload(request->resource, request->query);
                if (!payload || !OCRepPayloadSetPropBool(payload, property->m_name,
                        property->m_value))
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
                if (!OCRepPayloadGetPropBool((OCRepPayload *) request->payload,
                        property->m_name, &property->m_value))
                {
                    result = OC_EH_ERROR;
                    break;
                }
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *outPayload = CreatePayload(request->resource, request->query);
                if (!outPayload || !OCRepPayloadSetPropBool(outPayload, property->m_name,
                        property->m_value))
                {
                    result = OC_EH_ERROR;
                    break;
                }
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    OCRepPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

static OCEntityHandlerResult ErrorEntityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    (void) flag;
    (void) ctx;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                result = OC_EH_ERROR;
                break;
            }
        case OC_REST_POST:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCDiagnosticPayload *outPayload =
                        OCDiagnosticPayloadCreate("org.freedesktop.DBus.Error.Failed: Not allowed");
                if (!outPayload)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                result = OC_EH_ERROR;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    OCDiagnosticPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

static OCEntityHandlerResult ConfigurationEntityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    (void) flag;
    ConfigurationEntity *entity = (ConfigurationEntity *) ctx;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = CreatePayload(request->resource, request->query);
                if (!payload)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                if (entity->m_includeOptionalProperties)
                {
                    uint8_t nr;
                    OCGetNumberOfResourceTypes(request->resource, &nr);
                    for (size_t i = 0; i < nr; ++i)
                    {
                        if (!strcmp(OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION,
                                OCGetResourceTypeName(request->resource, i)))
                        {
                            size_t locDim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
                            if ((entity->m_includeDefaultLanguage &&
                                    !OCRepPayloadSetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE,
                                            entity->m_dl.c_str())) ||
                                    !OCRepPayloadSetDoubleArray(payload, OC_RSRVD_LOCATION,
                                            entity->m_loc, locDim) ||
                                    !OCRepPayloadSetPropString(payload, OC_RSRVD_LOCATION_NAME,
                                            entity->m_locn.c_str()) ||
                                    !OCRepPayloadSetPropString(payload, OC_RSRVD_CURRENCY,
                                            entity->m_c.c_str()) ||
                                    !OCRepPayloadSetPropString(payload, OC_RSRVD_REGION,
                                            entity->m_r.c_str()) ||
                                    !OCRepPayloadSetPropString(payload, "x.org.iotivity.con",
                                            entity->m_orgIoTivityCon.c_str()))
                            {
                                result = OC_EH_ERROR;
                                break;
                            }
                            size_t lnsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
                            lnsDim[0] = entity->m_ln.size();
                            OCRepPayload **lns =
                                    (OCRepPayload **) OICCalloc(lnsDim[0], sizeof(OCRepPayload *));
                            if (!lns)
                            {
                                result = OC_EH_ERROR;
                                break;
                            }
                            OCRepPayload **ln = lns;
                            for (auto &m_ln : entity->m_ln)
                            {
                                (*ln) = OCRepPayloadCreate();
                                if (!(*ln))
                                {
                                    result = OC_EH_ERROR;
                                    break;
                                }
                                if (!OCRepPayloadSetPropString((*ln), "language",
                                        m_ln.first.c_str()) ||
                                        !OCRepPayloadSetPropString((*ln), "value",
                                                m_ln.second.c_str()))
                                {
                                    result = OC_EH_ERROR;
                                    break;
                                }
                                ++ln;
                            }
                            if (!OCRepPayloadSetPropObjectArrayAsOwner(payload,
                                    OC_RSRVD_DEVICE_NAME_LOCALIZED, lns, lnsDim))
                            {
                                result = OC_EH_ERROR;
                                break;
                            }
                        }
                        else if (!strcmp(OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION,
                                OCGetResourceTypeName(request->resource, i)))
                        {
                            if (!OCRepPayloadSetPropString(payload, "x.org.iotivity.con.p",
                                    "con.p"))
                            {
                                result = OC_EH_ERROR;
                                break;
                            }
                            size_t mnpnsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
                            mnpnsDim[0] = entity->m_mnpn.size();
                            OCRepPayload **mnpns =
                                    (OCRepPayload **) OICCalloc(mnpnsDim[0], sizeof(OCRepPayload *));
                            if (!mnpns)
                            {
                                result = OC_EH_ERROR;
                                break;
                            }
                            OCRepPayload **mnpn = mnpns;
                            for (auto &m_mnpn : entity->m_mnpn)
                            {
                                (*mnpn) = OCRepPayloadCreate();
                                if (!(*mnpn))
                                {
                                    result = OC_EH_ERROR;
                                    break;
                                }
                                if (!OCRepPayloadSetPropString((*mnpn), "language",
                                        m_mnpn.first.c_str()) ||
                                        !OCRepPayloadSetPropString((*mnpn), "value",
                                                m_mnpn.second.c_str()))
                                {
                                    result = OC_EH_ERROR;
                                    break;
                                }
                                ++mnpn;
                            }
                            if (!OCRepPayloadSetPropObjectArrayAsOwner(payload,
                                    OC_RSRVD_PLATFORM_NAME, mnpns, mnpnsDim))
                            {
                                result = OC_EH_ERROR;
                                break;
                            }
                        }
                    }
                }
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(payload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
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
                OCRepPayload *payload = (OCRepPayload *) request->payload;
                char *s;
                if (OCRepPayloadGetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, &s))
                {
                    entity->m_dl = s;
                }
                size_t dim[MAX_REP_ARRAY_DEPTH];
                double *d;
                if (OCRepPayloadGetDoubleArray(payload, OC_RSRVD_LOCATION, &d, dim))
                {
                    memcpy(entity->m_loc, d, 2 * sizeof(double));
                }
                if (OCRepPayloadGetPropString(payload, OC_RSRVD_LOCATION_NAME, &s))
                {
                    entity->m_locn = s;
                }
                if (OCRepPayloadGetPropString(payload, OC_RSRVD_CURRENCY, &s))
                {
                    entity->m_c = s;
                }
                if (OCRepPayloadGetPropString(payload, OC_RSRVD_REGION, &s))
                {
                    entity->m_r = s;
                }
                if (OCRepPayloadGetPropString(payload, "x.org.iotivity.con", &s))
                {
                    entity->m_orgIoTivityCon = s;
                }
                size_t lnsDim[MAX_REP_ARRAY_DEPTH];
                OCRepPayload **lns;
                if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, &lns,
                        lnsDim))
                {
                    size_t dimTotal = calcDimTotal(lnsDim);
                    for (size_t i = 0; i < dimTotal; ++i)
                    {
                        char *l, *v;
                        if (OCRepPayloadGetPropString(lns[i], "language", &l) &&
                                OCRepPayloadGetPropString(lns[i], "value", &v))
                        {
                            entity->m_ln[l] = v;
                        }
                    }
                }
                size_t mnpnsDim[MAX_REP_ARRAY_DEPTH];
                OCRepPayload **mnpns;
                if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_PLATFORM_NAME, &mnpns,
                        mnpnsDim))
                {
                    size_t dimTotal = calcDimTotal(mnpnsDim);
                    for (size_t i = 0; i < dimTotal; ++i)
                    {
                        char *l, *v;
                        if (OCRepPayloadGetPropString(mnpns[i], "language", &l) &&
                                OCRepPayloadGetPropString(mnpns[i], "value", &v))
                        {
                            entity->m_mnpn[l] = v;
                        }
                    }
                }
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *outPayload = OCRepPayloadClone(payload);
                if (!outPayload)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    OCRepPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

class TestResource
{
public:
    virtual ~TestResource() { }
    virtual void Create()
    {
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt", NULL,
                "/resource/0", BoolEntityHandler, &m_value, OC_DISCOVERABLE));
        m_handles.push_back(handle);
    }
    virtual void Destroy()
    {
        for (OCResourceHandle handle : m_handles)
        {
            OCDeleteResource(handle);
        }
    }
    void Notify(const char *uri)
    {
        EXPECT_EQ(OC_STACK_OK, OCNotifyAllObservers(OCGetResourceHandleAtUri(uri), OC_HIGH_QOS));
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
protected:
    std::vector<OCResourceHandle> m_handles;
    BoolProperty m_value;
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource/0\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"value\": { \"readOnly\": true, \"type\": \"boolean\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
};

class TestDeviceResource : public TestResource
{
public:
    virtual ~TestDeviceResource() { }
    virtual void Create()
    {
        EXPECT_EQ(OC_STACK_OK, OCBindResourceTypeToResource(
                    OCGetResourceHandleAtUri(OC_RSRVD_DEVICE_URI), "oic.d.light"));
    }
};

class TestObservableResource : public TestResource
{
public:
    virtual ~TestObservableResource() { }
    virtual void Create()
    {
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt", NULL,
                "/resource/0", BoolEntityHandler, &m_value, OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
    }
};

static OCEntityHandlerResult MaintenanceEntityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    (void) flag;
    MaintenanceEntity *entity = (MaintenanceEntity *) ctx;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = CreatePayload(request->resource, request->query);
                if (!payload ||
                        !OCRepPayloadSetPropBool(payload, "fr", entity->m_fr) ||
                        !OCRepPayloadSetPropBool(payload, "rb", entity->m_rb))
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
                OCRepPayloadGetPropBool((OCRepPayload *) request->payload, "fr", &entity->m_fr);
                OCRepPayloadGetPropBool((OCRepPayload *) request->payload, "rb", &entity->m_rb);
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *outPayload = CreatePayload(request->resource, request->query);
                if (!outPayload ||
                        !OCRepPayloadSetPropBool(outPayload, "fr", entity->m_fr) ||
                        !OCRepPayloadSetPropBool(outPayload, "rb", entity->m_rb))
                {
                    result = OC_EH_ERROR;
                    break;
                }
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    OCRepPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

class TestReadWriteResource : public TestResource
{
public:
    TestReadWriteResource() : m_property("readWriteValue") { }
    virtual ~TestReadWriteResource() { }
    virtual void Create()
    {
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt", NULL,
                "/resource/0", BoolEntityHandler, &m_property, OC_DISCOVERABLE));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
    bool ReadWriteValue() { return m_property.m_value; }
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource/0\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"value\": { \"readOnly\": true, \"type\": \"boolean\" },"
            "        \"readWriteValue\": { \"type\": \"boolean\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    BoolProperty m_property;
};

class TestResources : public TestResource
{
public:
    TestResources() { }
    virtual ~TestResources() { }
    virtual void Create()
    {
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt", NULL,
                "/resource/0", BoolEntityHandler, &m_value[0], OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt", NULL,
                "/resource/1", BoolEntityHandler, &m_value[1], OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource/0\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    },"
            "    \"/resource/1\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"value\": { \"readOnly\": true, \"type\": \"boolean\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    BoolProperty m_value[2];
};

class TestObservableAndUnobservableResources : public TestResources
{
public:
    virtual ~TestObservableAndUnobservableResources() { }
    virtual void Create()
    {
        TestResources::Create();
        EXPECT_EQ(OC_STACK_OK, OCSetResourceProperties(m_handles[1], OC_OBSERVABLE));
    }
};

class TestCollectionResource : public TestResources
{
public:
    TestCollectionResource()
        : m_false("x.org.iotivity.rt.false"), m_true("x.org.iotivity.rt.true") { }
    virtual ~TestCollectionResource() { }
    virtual void Create()
    {
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt.false", NULL,
                "/resource/0", BoolEntityHandler, &m_false, OC_DISCOVERABLE));
        m_handles.push_back(handle);
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt.true", NULL,
                "/resource/1", BoolEntityHandler, &m_true, OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "oic.r.alljoynobject", "oic.if.ll",
                "/resource", NULL, NULL, OC_DISCOVERABLE));
        EXPECT_EQ(OC_STACK_OK, OCBindResourceTypeToResource(handle,
                OC_RSRVD_RESOURCE_TYPE_COLLECTION));
        EXPECT_EQ(OC_STACK_OK, OCBindResource(handle, m_handles[0]));
        EXPECT_EQ(OC_STACK_OK, OCBindResource(handle, m_handles[1]));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
    bool False() { return m_false.m_value; }
    bool True() { return m_true.m_value; }
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource/0\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt.false\" } ] } } }"
            "      }"
            "    },"
            "    \"/resource/1\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt.true\" } ] } } }"
            "      }"
            "    },"
            "    \"/resource\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.ll\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.r.alljoynobject\" }, { \"$ref\": \"#/definitions/oic.wk.col\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt.false\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"x.org.iotivity.rt.false\": { \"type\": \"boolean\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt.false\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    },"
            "    \"x.org.iotivity.rt.true\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"x.org.iotivity.rt.true\": { \"type\": \"boolean\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt.true\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    BoolProperty m_false;
    BoolProperty m_true;
};

class TestErrorResource : public TestResource
{
public:
    virtual ~TestErrorResource() { }
    virtual void Create()
    {
        TestResource::Create();
        EXPECT_EQ(OC_STACK_OK, OCBindResourceHandler(m_handles[0], ErrorEntityHandler, NULL));
    }
};

class TestServerResources : public TestResource
{
public:
    TestServerResources()
        : m_useOneConfigurationResource(false)
    {
        m_configuration.m_includeOptionalProperties = true;
    }
    virtual ~TestServerResources() { }
    virtual void Create()
    {
        /* oic.wk.d */
        OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, "n");
        OCStringLL *ll = NULL;
        OCResourcePayloadAddStringLL(&ll, "en");
        OCResourcePayloadAddStringLL(&ll, "en-dmn");
        OCResourcePayloadAddStringLL(&ll, "fr");
        OCResourcePayloadAddStringLL(&ll, "fr-dmn");
        OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MFG_NAME, ll);
        OCFreeOCStringLL(ll);
        OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MODEL_NUM, "dmno");
        ll = NULL;
        OCResourcePayloadAddStringLL(&ll, "en");
        OCResourcePayloadAddStringLL(&ll, "en-ld");
        OCResourcePayloadAddStringLL(&ll, "fr");
        OCResourcePayloadAddStringLL(&ll, "fr-ld");
        OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_DESCRIPTION, ll);
        OCFreeOCStringLL(ll);
        OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SOFTWARE_VERSION, "sv");
        OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, "piid");
        OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, "x.org.iotivity.d", "d");

        /* oic.wk.p */
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, "pi");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_URL, "mnml");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, "mndt");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, "mnhw");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, "mnsl");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_VERSION, "mnpv");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_OS_VERSION, "mnos");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_FIRMWARE_VERSION, "mnfv");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SYSTEM_TIME, "st");
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, "x.org.iotivity.p", "p");

        /* oic.wk.con */
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle,
                OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION, OC_RSRVD_INTERFACE_READ_WRITE, "/con",
                ConfigurationEntityHandler, &m_configuration,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);

        /* oic.wk.con.p */
        if (m_useOneConfigurationResource)
        {
            EXPECT_EQ(OC_STACK_OK, OCBindResourceTypeToResource(handle,
                    OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION));
        }
        else
        {
            EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle,
                    OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION, OC_RSRVD_INTERFACE_READ_WRITE,
                    "/con/p", ConfigurationEntityHandler, &m_configuration,
                    OC_DISCOVERABLE | OC_OBSERVABLE));
            m_handles.push_back(handle);
        }

        /* oic.wk.mnt */
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle,
                OC_RSRVD_RESOURCE_TYPE_MAINTENANCE, OC_RSRVD_INTERFACE_READ_WRITE,
                "/oic/mnt", MaintenanceEntityHandler, &m_maintenance,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson()
    {
        return m_useOneConfigurationResource ? m_oneIntrospectionJson : m_introspectionJson;
    }
    void UseOneConfigurationResource() { m_useOneConfigurationResource = true; }
    void SetMandatoryProperties() { m_configuration.m_includeOptionalProperties = false; }
    void SetAllProperties() { m_configuration.m_includeOptionalProperties = true; }
    ConfigurationEntity &Configuration() { return m_configuration; }
    bool FactoryReset() { return m_maintenance.m_fr; }
    bool Reboot() { return m_maintenance.m_rb; }
private:
    const char *m_oneIntrospectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/con\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.wk.con\" }, { \"$ref\": \"#/definitions/oic.wk.con.p\" } ] } } }"
            "      }"
            "    },"
            "    \"/oic/mnt\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\", \"oic.if.rw\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.wk.mnt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"oic.wk.con\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.con\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } }"
            "      }"
            "    },"
            "    \"oic.wk.con.p\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.con.p\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } }"
            "      }"
            "    },"
            "    \"oic.wk.mnt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.mnt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\", \"oic.if.rw\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/con\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.wk.con\" } ] } } }"
            "      }"
            "    },"
            "    \"/con/p\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.wk.con.p\" } ] } } }"
            "      }"
            "    },"
            "    \"/oic/mnt\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\", \"oic.if.rw\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.wk.mnt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"oic.wk.con\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.con\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } }"
            "      }"
            "    },"
            "    \"oic.wk.con.p\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.con.p\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } }"
            "      }"
            "    },"
            "    \"oic.wk.mnt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.mnt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\", \"oic.if.rw\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    bool m_useOneConfigurationResource;
    ConfigurationEntity m_configuration;
    MaintenanceEntity m_maintenance;
};

class TestDeviceConfigurationResource : public TestResource
{
public:
    virtual ~TestDeviceConfigurationResource() { }
    virtual void Create()
    {
        /* oic.wk.con */
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle,
                OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION, OC_RSRVD_INTERFACE_READ_WRITE, "/con",
                ConfigurationEntityHandler, &m_configuration,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
    void SetMandatoryProperties() { m_configuration.m_includeOptionalProperties = false; }
    void SetAllProperties() { m_configuration.m_includeOptionalProperties = true; }
    ConfigurationEntity &Configuration() { return m_configuration; }
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/con\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.wk.con\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"oic.wk.con\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.con\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    ConfigurationEntity m_configuration;
};

class TestPlatformConfigurationResource : public TestResource
{
public:
    virtual ~TestPlatformConfigurationResource() { }
    virtual void Create()
    {
        /* oic.wk.con.p */
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle,
                OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION, OC_RSRVD_INTERFACE_READ_WRITE,
                "/con/p", ConfigurationEntityHandler, &m_configuration,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
    void SetMandatoryProperties() { m_configuration.m_includeOptionalProperties = false; }
    void SetAllProperties() { m_configuration.m_includeOptionalProperties = true; }
    ConfigurationEntity &Configuration() { return m_configuration; }
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/con/p\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.wk.con.p\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"oic.wk.con.p\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.wk.con.p\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\", \"oic.if.rw\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    ConfigurationEntity m_configuration;
};

class OCFResource : public AJOCSetUp
{
public:
    TestResource *m_resource;
    VirtualBusAttachment *m_bus;
    DiscoverContext *m_context;
    AboutListener *m_aboutListener;
    OCFResource() : m_resource(NULL), m_bus(NULL), m_context(NULL), m_aboutListener(NULL) { }
    virtual ~OCFResource() { }
    void SetUpResource(TestResource *resource)
    {
        m_resource = resource;
        m_resource->Create();

        m_context = new DiscoverContext();
        Callback discoverCB(Discover, m_context);
        EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "/oic/res", NULL, 0, CT_DEFAULT,
                OC_HIGH_QOS, discoverCB, NULL, 0));
        EXPECT_EQ(OC_STACK_OK, discoverCB.Wait(1000));

        Resource *deviceResource = m_context->m_device->GetResourceUri("/oic/d");
        EXPECT_TRUE(deviceResource != NULL);
        ResourceCallback getDeviceCB;
        EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, deviceResource->m_uri.c_str(),
                &deviceResource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getDeviceCB, NULL, 0));
        EXPECT_EQ(OC_STACK_OK, getDeviceCB.Wait(1000));
        EXPECT_EQ(OC_STACK_OK, getDeviceCB.m_response->result);
        EXPECT_TRUE(getDeviceCB.m_response->payload != NULL);
        EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getDeviceCB.m_response->payload->type);
        OCRepPayload *payload = (OCRepPayload *) getDeviceCB.m_response->payload;
        char *piid;
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, &piid));

        m_bus = VirtualBusAttachment::Create(m_context->m_device->m_di.c_str(), piid, false);
        m_bus->SetAboutData(payload);

        Resource *platformResource = m_context->m_device->GetResourceUri("/oic/p");
        EXPECT_TRUE(platformResource != NULL);
        ResourceCallback getPlatformCB;
        EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, platformResource->m_uri.c_str(),
                &platformResource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getPlatformCB, NULL, 0));
        EXPECT_EQ(OC_STACK_OK, getPlatformCB.Wait(1000));
        EXPECT_EQ(OC_STACK_OK, getPlatformCB.m_response->result);
        EXPECT_TRUE(getPlatformCB.m_response->payload != NULL);
        EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getPlatformCB.m_response->payload->type);
        payload = (OCRepPayload *) getPlatformCB.m_response->payload;
        m_bus->SetAboutData(payload);

        Resource *deviceConfigResource =
                m_context->m_device->GetResourceType(OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION);
        if (deviceConfigResource)
        {
            ResourceCallback getDeviceConfigCB;
            EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET,
                    deviceConfigResource->m_uri.c_str(), &deviceConfigResource->m_addrs[0], 0,
                    CT_DEFAULT, OC_HIGH_QOS, getDeviceConfigCB, NULL, 0));
            EXPECT_EQ(OC_STACK_OK, getDeviceConfigCB.Wait(1000));
            EXPECT_EQ(OC_STACK_OK, getDeviceConfigCB.m_response->result);
            EXPECT_TRUE(getDeviceConfigCB.m_response->payload != NULL);
            EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getDeviceConfigCB.m_response->payload->type);
            payload = (OCRepPayload *) getDeviceConfigCB.m_response->payload;
            m_bus->SetAboutData(payload);
        }

        Resource *platformConfigResource =
                m_context->m_device->GetResourceType(OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION);
        if (platformConfigResource)
        {
            ResourceCallback getPlatformConfigCB;
            EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET,
                    platformConfigResource->m_uri.c_str(), &platformConfigResource->m_addrs[0], 0,
                    CT_DEFAULT, OC_HIGH_QOS, getPlatformConfigCB, NULL, 0));
            EXPECT_EQ(OC_STACK_OK, getPlatformConfigCB.Wait(1000));
            EXPECT_EQ(OC_STACK_OK, getPlatformConfigCB.m_response->result);
            EXPECT_TRUE(getPlatformConfigCB.m_response->payload != NULL);
            EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getPlatformConfigCB.m_response->payload->type);
            payload = (OCRepPayload *) getPlatformConfigCB.m_response->payload;
            m_bus->SetAboutData(payload);
        }

        auto collectionResource = FindResourceFromType(m_context->m_device->m_resources,
                "oic.r.alljoynobject");
        if (collectionResource != m_context->m_device->m_resources.end())
        {
            ResourceCallback getCollectionCB;
            EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, collectionResource->m_uri.c_str(),
                    &collectionResource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCollectionCB,
                    NULL, 0));
            EXPECT_EQ(OC_STACK_OK, getCollectionCB.Wait(1000));
            EXPECT_EQ(OC_STACK_OK, getCollectionCB.m_response->result);
            EXPECT_TRUE(getCollectionCB.m_response->payload != NULL);
            EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCollectionCB.m_response->payload->type);
            payload = (OCRepPayload *) getCollectionCB.m_response->payload;
            EXPECT_TRUE(m_context->m_device->SetCollectionLinks(collectionResource->m_uri, payload));
        }

        OCRepPayload *introspectionData;
        EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData,
                m_resource->IntrospectionJson()));
        EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));

        EXPECT_EQ(ER_OK, m_bus->Announce());

        m_aboutListener = new AboutListener(m_bus);
        EXPECT_EQ(OC_STACK_OK, m_aboutListener->Wait(1000));
    }
    void TearDownResource()
    {
        m_resource->Destroy();
        delete m_aboutListener;
        m_bus->Stop();
        bool deleted = false;
        std::thread t([&]()
                {
                    delete m_bus;
                    deleted = true;
                });
        while (!deleted)
        {
            OCProcess();
        }
        t.join();
        delete m_context;
    }
};

/*
 * 7.1.2 Exposing OCF resources to AllJoyn consumer applications
 */

TEST_F(OCFResource, EachOCFResourceShallBeMappedToASeparateAllJoynObject)
{
    TestResources resources;
    SetUpResource(&resources);

    EXPECT_TRUE(m_aboutListener->HasPath("/resource/0"));
    EXPECT_TRUE(m_aboutListener->HasPath("/resource/1"));

    TearDownResource();
}

TEST_F(OCFResource, EachOCFServerShallBeExposedAsASeparateAllJoynProducerApplicationWithItsOwnAboutData)
{
    // TODO This can't be unit tested yet
    FAIL();
}

TEST_F(OCFResource, TheAllJoynProducerApplicationShallImplementTheOicDVirtualAllJoynInterface)
{
    TestResource resource;
    SetUpResource(&resource);

    EXPECT_TRUE(m_aboutListener->HasInterface("oic.d.virtual"));

    TearDownResource();
}

TEST_F(OCFResource, TheAllJoynPeerIdShallBeTheOCFDeviceId)
{
    // TODO No API to set peer ID in AllJoyn
    FAIL();
}

TEST_F(OCFResource, TheTranslatorShallEitherNotTranslateTheResourceTypeOrAlgorithmicallyMapTheResourceType)
{
    /* Resource type is in a well-defined set */
    EXPECT_TRUE(TranslateResourceType("oic.wk.con")); /* Deep translation */
    EXPECT_FALSE(TranslateResourceType("oic.r.switch.binary")); /* Algorithmic translation */

    /* Resource type is not in a well-defined set */
    EXPECT_TRUE(TranslateResourceType("x.org.iotivity.rt"));
}

class ShallBeConvertedToAnInterfaceAsFollows : public ::testing::TestWithParam<const char *> { };

TEST_P(ShallBeConvertedToAnInterfaceAsFollows, RoundTrip)
{
    const char *ocName = GetParam();
    EXPECT_STREQ(ocName, ToOCName(ToAJName(ocName)).c_str());
}

INSTANTIATE_TEST_CASE_P(OCFResourceTypeName, ShallBeConvertedToAnInterfaceAsFollows,
        ::testing::Values(
            /* From spec */
            "x.example.-widget", "x.example.my--widget", "x.example.-my---widget",
            "x.xn--p1ai.example", "x.xn--90ae.example", "x.example.my-name-1"));

TEST_F(OCFResource, AnOCFDeviceTypeIsMappedToAnAllJoynInterfaceWithNoMembers)
{
    TestDeviceResource resource;
    SetUpResource(&resource);

    std::string ifaceName = ToAJName("oic.d.light");
    EXPECT_TRUE(m_aboutListener->HasInterface(ifaceName.c_str()));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface(ifaceName.c_str());
    EXPECT_TRUE(iface != NULL);
    EXPECT_EQ(0u, iface->GetProperties());
    EXPECT_EQ(0u, iface->GetMembers());

    TearDownResource();
}

TEST_F(OCFResource, EachOCFPropertyIsMappedToAnAllJoynPropertyInThatInterface)
{
    TestResources resources;
    SetUpResource(&resources);

    std::string ifaceName = ToAJName("x.org.iotivity.rt");
    EXPECT_TRUE(m_aboutListener->HasInterface(ifaceName.c_str()));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface(ifaceName.c_str());
    EXPECT_TRUE(iface != NULL);
    EXPECT_EQ(1u, iface->GetProperties());
    const ajn::InterfaceDescription::Property *prop = iface->GetProperty("value");
    EXPECT_TRUE(prop != NULL);

    TearDownResource();
}

TEST_F(OCFResource, TheEmitsChangedSignalValueForEachAllJoynPropertyShallBeSetToTrueIfTheResourceSupportsNotify)
{
    TestObservableResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);
    const ajn::InterfaceDescription *iface =
            resource0->GetInterface(ToAJName("x.org.iotivity.rt").c_str());
    EXPECT_TRUE(iface != NULL);
    const ajn::InterfaceDescription::Property *prop = iface->GetProperty("value");
    EXPECT_TRUE(prop != NULL);
    qcc::String emitsChanged;
    EXPECT_TRUE(prop->GetAnnotation(ajn::org::freedesktop::DBus::AnnotateEmitsChanged,
            emitsChanged));
    EXPECT_STREQ("true", emitsChanged.c_str());

    delete resource0;
    TearDownResource();
}

TEST_F(OCFResource, TheEmitsChangedSignalValueForEachAllJoynPropertyShallBeSetToFalseIfTheResourceDoesNotSupportsNotify)
{
    TestResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);
    const ajn::InterfaceDescription *iface =
            resource0->GetInterface(ToAJName("x.org.iotivity.rt").c_str());
    EXPECT_TRUE(iface != NULL);
    const ajn::InterfaceDescription::Property *prop = iface->GetProperty("value");
    EXPECT_TRUE(prop != NULL);
    qcc::String emitsChanged;
    bool got = prop->GetAnnotation(ajn::org::freedesktop::DBus::AnnotateEmitsChanged, emitsChanged);
    EXPECT_TRUE(!got || emitsChanged == "false"); // false is default value

    delete resource0;
    TearDownResource();
}

TEST_F(OCFResource, TheEmitsChangedSignalValueForEachAllJoynPropertyShallBeSetToTrueIfTheResourceSupportsNotifyOrFalseIfItDoesNot)
{
    // TODO This test fails since the same interface is used by both the obervable and unobservable resource
    TestObservableAndUnobservableResources resources;
    SetUpResource(&resources);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);
    const ajn::InterfaceDescription *iface =
            resource0->GetInterface(ToAJName("x.org.iotivity.rt").c_str());
    EXPECT_TRUE(iface != NULL);
    const ajn::InterfaceDescription::Property *prop = iface->GetProperty("value");
    EXPECT_TRUE(prop != NULL);
    qcc::String emitsChanged;
    bool got = prop->GetAnnotation(ajn::org::freedesktop::DBus::AnnotateEmitsChanged, emitsChanged);
    EXPECT_TRUE(!got || emitsChanged == "false"); // false is default value

    ajn::ProxyBusObject *resource1 = m_aboutListener->CreateProxyBusObject("/resource/1");
    EXPECT_TRUE(resource1 != NULL);
    iface = resource1->GetInterface(ToAJName("x.org.iotivity.rt").c_str());
    EXPECT_TRUE(iface != NULL);
    prop = iface->GetProperty("value");
    EXPECT_TRUE(prop != NULL);
    EXPECT_TRUE(prop->GetAnnotation(ajn::org::freedesktop::DBus::AnnotateEmitsChanged,
            emitsChanged));
    EXPECT_STREQ("true", emitsChanged.c_str());

    delete resource1;
    delete resource0;
    TearDownResource();
}

TEST_F(OCFResource, TheAccessAttributeForEachAllJoynPropertyShallBeReadIfTheOCFPropertyIsReadOnlyOrReadwriteIfTheOCFPropertyIsReadWrite)
{
    TestReadWriteResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);
    const ajn::InterfaceDescription *iface =
            resource0->GetInterface(ToAJName("x.org.iotivity.rt").c_str());
    EXPECT_TRUE(iface != NULL);

    const ajn::InterfaceDescription::Property *prop = iface->GetProperty("value");
    EXPECT_TRUE(prop != NULL);
    EXPECT_TRUE(prop->access & ajn::PROP_ACCESS_READ);

    prop = iface->GetProperty("readWriteValue");
    EXPECT_TRUE(prop != NULL);
    EXPECT_TRUE(prop->access & ajn::PROP_ACCESS_RW);

    TearDownResource();
}

TEST_F(OCFResource, IfTheResourceSupportsDeleteADeleteMethodShallAppearInTheInterface)
{
    // TODO Which interface to add delete method to? See example from oic.wk.d: no rt in
    // queryParameters and arguments provided as queryParameters, not in the body
    FAIL();
}

TEST_F(OCFResource, IfTheResourceSupportsCreateACreateMethodShallAppearInTheInterfaceWithInputArgumentsOfEachPropertyOfTheResourceToCreate)
{
    // TODO It cannot be determined yet if a resource supports create
    FAIL();
}

TEST_F(OCFResource, IfTheInputArgumentsOfEachPropertyOfTheResourceToCreateIsNotAvailableACreateWithDefaultValuesMethodShallAppearWhichTakesNoInputArguments)
{
    // TODO It cannot be determined yet if a resource supports create
    FAIL();
}

TEST_F(OCFResource, IfTheResourceSupportsUpdateThenAnAllJoynPropertySetOperationShallBeMappedToAPartialUpdateWithTheCorrespondingOCFProperty)
{
    TestReadWriteResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    EXPECT_FALSE(resource.ReadWriteValue());
    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);
    SetPropertyCall set(resource0);
    ajn::MsgArg value("b", true);
    EXPECT_EQ(ER_OK, set.Call(ToAJName("x.org.iotivity.rt").c_str(), "readWriteValue", value));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_TRUE(resource.ReadWriteValue());

    TearDownResource();
}

TEST_F(OCFResource, IfAResourceHasAResourceTypeOicRAllJoynobjectThenAllResourcesInTheCollectionShallBeTranslatedToASingleAllJoynObjectWhoseObjectPathIsTheOCFUriPathOfTheCollection)
{
    TestCollectionResource resource;
    SetUpResource(&resource);

    /* Verify proper AllJoyn object is created */
    EXPECT_TRUE(m_aboutListener->HasPath("/resource"));
    EXPECT_FALSE(m_aboutListener->HasPath("/resource/0"));
    EXPECT_FALSE(m_aboutListener->HasPath("/resource/1"));
    EXPECT_TRUE(m_aboutListener->HasInterface("org.iotivity.rt.false"));
    EXPECT_TRUE(m_aboutListener->HasInterface("org.iotivity.rt.true"));
    EXPECT_FALSE(m_aboutListener->HasInterface("oic.r.alljoynobject"));
    EXPECT_FALSE(m_aboutListener->HasInterface("oic.wk.col"));

    /* Exercise the created AllJoyn object */
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());
    ajn::ProxyBusObject *res = m_aboutListener->CreateProxyBusObject("/resource");
    EXPECT_TRUE(res != NULL);

    /* Get */
    ajn::MsgArg value("b", false);
    ajn::MsgArg variant("v", &value);
    GetPropertyCall get(res);
    EXPECT_EQ(ER_OK, get.Call(ToAJName("x.org.iotivity.rt.false").c_str(),
            "x.org.iotivity.rt.false"));
    EXPECT_EQ(ER_OK, get.Wait(1000));
    EXPECT_TRUE(variant == get.Value());
    EXPECT_EQ(ER_OK, get.Call(ToAJName("x.org.iotivity.rt.true").c_str(),
            "x.org.iotivity.rt.true"));
    EXPECT_EQ(ER_OK, get.Wait(1000));
    EXPECT_TRUE(variant == get.Value());

    /* Set */
    EXPECT_FALSE(resource.False());
    SetPropertyCall set(res);
    value.Set("b", true);
    EXPECT_EQ(ER_OK, set.Call(ToAJName("x.org.iotivity.rt.false").c_str(), "x.org.iotivity.rt.false",
            value));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_TRUE(resource.False());
    EXPECT_FALSE(resource.True());
    EXPECT_EQ(ER_OK, set.Call(ToAJName("x.org.iotivity.rt.true").c_str(), "x.org.iotivity.rt.true",
            value));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_TRUE(resource.True());

    /* GetAll */
    ajn::MsgArg dictEntry("{sv}", "x.org.iotivity.rt.false", &value);
    ajn::MsgArg dict("a{sv}", 1, &dictEntry);
    GetAllPropertiesCall getAll(res);
    EXPECT_EQ(ER_OK, getAll.Call(ToAJName("x.org.iotivity.rt.false").c_str()));
    EXPECT_EQ(ER_OK, getAll.Wait(1000));
    EXPECT_TRUE(dict == getAll.Values());
    dictEntry.v_dictEntry.key->Set("s", "x.org.iotivity.rt.true");
    EXPECT_EQ(ER_OK, getAll.Call(ToAJName("x.org.iotivity.rt.true").c_str()));
    EXPECT_EQ(ER_OK, getAll.Wait(1000));
    EXPECT_TRUE(dict == getAll.Values());

    delete res;
    TearDownResource();
}

TEST_F(OCFResource, IfAnOCFOperationFailsTheErrorNameAndErrorMessageShallBeExtractedFromTheErrorMessage)
{
    TestErrorResource resource;
    SetUpResource(&resource);

    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());
    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);

    GetPropertyCall get(resource0);
    EXPECT_EQ(ER_OK, get.Call(ToAJName("x.org.iotivity.rt").c_str(), "value", true));
    EXPECT_NE(ER_OK, get.Wait(1000));
    EXPECT_EQ("org.openconnectivity.Error.Code500", get.ErrorName());
    EXPECT_EQ("", get.ErrorDescription());

    delete resource0;
    TearDownResource();
}

TEST_F(OCFResource, IfAnOCFOperationFailsTheErrorNameShallBeOrgOpenconnectivityErrorErrorCode)
{
    TestErrorResource resource;
    SetUpResource(&resource);

    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());
    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);

    SetPropertyCall set(resource0);
    ajn::MsgArg value("b", true);
    EXPECT_EQ(ER_OK, set.Call(ToAJName("x.org.iotivity.rt").c_str(), "value", value, true));
    EXPECT_NE(ER_OK, set.Wait(1000));
    EXPECT_EQ("org.freedesktop.DBus.Error.Failed", set.ErrorName());
    EXPECT_EQ("Not allowed", set.ErrorDescription());

    delete resource0;
    TearDownResource();
}

/*
 * 7.1.2.1 Exposing an OCF server as a Virtual AllJoyn Producer
 */

TEST_F(OCFResource, AllJoynAboutDataFields)
{
    TestServerResources resources;
    SetUpResource(&resources);

    ajn::AboutData aboutData = m_aboutListener->GetAboutData("");
    uint8_t *appId;
    size_t n;
    EXPECT_EQ(ER_OK, aboutData.GetAppId(&appId, &n));
    EXPECT_EQ((size_t) UUID_IDENTITY_SIZE, n);
    char idStr[UUID_STRING_SIZE];
    OCConvertUuidToString(appId, idStr);
    EXPECT_STREQ(OCGetServerInstanceIDString(), idStr);
    char *s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDefaultLanguage(&s));
    EXPECT_STREQ("en", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDeviceId(&s));
    EXPECT_STREQ("pi", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetModelNumber(&s));
    EXPECT_STREQ("dmno", s);
    n = 2;
    EXPECT_EQ(n, aboutData.GetSupportedLanguages());
    const char *langs[2];
    EXPECT_EQ(2u, aboutData.GetSupportedLanguages(langs, n));
    EXPECT_STREQ("en", langs[0]);
    EXPECT_STREQ("fr", langs[1]);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDateOfManufacture(&s));
    EXPECT_STREQ("mndt", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetSoftwareVersion(&s));
    EXPECT_STREQ("sv", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetHardwareVersion(&s));
    EXPECT_STREQ("mnhw", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetSupportUrl(&s));
    EXPECT_STREQ("mnsl", s);
    s = NULL;
    ajn::MsgArg *value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnml", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("mnml", s);
    s = NULL;
    value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnpv", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("mnpv", s);
    s = NULL;
    value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnos", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("mnos", s);
    s = NULL;
    value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnfv", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("mnfv", s);
    s = NULL;
    value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.st", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("st", s);
    s = NULL;
    value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.piid", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("piid", s);
    s = NULL;
    value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.iotivity.d", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("d", s);
    s = NULL;
    value = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.iotivity.p", value));
    EXPECT_EQ(ER_OK, value->Get("s", &s));
    EXPECT_STREQ("p", s);

    aboutData = m_aboutListener->GetAboutData("en");
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDeviceName(&s));
    EXPECT_STREQ("en-mnpn", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetAppName(&s));
    EXPECT_STREQ("en-ln", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetManufacturer(&s));
    EXPECT_STREQ("en-dmn", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDescription(&s));
    EXPECT_STREQ("en-ld", s);

    aboutData = m_aboutListener->GetAboutData("fr");
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDeviceName(&s));
    EXPECT_STREQ("fr-mnpn", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetAppName(&s));
    EXPECT_STREQ("fr-ln", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetManufacturer(&s));
    EXPECT_STREQ("fr-dmn", s);
    s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDescription(&s));
    EXPECT_STREQ("fr-ld", s);

    TearDownResource();
}

TEST_F(OCFResource, AllJoynConfigurationDataFields)
{
    TestServerResources resources;
    SetUpResource(&resources);

    ajn::MsgArg configData = m_aboutListener->GetConfigurations("");
    char *s;
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "DefaultLanguage", &s));
    EXPECT_STREQ("en", s);
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "DeviceName", &s));
    EXPECT_STREQ("en-mnpn", s);
    size_t n;
    double *d;
    EXPECT_EQ(ER_OK, configData.GetElement("{sad}", "org.openconnectivity.loc", &n, &d));
    EXPECT_EQ(2u, n);
    EXPECT_EQ(-1, d[0]);
    EXPECT_EQ(1, d[1]);
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "org.openconnectivity.locn", &s));
    EXPECT_STREQ("locn", s);
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "org.openconnectivity.c", &s));
    EXPECT_STREQ("c", s);
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "org.openconnectivity.r", &s));
    EXPECT_STREQ("r", s);
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "org.iotivity.con", &s));
    EXPECT_STREQ("con", s);
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "org.iotivity.con.p", &s));
    EXPECT_STREQ("con.p", s);

    configData = m_aboutListener->GetConfigurations("en");
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "DeviceName", &s));
    EXPECT_STREQ("en-mnpn", s);

    configData = m_aboutListener->GetConfigurations("fr");
    EXPECT_EQ(ER_OK, configData.GetElement("{ss}", "DeviceName", &s));
    EXPECT_STREQ("fr-mnpn", s);

    TearDownResource();
}

TEST_F(OCFResource, FactoryResetShallBeMappedToOicWkMntPropertyFactory_Reset)
{
    TestServerResources resources;
    SetUpResource(&resources);

    EXPECT_FALSE(resources.FactoryReset());
    ajn::ProxyBusObject *config = m_aboutListener->CreateProxyBusObject("/Config");
    EXPECT_TRUE(config != NULL);
    MethodCall factoryReset(m_bus, config);
    EXPECT_EQ(ER_OK, factoryReset.Call("org.alljoyn.Config", "FactoryReset", NULL, 0));
    EXPECT_EQ(ER_OK, factoryReset.Wait(1000));
    EXPECT_TRUE(resources.FactoryReset());

    delete config;
    TearDownResource();
}

TEST_F(OCFResource, RestartShallBeMappedToOicWkMntPropertyReboot)
{
    TestServerResources resources;
    SetUpResource(&resources);

    EXPECT_FALSE(resources.Reboot());
    ajn::ProxyBusObject *config = m_aboutListener->CreateProxyBusObject("/Config");
    EXPECT_TRUE(config != NULL);
    MethodCall restart(m_bus, config);
    EXPECT_EQ(ER_OK, restart.Call("org.alljoyn.Config", "Restart", NULL, 0));
    EXPECT_EQ(ER_OK, restart.Wait(1000));
    EXPECT_TRUE(resources.Reboot());

    delete config;
    TearDownResource();
}

/*
 * Additional tests
 */

class PropertiesChangedListener : public ajn::ProxyBusObject::PropertiesChangedListener
{
public:
    size_t m_calls;
    PropertiesChangedListener() : m_calls(0) { }
    virtual ~PropertiesChangedListener() { }
    const ajn::MsgArg &Changed() { return m_changed; }
private:
    ajn::MsgArg m_changed;
    void PropertiesChanged(ajn::ProxyBusObject& proxyObj, const char* ifaceName,
            const ajn::MsgArg& changed, const ajn::MsgArg& invalidated, void* context)
    {
        (void) proxyObj;
        (void) ifaceName;
        m_changed = changed;
        (void) invalidated;
        (void) context;
        ++m_calls;
    }
};

TEST_F(OCFResource, Observe)
{
    TestObservableResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::ProxyBusObject *resource0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(resource0 != NULL);
    PropertiesChangedListener listener;
    EXPECT_EQ(ER_OK, resource0->RegisterPropertiesChangedListener("org.iotivity.rt", NULL, 0,
            listener, NULL));

    Wait(1000); /* Must wait for observe request to be processed */
    listener.m_calls = 0;
    resource.Notify("/resource/0");
    Wait(1000);
    EXPECT_EQ(1u, listener.m_calls);

    delete resource0;
    TearDownResource();
}

TEST_F(OCFResource, ObserveCollectionResource)
{
    TestCollectionResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::ProxyBusObject *res = m_aboutListener->CreateProxyBusObject("/resource");
    EXPECT_TRUE(res != NULL);
    PropertiesChangedListener listener;
    EXPECT_EQ(ER_OK, res->RegisterPropertiesChangedListener("org.iotivity.rt.true", NULL, 0,
            listener, NULL));

    Wait(1000); /* Must wait for observe request to be processed */
    listener.m_calls = 0;
    resource.Notify("/resource/1");
    Wait(1000);
    EXPECT_EQ(1u, listener.m_calls);

    delete res;
    TearDownResource();
}

TEST_F(OCFResource, DeviceConfiguration)
{
    TestDeviceConfigurationResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::ProxyBusObject *config = m_aboutListener->CreateProxyBusObject("/Config");
    EXPECT_TRUE(config != NULL);

    /* org.freedesktop.DBus.Properties */
    ajn::MsgArg property;
    EXPECT_EQ(ER_OK, config->GetProperty("org.alljoyn.Config", "Version", property));
    EXPECT_EQ(1, property.v_variant.val->v_uint16);

    EXPECT_NE(ER_OK, config->SetProperty("org.alljoyn.Config", "Version", property));

    ajn::MsgArg properties;
    EXPECT_EQ(ER_OK, config->GetAllProperties("org.alljoyn.Config", properties));
    uint16_t version;
    EXPECT_EQ(1u, properties.v_array.GetNumElements());
    EXPECT_EQ(ER_OK, properties.GetElement("{sq}", "Version", &version));
    EXPECT_EQ(1, version);

    /* org.alljoyn.Config.GetConfigurations */

    /* Mandatory properties */
    resource.SetMandatoryProperties();
    ajn::MsgArg arg = m_aboutListener->GetConfigurations("");
    EXPECT_EQ(0u, arg.v_array.GetNumElements());

    /* All properties */
    resource.SetAllProperties();
    arg = m_aboutListener->GetConfigurations("");
    EXPECT_EQ(8u, arg.v_array.GetNumElements());
    char *s = NULL;
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "DefaultLanguage", &s));
    EXPECT_STREQ("en", s);
    size_t n;
    double *ds = NULL;
    EXPECT_EQ(ER_OK, arg.GetElement("{sad}", "org.openconnectivity.loc", &n, &ds));
    EXPECT_EQ(2u, n);
    EXPECT_EQ(-1, ds[0]);
    EXPECT_EQ(1, ds[1]);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.openconnectivity.locn", &s));
    EXPECT_STREQ("locn", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.openconnectivity.c", &s));
    EXPECT_STREQ("c", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.openconnectivity.r", &s));
    EXPECT_STREQ("r", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.iotivity.con", &s));
    EXPECT_STREQ("con", s);

    delete config;
    TearDownResource();
}

TEST_F(OCFResource, PlatformConfiguration)
{
    TestPlatformConfigurationResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    /* org.alljoyn.Config.GetConfigurations */

    /* All properties (there are no mandatory properties) */
    resource.SetAllProperties();
    ajn::MsgArg arg = m_aboutListener->GetConfigurations("");
    char *s = NULL;
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "DeviceName", &s));
    EXPECT_STREQ("en-mnpn", s);

    TearDownResource();
}

static void ConfigurationPropertiesVerify(AboutListener *aboutListener,
        TestServerResources *resources)
{
    /* org.alljoyn.Config.GetConfigurations */

    /* Mandatory properties */
    resources->SetMandatoryProperties();
    ajn::MsgArg arg = aboutListener->GetConfigurations("");
    EXPECT_EQ(0u, arg.v_array.GetNumElements());

    /* All properties */
    resources->SetAllProperties();
    arg = aboutListener->GetConfigurations("");
    EXPECT_EQ(10u, arg.v_array.GetNumElements());
    char *s = NULL;
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "DefaultLanguage", &s));
    EXPECT_STREQ("en", s);
    size_t n;
    double *ds = NULL;
    EXPECT_EQ(ER_OK, arg.GetElement("{sad}", "org.openconnectivity.loc", &n, &ds));
    EXPECT_EQ(2u, n);
    EXPECT_EQ(-1, ds[0]);
    EXPECT_EQ(1, ds[1]);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.openconnectivity.locn", &s));
    EXPECT_STREQ("locn", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.openconnectivity.c", &s));
    EXPECT_STREQ("c", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.openconnectivity.r", &s));
    EXPECT_STREQ("r", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.iotivity.con", &s));
    EXPECT_STREQ("con", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "org.iotivity.con.p", &s));
    EXPECT_STREQ("con.p", s);
    EXPECT_EQ(ER_OK, arg.GetElement("{ss}", "DeviceName", &s));
    EXPECT_STREQ("en-mnpn", s);
}

TEST_F(OCFResource, ConfigurationOneResource)
{
    TestServerResources resources;
    resources.UseOneConfigurationResource();
    SetUpResource(&resources);

    ConfigurationPropertiesVerify(m_aboutListener, &resources);

    TearDownResource();
}

TEST_F(OCFResource, ConfigurationTwoResources)
{
    TestServerResources resources;
    SetUpResource(&resources);

    ConfigurationPropertiesVerify(m_aboutListener, &resources);

    TearDownResource();
}

TEST_F(OCFResource, UpdateDeviceConfiguration)
{
    TestDeviceConfigurationResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::MsgArg elems[8];
    ajn::MsgArg *elem = elems;
    (elem++)->Set("{sv}", "DefaultLanguage", new ajn::MsgArg("s", "fr"));
    double newLoc[2] = { -10.0, 10.0 };
    (elem++)->Set("{sv}", "org.openconnectivity.loc", new ajn::MsgArg("ad", 2, newLoc));
    (elem++)->Set("{sv}", "org.openconnectivity.locn", new ajn::MsgArg("s", "new-locn"));
    (elem++)->Set("{sv}", "org.openconnectivity.c", new ajn::MsgArg("s", "new-c"));
    (elem++)->Set("{sv}", "org.openconnectivity.r", new ajn::MsgArg("s", "new-r"));
    (elem++)->Set("{sv}", "org.iotivity.con", new ajn::MsgArg("s", "new-con"));
    ajn::MsgArg arg("a{sv}", elem - elems, elems);
    m_aboutListener->UpdateConfigurations("", arg);

    EXPECT_EQ("fr", resource.Configuration().m_dl);
    EXPECT_EQ(newLoc[0], resource.Configuration().m_loc[0]);
    EXPECT_EQ(newLoc[1], resource.Configuration().m_loc[1]);
    EXPECT_EQ("new-locn", resource.Configuration().m_locn);
    EXPECT_EQ("new-c", resource.Configuration().m_c);
    EXPECT_EQ("new-r", resource.Configuration().m_r);
    EXPECT_EQ("new-con", resource.Configuration().m_orgIoTivityCon);

    TearDownResource();
}

TEST_F(OCFResource, UpdatePlatformConfiguration)
{
    TestPlatformConfigurationResource resource;
    SetUpResource(&resource);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::MsgArg elems[1];
    ajn::MsgArg *elem = elems;
    (elem++)->Set("{sv}", "DeviceName", new ajn::MsgArg("s", "new-en-mnpn"));
    ajn::MsgArg arg("a{sv}", elem - elems, elems);
    m_aboutListener->UpdateConfigurations("en", arg);

    EXPECT_EQ("new-en-mnpn", resource.Configuration().m_mnpn["en"]);

    TearDownResource();
}

TEST_F(OCFResource, UpdateConfigurationOneResource)
{
    TestServerResources resources;
    resources.UseOneConfigurationResource();
    SetUpResource(&resources);

    ajn::MsgArg elems[9];
    ajn::MsgArg *elem = elems;
    (elem++)->Set("{sv}", "DefaultLanguage", new ajn::MsgArg("s", "fr"));
    double newLoc[2] = { -10.0, 10.0 };
    (elem++)->Set("{sv}", "org.openconnectivity.loc", new ajn::MsgArg("ad", 2, newLoc));
    (elem++)->Set("{sv}", "org.openconnectivity.locn", new ajn::MsgArg("s", "new-locn"));
    (elem++)->Set("{sv}", "org.openconnectivity.c", new ajn::MsgArg("s", "new-c"));
    (elem++)->Set("{sv}", "org.openconnectivity.r", new ajn::MsgArg("s", "new-r"));
    (elem++)->Set("{sv}", "org.iotivity.con", new ajn::MsgArg("s", "new-con"));
    (elem++)->Set("{sv}", "DeviceName", new ajn::MsgArg("s", "new-fr-mnpn"));
    ajn::MsgArg arg("a{sv}", elem - elems, elems);
    m_aboutListener->UpdateConfigurations("fr", arg);

    EXPECT_EQ("fr", resources.Configuration().m_dl);
    EXPECT_EQ(newLoc[0], resources.Configuration().m_loc[0]);
    EXPECT_EQ(newLoc[1], resources.Configuration().m_loc[1]);
    EXPECT_EQ("new-locn", resources.Configuration().m_locn);
    EXPECT_EQ("new-c", resources.Configuration().m_c);
    EXPECT_EQ("new-r", resources.Configuration().m_r);
    EXPECT_EQ("new-con", resources.Configuration().m_orgIoTivityCon);
    EXPECT_EQ("new-fr-mnpn", resources.Configuration().m_mnpn["fr"]);

    TearDownResource();
}

TEST_F(OCFResource, UpdateConfigurationTwoResources)
{
    TestServerResources resources;
    SetUpResource(&resources);

    ajn::MsgArg elems[9];
    ajn::MsgArg *elem = elems;
    (elem++)->Set("{sv}", "DefaultLanguage", new ajn::MsgArg("s", "fr"));
    double newLoc[2] = { -10.0, 10.0 };
    (elem++)->Set("{sv}", "org.openconnectivity.loc", new ajn::MsgArg("ad", 2, newLoc));
    (elem++)->Set("{sv}", "org.openconnectivity.locn", new ajn::MsgArg("s", "new-locn"));
    (elem++)->Set("{sv}", "org.openconnectivity.c", new ajn::MsgArg("s", "new-c"));
    (elem++)->Set("{sv}", "org.openconnectivity.r", new ajn::MsgArg("s", "new-r"));
    (elem++)->Set("{sv}", "org.iotivity.con", new ajn::MsgArg("s", "new-con"));
    (elem++)->Set("{sv}", "DeviceName", new ajn::MsgArg("s", "new-fr-mnpn"));
    ajn::MsgArg arg("a{sv}", elem - elems, elems);
    m_aboutListener->UpdateConfigurations("fr", arg);

    EXPECT_EQ("fr", resources.Configuration().m_dl);
    EXPECT_EQ(newLoc[0], resources.Configuration().m_loc[0]);
    EXPECT_EQ(newLoc[1], resources.Configuration().m_loc[1]);
    EXPECT_EQ("new-locn", resources.Configuration().m_locn);
    EXPECT_EQ("new-c", resources.Configuration().m_c);
    EXPECT_EQ("new-r", resources.Configuration().m_r);
    EXPECT_EQ("new-con", resources.Configuration().m_orgIoTivityCon);
    EXPECT_EQ("new-fr-mnpn", resources.Configuration().m_mnpn["fr"]);

    TearDownResource();
}

TEST_F(OCFResource, UpdateConfigurationWithDefaultLanguage)
{
    TestServerResources resources;
    SetUpResource(&resources);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    ajn::MsgArg elems[1];
    ajn::MsgArg *elem = elems;
    (elem++)->Set("{sv}", "DeviceName", new ajn::MsgArg("s", "new-en-mnpn"));
    ajn::MsgArg arg("a{sv}", elem - elems, elems);
    m_aboutListener->UpdateConfigurations("", arg);

    EXPECT_EQ("new-en-mnpn", resources.Configuration().m_mnpn["en"]);

    TearDownResource();
}

TEST_F(OCFResource, UpdateConfigurationWithDefaultLanguageWhenNotPresentInResource)
{
    TestServerResources resources;
    resources.Configuration().m_includeDefaultLanguage = false;
    SetUpResource(&resources);
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());

    /* The translator synthesizes the required DefaultLanguage */
    ajn::AboutData aboutData = m_aboutListener->GetAboutData("");
    char *s = NULL;
    EXPECT_EQ(ER_OK, aboutData.GetDefaultLanguage(&s));
    EXPECT_STREQ("en", s);

    ajn::MsgArg elems[1];
    ajn::MsgArg *elem = elems;
    (elem++)->Set("{sv}", "DeviceName", new ajn::MsgArg("s", "new-en-mnpn"));
    ajn::MsgArg arg("a{sv}", elem - elems, elems);
    m_aboutListener->UpdateConfigurations("", arg);

    EXPECT_EQ("new-en-mnpn", resources.Configuration().m_mnpn["en"]);

    TearDownResource();
}

/*
 * Additional tests
 */

struct NumericProperties
{
    int64_t m_byte;
    int64_t m_int64small;
    std::string m_int64large;
    int64_t m_uint64small;
    std::string m_uint64large;
    NumericProperties()
        : m_byte(1), m_int64small(-1), m_int64large("-1"), m_uint64small(1), m_uint64large("1") { }
};

static OCEntityHandlerResult NumericPropertiesEntityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    (void) flag;
    NumericProperties *properties = (NumericProperties *) ctx;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = CreatePayload(request->resource, request->query);
                if (!payload ||
                        !OCRepPayloadSetPropInt(payload, "byte", properties->m_byte) ||
                        !OCRepPayloadSetPropInt(payload, "int64small", properties->m_int64small) ||
                        !OCRepPayloadSetPropString(payload, "int64large", properties->m_int64large.c_str()) ||
                        !OCRepPayloadSetPropInt(payload, "uint64small", properties->m_uint64small) ||
                        !OCRepPayloadSetPropString(payload, "uint64large", properties->m_uint64large.c_str()))
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
                OCRepPayload *outPayload = CreatePayload(request->resource, request->query);
                if (!outPayload)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                char *s;
                if (OCRepPayloadGetPropInt((OCRepPayload *) request->payload, "byte",
                        &properties->m_byte))
                {
                    OCRepPayloadSetPropInt(outPayload, "byte", properties->m_byte);
                }
                if (OCRepPayloadGetPropInt((OCRepPayload *) request->payload, "int64small",
                        &properties->m_int64small))
                {
                    OCRepPayloadSetPropInt(outPayload, "int64small", properties->m_int64small);
                }
                if (OCRepPayloadGetPropString((OCRepPayload *) request->payload, "int64large", &s))
                {
                    properties->m_int64large = s;
                    OCRepPayloadSetPropString(outPayload, "int64large", s);
                }
                OCRepPayloadGetPropInt((OCRepPayload *) request->payload, "uint64small",
                        &properties->m_uint64small);
                if (OCRepPayloadGetPropString((OCRepPayload *) request->payload, "uint64large", &s))
                {
                    properties->m_uint64large = s;
                    OCRepPayloadSetPropString(outPayload, "uint64large", s);
                }
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    OCRepPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

class TestNumericTypesResource : public TestResource
{
public:
    virtual ~TestNumericTypesResource() { }
    virtual void Create()
    {
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt", NULL,
                "/resource/0", NumericPropertiesEntityHandler, &m_properties,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
    NumericProperties &Properties() { return m_properties; }
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource/0\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"byte\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 255 },"
            "        \"int64small\": { \"type\": \"integer\",\"minimum\": -9007199254740992, \"maximum\": 9007199254740992 },"
            "        \"int64large\": { \"type\": \"string\", \"format\": \"int64\" },"
            "        \"uint64small\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 9007199254740992 },"
            "        \"uint64large\": { \"type\": \"string\", \"format\": \"uint64\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    NumericProperties m_properties;
};

TEST_F(OCFResource, NumericTypes)
{
    TestNumericTypesResource resource;
    SetUpResource(&resource);

    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());
    ajn::ProxyBusObject *res = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(res != NULL);

    /* Get */
    ajn::MsgArg value("y", 1);
    ajn::MsgArg variant("v", &value);
    GetPropertyCall get(res);
    EXPECT_EQ(ER_OK, get.Call("org.iotivity.rt", "byte"));
    EXPECT_EQ(ER_OK, get.Wait(1000));
    EXPECT_TRUE(variant == get.Value());

    /* GetAll */
    ajn::MsgArg entries[5];
    entries[0] = ajn::MsgArg("{sv}", "byte", new ajn::MsgArg("y", 1));
    entries[1] = ajn::MsgArg("{sv}", "int64small", new ajn::MsgArg("x", (int64_t) -1));
    entries[2] = ajn::MsgArg("{sv}", "int64large", new ajn::MsgArg("x", (int64_t) -1));
    entries[3] = ajn::MsgArg("{sv}", "uint64small", new ajn::MsgArg("t", 1));
    entries[4] = ajn::MsgArg("{sv}", "uint64large", new ajn::MsgArg("t", 1));
    ajn::MsgArg dict("a{sv}", 5, entries);
    GetAllPropertiesCall getAll(res);
    EXPECT_EQ(ER_OK, getAll.Call("org.iotivity.rt"));
    EXPECT_EQ(ER_OK, getAll.Wait(1000));
    EXPECT_TRUE(dict == getAll.Values());

    /* PropertiesChanged */
    PropertiesChangedListener listener;
    EXPECT_EQ(ER_OK, res->RegisterPropertiesChangedListener("org.iotivity.rt", NULL, 0, listener,
            NULL));
    Wait(1000); /* Must wait for observe request to be processed */
    listener.m_calls = 0;
    resource.Notify("/resource/0");
    Wait(1000);
    EXPECT_EQ(1u, listener.m_calls);
    EXPECT_TRUE(dict == listener.Changed());

    /* Set */
    SetPropertyCall set(res);
    EXPECT_EQ(ER_OK, set.Call("org.iotivity.rt", "byte", *new ajn::MsgArg("y", 2)));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_EQ(2, resource.Properties().m_byte);
    EXPECT_EQ(ER_OK, set.Call("org.iotivity.rt", "int64small", *new ajn::MsgArg("x", (int64_t) -2)));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_EQ(-2, resource.Properties().m_int64small);
    EXPECT_EQ(ER_OK, set.Call("org.iotivity.rt", "int64large", *new ajn::MsgArg("x", (int64_t) -2)));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_STREQ("-2", resource.Properties().m_int64large.c_str());
    EXPECT_EQ(ER_OK, set.Call("org.iotivity.rt", "uint64small", *new ajn::MsgArg("t", 2)));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_EQ(2, resource.Properties().m_uint64small);
    EXPECT_EQ(ER_OK, set.Call("org.iotivity.rt", "uint64large", *new ajn::MsgArg("t", 2)));
    EXPECT_EQ(ER_OK, set.Wait(1000));
    EXPECT_STREQ("2", resource.Properties().m_uint64large.c_str());

    delete res;
    TearDownResource();
}

class VariantProperties
{
public:
    virtual ~VariantProperties() { }
    virtual bool Get(OCRepPayload *payload) = 0;
    virtual void Post(const OCRepPayload *payload, OCRepPayload *outPayload) = 0;
};

class VariantProperties0 : public VariantProperties
{
public:
    int64_t m_byte;
    int64_t m_uint16;
    int64_t m_int16;
    int64_t m_uint32;
    int64_t m_int32;
    int64_t m_uint64;
    int64_t m_int64;
    double m_double;
    std::string m_uint64large;
    std::string m_int64large;
    std::string m_string;
    OCByteString m_stringbase64;
    OCByteString m_stringpattern;
    int64_t m_variant;
    VariantProperties0()
        : m_byte(1), m_uint16(1), m_int16(1), m_uint32(1), m_int32(1), m_uint64(1), m_int64(1),
          m_double(1.0), m_uint64large("1"), m_int64large("1"), m_string("one"),
          m_stringbase64({ NULL, 0 }), m_stringpattern({ NULL, 0 }), m_variant(1)
    {
        uint8_t bs[] = { 0, 1 };
        OCByteString byteString = { bs, 2 };
        OCByteStringCopy(&m_stringbase64, &byteString);
        OCByteStringCopy(&m_stringpattern, &byteString);
    }
    virtual bool Get(OCRepPayload *payload)
    {
        OCRepPayload *object = OCRepPayloadCreate();
        return object &&
                OCRepPayloadSetPropInt(object, "byte", m_byte) &&
                OCRepPayloadSetPropInt(object, "uint16", m_uint16) &&
                OCRepPayloadSetPropInt(object, "int16", m_int16) &&
                OCRepPayloadSetPropInt(object, "uint32", m_uint32) &&
                OCRepPayloadSetPropInt(object, "int32", m_int32) &&
                OCRepPayloadSetPropInt(object, "uint64", m_uint64) &&
                OCRepPayloadSetPropInt(object, "int64", m_int64) &&
                OCRepPayloadSetPropDouble(object, "double", m_double) &&
                OCRepPayloadSetPropString(object, "uint64large", m_uint64large.c_str()) &&
                OCRepPayloadSetPropString(object, "int64large", m_int64large.c_str()) &&
                OCRepPayloadSetPropString(object, "string", m_string.c_str()) &&
                OCRepPayloadSetPropByteString(object, "stringbase64", m_stringbase64) &&
                OCRepPayloadSetPropByteString(object, "stringpattern", m_stringpattern) &&
                OCRepPayloadSetPropInt(object, "variant", m_variant) &&
                OCRepPayloadSetPropObjectAsOwner(payload, "object", object);
    }
    virtual void Post(const OCRepPayload *payload, OCRepPayload *outPayload)
    {
        OCRepPayload *object;
        OCRepPayloadGetPropObject(payload, "object", &object);
        OCRepPayload *outObject = OCRepPayloadCreate();
        if (OCRepPayloadGetPropInt(object, "byte", &m_byte))
        {
            OCRepPayloadSetPropInt(outObject, "byte", m_byte);
        }
        if (OCRepPayloadGetPropInt(object, "uint16", &m_uint16))
        {
            OCRepPayloadSetPropInt(outObject, "uint16", m_uint16);
        }
        if (OCRepPayloadGetPropInt(object, "int16", &m_int16))
        {
            OCRepPayloadSetPropInt(outObject, "int16", m_int16);
        }
        if (OCRepPayloadGetPropInt(object, "uint32", &m_uint32))
        {
            OCRepPayloadSetPropInt(outObject, "uint32", m_uint32);
        }
        if (OCRepPayloadGetPropInt(object, "int32", &m_int32))
        {
            OCRepPayloadSetPropInt(outObject, "int32", m_int32);
        }
        if (OCRepPayloadGetPropInt(object, "uint64", &m_uint64))
        {
            OCRepPayloadSetPropInt(outObject, "uint64", m_uint64);
        }
        if (OCRepPayloadGetPropInt(object, "int64", &m_int64))
        {
            OCRepPayloadSetPropInt(outObject, "int64", m_int64);
        }
        if (OCRepPayloadGetPropDouble(object, "double", &m_double))
        {
            OCRepPayloadSetPropDouble(outObject, "double", m_double);
        }
        char *s;
        if (OCRepPayloadGetPropString(object, "uint64large", &s))
        {
            m_uint64large = s;
            OCRepPayloadSetPropString(outObject, "uint64large", m_uint64large.c_str());
        }
        if (OCRepPayloadGetPropString(object, "int64large", &s))
        {
            m_int64large = s;
            OCRepPayloadSetPropString(outObject, "int64large", m_int64large.c_str());
        }
        if (OCRepPayloadGetPropString(object, "string", &s))
        {
            m_string = s;
            OCRepPayloadSetPropString(outObject, "string", m_string.c_str());
        }
        if (OCRepPayloadGetPropByteString(object, "stringbase64", &m_stringbase64))
        {
            OCRepPayloadSetPropByteString(outObject, "stringbase64", m_stringbase64);
        }
        if (OCRepPayloadGetPropByteString(object, "stringpattern", &m_stringpattern))
        {
            OCRepPayloadSetPropByteString(outObject, "stringpattern", m_stringpattern);
        }
        if (OCRepPayloadGetPropInt(object, "variant", &m_variant))
        {
            OCRepPayloadSetPropInt(outObject, "variant", m_variant);
        }
        OCRepPayloadSetPropObjectAsOwner(outPayload, "object", outObject);
    }
};

class VariantProperties1 : public VariantProperties
{
public:
    OCRepPayload* m_struct0;
    VariantProperties1()
        : m_struct0(NULL)
    {
        m_struct0 = OCRepPayloadCreate();
        OCRepPayloadSetPropInt(m_struct0, "0", 1);
        OCRepPayloadSetPropString(m_struct0, "1", "one");
    }
    virtual bool Get(OCRepPayload *payload)
    {
        OCRepPayload *object = OCRepPayloadCreate();
        return object &&
                OCRepPayloadSetPropObject(object, "struct0", m_struct0) &&
                OCRepPayloadSetPropObjectAsOwner(payload, "object", object);
    }
    virtual void Post(const OCRepPayload *payload, OCRepPayload *outPayload)
    {
        OCRepPayload *object;
        OCRepPayloadGetPropObject(payload, "object", &object);
        OCRepPayload *outObject = OCRepPayloadCreate();
        if (OCRepPayloadGetPropObject(object, "struct0", &m_struct0))
        {
            OCRepPayloadSetPropObject(outObject, "struct0", m_struct0);
        }
        OCRepPayloadSetPropObjectAsOwner(outPayload, "object", outObject);
    }
};

class VariantProperties2 : public VariantProperties
{
public:
    OCRepPayload* m_struct1;
    VariantProperties2()
        : m_struct1(NULL)
    {
        m_struct1 = OCRepPayloadCreate();
        OCRepPayloadSetPropInt(m_struct1, "int", 1);
        OCRepPayloadSetPropString(m_struct1, "string", "one");
    }
    virtual bool Get(OCRepPayload *payload)
    {
        OCRepPayload *object = OCRepPayloadCreate();
        return object &&
                OCRepPayloadSetPropObject(object, "struct1", m_struct1) &&
                OCRepPayloadSetPropObjectAsOwner(payload, "object", object);
    }
    virtual void Post(const OCRepPayload *payload, OCRepPayload *outPayload)
    {
        OCRepPayload *object;
        OCRepPayloadGetPropObject(payload, "object", &object);
        OCRepPayload *outObject = OCRepPayloadCreate();
        if (OCRepPayloadGetPropObject(object, "struct1", &m_struct1))
        {
            OCRepPayloadSetPropObject(outObject, "struct1", m_struct1);
        }
        OCRepPayloadSetPropObjectAsOwner(outPayload, "object", outObject);
    }
};

class VariantProperties3 : public VariantProperties
{
public:
    OCRepPayload* m_dict;
    VariantProperties3()
        : m_dict(NULL)
    {
        m_dict = OCRepPayloadCreate();
        OCRepPayloadSetPropInt(m_dict, "int", 1);
        OCRepPayloadSetPropString(m_dict, "string", "one");
    }
    virtual bool Get(OCRepPayload *payload)
    {
        OCRepPayload *object = OCRepPayloadCreate();
        return object &&
                OCRepPayloadSetPropObject(object, "dict", m_dict) &&
                OCRepPayloadSetPropObjectAsOwner(payload, "object", object);
    }
    virtual void Post(const OCRepPayload *payload, OCRepPayload *outPayload)
    {
        OCRepPayload *object;
        OCRepPayloadGetPropObject(payload, "object", &object);
        OCRepPayload *outObject = OCRepPayloadCreate();
        if (OCRepPayloadGetPropObject(object, "dict", &m_dict))
        {
            OCRepPayloadSetPropObject(outObject, "dict", m_dict);
        }
        OCRepPayloadSetPropObjectAsOwner(outPayload, "object", outObject);
    }
};

static OCEntityHandlerResult VariantPropertiesEntityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request, void *ctx)
{
    (void) flag;
    VariantProperties *properties = (VariantProperties *) ctx;
    OCEntityHandlerResult result;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                OCRepPayload *payload = CreatePayload(request->resource, request->query);
                if (!payload || !properties->Get(payload))
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
                OCRepPayload *outPayload = CreatePayload(request->resource, request->query);
                if (!outPayload)
                {
                    result = OC_EH_ERROR;
                    break;
                }
                properties->Post((const OCRepPayload *) request->payload, outPayload);
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;
                result = OC_EH_OK;
                response.ehResult = result;
                response.payload = reinterpret_cast<OCPayload *>(outPayload);
                OCStackResult doResult = OCDoResponse(&response);
                if (doResult != OC_STACK_OK)
                {
                    OCRepPayloadDestroy(outPayload);
                }
                break;
            }
        default:
            result = OC_EH_METHOD_NOT_ALLOWED;
            break;
    }
    return result;
}

class TestVariantTypesResource : public TestResource
{
public:
    virtual ~TestVariantTypesResource() { }
    virtual void Create()
    {
        OCResourceHandle handle;
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt0", NULL,
                "/resource/0", VariantPropertiesEntityHandler, &m_properties0,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt1", NULL,
                "/resource/1", VariantPropertiesEntityHandler, &m_properties1,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt2", NULL,
                "/resource/2", VariantPropertiesEntityHandler, &m_properties2,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&handle, "x.org.iotivity.rt3", NULL,
                "/resource/3", VariantPropertiesEntityHandler, &m_properties3,
                OC_DISCOVERABLE | OC_OBSERVABLE));
        m_handles.push_back(handle);
    }
    virtual const char *IntrospectionJson() { return m_introspectionJson; }
    VariantProperties0 m_properties0;
    VariantProperties1 m_properties1;
    VariantProperties2 m_properties2;
    VariantProperties3 m_properties3;
private:
    const char *m_introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource/0\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt0\" } ] } } }"
            "      }"
            "    },"
            "    \"/resource/1\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt1\" } ] } } }"
            "      }"
            "    },"
            "    \"/resource/2\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt2\" } ] } } }"
            "      }"
            "    },"
            "    \"/resource/3\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt3\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt0\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"object\": {"
            "          \"type\": \"object\","
            "          \"properties\": {"
            "            \"byte\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 255 },"
            "            \"uint16\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 65535 },"
            "            \"int16\": { \"type\": \"integer\", \"minimum\": -32768, \"maximum\": 32767 },"
            "            \"uint32\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 4294967295 },"
            "            \"int32\": { \"type\": \"integer\", \"minimum\": -2147483648, \"maximum\": 2147483647 },"
            "            \"uint64\": { \"type\": \"integer\", \"minimum\": 0 },"
            "            \"int64\": { \"type\": \"integer\" },"
            "            \"double\": { \"type\": \"number\" },"
            "            \"uint64large\": { \"type\": \"string\", \"pattern\": \"^0([1-9][0-9]{0,19})$\" },"
            "            \"int64large\": { \"type\": \"string\", \"pattern\": \"^0(-?[1-9][0-9]{0,18)}$\" },"
            "            \"string\": { \"type\": \"string\" },"
            "            \"stringbase64\": { \"type\": \"string\", \"media\": { \"binaryEncoding\": \"base64\" } },"
            "            \"stringpattern\": { \"type\": \"string\", \"pattern\": \"^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$\" },"
            "            \"variant\": { \"type\": [ \"integer\", \"string\" ] }"
            "          }"
            "        },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt0\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    },"
            "    \"x.org.iotivity.rt1\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"object\": {"
            "          \"type\": \"object\","
            "          \"properties\": {"
            "            \"struct0\": { \"type\": \"array\", \"items\": [ { \"type\": \"integer\", \"minimum\": -2147483648, \"maximum\": 2147483647 }, { \"type\": \"string\" } ], \"minItems\": 2, \"maxItems\": 2 }"
            "          }"
            "        },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt1\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    },"
            "    \"x.org.iotivity.rt2\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"object\": {"
            "          \"type\": \"object\","
            "          \"properties\": {"
            "            \"struct1\": { \"type\": \"object\", \"properties\": { \"int\": { \"type\": \"integer\", \"minimum\": -2147483648, \"maximum\": 2147483647 }, \"string\": { \"type\": \"string\" } } }"
            "          }"
            "        },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt2\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    },"
            "    \"x.org.iotivity.rt3\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"object\": {"
            "          \"type\": \"object\","
            "          \"properties\": {"
            "            \"dict\": { \"type\": \"object\" }"
            "          }"
            "        },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt3\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
};

TEST_F(OCFResource, VariantTypes)
{
    TestVariantTypesResource resource;
    SetUpResource(&resource);

    /* Need to use multiple resources to work around localhost blockwise transfer issue */
    EXPECT_EQ(ER_OK, m_aboutListener->JoinSession());
    ajn::ProxyBusObject *res0 = m_aboutListener->CreateProxyBusObject("/resource/0");
    EXPECT_TRUE(res0 != NULL);
    ajn::ProxyBusObject *res1 = m_aboutListener->CreateProxyBusObject("/resource/1");
    EXPECT_TRUE(res1 != NULL);
    ajn::ProxyBusObject *res2 = m_aboutListener->CreateProxyBusObject("/resource/2");
    EXPECT_TRUE(res2 != NULL);
    ajn::ProxyBusObject *res3 = m_aboutListener->CreateProxyBusObject("/resource/3");
    EXPECT_TRUE(res2 != NULL);

    ajn::MsgArg entries[16];
    ajn::MsgArg *entry;
    ajn::MsgArg value;
    ajn::MsgArg values;
    ajn::MsgArg elems[2];
    elems[0].Set("{sv}", "int", new ajn::MsgArg("i", 1));
    elems[1].Set("{sv}", "string", new ajn::MsgArg("s", "one"));

    /* Get */
    GetPropertyCall get0(res0);
    EXPECT_EQ(ER_OK, get0.Call("org.iotivity.rt0", "object"));
    EXPECT_EQ(ER_OK, get0.Wait(1000));
    GetPropertyCall get1(res1);
    EXPECT_EQ(ER_OK, get1.Call("org.iotivity.rt1", "object"));
    EXPECT_EQ(ER_OK, get1.Wait(1000));
    GetPropertyCall get2(res2);
    EXPECT_EQ(ER_OK, get2.Call("org.iotivity.rt2", "object"));
    EXPECT_EQ(ER_OK, get2.Wait(1000));
    GetPropertyCall get3(res3);
    EXPECT_EQ(ER_OK, get3.Call("org.iotivity.rt3", "object"));
    EXPECT_EQ(ER_OK, get3.Wait(1000));

    /* GetAll */
    GetAllPropertiesCall getAll0(res0);
    EXPECT_EQ(ER_OK, getAll0.Call("org.iotivity.rt0"));
    EXPECT_EQ(ER_OK, getAll0.Wait(1000));
    GetAllPropertiesCall getAll1(res1);
    EXPECT_EQ(ER_OK, getAll1.Call("org.iotivity.rt1"));
    EXPECT_EQ(ER_OK, getAll1.Wait(1000));
    GetAllPropertiesCall getAll2(res2);
    EXPECT_EQ(ER_OK, getAll2.Call("org.iotivity.rt2"));
    EXPECT_EQ(ER_OK, getAll2.Wait(1000));
    GetAllPropertiesCall getAll3(res3);
    EXPECT_EQ(ER_OK, getAll3.Call("org.iotivity.rt3"));
    EXPECT_EQ(ER_OK, getAll3.Wait(1000));

    /* PropertiesChanged */
    PropertiesChangedListener listener0;
    EXPECT_EQ(ER_OK, res0->RegisterPropertiesChangedListener("org.iotivity.rt0", NULL, 0, listener0,
            NULL));
    PropertiesChangedListener listener1;
    EXPECT_EQ(ER_OK, res1->RegisterPropertiesChangedListener("org.iotivity.rt1", NULL, 0, listener1,
            NULL));
    PropertiesChangedListener listener2;
    EXPECT_EQ(ER_OK, res2->RegisterPropertiesChangedListener("org.iotivity.rt2", NULL, 0, listener2,
            NULL));
    PropertiesChangedListener listener3;
    EXPECT_EQ(ER_OK, res3->RegisterPropertiesChangedListener("org.iotivity.rt3", NULL, 0, listener3,
            NULL));
    Wait(1000); /* Must wait for observe request to be processed */
    listener0.m_calls = 0;
    listener1.m_calls = 0;
    listener2.m_calls = 0;
    listener3.m_calls = 0;
    resource.Notify("/resource/0");
    resource.Notify("/resource/1");
    resource.Notify("/resource/2");
    resource.Notify("/resource/3");
    Wait(1000);
    EXPECT_EQ(1u, listener0.m_calls);
    EXPECT_EQ(1u, listener1.m_calls);
    EXPECT_EQ(1u, listener2.m_calls);
    EXPECT_EQ(1u, listener3.m_calls);

    /* Verify */
    entry = entries;
    (entry++)->Set("{sv}", "byte", new ajn::MsgArg("y", 1));
    (entry++)->Set("{sv}", "uint16", new ajn::MsgArg("q", 1));
    (entry++)->Set("{sv}", "int16", new ajn::MsgArg("n", 1));
    (entry++)->Set("{sv}", "uint32", new ajn::MsgArg("u", 1));
    (entry++)->Set("{sv}", "int32", new ajn::MsgArg("i", 1));
    (entry++)->Set("{sv}", "uint64", new ajn::MsgArg("t", 1));
    (entry++)->Set("{sv}", "int64", new ajn::MsgArg("x", 1));
    (entry++)->Set("{sv}", "double", new ajn::MsgArg("d", 1.0));
    (entry++)->Set("{sv}", "uint64large", new ajn::MsgArg("t", 1));
    (entry++)->Set("{sv}", "int64large", new ajn::MsgArg("x", (int64_t) 1));
    (entry++)->Set("{sv}", "string", new ajn::MsgArg("s", "one"));
    uint8_t ay0[] = { 0, 1 };
    (entry++)->Set("{sv}", "stringbase64", new ajn::MsgArg("ay", 2, ay0));
    (entry++)->Set("{sv}", "stringpattern", new ajn::MsgArg("ay", 2, ay0));
    (entry++)->Set("{sv}", "variant", new ajn::MsgArg("v", new ajn::MsgArg("i", 1)));
    value.Set("v", new ajn::MsgArg("a{sv}", entry - entries, entries));
    EXPECT_TRUE(value == get0.Value());
    values.Set("a{sv}", 1, new ajn::MsgArg("{sv}", "object", new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_TRUE(values == getAll0.Values());
    EXPECT_TRUE(values == listener0.Changed());

    entry = entries;
    (entry++)->Set("{sv}", "struct0", new ajn::MsgArg("(is)", 1, "one"));
    value.Set("v", new ajn::MsgArg("a{sv}", entry - entries, entries));
    EXPECT_TRUE(value == get1.Value());
    values.Set("a{sv}", 1, new ajn::MsgArg("{sv}", "object", new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_TRUE(values == getAll1.Values());
    EXPECT_TRUE(values == listener1.Changed());

    entry = entries;
    (entry++)->Set("{sv}", "struct1", new ajn::MsgArg("a{sv}", 2, elems));
    value.Set("v", new ajn::MsgArg("a{sv}", entry - entries, entries));
    EXPECT_TRUE(value == get2.Value());
    values.Set("a{sv}", 1, new ajn::MsgArg("{sv}", "object", new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_TRUE(values == getAll2.Values());
    EXPECT_TRUE(values == listener2.Changed());

    entry = entries;
    (entry++)->Set("{sv}", "dict", new ajn::MsgArg("a{sv}", 2, elems));
    value.Set("v", new ajn::MsgArg("a{sv}", entry - entries, entries));
    EXPECT_TRUE(value == get3.Value());
    values.Set("a{sv}", 1, new ajn::MsgArg("{sv}", "object", new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_TRUE(values == getAll3.Values());
    EXPECT_TRUE(values == listener3.Changed());

    /* Set */
    SetPropertyCall set0(res0);
    entry = entries;
    (entry++)->Set("{sv}", "byte", new ajn::MsgArg("y", 2));
    (entry++)->Set("{sv}", "uint16", new ajn::MsgArg("q", 2));
    (entry++)->Set("{sv}", "int16", new ajn::MsgArg("n", 2));
    (entry++)->Set("{sv}", "uint32", new ajn::MsgArg("u", 2));
    (entry++)->Set("{sv}", "int32", new ajn::MsgArg("i", 2));
    (entry++)->Set("{sv}", "uint64", new ajn::MsgArg("t", 2));
    (entry++)->Set("{sv}", "int64", new ajn::MsgArg("x", 2));
    (entry++)->Set("{sv}", "double", new ajn::MsgArg("d", 2.0));
    (entry++)->Set("{sv}", "uint64large", new ajn::MsgArg("t", 2));
    (entry++)->Set("{sv}", "int64large", new ajn::MsgArg("x", (int64_t) 2));
    (entry++)->Set("{sv}", "string", new ajn::MsgArg("s", "two"));
    uint8_t ay2[] = { 2, 3 };
    (entry++)->Set("{sv}", "stringbase64", new ajn::MsgArg("ay", 2, ay2));
    (entry++)->Set("{sv}", "stringpattern", new ajn::MsgArg("ay", 2, ay2));
    (entry++)->Set("{sv}", "variant", new ajn::MsgArg("v", new ajn::MsgArg("i", 2)));
    EXPECT_EQ(ER_OK, set0.Call("org.iotivity.rt0", "object",
            *new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_EQ(ER_OK, set0.Wait(1000));
    /* Verify */
    EXPECT_EQ(2, resource.m_properties0.m_byte);
    EXPECT_EQ(2, resource.m_properties0.m_uint16);
    EXPECT_EQ(2, resource.m_properties0.m_int16);
    EXPECT_EQ(2, resource.m_properties0.m_uint32);
    EXPECT_EQ(2, resource.m_properties0.m_int32);
    EXPECT_EQ(2, resource.m_properties0.m_uint64);
    EXPECT_EQ(2, resource.m_properties0.m_int64);
    EXPECT_EQ(2.0, resource.m_properties0.m_double);
    EXPECT_STREQ("2", resource.m_properties0.m_uint64large.c_str());
    EXPECT_STREQ("2", resource.m_properties0.m_int64large.c_str());
    EXPECT_STREQ("two", resource.m_properties0.m_string.c_str());
    EXPECT_EQ(0, memcmp(ay2, resource.m_properties0.m_stringbase64.bytes, 2));
    EXPECT_EQ(0, memcmp(ay2, resource.m_properties0.m_stringpattern.bytes, 2));
    EXPECT_EQ(2, resource.m_properties0.m_variant);
    SetPropertyCall set1(res1);
    entry = entries;
    (entry++)->Set("{sv}", "struct0", new ajn::MsgArg("(is)", 2, "two"));
    EXPECT_EQ(ER_OK, set1.Call("org.iotivity.rt1", "object",
            *new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_EQ(ER_OK, set1.Wait(1000));
    int64_t i;
    EXPECT_TRUE(OCRepPayloadGetPropInt(resource.m_properties1.m_struct0, "0", &i));
    EXPECT_EQ(2, i);
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(resource.m_properties1.m_struct0, "1", &s));
    EXPECT_STREQ("two", s);
    SetPropertyCall set2(res2);
    entry = entries;
    elems[0].Set("{sv}", "int", new ajn::MsgArg("i", 2));
    elems[1].Set("{sv}", "string", new ajn::MsgArg("s", "two"));
    (entry++)->Set("{sv}", "struct1", new ajn::MsgArg("a{sv}", 2, elems));
    EXPECT_EQ(ER_OK, set2.Call("org.iotivity.rt2", "object",
            *new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_EQ(ER_OK, set2.Wait(1000));
    EXPECT_TRUE(OCRepPayloadGetPropInt(resource.m_properties2.m_struct1, "int", &i));
    EXPECT_EQ(2, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(resource.m_properties2.m_struct1, "string", &s));
    EXPECT_STREQ("two", s);
    SetPropertyCall set3(res3);
    entry = entries;
    (entry++)->Set("{sv}", "dict", new ajn::MsgArg("a{sv}", 2, elems));
    EXPECT_EQ(ER_OK, set3.Call("org.iotivity.rt3", "object",
            *new ajn::MsgArg("a{sv}", entry - entries, entries)));
    EXPECT_EQ(ER_OK, set3.Wait(1000));
    EXPECT_TRUE(OCRepPayloadGetPropInt(resource.m_properties3.m_dict, "int", &i));
    EXPECT_EQ(2, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(resource.m_properties3.m_dict, "string", &s));
    EXPECT_STREQ("two", s);

    delete res0;
    delete res1;
    delete res2;
    delete res3;
    TearDownResource();
}
