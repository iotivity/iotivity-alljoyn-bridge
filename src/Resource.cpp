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

#include "Resource.h"

#include "Log.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocstack.h"
#include <assert.h>

#define INTERFACE_DEFAULT_QUERY "if=" OC_RSRVD_INTERFACE_DEFAULT

static std::vector<OCDevAddr> GetDevAddrs(OCDevAddr origin, const char *di,
        OCResourcePayload *resource)
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
        memset(addr.addr, 0, MAX_ADDR_STR_SIZE);
        for (size_t i = 0, j = 0; (i < MAX_ADDR_STR_SIZE) && ep->addr[i]; ++i, ++j)
        {
            addr.addr[j] = ep->addr[i];
            if (addr.flags & OC_IP_USE_V6 && !strncmp(&ep->addr[i], "%25", 3))
            {
                i += 2;
            }
        }
        addr.ifindex = 0;
        addr.routeData[0] = '\0';
        strncpy(addr.remoteId, di, MAX_IDENTITY_SIZE);
        addrs.push_back(addr);
    }
    return addrs;
}

Resource::Resource(OCDevAddr origin, const char *di, OCResourcePayload *resource)
    : m_uri(resource->uri), m_isObservable(resource->bitmap & OC_OBSERVABLE)
{
    for (OCStringLL *ifc = resource->interfaces; ifc; ifc = ifc->next)
    {
        m_ifs.push_back(ifc->value);
    }
    for (OCStringLL *type = resource->types; type; type = type->next)
    {
        m_rts.push_back(type->value);
    }
    m_addrs = GetDevAddrs(origin, di, resource);
}

bool Resource::IsSecure()
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

Device::Device(OCDevAddr origin, OCDiscoveryPayload *payload)
    : m_di(payload->sid)
{
    for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
         resource = resource->next)
    {
        m_resources.push_back(Resource(origin, payload->sid, resource));
    }
}

Resource *Device::GetResourceUri(const char *uri)
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

Resource *Device::GetResourceType(const char *rt)
{
    for (auto &resource : m_resources)
    {
        if (HasResourceType(resource.m_rts, rt))
        {
            return &resource;
        }
    }
    return NULL;
}

bool Device::IsVirtual()
{
    Resource *resource = GetResourceUri(OC_RSRVD_DEVICE_URI);
    if (resource)
    {
        return HasResourceType(resource->m_rts, "oic.d.virtual");
    }
    return false;
}

bool Device::SetCollectionLinks(std::string collectionUri, OCRepPayload *payload)
{
    bool success = false;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
    size_t dimTotal;
    OCRepPayload **links = NULL;
    std::vector<Resource>::iterator collection = FindResourceFromUri(m_resources, collectionUri);
    assert(collection != m_resources.end());
    if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_LINKS, &links, dim))
    {
        dimTotal = calcDimTotal(dim);
    }
    else
    {
        for (OCRepPayload *p = payload; p; p = p->next)
        {
            ++dim[0];
        }
        links = (OCRepPayload **) OICCalloc(dim[0], sizeof(OCRepPayload*));
        if (!links)
        {
            goto exit;
        }
        dimTotal = calcDimTotal(dim);
        OCRepPayload *p = payload;
        for (size_t i = 0; i < dimTotal; ++i)
        {
            links[i] = OCRepPayloadClone(p);
            p = p->next;
        }
    }
    for (size_t i = 0; i < dimTotal; ++i)
    {
        char *href = NULL;
        if (!OCRepPayloadGetPropString(links[i], OC_RSRVD_HREF, &href))
        {
            href = OICStrdup(links[i]->uri);
        }
        if (!href)
        {
            goto exit;
        }
        auto rit = FindResourceFromUri(m_resources, href);
        if (rit != m_resources.end())
        {
            collection->m_resources.push_back(*rit);
            m_resources.erase(rit);
            /* Find collection again since resources has been modified */
            collection = FindResourceFromUri(m_resources, collectionUri);
            assert(collection != m_resources.end());
        }
        OICFree(href);
    }
    success = true;

