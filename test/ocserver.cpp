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

#include <boost/algorithm/string/join.hpp>
#include <boost/tokenizer.hpp>
#include <iostream>
#include <map>
#include <poll.h>

#include "cJSON.h"
#include "cbor.h"
#include "cborjson.h"
#include "ocpayload.h"
#include "ocpayloadcbor.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#if __WITH_DTLS__
#include "pinoxmcommon.h"
#endif

#define OC_RSRVD_FRIENDLY_NAME "n"

static void LogPayload(OCPayload *payload)
{
    uint8_t *buffer = NULL;
    size_t size;
    OCStackResult result = OCConvertPayload(payload, OC_FORMAT_CBOR, &buffer, &size);
    if (result != OC_STACK_OK)
    {
        return;
    }
    CborParser parser;
    CborValue it;
    CborError err = cbor_parser_init(buffer, size, CborConvertDefaultFlags, &parser, &it);
    char value[8192];
    FILE *fp = fmemopen(value, 8192, "w");
    while (err == CborNoError)
    {
        err = cbor_value_to_json_advance(fp, &it, CborConvertDefaultFlags);
    }
    fclose(fp);
    cJSON *json = cJSON_Parse(value);
    char *valueStr = cJSON_Print(json);
    std::cout << valueStr << std::endl;
    free(valueStr);
    cJSON_Delete(json);
    OICFree(buffer);
}

struct Configuration
{
    std::string n;
    std::string loc;
    std::string locn;
    std::string c;
    std::string r;
    std::string dl;
    std::map<std::string, std::string> ln;
};

