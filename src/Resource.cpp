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