exit:
    if (links)
    {
        dimTotal = calcDimTotal(dim);
        for(size_t i = 0; i < dimTotal; ++i)
        {
            OCRepPayloadDestroy(links[i]);
        }
    }
    OICFree(links);
    return success;
}

std::vector<Resource>::iterator FindResourceFromUri(std::vector<Resource> &resources,
        std::string uri)
{
    return std::find_if(resources.begin(), resources.end(),
            [uri](Resource &r) -> bool {return r.m_uri == uri;});
}

std::vector<Resource>::iterator FindResourceFromType(std::vector<Resource> &resources,
        std::string rt)
{
    return std::find_if(resources.begin(), resources.end(),
            [rt](Resource &r) -> bool {return HasResourceType(r.m_rts, rt);});
}

OCStackResult CreateResource(OCResourceHandle *handle, const char *uri, const char *typeName,
        const char *interfaceName, OCEntityHandler entityHandler, void *callbackParam,
        uint8_t properties)
{
    OCStackResult result;
    *handle = OCGetResourceHandleAtUri(uri);
    if (*handle)
    {
        result = OC_STACK_OK;
    }
    else
    {
        result = OCCreateResource(handle, typeName, interfaceName, uri, entityHandler,
                callbackParam, properties);
    }
    return result;
}

static const char *MethodText(OCMethod method)
{
    static const char *text[] = {
        "NoMethod", "Get", "Put", "Post", "Delete", "Observe", "ObserveAll", NULL, "Presence",
        "Discover"
    };
    int i = 0;
    for (int m = method; m; m /= 2)
    {
        ++i;
    }
    return text[i];
}

struct DoContext
{
    OCDoHandle m_handle;
    OCMethod m_method;
    std::string m_uri;
    std::vector<OCDevAddr> m_destinations;
    OCPayload *m_payload;
    OCCallbackData m_cbData;
    OCHeaderOption *m_options;
    uint8_t m_numOptions;

    std::vector<OCDevAddr>::iterator m_destination;
    ~DoContext()
    {
        OCPayloadDestroy(m_payload);
        if (m_options)
        {
            OICFree(m_options);
        }
    }
};

static void DoContextDeleter(void *ctx)
{
    DoContext *context = (DoContext *) ctx;
    if (context->m_cbData.cd)
    {
        context->m_cbData.cd(context->m_cbData.context);
        delete context;
    }
}

static OCStackResult DoResource(DoContext *context);

static OCStackApplicationResult DoResourceCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    DoContext *context = (DoContext *) ctx;
    LOG(LOG_INFO, "%sCB(ctx=%p,handle=%p,response=%p) result=%d",
            MethodText(context->m_method), ctx, handle, response, response ? response->result : -1);

    /* Retry with other endpoints when they are available */
    if (response && (context->m_destination != context->m_destinations.end()))
    {
        if ((OC_STACK_RESOURCE_CHANGED < response->result) &&
                /* Don't expect a retry to succeed for these: */
                (OC_STACK_INVALID_QUERY != response->result))
        {
            OCStackResult result = DoResource(context);
            if (result == OC_STACK_OK)
            {
                return OC_STACK_DELETE_TRANSACTION;
            }
        }
    }
    OCStackApplicationResult result = context->m_cbData.cb(context->m_cbData.context, context,
            response);
    if ((result == OC_STACK_DELETE_TRANSACTION) && !context->m_cbData.cd)
    {
        delete context;
    }
    return result;
}

