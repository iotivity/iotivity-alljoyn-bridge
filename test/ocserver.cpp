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
#include <poll.h>

#include "cbor.h"
#include "cborjson.h"
#include "cJSON.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocstack.h"

extern "C" OCStackResult OCConvertPayload(OCPayload *payload, uint8_t **outPayload, size_t *size);

#define OC_RSRVD_FRIENDLY_NAME "n"
#define OC_RSRVD_INSTANCE_ID "id"

static void LogPayload(OCPayload *payload)
{
    uint8_t *buffer = NULL;
    size_t size;
    OCStackResult result = OCConvertPayload(payload, &buffer, &size);
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

typedef struct Resource {
    const char *n;
    const char *id;
    bool value;
} Resource;

static OCEntityHandlerResult entityHandler(OCEntityHandlerFlag flag,
        OCEntityHandlerRequest *request,
        void *ctx)
{
    (void) flag;
    Resource *resource = (Resource*)ctx;
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
    char** array = NULL;
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
            array = (char**)OICCalloc(n, sizeof(char*));
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
            array = (char**)OICCalloc(n, sizeof(char*));
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
            for (OCRepPayloadValue* value = ((OCRepPayload*)request->payload)->values; value; value = value->next)
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
        for(uint8_t i = 0; i < n; ++i)
        {
            OICFree(array[i]);
        }
        OICFree(array);
    }
    OCRepPayloadDestroy(payload);
    return ehResult;
}

int main(int, char **)
{
    std::cout.setf(std::ios::boolalpha);

    if (OCInit1(OC_CLIENT_SERVER, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS) != OC_STACK_OK)
    {
        std::cerr << "OCStack init error" << std::endl;
        return EXIT_FAILURE;
    }

    // TODO SetDeviceInfo
    // TODO SetPlatformInfo
    Resource resource = { "Friendly Name", "Instance ID", false };
    OCResourceHandle handle;
    if (OCCreateResource(&handle, "oic.r.switch.binary", "oic.if.a", "/BinarySwitchResURI",
                         entityHandler, &resource, OC_DISCOVERABLE | OC_OBSERVABLE) != OC_STACK_OK)
    {
        std::cerr << "OCCreateResource error" << std::endl;
        return EXIT_FAILURE;
    }

    for (;;)
    {
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        int ret = poll(&pfd, 1, 100);
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
