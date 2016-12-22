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

#include "ocstack.h"
#include "ocpayload.h"
#include "payload_logging.h"
#include "cbor.h"
#include "cborjson.h"
#include "cJSON.h"

extern "C" OCStackResult OCParsePayload(OCPayload **outPayload, OCPayloadType payloadType,
                                        const uint8_t *payload, size_t payloadSize);
extern "C" OCStackResult OCConvertPayload(OCPayload *payload, uint8_t **outPayload, size_t *size);

static CborError ConvertJSONToCBOR(CborEncoder *encoder, cJSON *json)
{
    CborEncoder containerEncoder;
    CborError err = CborNoError;

    if (json->string)
    {
        err = cbor_encode_text_stringz(encoder, json->string);
    }
    switch (json->type)
    {
        case cJSON_False:
            err = cbor_encode_boolean(encoder, false);
            break;
        case cJSON_True:
            err = cbor_encode_boolean(encoder, true);
            break;
        case cJSON_NULL:
            err = cbor_encode_null(encoder);
            break;
        case cJSON_Number:
            err = cbor_encode_double(encoder, json->valuedouble);
            break;
        case cJSON_String:
            err = cbor_encode_text_stringz(encoder, json->valuestring);
            break;
        case cJSON_Array:
            err = cbor_encoder_create_array(encoder, &containerEncoder, cJSON_GetArraySize(json));
            for (cJSON *child = json->child; err == CborNoError && child; child = child->next)
            {
                err = ConvertJSONToCBOR(&containerEncoder, child);
            }
            if (err == CborNoError)
            {
                err = cbor_encoder_close_container(encoder, &containerEncoder);
            }
            break;
        case cJSON_Object:
            err = cbor_encoder_create_map(encoder, &containerEncoder, cJSON_GetArraySize(json));
            for (cJSON *child = json->child; err == CborNoError && child; child = child->next)
            {
                err = ConvertJSONToCBOR(&containerEncoder, child);
            }
            if (err == CborNoError)
            {
                err = cbor_encoder_close_container(encoder, &containerEncoder);
            }
            break;
    }
    return err;
}

static size_t ConvertJSONToCBOR(const char *jsonStr, uint8_t *buffer, size_t size)
{
    CborEncoder encoder;
    CborError err = CborNoError;

    cbor_encoder_init(&encoder, buffer, size, 0);
    cJSON *json = cJSON_Parse(jsonStr);
    for (; err == CborNoError && json; json = json->next)
    {
        err = ConvertJSONToCBOR(&encoder, json);
    }
    if (json)
    {
        cJSON_Delete(json);
    }
    if (err == CborNoError)
    {
        return cbor_encoder_get_buffer_size(&encoder, buffer);
    }
    else
    {
        return 0;
    }
}