static OCStackResult DoResource(DoContext *context)
{
    const OCDevAddr *destination = NULL;
    if (context->m_destination != context->m_destinations.end())
    {
        destination = &(*context->m_destination);
        ++context->m_destination;
    }
    OCCallbackData cbData;
    cbData.cb = DoResourceCB;
    cbData.context = context;
    cbData.cd = context->m_cbData.cd ? DoContextDeleter : NULL;
    OCStackResult result = OCDoRequest(&context->m_handle, context->m_method,
            context->m_uri.c_str(), destination, context->m_payload, CT_DEFAULT, OC_HIGH_QOS,
            &cbData, context->m_options, context->m_numOptions);
    int severity = (result == OC_STACK_OK) ? LOG_INFO : LOG_ERR;
    LOG(severity, "%s(uri=%s,destination={adapter=%d,flags=0x%x,addr=%s,port=%d}) handle=%p - %d",
            MethodText(context->m_method), context->m_uri.c_str(),
            destination ? destination->adapter : 0, destination ? destination->flags : 0,
            destination ? destination->addr : NULL, destination ? destination->port: 0,
            context->m_handle, result);
    return result;
}

OCStackResult DoResource(DoHandle *handle, OCMethod method, const char *uri,
        const std::vector<OCDevAddr> &destinations, OCPayload *payload, OCCallbackData *cbData,
        OCHeaderOption *options, uint8_t numOptions)
{
    DoContext *context = new DoContext();
    context->m_method = method;
    context->m_uri = uri;
    context->m_destinations = destinations;
    context->m_payload = payload;
    memcpy(&context->m_cbData, cbData, sizeof(OCCallbackData));
    context->m_numOptions = numOptions;
    if (context->m_numOptions)
    {
        context->m_options = (OCHeaderOption *) OICCalloc(context->m_numOptions,
                sizeof(OCHeaderOption));
        memcpy(context->m_options, options, context->m_numOptions * sizeof(OCHeaderOption));
    }

    context->m_destination = context->m_destinations.begin();
    *handle = context;

    return DoResource(context);
}

OCStackResult DoResource(DoHandle *handle, OCMethod method, const char *uri,
        const OCDevAddr* destination, OCPayload *payload, OCCallbackData *cbData,
        OCHeaderOption *options, uint8_t numOptions)
{
    std::vector<OCDevAddr> destinations;
    if (destination)
    {
        destinations.push_back(*destination);
    }
    return DoResource(handle, method, uri, destinations, payload, cbData, options, numOptions);
}

OCStackResult Cancel(DoHandle handle, OCQualityOfService qos, OCHeaderOption *options,
        uint8_t numOptions)
{
    DoContext *context = (DoContext *) handle;
    OCDoHandle h = context->m_handle;
    if (!context->m_cbData.cd)
    {
        delete context;
    }
    return OCCancel(h, qos, options, numOptions);
}

std::map<std::string, std::string> ParseQuery(OCResourceHandle resource, const char *query)
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
    /* Default interface for multi-value rt resources is the baseline interface. */
    if (queryMap.find(OC_RSRVD_INTERFACE) == queryMap.end())
    {
        uint8_t n = 0;
        if ((OCGetNumberOfResourceTypes(resource, &n) == OC_STACK_OK) && n)
        {
            queryMap[OC_RSRVD_INTERFACE] = OC_RSRVD_INTERFACE_DEFAULT;
        }
    }
    return queryMap;
}

// TODO Use public core API when it becomes available
static OCEndpointPayload *ParseEndpoint(OCRepPayload *payload)
{
    OCEndpointPayload *ep = NULL;
    char *s = NULL;
    int64_t n;

    ep = (OCEndpointPayload *) OICCalloc(1, sizeof(OCEndpointPayload));
    if (!ep)
    {
        goto exit;
    }
    if (OCRepPayloadGetPropString(payload, OC_RSRVD_ENDPOINT, &s))
    {
        // 0         1         2         3         4         5         6
        // 01234567890123456789012345678901234567890123456789012345678901234
        // coaps+tcp://[xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:yyy.yyy.yyy.yyy]:xxxxx
        char *tps = (char *) OICMalloc(10);
        char *addr = (char *) OICMalloc(48);
        int port;
        if (tps && addr && (sscanf(s, "%10s://%48s:%d", tps, addr, &port) == 3))
        {
            ep->tps = tps;
            ep->addr = addr;
            ep->family = strchr(addr, '[') ? OC_IP_USE_V6 : OC_IP_USE_V4;
            if (!strncmp(tps, "coaps", 5))
            {
                ep->family = (OCTransportFlags) (ep->family | OC_FLAG_SECURE);
            }
            ep->port = (uint16_t) port;
        }
        else
        {
            OICFree(addr);
            OICFree(tps);
            goto exit;
        }
        OICFree(s);
        s = NULL;
    }
    if (OCRepPayloadGetPropInt(payload, OC_RSRVD_TCP_PORT, &n))
    {
        ep->pri = (uint16_t) n;
    }
    else
    {
        ep->pri = 1;
    }
    return ep;

exit:
    OICFree(s);
    OICFree(ep);
    return NULL;
}