static OCEntityHandlerResult configurationHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    (void) flag;
    Configuration *resource = (Configuration *) ctx;
    OCEntityHandlerResult ehResult;
    OCStackResult result;

    switch (request->method)
    {
        case OC_REST_DISCOVER:
        case OC_REST_GET:
        case OC_REST_OBSERVE:
            std::cout << "GET" << std::endl;
            break;
        case OC_REST_POST:
            std::cout << "POST" << std::endl;
            break;
        default:
            std::cout << request->method << std::endl;
            break;
    }
    std::cout << "Uri-Path: " << OCGetResourceUri(request->resource) << std::endl;
    std::cout << "Uri-Query: " << (request->query ? request->query : "") << std::endl;

    std::string itf;
    OCRepPayload *payload = NULL;
    uint8_t n;
    char **array = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0, 0, 0 };

    ehResult = OC_EH_BAD_REQ;
    std::string queryStr = request->query;
    boost::tokenizer<boost::char_separator<char>>
            queries(queryStr, boost::char_separator<char>(OC_QUERY_SEPARATOR));
    for (auto query = queries.begin(); query != queries.end(); ++query)
    {
        boost::tokenizer<boost::char_separator<char>>
                params(*query, boost::char_separator<char>("="));
        auto param = params.begin();
        std::string key, value;
        key = *param++;
        if (param != params.end())
        {
            value = *param++;
        }
        if (key == OC_RSRVD_INTERFACE && itf.empty())
        {
            itf = value;
        }
        else
        {
            goto error;
        }
    }
    if (!itf.empty())
    {
        result = OCGetNumberOfResourceInterfaces(request->resource, &n);
        if (result != OC_STACK_OK)
        {
            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
            goto error;
        }
        for (uint8_t i = 0; i < n; ++i)
        {
            const char *itfName = OCGetResourceInterfaceName(request->resource, i);
            if (!itfName)
            {
                ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                goto error;
            }
            if (itf == itfName)
            {
                ehResult = OC_EH_OK;
                break;
            }
        }
        if (ehResult != OC_EH_OK)
        {
            goto error;
        }
    }

    ehResult = OC_EH_INTERNAL_SERVER_ERROR;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;

                payload = OCRepPayloadCreate();
                if (!payload)
                {
                    goto error;
                }
                if (itf == OC_RSRVD_INTERFACE_DEFAULT)
                {
                    // oic.r.core (all ReadOnly)
                    // rt
                    result = OCGetNumberOfResourceTypes(request->resource, &n);
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
                        array[i] = OICStrdup(OCGetResourceTypeName(request->resource, i));
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
                    // if
                    result = OCGetNumberOfResourceInterfaces(request->resource, &n);
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
                        array[i] = OICStrdup(OCGetResourceInterfaceName(request->resource, i));
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
                    // n
                    if (!OCRepPayloadSetPropString(payload, OC_RSRVD_FRIENDLY_NAME, resource->n.c_str()))
                    {
                        goto error;
                    }
                    // TODO id
                }
                // oic.wk.con
                if (!OCRepPayloadSetPropString(payload, "loc", resource->loc.c_str()))
                {
                    goto error;
                }
                if (!OCRepPayloadSetPropString(payload, "locn", resource->locn.c_str()))
                {
                    goto error;
                }
                if (!OCRepPayloadSetPropString(payload, "c", resource->c.c_str()))
                {
                    goto error;
                }
                if (!OCRepPayloadSetPropString(payload, "r", resource->r.c_str()))
                {
                    goto error;
                }
                if (!OCRepPayloadSetPropString(payload, "dl", resource->dl.c_str()))
                {
                    goto error;
                }
                size_t dim[MAX_REP_ARRAY_DEPTH] = { resource->ln.size(), 0, 0 };
                OCRepPayload **objArray = (OCRepPayload **) OICCalloc(calcDimTotal(dim), sizeof(OCRepPayload *));
                if (!objArray)
                {
                    goto error;
                }
                else
                {
                    size_t i = 0;
                    for (std::map<std::string, std::string>::iterator it = resource->ln.begin();
                         it != resource->ln.end(); ++it)
                    {
                        OCRepPayload *value = OCRepPayloadCreate();
                        if (!value)
                        {
                            goto error;
                        }
                        if (!OCRepPayloadSetPropString(value, "language", it->first.c_str()) ||
                            !OCRepPayloadSetPropString(value, "value", it->second.c_str()))
                        {
                            goto error;
                        }
                        objArray[i++] = value;
                    }
                }
                if (!OCRepPayloadSetPropObjectArrayAsOwner(payload, "ln", objArray, dim))
                {
                    goto error;
                }
                // vendor-defined property
                if (!OCRepPayloadSetPropString(payload, "x.example.vendor", "Vendor-defined property"))
                {
                    goto error;
                }

                ehResult = OC_EH_OK;
                response.ehResult = ehResult;
                response.payload = reinterpret_cast<OCPayload *>(payload);
                result = OCDoResponse(&response);
                if (result != OC_STACK_OK)
                {
                    std::cout << "OCDoResponse - " << result << std::endl;
                    OCRepPayloadDestroy(payload);
                }
                payload = NULL;
                break;
            }
        case OC_REST_POST:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;

                payload = OCRepPayloadCreate();
                if (!payload)
                {
                    goto error;
                }
                if (request->payload)
                {
                    LogPayload(request->payload);
                    OCRepPayload *reqPayload = (OCRepPayload *) request->payload;
                    ehResult = OC_EH_OK;
                    char *s;
                    if (OCRepPayloadGetPropString(reqPayload, OC_RSRVD_FRIENDLY_NAME, &s))
                    {
                        resource->n = s;
                        if (!OCRepPayloadSetPropString(payload, OC_RSRVD_FRIENDLY_NAME, s))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    if (OCRepPayloadGetPropString(reqPayload, "loc", &s))
                    {
                        resource->loc = s;
                        if (!OCRepPayloadSetPropString(payload, "loc", s))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    if (OCRepPayloadGetPropString(reqPayload, "locn", &s))
                    {
                        resource->locn = s;
                        if (!OCRepPayloadSetPropString(payload, "locn", s))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    if (OCRepPayloadGetPropString(reqPayload, "c", &s))
                    {
                        resource->c = s;
                        if (!OCRepPayloadSetPropString(payload, "c", s))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    if (OCRepPayloadGetPropString(reqPayload, "r", &s))
                    {
                        resource->r = s;
                        if (!OCRepPayloadSetPropString(payload, "r", s))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    if (OCRepPayloadGetPropString(reqPayload, "dl", &s))
                    {
                        resource->dl = s;
                        if (!OCRepPayloadSetPropString(payload, "dl", s))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
                    OCRepPayload **objArray = NULL;
                    if (OCRepPayloadGetPropObjectArray(reqPayload, "ln", &objArray, dim))
                    {
                        size_t dimTotal = calcDimTotal(dim);
                        for (size_t i = 0; i < dimTotal; ++i)
                        {
                            char *language;
                            char *value;
                            if (OCRepPayloadGetPropString(objArray[i], "language", &language) &&
                                OCRepPayloadGetPropString(objArray[i], "value", &value))
                            {
                                resource->ln[language] = value;
                            }
                        }
                        /* Copy the value into the response payload */
                        if (!OCRepPayloadSetPropObjectArrayAsOwner(payload, "ln", objArray, dim))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                }
                else
                {
                    ehResult = OC_EH_BAD_REQ;
                }

                response.ehResult = ehResult;
                if (ehResult != OC_EH_OK)
                {
                    OCRepPayloadDestroy(payload);
                    payload = NULL;
                }
                response.payload = reinterpret_cast<OCPayload *>(payload);
                result = OCDoResponse(&response);
                if (result != OC_STACK_OK)
                {
                    std::cout << "OCDoResponse - " << result << std::endl;
                    OCRepPayloadDestroy(payload);
                }
                payload = NULL;
                break;
            }
        default:
            ehResult = OC_EH_METHOD_NOT_ALLOWED;
            goto error;
    }

    return ehResult;

error:
    if (array)
    {
        for (uint8_t i = 0; i < n; ++i)
        {
            OICFree(array[i]);
        }
        OICFree(array);
    }
    OCRepPayloadDestroy(payload);
    return ehResult;
}

struct Maintenance
{
    std::string n;
};

static OCEntityHandlerResult maintenanceHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    (void) flag;
    Maintenance *resource = (Maintenance *) ctx;
    OCEntityHandlerResult ehResult;
    OCStackResult result;

    switch (request->method)
    {
        case OC_REST_DISCOVER:
        case OC_REST_GET:
        case OC_REST_OBSERVE:
            std::cout << "GET" << std::endl;
            break;
        case OC_REST_POST:
            std::cout << "POST" << std::endl;
            break;
        default:
            std::cout << request->method << std::endl;
            break;
    }
    std::cout << "Uri-Path: " << OCGetResourceUri(request->resource) << std::endl;
    std::cout << "Uri-Query: " << (request->query ? request->query : "") << std::endl;

    std::string itf;
    OCRepPayload *payload = NULL;
    uint8_t n;
    char **array = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0, 0, 0 };

    ehResult = OC_EH_BAD_REQ;
    std::string queryStr = request->query;
    boost::tokenizer<boost::char_separator<char>>
            queries(queryStr, boost::char_separator<char>(OC_QUERY_SEPARATOR));
    for (auto query = queries.begin(); query != queries.end(); ++query)
    {
        boost::tokenizer<boost::char_separator<char>>
                params(*query, boost::char_separator<char>("="));
        auto param = params.begin();
        std::string key, value;
        key = *param++;
        if (param != params.end())
        {
            value = *param++;
        }
        if (key == OC_RSRVD_INTERFACE && itf.empty())
        {
            itf = value;
        }
        else
        {
            goto error;
        }
    }
    if (!itf.empty())
    {
        result = OCGetNumberOfResourceInterfaces(request->resource, &n);
        if (result != OC_STACK_OK)
        {
            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
            goto error;
        }
        for (uint8_t i = 0; i < n; ++i)
        {
            const char *itfName = OCGetResourceInterfaceName(request->resource, i);
            if (!itfName)
            {
                ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                goto error;
            }
            if (itf == itfName)
            {
                ehResult = OC_EH_OK;
                break;
            }
        }
        if (ehResult != OC_EH_OK)
        {
            goto error;
        }
    }

    ehResult = OC_EH_INTERNAL_SERVER_ERROR;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;

                payload = OCRepPayloadCreate();
                if (!payload)
                {
                    goto error;
                }
                if (itf == OC_RSRVD_INTERFACE_DEFAULT)
                {
                    // oic.r.core (all ReadOnly)
                    // rt
                    result = OCGetNumberOfResourceTypes(request->resource, &n);
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
                        array[i] = OICStrdup(OCGetResourceTypeName(request->resource, i));
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
                    // if
                    result = OCGetNumberOfResourceInterfaces(request->resource, &n);
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
                        array[i] = OICStrdup(OCGetResourceInterfaceName(request->resource, i));
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
                    // n
                    if (!OCRepPayloadSetPropString(payload, OC_RSRVD_FRIENDLY_NAME, resource->n.c_str()))
                    {
                        goto error;
                    }
                    // TODO id
                }
                // oic.wk.mnt
                if (!OCRepPayloadSetPropBool(payload, "fr", false))
                {
                    goto error;
                }
                if (!OCRepPayloadSetPropBool(payload, "rb", false))
                {
                    goto error;
                }

                ehResult = OC_EH_OK;
                response.ehResult = ehResult;
                response.payload = reinterpret_cast<OCPayload *>(payload);
                result = OCDoResponse(&response);
                if (result != OC_STACK_OK)
                {
                    std::cout << "OCDoResponse - " << result << std::endl;
                    OCRepPayloadDestroy(payload);
                }
                payload = NULL;
                break;
            }
        case OC_REST_POST:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;

                payload = OCRepPayloadCreate();
                if (!payload)
                {
                    goto error;
                }
                if (request->payload)
                {
                    LogPayload(request->payload);
                    OCRepPayload *reqPayload = (OCRepPayload *) request->payload;
                    ehResult = OC_EH_OK;
                    char *s;
                    if (OCRepPayloadGetPropString(reqPayload, OC_RSRVD_FRIENDLY_NAME, &s))
                    {
                        resource->n = s;
                        if (!OCRepPayloadSetPropString(payload, OC_RSRVD_FRIENDLY_NAME, s))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    bool b;
                    if (OCRepPayloadGetPropBool(reqPayload, "fr", &b))
                    {
                        if (!OCRepPayloadSetPropBool(payload, "fr", b))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                    if (OCRepPayloadGetPropBool(reqPayload, "rb", &b))
                    {
                        if (!OCRepPayloadSetPropBool(payload, "rb", b))
                        {
                            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                            goto error;
                        }
                    }
                }
                else
                {
                    ehResult = OC_EH_BAD_REQ;
                }

                response.ehResult = ehResult;
                if (ehResult != OC_EH_OK)
                {
                    OCRepPayloadDestroy(payload);
                    payload = NULL;
                }
                response.payload = reinterpret_cast<OCPayload *>(payload);
                result = OCDoResponse(&response);
                if (result != OC_STACK_OK)
                {
                    std::cout << "OCDoResponse - " << result << std::endl;
                    OCRepPayloadDestroy(payload);
                }
                payload = NULL;
                break;
            }
        default:
            ehResult = OC_EH_METHOD_NOT_ALLOWED;
            goto error;
    }

    return ehResult;

error:
    if (array)
    {
        for (uint8_t i = 0; i < n; ++i)
        {
            OICFree(array[i]);
        }
        OICFree(array);
    }
    OCRepPayloadDestroy(payload);
    return ehResult;
}

struct Resource
{
    const char *n;
    const char *id;
    bool value;
};

static OCEntityHandlerResult entityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    (void) flag;
    Resource *resource = (Resource *)ctx;
    OCEntityHandlerResult ehResult;
    OCStackResult result;

    switch (request->method)
    {
        case OC_REST_DISCOVER:
        case OC_REST_GET:
        case OC_REST_OBSERVE:
            std::cout << "GET" << std::endl;
            break;
        case OC_REST_POST:
            std::cout << "POST" << std::endl;
            break;
        default:
            std::cout << request->method << std::endl;
            break;
    }
    std::cout << "Uri-Path: " << OCGetResourceUri(request->resource) << std::endl;
    std::cout << "Uri-Query: " << (request->query ? request->query : "") << std::endl;

    std::string itf;
    OCRepPayload *payload = NULL;
    uint8_t n;
    char **array = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0, 0, 0 };

    ehResult = OC_EH_BAD_REQ;
    std::string queryStr = request->query;
    boost::tokenizer<boost::char_separator<char>>
            queries(queryStr, boost::char_separator<char>(OC_QUERY_SEPARATOR));
    for (auto query = queries.begin(); query != queries.end(); ++query)
    {
        boost::tokenizer<boost::char_separator<char>>
                params(*query, boost::char_separator<char>("="));
        auto param = params.begin();
        std::string key, value;
        key = *param++;
        if (param != params.end())
        {
            value = *param++;
        }
        if (key == OC_RSRVD_INTERFACE && itf.empty())
        {
            itf = value;
        }
    }
    if (!itf.empty())
    {
        result = OCGetNumberOfResourceInterfaces(request->resource, &n);
        if (result != OC_STACK_OK)
        {
            ehResult = OC_EH_INTERNAL_SERVER_ERROR;
            goto error;
        }
        for (uint8_t i = 0; i < n; ++i)
        {
            const char *itfName = OCGetResourceInterfaceName(request->resource, i);
            if (!itfName)
            {
                ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                goto error;
            }
            if (itf == itfName)
            {
                ehResult = OC_EH_OK;
                break;
            }
        }
        if (ehResult != OC_EH_OK)
        {
            goto error;
        }
    }

    ehResult = OC_EH_INTERNAL_SERVER_ERROR;
    switch (request->method)
    {
        case OC_REST_GET:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;

                payload = OCRepPayloadCreate();
                if (!payload)
                {
                    goto error;
                }
                if (itf == OC_RSRVD_INTERFACE_DEFAULT)
                {
                    // oic.r.core (all ReadOnly)
                    // rt
                    result = OCGetNumberOfResourceTypes(request->resource, &n);
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
                        array[i] = OICStrdup(OCGetResourceTypeName(request->resource, i));
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
                    // if
                    result = OCGetNumberOfResourceInterfaces(request->resource, &n);
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
                        array[i] = OICStrdup(OCGetResourceInterfaceName(request->resource, i));
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
                    // n
                    if (!OCRepPayloadSetPropString(payload, OC_RSRVD_FRIENDLY_NAME, resource->n))
                    {
                        goto error;
                    }
                    // id
                    if (!OCRepPayloadSetPropString(payload, OC_RSRVD_INSTANCE_ID, resource->id))
                    {
                        goto error;
                    }
                }
                // oic.r.baseresource { value, range }
                // oic.r.switch.binary { value }
                if (!OCRepPayloadSetPropBool(payload, "value", resource->value))
                {
                    goto error;
                }

                ehResult = OC_EH_OK;
                response.ehResult = ehResult;
                response.payload = reinterpret_cast<OCPayload *>(payload);
                result = OCDoResponse(&response);
                if (result != OC_STACK_OK)
                {
                    std::cout << "OCDoResponse - " << result << std::endl;
                    OCRepPayloadDestroy(payload);
                }
                payload = NULL;
                break;
            }
        case OC_REST_POST:
            {
                OCEntityHandlerResponse response;
                memset(&response, 0, sizeof(response));
                response.requestHandle = request->requestHandle;
                response.resourceHandle = request->resource;

                payload = OCRepPayloadCreate();
                if (!payload)
                {
                    goto error;
                }
                if (request->payload)
                {
                    ehResult = OC_EH_FORBIDDEN;
                    LogPayload(request->payload);
                    for (OCRepPayloadValue *value = ((OCRepPayload *)request->payload)->values; value;
                         value = value->next)
                    {
                        if (!strcmp(value->name, "value"))
                        {
                            if (value->type != OCREP_PROP_BOOL)
                            {
                                ehResult = OC_EH_BAD_REQ;
                                goto error;
                            }
                            resource->value = value->b;
                            if (!OCRepPayloadSetPropBool(payload, "value", resource->value))
                            {
                                ehResult = OC_EH_INTERNAL_SERVER_ERROR;
                                goto error;
                            }
                            ehResult = OC_EH_OK;
                        }
                    }
                }
                else
                {
                    ehResult = OC_EH_BAD_REQ;
                }

                response.ehResult = ehResult;
                if (ehResult != OC_EH_OK)
                {
                    OCRepPayloadDestroy(payload);
                    payload = NULL;
                }
                response.payload = reinterpret_cast<OCPayload *>(payload);
                result = OCDoResponse(&response);
                if (result != OC_STACK_OK)
                {
                    std::cout << "OCDoResponse - " << result << std::endl;
                    OCRepPayloadDestroy(payload);
                }
                payload = NULL;
                break;
            }
        default:
            ehResult = OC_EH_METHOD_NOT_ALLOWED;
            goto error;
    }

    return ehResult;

