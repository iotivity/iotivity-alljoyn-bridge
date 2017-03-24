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
#include <sstream>

#include "cJSON.h"
#include "cacommon.h"
#include "cbor.h"
#include "cborjson.h"
#include "coap/pdu.h"
#include "ocpayload.h"
#include "ocpayloadcbor.h"
#include "ocstack.h"
#include "payload_logging.h"
#if __WITH_DTLS__
#include "pinoxmcommon.h"
#endif

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

static void LogResponse(OCMethod method, OCClientResponse *response, OCPayloadFormat format)
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
    OCStackResult result = OCConvertPayload(response->payload, format, &buffer, &size);
    if (result != OC_STACK_OK)
    {
        return;
    }
    CborParser parser;
    CborValue it;
    CborError err = cbor_parser_init(buffer, size, CborConvertDefaultFlags, &parser, &it);
    char value[32768];
    FILE *fp = fmemopen(value, 32768, "w");
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

struct Resource
{
    std::string di;
    OCDevAddr devAddr;
    std::string uri;
};
static std::vector<Resource> g_resources;

static OCStackApplicationResult DiscoverCB(void *context, OCDoHandle,
        OCClientResponse *response)
{
    uint16_t format = (uint16_t)(uintptr_t)context;
    switch (format)
    {
        case COAP_MEDIATYPE_APPLICATION_VND_OCF_CBOR:
            LogResponse(OC_REST_DISCOVER, response, OC_FORMAT_VND_OCF_CBOR);
            break;
        case COAP_MEDIATYPE_APPLICATION_CBOR:
            LogResponse(OC_REST_DISCOVER, response, OC_FORMAT_CBOR);
            break;
    }
    if (response)
    {
        OCDiscoveryPayload *payload = (OCDiscoveryPayload *) response->payload;
        while (payload)
        {
            OCResourcePayload *resource = (OCResourcePayload *) payload->resources;
            while (resource)
            {
                Resource r;
                r.di = payload->sid;
                if (resource->eps)
                {
                    OCEndpointPayload *ep = resource->eps;
                    if (!strcmp(ep->tps, "coap") || !strcmp(ep->tps, "coaps"))
                    {
                        r.devAddr.adapter = OC_ADAPTER_IP;
                    }
                    else if (!strcmp(ep->tps, "coap+tcp") || !strcmp(ep->tps, "coaps+tcp"))
                    {
                        r.devAddr.adapter = OC_ADAPTER_TCP;
                    }
                    r.devAddr.flags = ep->family;
                    r.devAddr.port = ep->port;
                    strncpy(r.devAddr.addr, ep->addr, MAX_ADDR_STR_SIZE);
                    r.devAddr.ifindex = 0;
                    r.devAddr.routeData[0] = '\0';
                    strncpy(r.devAddr.remoteId, payload->sid, MAX_IDENTITY_SIZE);
                }
                else
                {
                    r.devAddr = response->devAddr;
                    if (resource->secure)
                    {
                        r.devAddr.flags = (OCTransportFlags) (r.devAddr.flags | OC_FLAG_SECURE);
                        if (response->devAddr.adapter == OC_ADAPTER_IP)
                        {
                            r.devAddr.port = resource->port;
                        }
#ifdef TCP_ADAPTER
                        else if (response->devAddr.adapter == OC_ADAPTER_TCP)
                        {
                            r.devAddr.port = resource->tcpPort;
                        }
#endif
                    }
                }
                if (!strncmp(resource->uri, "ocf://", 6))
                {
                    r.uri = strchr(resource->uri + 6, '/');
                }
                else
                {
                    r.uri = resource->uri;
                }
                g_resources.push_back(r);
                resource = resource->next;
            }
            payload = payload->next;
        }
    }
    return OC_STACK_KEEP_TRANSACTION;
}