OCResourcePayload *ParseLink(OCRepPayload *payload)
{
    OCResourcePayload *rp = NULL;
    size_t ssDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    char **ss = NULL;
    OCRepPayload *o = NULL;
    size_t osDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **os = NULL;

    rp = (OCResourcePayload *) OICCalloc(1, sizeof(OCResourcePayload));
    if (!rp)
    {
        goto exit;
    }
    if (!OCRepPayloadGetPropString(payload, OC_RSRVD_HREF, &rp->uri))
    {
        rp->uri = OICStrdup(payload->uri);
    }
    OCRepPayloadGetPropString(payload, OC_RSRVD_REL, &rp->rel);
    OCRepPayloadGetPropString(payload, OC_RSRVD_URI, &rp->anchor);
    if (OCRepPayloadGetStringArray(payload, OC_RSRVD_RESOURCE_TYPE, &ss, ssDim))
    {
        size_t dimTotal = calcDimTotal(ssDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OCResourcePayloadAddStringLL(&rp->types, ss[i]);
            OICFree(ss[i]);
        }
        OICFree(ss);
        ss = NULL;
    }
    else
    {
        rp->types = CloneOCStringLL(payload->types);
    }
    if (OCRepPayloadGetStringArray(payload, OC_RSRVD_INTERFACE, &ss, ssDim))
    {
        size_t dimTotal = calcDimTotal(ssDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OCResourcePayloadAddStringLL(&rp->interfaces, ss[i]);
            OICFree(ss[i]);
        }
        OICFree(ss);
        ss = NULL;
    }
    else
    {
        rp->interfaces = CloneOCStringLL(payload->interfaces);
    }
    if (OCRepPayloadGetPropObject(payload, OC_RSRVD_POLICY, &o))
    {
        int64_t n;
        if (OCRepPayloadGetPropInt(o, OC_RSRVD_BITMAP, &n))
        {
            rp->bitmap = (uint8_t) n;
        }
        OCRepPayloadGetPropBool(o, OC_RSRVD_SECURE, &rp->secure);
        if (OCRepPayloadGetPropInt(o, OC_RSRVD_HOSTING_PORT, &n))
        {
            rp->port = (uint16_t) n;
        }
#ifdef TCP_ADAPTER
        if (OCRepPayloadGetPropInt(o, OC_RSRVD_TCP_PORT, &n))
        {
            rp->tcpPort = (uint16_t) n;
        }
#endif
        OCRepPayloadDestroy(o);
        o = NULL;
    }
    if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_ENDPOINTS, &os, osDim))
    {
        size_t dimTotal = calcDimTotal(osDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OCEndpointPayload *ep = ParseEndpoint(os[i]);
            if (!ep)
            {
                goto exit;
            }
            OCResourcePayloadAddNewEndpoint(rp, ep);
            OCRepPayloadDestroy(os[i]);
        }
        OICFree(os);
        os = NULL;
    }
    return rp;

exit:
    if (os)
    {
        size_t dimTotal = calcDimTotal(osDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(os[i]);
        }
        OICFree(os);
    }
    OCRepPayloadDestroy(o);
    if (ss)
    {
        size_t dimTotal = calcDimTotal(ssDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(ss[i]);
        }
        OICFree(ss);
    }
    OICFree(rp);
    return NULL;
}

