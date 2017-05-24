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

#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocstack.h"

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
        strncpy(addr.addr, ep->addr, MAX_ADDR_STR_SIZE);
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

OCStackResult DoResource(OCDoHandle *handle, OCMethod method, const char *uri,
        const OCDevAddr *destination, OCPayload *payload, OCCallbackData *cbData,
        OCHeaderOption *options, uint8_t numOptions)
{
    return OCDoResource(handle, method, uri, destination, payload, CT_DEFAULT, OC_HIGH_QOS, cbData,
            options, numOptions);
}

OCStackResult DoResource(OCDoHandle *handle, OCMethod method, const char *uri,
        const std::vector<OCDevAddr> &destinations, OCPayload *payload, OCCallbackData *cbData,
        OCHeaderOption *options, uint8_t numOptions)
{
    /* Prefer secure destination when present otherwise just use the first destination */
    const OCDevAddr *destination = destinations.empty() ? NULL : &destinations[0];
    for (std::vector<OCDevAddr>::const_iterator it = destinations.begin(); it != destinations.end();
         ++it)
    {
        if (it->flags & OC_FLAG_SECURE)
        {
            destination = &(*it);
            break;
        }
    }
    return OCDoResource(handle, method, uri, destination, payload, CT_DEFAULT, OC_HIGH_QOS, cbData,
            options, numOptions);
}

std::map<std::string, std::string> ParseQuery(const char *query)
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

bool IsValidRequest(OCEntityHandlerRequest *request)
{
    OCStackResult result;
    uint8_t n;

    auto queryMap = ParseQuery(request->query);
    bool hasItf = false;
    auto itf = queryMap.find("if");
    if (itf != queryMap.end())
    {
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

bool SetLinks(OCRepPayload *payload, OCResourceHandle *resources, uint8_t numResources)
{
    OCRepPayload **array = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0, 0, 0 };

    array = (OCRepPayload **)OICCalloc(numResources, sizeof(OCRepPayload *));
    if (!array)
    {
        goto error;
    }
    for (uint8_t i = 0; i < numResources; ++i)
    {
        array[i] = OCRepPayloadCreate();
        if (!array[i])
        {
            goto error;
        }
        if (!OCRepPayloadSetPropString(array[i], OC_RSRVD_HREF, OCGetResourceUri(resources[i])) ||
                !SetResourceTypes(array[i], resources[i]) ||
                !SetInterfaces(array[i], resources[i]))
        {
            goto error;
        }
        // TODO eps?
    }
    dim[0] = numResources;
    if (!OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_LINKS, array, dim))
    {
        goto error;
    }
    array = NULL;
    return true;

error:
    if (array)
    {
        for (uint8_t i = 0; i < numResources; ++i)
        {
            OCRepPayloadDestroy(array[i]);
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