static void LogResponse(OCMethod method, OCClientResponse *response)
{
    if (!response)
    {
        return;
    }

    switch (method)
    {
        case OC_REST_DISCOVER:
        case OC_REST_GET:
        case OC_REST_OBSERVE:
            std::cout << "GET";
            break;
        case OC_REST_POST:
            std::cout << "POST";
            break;
        default:
            std::cout << method;
            break;
    }
    std::cout << " - " << response->result << std::endl;

    std::cout << "Uri-Host: " << response->devAddr.addr << std::endl
              << "Uri-Port: " << response->devAddr.port << std::endl
              << "Uri-Path: " << (response->resourceUri ? response->resourceUri : "") << std::endl;
    if (method & OC_REST_OBSERVE)
    {
        std::cout << "Observe: " << response->sequenceNumber << std::endl;
    }

    uint8_t *buffer = NULL;
    size_t size;
    OCStackResult result = OCConvertPayload(response->payload, &buffer, &size);
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

#define DEFAULT_CONTEXT_VALUE 0x99

struct Resource
{
    OCDevAddr host;
    std::string uri;
};
static std::vector<Resource> g_resources;

static OCStackApplicationResult onDiscover(void *, OCDoHandle,
        OCClientResponse *response)
{
    LogResponse(OC_REST_DISCOVER, response);
    if (response)
    {
        OCDiscoveryPayload *payload = (OCDiscoveryPayload *) response->payload;
        while (payload)
        {
            Resource r;
            r.host = response->devAddr;
            r.uri = payload->uri ? payload->uri : "/oic/res";
            g_resources.push_back(r);
            OCResourcePayload *resource = (OCResourcePayload *) payload->resources;
            while (resource)
            {
                Resource r;
                r.host = response->devAddr;
                r.uri = resource->uri;
                g_resources.push_back(r);
                resource = resource->next;
            }
            payload = payload->next;
        }
    }
    return OC_STACK_KEEP_TRANSACTION;
}

static OCStackApplicationResult onGet(void *, OCDoHandle,
                                      OCClientResponse *response)
{
    LogResponse(OC_REST_GET, response);
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult onPost(void *, OCDoHandle,
                                       OCClientResponse *response)
{
    LogResponse(OC_REST_POST, response);
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult onObserve(void *, OCDoHandle,
        OCClientResponse *response)
{
    LogResponse(OC_REST_OBSERVE, response);
    if (response->payload)
    {
        return OC_STACK_KEEP_TRANSACTION;
    }
    else
    {
        return OC_STACK_DELETE_TRANSACTION;
    }
}

static void usage()
{
    std::cout << "Usage:" << std::endl
              << std::endl
              << "Find smart home bridge devices:" << std::endl
              << "  find /oic/res?rt=oic.d.bridge" << std::endl
              << "Find light devices:" << std::endl
              << "  find /oic/res?rt=oic.d.light" << std::endl
              << "Find all devices:" << std::endl
              << "  find /oic/res" << std::endl
              << std::endl
              << "List all found devices:" << std::endl
              << "  list" << std::endl
              << std::endl
              << "Get attributes of a resource:" << std::endl
              << "  get INDEX [QUERY PARAM]..." << std::endl
              << std::endl
              << "Post attributes of a resource:" << std::endl
              << "  post INDEX [QUERY PARAM]... [JSON]" << std::endl
              << std::endl
              << "Observe attributes of a resource:" << std::endl
              << "  observe INDEX [QUERY PARAM]" << std::endl;
}

int main(int, char **)
{
    std::cout.setf(std::ios::boolalpha);

    if (OCInit1(OC_CLIENT, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS) != OC_STACK_OK)
    {
        std::cerr << "OCStack init error" << std::endl;
        return EXIT_FAILURE;
    }

    for (;;)
    {
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        int ret = poll(&pfd, 1, 1000);
        if (ret != 1)
        {
            if (OCProcess() != OC_STACK_OK)
            {
                std::cerr << "OCStack process error" << std::endl;
                return 0;
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
            usage();
            continue;
        }
        std::string cmd = *token++;
        if (cmd == "list")
        {
            for (size_t i = 0; i < g_resources.size(); ++i)
            {
                std::cout << "[" << i << "]" << std::endl;
                std::cout << "\tcoap://" << g_resources[i].host.addr << ":" << g_resources[i].host.port <<
                          g_resources[i].uri << std::endl;
            }
        }
        else if (cmd == "find")
        {
            std::string requestURI = *token++;
            OCCallbackData cbData;
            cbData.cb = onDiscover;
            cbData.context = (void *)DEFAULT_CONTEXT_VALUE;
            cbData.cd = NULL;
            OCStackResult result = OCDoResource(NULL, OC_REST_DISCOVER, requestURI.c_str(), NULL, 0, CT_DEFAULT,
                                                OC_HIGH_QOS, &cbData, NULL, 0);
            std::cout << "find " << requestURI << " - " << result << std::endl;
        }
        else if (cmd == "get")
        {
            size_t i = std::stoi(*token++);
            std::string query;
            while (token != tokens.end())
            {
                std::string str = *token++;
                if (str != "{")
                {
                    if (!query.empty())
                    {
                        query += ";";
                    }
                    query += str;
                }
            }
            std::string uri = g_resources[i].uri;
            if (!query.empty())
            {
                uri += "?" + query;
            }
            OCCallbackData cbData;
            cbData.cb = onGet;
            cbData.context = (void *)DEFAULT_CONTEXT_VALUE;
            cbData.cd = NULL;
            OCStackResult result = OCDoResource(NULL, OC_REST_GET, uri.c_str(), &g_resources[i].host,
                                                NULL, CT_DEFAULT, OC_HIGH_QOS, &cbData, NULL, 0);
            std::cout << "get " << g_resources[i].host.addr << ":" << g_resources[i].host.port <<
                      g_resources[i].uri << " - " << result << std::endl;
        }
        else if (cmd == "put")
        {
            // TODO
        }
        else if (cmd == "post")
        {
            size_t i = std::stoi(*token++);
            std::string query, body;
            while (token != tokens.end())
            {
                std::string str = *token++;
                if (str.find('=') != std::string::npos)
                {
                    if (!query.empty())
                    {
                        query += ";";
                    }
                    query += str;
                }
                else
                {
                    body += str;
                }
            }
            std::string uri = g_resources[i].uri;
            if (!query.empty())
            {
                uri += "?" + query;
            }
            OCRepPayload *payload = NULL;
            size_t size = 8192;
            uint8_t buffer[8192];
            size = ConvertJSONToCBOR(body.c_str(), buffer, size);
            if (size)
            {
                payload = OCRepPayloadCreate();
                OCParsePayload((OCPayload **) &payload, PAYLOAD_TYPE_REPRESENTATION, buffer, size);
            }
            OCCallbackData cbData;
            cbData.cb = onPost;
            cbData.context = (void *)DEFAULT_CONTEXT_VALUE;
            cbData.cd = NULL;
            OCStackResult result = OCDoResource(NULL, OC_REST_POST, uri.c_str(), &g_resources[i].host,
                                                (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS, &cbData, NULL, 0);
            std::cout << "post " << g_resources[i].host.addr << ":" << g_resources[i].host.port <<
                      g_resources[i].uri << " - " << result << std::endl;
        }
        else if (cmd == "observe")
        {
            size_t i = std::stoi(*token++);
            std::string query;
            while (token != tokens.end())
            {
                std::string str = *token++;
                if (str.find('=') != std::string::npos)
                {
                    if (!query.empty())
                    {
                        query += ";";
                    }
                    query += str;
                }
            }
            std::string uri = g_resources[i].uri;
            if (!query.empty())
            {
                uri += "?" + query;
            }
            OCDoHandle handle;
            OCCallbackData cbData;
            cbData.cb = onObserve;
            cbData.context = (void *)DEFAULT_CONTEXT_VALUE;
            cbData.cd = NULL;
            OCStackResult result = OCDoResource(&handle, OC_REST_OBSERVE, uri.c_str(), &g_resources[i].host,
                                                NULL, CT_DEFAULT, OC_HIGH_QOS, &cbData, NULL, 0);
            std::cout << "observe " << g_resources[i].host.addr << ":" << g_resources[i].host.port <<
                      g_resources[i].uri << " (" << handle << ") - " << result << std::endl;
        }
        else if (cmd == "cancel")
        {
            OCDoHandle handle = (OCDoHandle) (intptr_t) std::stoi(*token++, 0, 0);
            OCStackResult result = OCCancel(handle, OC_HIGH_QOS, NULL, 0);
            std::cout << "cancel (" << handle << ") - " << result << std::endl;
        }
        else if (cmd == "delete")
        {
            // TODO
        }
        else if (cmd == "quit")
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