bool IsValidRequest(OCEntityHandlerRequest *request)
{
    OCStackResult result;
    uint8_t n;

    auto queryMap = ParseQuery(request->resource, request->query);
    auto itf = queryMap.find("if");
    bool hasItf = false;
    result = OCGetNumberOfResourceInterfaces(request->resource, &n);
    if (result != OC_STACK_OK)
    {
        return false;
    }
    for (uint8_t i = 0; i < n; ++i)
    {
        const char *name = OCGetResourceInterfaceName(request->resource, i);
        if (!name)
        {
            return false;
        }
        if (itf->second == name)
        {
            hasItf = true;
            break;
        }
    }
    if (!hasItf)
    {
        return false;
    }
    bool hasRt = false;
    auto rt = queryMap.find("rt");
    if (rt != queryMap.end())
    {
        result = OCGetNumberOfResourceTypes(request->resource, &n);
        if (result != OC_STACK_OK)
        {
            return false;
        }
        for (uint8_t i = 0; i < n; ++i)
        {
            const char *name = OCGetResourceTypeName(request->resource, i);
            if (!name)
            {
                return false;
            }
            if (rt->second == name)
            {
                hasRt = true;
                break;
            }
        }
        if (!hasRt)
        {
            return false;
        }
    }
    return true;
}

bool SetResourceTypes(OCRepPayload *payload, OCResourceHandle resource)
{
    uint8_t n;
    char **array = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0, 0, 0 };
    OCStackResult result;

    result = OCGetNumberOfResourceTypes(resource, &n);
    if (result != OC_STACK_OK)
    {
        goto error;
    }
    array = (char **)OICCalloc(n, sizeof(char *));
    if (!array)
    {
        goto error;
    }
    for (uint8_t i = 0; i < n; ++i)
    {
        array[i] = OICStrdup(OCGetResourceTypeName(resource, i));
        if (!array[i])
        {
            goto error;
        }
    }
    dim[0] = n;
    if (!OCRepPayloadSetStringArrayAsOwner(payload, OC_RSRVD_RESOURCE_TYPE, array, dim))
    {
        goto error;
    }
    array = NULL;
    return true;

error:
    if (array)
    {
        for (uint8_t i = 0; i < n; ++i)
        {
            OICFree(array[i]);
        }
        OICFree(array);
    }
    return false;
}

bool SetInterfaces(OCRepPayload *payload, OCResourceHandle resource)
{
    uint8_t n;
    char **array = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0, 0, 0 };
    OCStackResult result;

    result = OCGetNumberOfResourceInterfaces(resource, &n);
    if (result != OC_STACK_OK)
    {
        goto error;
    }
    array = (char **)OICCalloc(n, sizeof(char *));
    if (!array)
    {
        goto error;
    }
    for (uint8_t i = 0; i < n; ++i)
    {
        array[i] = OICStrdup(OCGetResourceInterfaceName(resource, i));
        if (!array[i])
        {
            goto error;
        }
    }
    dim[0] = n;
    if (!OCRepPayloadSetStringArrayAsOwner(payload, OC_RSRVD_INTERFACE, array, dim))
    {
        goto error;
    }
    array = NULL;
    return true;

error:
    if (array)
    {
        for (uint8_t i = 0; i < n; ++i)
        {
            OICFree(array[i]);
        }
        OICFree(array);
    }
    return false;
}

OCRepPayload *CreatePayload(OCResourceHandle resource, const char *query)
{
    OCRepPayload *payload = NULL;

    payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    if (!OCRepPayloadSetUri(payload, OCGetResourceUri(resource)))
    {
        goto error;
    }
    if (query && strstr(query, INTERFACE_DEFAULT_QUERY))
    {
        if (!SetResourceTypes(payload, resource) ||
                !SetInterfaces(payload, resource))
        {
            goto error;
        }
    }
    return payload;

error:
    OCRepPayloadDestroy(payload);
    return NULL;
}