static OCStackApplicationResult GetCB(void *, OCDoHandle,
        OCClientResponse *response)
{
    LogResponse(OC_REST_GET, response, OC_FORMAT_CBOR);
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult PostCB(void *context, OCDoHandle,
        OCClientResponse *response)
{
    uint16_t format = (uint16_t)(uintptr_t)context;
    switch (format)
    {
        case COAP_MEDIATYPE_APPLICATION_VND_OCF_CBOR:
            LogResponse(OC_REST_POST, response, OC_FORMAT_VND_OCF_CBOR);
            break;
        case COAP_MEDIATYPE_APPLICATION_CBOR:
            LogResponse(OC_REST_POST, response, OC_FORMAT_CBOR);
            break;
    }
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult ObserveCB(void *, OCDoHandle,
        OCClientResponse *response)
{
    LogResponse(OC_REST_OBSERVE, response, OC_FORMAT_CBOR);
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
              << "  find rt=oic.d.bridge" << std::endl
              << "Find light devices:" << std::endl
              << "  find rt=oic.d.light" << std::endl
              << "Find all devices:" << std::endl
              << "  find" << std::endl
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

static FILE *PSOpenCB(const char *suffix, const char *mode)
{
    std::string path = std::string("occlient_") + suffix;
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

    for (;;)
    {
        struct pollfd pfd = { STDIN_FILENO, POLLIN, 0 };
        int ret = poll(&pfd, 1, 1);
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
                std::cout << "[" << i << "] "
                          << g_resources[i].di << " "
                          << g_resources[i].uri << std::endl;
            }
        }
        else if (cmd == "find")
        {
            uint16_t optionID = CA_OPTION_ACCEPT;
            uint16_t format = COAP_MEDIATYPE_APPLICATION_VND_OCF_CBOR;
            std::string query;
            while (token != tokens.end())
            {
                std::string str = *token++;
                if (str == "Accept:")
                {
                    str = *token++;
                    if (str == "application/cbor")
                    {
                        format = COAP_MEDIATYPE_APPLICATION_CBOR;
                    }
                }
                else
                {
                    if (!query.empty())
                    {
                        query += ";";
                    }
                    query += str;
                }
            }
            std::string uri = "/oic/res";
            if (!query.empty())
            {
                uri += "?" + query;
            }
            OCCallbackData cbData;
            cbData.cb = DiscoverCB;
            cbData.context = (void *)(uintptr_t)format;
            cbData.cd = NULL;
            OCHeaderOption options[1];
            size_t numOptions = 0;
            OCSetHeaderOption(options, &numOptions, optionID, &format, sizeof(format));
            OCStackResult result = OCDoResource(NULL, OC_REST_DISCOVER, uri.c_str(), NULL, 0,
                    CT_DEFAULT, OC_HIGH_QOS, &cbData, options, numOptions);
            std::cout << "find " << uri << " - " << result << std::endl;
        }
        else if (cmd == "get")
        {
            uint16_t optionID = CA_OPTION_ACCEPT;
            uint16_t format = COAP_MEDIATYPE_APPLICATION_CBOR;
            std::string uri;
            OCDevAddr *devAddr;
            try
            {
                size_t i = std::stoi(*token);
                uri = g_resources[i].uri;
                devAddr = &g_resources[i].devAddr;
            }
            catch (std::exception ex)
            {
                uri = *token;
            }
            ++token;
            std::string query;
            while (token != tokens.end())
            {
                std::string str = *token++;
                if (str == "Accept:")
                {
                    str = *token++;
                    if (str == "application/vnd.ocf+cbor")
                    {
                        format = COAP_MEDIATYPE_APPLICATION_VND_OCF_CBOR;
                    }
                }
                else
                {
                    if (!query.empty())
                    {
                        query += ";";
                    }
                    query += str;
                }
            }
            if (!query.empty())
            {
                uri += "?" + query;
            }
            OCCallbackData cbData;
            cbData.cb = GetCB;
            cbData.context = NULL;
            cbData.cd = NULL;
            OCHeaderOption options[1];
            size_t numOptions = 0;
            OCSetHeaderOption(options, &numOptions, optionID, &format, sizeof(format));
            OCStackResult result = OCDoResource(NULL, OC_REST_GET, uri.c_str(), devAddr,
                    NULL, CT_DEFAULT, OC_HIGH_QOS, &cbData, options, numOptions);
            std::cout << "get " << uri << " - " << result << std::endl;
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
            size_t size = 32768;
            uint8_t buffer[32768];
            size = ConvertJSONToCBOR(body.c_str(), buffer, size);
            if (size)
            {
                payload = OCRepPayloadCreate();
                OCParsePayload((OCPayload **) &payload, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
                        buffer, size);
            }
            OCDevAddr *devAddr = &g_resources[i].devAddr;
            OCCallbackData cbData;
            cbData.cb = PostCB;
            cbData.context = NULL;
            cbData.cd = NULL;
            OCStackResult result = OCDoResource(NULL, OC_REST_POST, uri.c_str(), devAddr,
                    (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS, &cbData, NULL, 0);
            std::cout << "post " << g_resources[i].uri << " - " << result << std::endl;
        }
        else if (cmd == "observe")
        {
            uint16_t optionID = CA_OPTION_ACCEPT;
            uint16_t format = COAP_MEDIATYPE_APPLICATION_CBOR;
            size_t i = std::stoi(*token++);
            std::string query;
            while (token != tokens.end())
            {
                std::string str = *token++;
                if (str == "Accept:")
                {
                    str = *token++;
                    if (str == "application/vnd.ocf+cbor")
                    {
                        format = COAP_MEDIATYPE_APPLICATION_VND_OCF_CBOR;
                    }
                }
                else
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
            OCDevAddr *devAddr = &g_resources[i].devAddr;
            OCDoHandle handle;
            OCCallbackData cbData;
            cbData.cb = ObserveCB;
            cbData.context = (void *)(uintptr_t)format;
            cbData.cd = NULL;
            OCHeaderOption options[1];
            size_t numOptions = 0;
            OCSetHeaderOption(options, &numOptions, optionID, &format, sizeof(format));
            OCStackResult result = OCDoResource(&handle, OC_REST_OBSERVE, uri.c_str(), devAddr,
                    NULL, CT_DEFAULT, OC_HIGH_QOS, &cbData, options, numOptions);
            std::cout << "observe " << g_resources[i].uri << " (" << handle << ") - " << result
                      << std::endl;
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