error:
    if (array)
    {
        for (uint8_t i = 0; i < n; ++i)
        {
            OICFree(array[i]);
        }
        OICFree(array);
    }
    OCRepPayloadDestroy(payload);
    return ehResult;
}

static FILE *PSOpenCB(const char *suffix, const char *mode)
{
    std::string path = std::string("ocserver_") + suffix;
    return fopen(path.c_str(), mode);
}

#if __WITH_DTLS__
static void DisplayPinCB(char *pin, size_t pinSize, void *context)
{
    OC_UNUSED(pinSize);
    OC_UNUSED(context);
    std::cout << "DisplayPinCB(pin=" << pin << ",...)" << std::endl;
}

static void ClosePinDisplayCB()
{
    std::cout << "ClosePinDisplayCB()" << std::endl;
}
#endif

int main(int, char **)
{
    std::cout.setf(std::ios::boolalpha);

    OCPersistentStorage ps = { PSOpenCB, fread, fwrite, fclose, unlink };
    if (OCRegisterPersistentStorageHandler(&ps) != OC_STACK_OK)
    {
        std::cerr << "OCRegisterPersistentStorageHandler error" << std::endl;
        return EXIT_FAILURE;
    }
#if __WITH_DTLS__
    if (SetDisplayPinWithContextCB(DisplayPinCB, NULL) != OC_STACK_OK)
    {
        std::cerr << "SetDisplayPinWithContextCB error" << std::endl;
        return EXIT_FAILURE;
    }
    SetClosePinDisplayCB(ClosePinDisplayCB);
    if (SetRandomPinPolicy(OXM_RANDOM_PIN_DEFAULT_SIZE, (OicSecPinType_t) OXM_RANDOM_PIN_DEFAULT_PIN_TYPE) != OC_STACK_OK)
    {
        std::cerr << "SetRandomPinPolicy error" << std::endl;
        return EXIT_FAILURE;
    }
#endif
    if (OCInit1(OC_CLIENT_SERVER, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS) != OC_STACK_OK)
    {
        std::cerr << "OCStack init error" << std::endl;
        return EXIT_FAILURE;
    }
    std::cout << "di=" << OCGetServerInstanceIDString() << std::endl;

    OCResourceHandle handle;
    OCStackResult result;
    /* Add device types */
    handle = OCGetResourceHandleAtUri(OC_RSRVD_DEVICE_URI);
    if (!handle)
    {
        std::cerr << "OCGetResourceHandleAtUri(" << OC_RSRVD_DEVICE_URI << ") failed" << std::endl;
        return EXIT_FAILURE;
    }
    result = OCBindResourceTypeToResource(handle, "oic.d.light");
    if (result != OC_STACK_OK)
    {
        std::cerr << "OCBindResourceTypeToResource() - " << result << std::endl;
        return EXIT_FAILURE;
    }
    result = OCBindResourceTypeToResource(handle, "x.example.-fan---device");
    if (result != OC_STACK_OK)
    {
        std::cerr << "OCBindResourceTypeToResource() - " << result << std::endl;
        return EXIT_FAILURE;
    }
    /* Set device info */
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, "device-name");
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_ID, OCGetServerInstanceIDString());
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SOFTWARE_VERSION, "software-version");
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MODEL_NUM, "model-num");
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, "x.example.foo", "bar");
    /* Set platform info */
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, "platform-id");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_URL, "mfg-url");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, "mfg-date");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_VERSION, "platform-version");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_OS_VERSION, "os-version");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, "hw-version");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_FIRMWARE_VERSION, "fw-version");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, "support-url");
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SYSTEM_TIME, "system-time");
    /* Create configuration resource */
    Configuration configuration;
    configuration.n = "My Friendly Device Name";
    configuration.loc = "My Location Information";
    configuration.locn = "My Location Name";
    configuration.c = "USD";
    configuration.r = "MyRegion";
    configuration.dl = "en";
    configuration.ln["en"] = "My Friendly Device Name";
    configuration.ln["es"] = "My Friendly Device Name";
    if (OCCreateResource(&handle, "oic.wk.con", "oic.if.rw", "/oic/con", configurationHandler,
                    &configuration, OC_DISCOVERABLE | OC_OBSERVABLE | OC_SECURE) != OC_STACK_OK)
    {
        std::cerr << "OCCreateResource error" << std::endl;
        return EXIT_FAILURE;
    }
    /* Create maintenance resource */
    Maintenance maintenance;
    maintenance.n = "My Maintenance Actions";
    if (OCCreateResource(&handle, "oic.wk.mnt", "oic.if.rw", "/oic/mnt", maintenanceHandler,
                    &maintenance, OC_DISCOVERABLE | OC_OBSERVABLE | OC_SECURE) != OC_STACK_OK)
    {
        std::cerr << "OCCreateResource error" << std::endl;
        return EXIT_FAILURE;
    }
    /* Create resources */
    Resource resource = { "Friendly Name", "Instance ID", false };
    if (OCCreateResource(&handle, "oic.r.switch.binary", "oic.if.a", "/switch/0", entityHandler,
                    &resource, OC_DISCOVERABLE | OC_OBSERVABLE) != OC_STACK_OK)
    {
        std::cerr << "OCCreateResource error" << std::endl;
        return EXIT_FAILURE;
    }
    if (OCCreateResource(&handle, "x.example.-binary-switch", "oic.if.a", "/switch/1", entityHandler,
                    &resource, OC_DISCOVERABLE) != OC_STACK_OK)
    {
        std::cerr << "OCCreateResource error" << std::endl;
        return EXIT_FAILURE;
    }
    if (OCCreateResource(&handle, "oic.r.switch.binary", "oic.if.a", "/switch/2", entityHandler,
                    &resource, OC_DISCOVERABLE) != OC_STACK_OK)
    {
        std::cerr << "OCCreateResource error" << std::endl;
        return EXIT_FAILURE;
    }
    result = OCBindResourceTypeToResource(handle, "x.example.-binary-switch");
    if (result != OC_STACK_OK)
    {
        std::cerr << "OCBindResourceTypeToResource() - " << result << std::endl;
        return EXIT_FAILURE;
    }

    for (;;)
    {
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        int ret = poll(&pfd, 1, 1);
        if (ret != 1)
        {
            if (OCProcess() != OC_STACK_OK)
            {
                std::cerr << "OCStack process error" << std::endl;
                return EXIT_FAILURE;
            }
            continue;
        }
        std::string line;
        std::getline(std::cin, line);
        boost::char_separator<char> sep(" ");
        boost::tokenizer<boost::char_separator<char>> tokens(line, sep);
        auto token = tokens.begin();
        if (token == tokens.end())
        {
            continue;
        }
        std::string cmd = *token++;
        if (cmd == "quit")
        {
            break;
        }
    }

    if (OCStop() != OC_STACK_OK)
    {
        std::cerr << "OCStack stop error" << std::endl;
    }

    return EXIT_SUCCESS;
}
