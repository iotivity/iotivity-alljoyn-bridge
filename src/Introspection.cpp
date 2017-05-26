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

#include "Introspection.h"

#include "Log.h"
#include "Name.h"
#include "Payload.h"
#include "Plugin.h"
#include "Signature.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "cJSON.h"
#include <assert.h>

#define VERIFY_CBOR(err)                                            \
    if ((CborNoError != (err)) && (CborErrorOutOfMemory != (err)))  \
    {                                                               \
        goto exit;                                                  \
    }

static bool SetPropertiesSchema(OCRepPayload *parent, OCRepPayload *obj);

static int64_t Pair(CborEncoder *cbor, const char *key, const char *value)
{
    int64_t err = CborNoError;
    err |= cbor_encode_text_stringz(cbor, key);
    VERIFY_CBOR(err);
    err |= cbor_encode_text_stringz(cbor, value);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Pair(CborEncoder *cbor, const char *key, bool value)
{
    int64_t err = CborNoError;
    err |= cbor_encode_text_stringz(cbor, key);
    VERIFY_CBOR(err);
    err |= cbor_encode_boolean(cbor, value);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Pair(CborEncoder *cbor, const char *key, uint64_t value)
{
    int64_t err = CborNoError;
    err |= cbor_encode_text_stringz(cbor, key);
    VERIFY_CBOR(err);
    err |= cbor_encode_uint(cbor, value);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Pair(CborEncoder *cbor, const char *key, int64_t value)
{
    int64_t err = CborNoError;
    err |= cbor_encode_text_stringz(cbor, key);
    VERIFY_CBOR(err);
    err |= cbor_encode_int(cbor, value);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Pair(CborEncoder *cbor, const char *key, double value)
{
    int64_t err = CborNoError;
    err |= cbor_encode_text_stringz(cbor, key);
    VERIFY_CBOR(err);
    err |= cbor_encode_double(cbor, value);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Info(CborEncoder *cbor, const char *title, const char *version)
{
    int64_t err = CborNoError;
    CborEncoder info;
    err |= cbor_encode_text_stringz(cbor, "info");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_map(cbor, &info, CborIndefiniteLength);
    VERIFY_CBOR(err);
    err |= Pair(&info, "title", title);
    VERIFY_CBOR(err);
    err |= Pair(&info, "version", version);
    VERIFY_CBOR(err);
    err |= cbor_encoder_close_container(cbor, &info);
    VERIFY_CBOR(err);
exit:
    return err;
}

static bool ImplementsPost(OCResourceHandle h)
{
    uint8_t ni;
    OCStackResult result = OCGetNumberOfResourceInterfaces(h, &ni);
    if (result != OC_STACK_OK)
    {
        return false;
    }
    for (uint8_t j = 0; j < ni; ++j)
    {
        const char *ifn = OCGetResourceInterfaceName(h, j);
        if (!strcmp(ifn, OC_RSRVD_INTERFACE_READ_WRITE))
        {
            return true;
        }
    }
    return false;
}

static int64_t QueryParameters(CborEncoder *cbor, OCResourceHandle h)
{
    OCStackResult result;
    int64_t err = CborNoError;
    CborEncoder map;
    err |= cbor_encoder_create_map(cbor, &map, CborIndefiniteLength);
    VERIFY_CBOR(err);
    err |= Pair(&map, "name", "if");
    VERIFY_CBOR(err);
    err |= Pair(&map, "in", "query");
    VERIFY_CBOR(err);
    err |= Pair(&map, "type", "string");
    VERIFY_CBOR(err);
    CborEncoder enumArr;
    err |= cbor_encode_text_stringz(&map, "enum");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_array(&map, &enumArr, CborIndefiniteLength);
    VERIFY_CBOR(err);
    uint8_t ni;
    result = OCGetNumberOfResourceInterfaces(h, &ni);
    if (result != OC_STACK_OK)
    {
        err |= CborErrorInternalError;
        goto exit;
    }
    for (uint8_t j = 0; j < ni; ++j)
    {
        err |= cbor_encode_text_stringz(&enumArr, OCGetResourceInterfaceName(h, j));
        VERIFY_CBOR(err);
    }
    err |= cbor_encoder_close_container(&map, &enumArr);
    VERIFY_CBOR(err);
    err |= cbor_encoder_close_container(cbor, &map);
    VERIFY_CBOR(err);
exit:
    return result;
}

static int64_t Schema(CborEncoder *cbor, OCResourceHandle h)
{
    OCStackResult result;
    int64_t err = CborNoError;
    CborEncoder schema;
    err |= cbor_encode_text_stringz(cbor, "schema");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_map(cbor, &schema, CborIndefiniteLength);
    VERIFY_CBOR(err);
    CborEncoder oneOf;
    err |= cbor_encode_text_stringz(&schema, "oneOf");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_array(&schema, &oneOf, CborIndefiniteLength);
    VERIFY_CBOR(err);
    uint8_t nrt;
    result = OCGetNumberOfResourceTypes(h, &nrt);
    if (result != OC_STACK_OK)
    {
        err |= CborErrorInternalError;
        goto exit;
    }
    for (uint8_t j = 0; j < nrt; ++j)
    {
        std::string ref = std::string("#/definitions/") + OCGetResourceTypeName(h, j);
        CborEncoder nestedSchema;
        err |= cbor_encoder_create_map(&oneOf, &nestedSchema, 1);
        VERIFY_CBOR(err);
        err |= Pair(&nestedSchema, "$ref", ref.c_str());
        VERIFY_CBOR(err);
        err |= cbor_encoder_close_container(&oneOf, &nestedSchema);
        VERIFY_CBOR(err);
    }
    err |= cbor_encoder_close_container(&schema, &oneOf);
    VERIFY_CBOR(err);
    err |= cbor_encoder_close_container(cbor, &schema);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Responses(CborEncoder *cbor, OCResourceHandle h)
{
    int64_t err = CborNoError;
    CborEncoder responses;
    err |= cbor_encode_text_stringz(cbor, "responses");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_map(cbor, &responses, CborIndefiniteLength);
    VERIFY_CBOR(err);
    CborEncoder response;
    err |= cbor_encode_text_stringz(&responses, "200");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_map(&responses, &response, CborIndefiniteLength);
    VERIFY_CBOR(err);
    err |= Pair(&response, "description", "");
    VERIFY_CBOR(err);
    err |= Schema(&response, h);
    VERIFY_CBOR(err);
    err |= cbor_encoder_close_container(&responses, &response);
    VERIFY_CBOR(err);
    err |= cbor_encoder_close_container(cbor, &responses);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Paths(CborEncoder *cbor)
{
    OCStackResult result = OC_STACK_ERROR;
    int64_t err = CborNoError;
    CborEncoder paths;
    err |= cbor_encode_text_stringz(cbor, "paths");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_map(cbor, &paths, CborIndefiniteLength);
    VERIFY_CBOR(err);
    uint8_t nr;
    result = OCGetNumberOfResources(&nr);
    if (result != OC_STACK_OK)
    {
        err |= CborErrorInternalError;
        goto exit;
    }
    for (uint8_t i = 0; i < nr; ++i)
    {
        OCResourceHandle h = OCGetResourceHandle(i);
        if (!(OCGetResourceProperties(h) & OC_ACTIVE))
        {
            continue;
        }
        const char *uri = OCGetResourceUri(h);
        if (!strcmp(uri, OC_RSRVD_WELL_KNOWN_URI) ||
                !strcmp(uri, OC_RSRVD_DEVICE_URI) ||
                !strcmp(uri, OC_RSRVD_PLATFORM_URI) ||
                !strncmp(uri, "/oic/sec", 8))
        {
            // TODO skip unless there are vendor specific properties
            continue;
        }
        CborEncoder path;
        err |= cbor_encode_text_stringz(&paths, uri);
        VERIFY_CBOR(err);
        err |= cbor_encoder_create_map(&paths, &path, CborIndefiniteLength);
        VERIFY_CBOR(err);
        CborEncoder get;
        err |= cbor_encode_text_stringz(&path, "get");
        VERIFY_CBOR(err);
        err |= cbor_encoder_create_map(&path, &get, CborIndefiniteLength);
        VERIFY_CBOR(err);
        CborEncoder parameters;
        err |= cbor_encode_text_stringz(&get, "parameters");
        VERIFY_CBOR(err);
        err |= cbor_encoder_create_array(&get, &parameters, CborIndefiniteLength);
        VERIFY_CBOR(err);
        err |= QueryParameters(&parameters, h);
        VERIFY_CBOR(err);
        err |= cbor_encoder_close_container(&get, &parameters);
        VERIFY_CBOR(err);
        err |= Responses(&get, h);
        VERIFY_CBOR(err);
        err |= cbor_encoder_close_container(&path, &get);
        VERIFY_CBOR(err);
        if (ImplementsPost(h))
        {
            CborEncoder post;
            err |= cbor_encode_text_stringz(&path, "post");
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_map(&path, &post, CborIndefiniteLength);
            VERIFY_CBOR(err);
            CborEncoder parameters;
            err |= cbor_encode_text_stringz(&post, "parameters");
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_array(&post, &parameters, CborIndefiniteLength);
            VERIFY_CBOR(err);
            err |= QueryParameters(&parameters, h);
            VERIFY_CBOR(err);
            CborEncoder body;
            err |= cbor_encoder_create_map(&parameters, &body, CborIndefiniteLength);
            VERIFY_CBOR(err);
            err |= Pair(&body, "name", "body");
            VERIFY_CBOR(err);
            err |= Pair(&body, "in", "body");
            VERIFY_CBOR(err);
            err |= Schema(&body, h);
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&parameters, &body);
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&post, &parameters);
            VERIFY_CBOR(err);
            err |= Responses(&post, h);
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&path, &post);
            VERIFY_CBOR(err);
        }
        err |= cbor_encoder_close_container(&paths, &path);
        VERIFY_CBOR(err);
    }
    err |= cbor_encoder_close_container(cbor, &paths);
    VERIFY_CBOR(err);
exit:
    return err;
}

static uint64_t GetUnsignedTypeMax(const char *signature)
{
    switch (signature[0])
    {
        case ajn::ALLJOYN_BYTE: return UINT8_MAX;
        case ajn::ALLJOYN_UINT16: return UINT16_MAX;
        case ajn::ALLJOYN_UINT32: return UINT32_MAX;
        default: return UINT64_MAX;
    }
}

static int64_t GetTypeMin(const char *signature)
{
    switch (signature[0])
    {
        case ajn::ALLJOYN_INT16: return INT16_MIN;
        case ajn::ALLJOYN_INT32: return INT32_MIN;
        default: return INT64_MIN;
    }
}

static int64_t GetTypeMax(const char *signature)
{
    switch (signature[0])
    {
        case ajn::ALLJOYN_INT16: return INT16_MAX;
        case ajn::ALLJOYN_INT32: return INT32_MAX;
        default: return INT64_MAX;
    }
}

static int64_t GetJsonType(CborEncoder *cbor, const char *ajType,
        const qcc::String &typeMin, const qcc::String &typeMax, const qcc::String &typeDefault)
{
    int64_t err = CborNoError;
    switch (ajType[0])
    {
        case ajn::ALLJOYN_BOOLEAN:
            err |= Pair(cbor, "type", "boolean");
            VERIFY_CBOR(err);
            if (!typeDefault.empty())
            {
                err |= Pair(cbor, "default", (typeDefault == "true"));
                VERIFY_CBOR(err);
            }
            break;
        case ajn::ALLJOYN_BYTE:
        case ajn::ALLJOYN_UINT16:
        case ajn::ALLJOYN_UINT32:
        case ajn::ALLJOYN_UINT64:
            {
                uint64_t min = typeMin.empty() ? 0 : strtoull(typeMin.c_str(), NULL, 0);
                uint64_t max = typeMax.empty() ? GetUnsignedTypeMax(ajType) : strtoull(typeMax.c_str(), NULL, 0);
                if (max <= MAX_SAFE_INTEGER)
                {
                    err |= Pair(cbor, "type", "integer");
                    VERIFY_CBOR(err);
                    err |= Pair(cbor, "minimum", min);
                    VERIFY_CBOR(err);
                    err |= Pair(cbor, "maximum", max);
                    VERIFY_CBOR(err);
                }
                else
                {
                    err |= Pair(cbor, "type", "string");
                    VERIFY_CBOR(err);
                    err |= Pair(cbor, "format", "uint64");
                    VERIFY_CBOR(err);
                }
                if (!typeDefault.empty())
                {
                    uint64_t v;
                    sscanf(typeDefault.c_str(), "%" SCNu64, &v);
                    err |= Pair(cbor, "default", v);
                    VERIFY_CBOR(err);
                }
            }
            break;
        case ajn::ALLJOYN_INT16:
        case ajn::ALLJOYN_INT32:
        case ajn::ALLJOYN_INT64:
            {
                int64_t min = typeMin.empty() ? GetTypeMin(ajType) : strtoull(typeMin.c_str(), NULL, 0);
                int64_t max = typeMax.empty() ? GetTypeMax(ajType) : strtoull(typeMax.c_str(), NULL, 0);
                if (MIN_SAFE_INTEGER <= min && max <= MAX_SAFE_INTEGER)
                {
                    err |= Pair(cbor, "type", "integer");
                    VERIFY_CBOR(err);
                    err |= Pair(cbor, "minimum", min);
                    VERIFY_CBOR(err);
                    err |= Pair(cbor, "maximum", max);
                    VERIFY_CBOR(err);
                }
                else
                {
                    err |= Pair(cbor, "type", "string");
                    VERIFY_CBOR(err);
                    err |= Pair(cbor, "format", "int64");
                    VERIFY_CBOR(err);
                }
                if (!typeDefault.empty())
                {
                    int64_t v;
                    sscanf(typeDefault.c_str(), "%" SCNd64, &v);
                    err |= Pair(cbor, "default", v);
                    VERIFY_CBOR(err);
                }
            }
            break;
        case ajn::ALLJOYN_DOUBLE:
            err |= Pair(cbor, "type", "number");
            VERIFY_CBOR(err);
            if (!typeDefault.empty())
            {
                err |= Pair(cbor, "default", std::stod(typeDefault));
                VERIFY_CBOR(err);
            }
            break;
        case ajn::ALLJOYN_STRING:
        case ajn::ALLJOYN_OBJECT_PATH:
        case ajn::ALLJOYN_SIGNATURE:
            err |= Pair(cbor, "type", "string");
            VERIFY_CBOR(err);
            if (!typeDefault.empty())
            {
                err |= Pair(cbor, "default", typeDefault.c_str());
                VERIFY_CBOR(err);
            }
            break;
        case ajn::ALLJOYN_ARRAY:
            switch (ajType[1])
            {
                case ajn::ALLJOYN_BYTE:
                    err |= Pair(cbor, "type", "string");
                    VERIFY_CBOR(err);
                    CborEncoder media;
                    err |= cbor_encode_text_stringz(cbor, "media");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_create_map(cbor, &media, 1);
                    VERIFY_CBOR(err);
                    err |= Pair(&media, "binaryEncoding", "base64");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(cbor, &media);
                    VERIFY_CBOR(err);
                    break;
                case ajn::ALLJOYN_VARIANT:
                    err |= Pair(cbor, "type", "undefined");
                    VERIFY_CBOR(err);
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    err |= Pair(cbor, "type", "object");
                    VERIFY_CBOR(err);
                    break;
                default:
                    err |= Pair(cbor, "type", "array");
                    VERIFY_CBOR(err);
                    CborEncoder items;
                    err |= cbor_encode_text_stringz(cbor, "items");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_create_map(cbor, &items, CborIndefiniteLength);
                    VERIFY_CBOR(err);
                    qcc::String min, max, def;
                    err |= GetJsonType(&items, &ajType[1], min, max, def);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(cbor, &items);
                    VERIFY_CBOR(err);
                    break;
            }
            break;
        case ajn::ALLJOYN_STRUCT_OPEN:
            err |= Pair(cbor, "type", "array");
            VERIFY_CBOR(err);
            break;
        case ajn::ALLJOYN_VARIANT:
            CborEncoder anyOf;
            err |= cbor_encode_text_stringz(cbor, "anyOf");
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_array(cbor, &anyOf, CborIndefiniteLength);
            VERIFY_CBOR(err);
            CborEncoder type;
            err |= cbor_encoder_create_map(&anyOf, &type, 1);
            VERIFY_CBOR(err);
            err |= Pair(&type, "type", "string");
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&anyOf, &type);
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_map(&anyOf, &type, 1);
            VERIFY_CBOR(err);
            err |= Pair(&type, "type", "integer");
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&anyOf, &type);
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_map(&anyOf, &type, 1);
            VERIFY_CBOR(err);
            err |= Pair(&type, "type", "number");
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&anyOf, &type);
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_map(&anyOf, &type, 1);
            VERIFY_CBOR(err);
            err |= Pair(&type, "type", "object");
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&anyOf, &type);
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_map(&anyOf, &type, 1);
            VERIFY_CBOR(err);
            err |= Pair(&type, "type", "array");
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&anyOf, &type);
            VERIFY_CBOR(err);
            err |= cbor_encoder_create_map(&anyOf, &type, 1);
            VERIFY_CBOR(err);
            err |= Pair(&type, "type", "boolean");
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(&anyOf, &type);
            VERIFY_CBOR(err);
            err |= cbor_encoder_close_container(cbor, &anyOf);
            VERIFY_CBOR(err);
            break;
        case '[':
            {
                std::string def = ajType;
                def = "#/definitions/" + def.substr(1, def.size() - 2);
                err |= Pair(cbor, "$ref", def.c_str());
                VERIFY_CBOR(err);
            }
            break;
        default:
            err |= Pair(cbor, "type", "undefined");
            VERIFY_CBOR(err);
            break;
    }
exit:
    return err;
}

static int64_t Property(CborEncoder *cbor, std::string propName, qcc::String description,
        bool readOnly, qcc::String sig, qcc::String min, qcc::String max, qcc::String def)
{
    int64_t err = CborNoError;
    CborEncoder prop;
    err |= cbor_encode_text_stringz(cbor, propName.c_str());
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_map(cbor, &prop, CborIndefiniteLength);
    VERIFY_CBOR(err);
    if (!description.empty())
    {
        err |= Pair(&prop, "description", description.c_str());
        VERIFY_CBOR(err);
    }
    if (readOnly)
    {
        err |= Pair(&prop, "readOnly", true);
        VERIFY_CBOR(err);
    }
    err |= GetJsonType(&prop, sig.c_str(), min, max, def);
    VERIFY_CBOR(err);
    err |= cbor_encoder_close_container(cbor, &prop);
    VERIFY_CBOR(err);
exit:
    return err;
}

static int64_t Properties(CborEncoder *cbor, const char *ajSoftwareVersion,
        const ajn::InterfaceDescription::Member *member, const char *memberSignature, size_t &argN,
        bool readOnly)
{
    int64_t err = CborNoError;
    size_t numArgs = CountCompleteTypes(memberSignature);
    const char *signature = memberSignature;
    const char *argSignature = signature;
    const char *argNames = member->argNames.c_str();
    for (size_t k = 0; k < argN; ++k)
    {
        NextArgName(argNames, k);
    }
    for (size_t k = 0; k < numArgs; ++k, ++argN)
    {
        ParseCompleteType(signature);
        qcc::String sig(argSignature, signature - argSignature);
        argSignature = signature;
        std::string argName = NextArgName(argNames, argN);
        if (strcmp(ajSoftwareVersion, "v16.10.00") >= 0)
        {
            member->GetArgAnnotation(argName.c_str(), "org.alljoyn.Bus.Type.Name", sig);
        }
        std::string propName = GetPropName(member, argName);
        qcc::String min, max, def;
        member->GetAnnotation("org.alljoyn.Bus.Type.Min", min);
        member->GetAnnotation("org.alljoyn.Bus.Type.Max", max);
        member->GetAnnotation("org.alljoyn.Bus.Type.Default", def);
        err |= Property(cbor, propName, member->description, readOnly, sig, min, max, def);
        VERIFY_CBOR(err);
    }
exit:
    return err;
}

static int64_t Definitions(CborEncoder *cbor, ajn::BusAttachment *bus,
        const char *ajSoftwareVersion)
{
    size_t numIfaces = 0;
    const ajn::InterfaceDescription **ifaces = NULL;
    const ajn::InterfaceDescription::Property **props = NULL;
    const ajn::InterfaceDescription::Member **members = NULL;
    qcc::String *names = NULL;
    qcc::String *values = NULL;
    int64_t err = CborNoError;
    CborEncoder definitions;
    err |= cbor_encode_text_stringz(cbor, "definitions");
    VERIFY_CBOR(err);
    err |= cbor_encoder_create_map(cbor, &definitions, CborIndefiniteLength);
    VERIFY_CBOR(err);

    if (bus)
    {
        numIfaces = bus->GetInterfaces(NULL, 0);
        ifaces = new const ajn::InterfaceDescription*[numIfaces];
        bus->GetInterfaces(ifaces, numIfaces);
        for (size_t i = 0; i < numIfaces; ++i)
        {
            const char *ifaceName = ifaces[i]->GetName();
            if (!TranslateInterface(ifaceName))
            {
                continue;
            }
            size_t numProps = ifaces[i]->GetProperties(NULL, 0);
            props = new const ajn::InterfaceDescription::Property*[numProps];
            ifaces[i]->GetProperties(props, numProps);
            static const char *emitsChangedValues[] =
                    { "const", "false", "true", "invalidates", NULL };
            for (const char **emitsChanged = emitsChangedValues; *emitsChanged; ++emitsChanged)
            {
                bool hasProps = false;
                CborEncoder definition;
                CborEncoder properties;
                uint8_t access = NONE;
                std::string rt = GetResourceTypeName(ifaces[i], *emitsChanged);
                for (size_t j = 0; j < numProps; ++j)
                {
                    qcc::String value = (props[j]->name == "Version") ? "const" : "false";
                    props[j]->GetAnnotation(::ajn::org::freedesktop::DBus::AnnotateEmitsChanged,
                            value);
                    if (value != *emitsChanged)
                    {
                        continue;
                    }
                    if (!hasProps)
                    {
                        err |= cbor_encode_text_stringz(&definitions, rt.c_str());
                        VERIFY_CBOR(err);
                        err |= cbor_encoder_create_map(&definitions, &definition,
                                CborIndefiniteLength);
                        VERIFY_CBOR(err);
                        err |= Pair(&definition, "type", "object");
                        VERIFY_CBOR(err);
                        err |= cbor_encode_text_stringz(&definition, "properties");
                        VERIFY_CBOR(err);
                        err |= cbor_encoder_create_map(&definition, &properties,
                                CborIndefiniteLength);
                        VERIFY_CBOR(err);
                        hasProps = true;
                    }
                    std::string propName = GetPropName(ifaces[i], props[j]->name);
                    /*
                     * Annotations prior to v16.10.00 are not guaranteed to
                     * appear in the order they were specified, so are
                     * unreliable.
                     */
                    qcc::String signature = props[j]->signature;
                    if (strcmp(ajSoftwareVersion, "v16.10.00") >= 0)
                    {
                        props[j]->GetAnnotation("org.alljoyn.Bus.Type.Name", signature);
                    }
                    qcc::String min, max, def;
                    props[j]->GetAnnotation("org.alljoyn.Bus.Type.Min", min);
                    props[j]->GetAnnotation("org.alljoyn.Bus.Type.Max", max);
                    props[j]->GetAnnotation("org.alljoyn.Bus.Type.Default", def);
                    err  |= Property(&properties, propName, props[j]->description,
                            (props[j]->access == ajn::PROP_ACCESS_READ), signature, min, max, def);
                    VERIFY_CBOR(err);
                    switch (props[j]->access)
                    {
                        case ajn::PROP_ACCESS_RW:
                        case ajn::PROP_ACCESS_WRITE:
                            access |= READWRITE;
                            break;
                        case ajn::PROP_ACCESS_READ:
                            access |= READ;
                            break;
                    }
                }
                if (hasProps)
                {
                    CborEncoder rtMap;
                    err |= cbor_encode_text_stringz(&properties, "rt");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_create_map(&properties, &rtMap, CborIndefiniteLength);
                    VERIFY_CBOR(err);
                    err |= cbor_encode_text_stringz(&rtMap, "readOnly");
                    VERIFY_CBOR(err);
                    err |= cbor_encode_boolean(&rtMap, true);
                    VERIFY_CBOR(err);
                    err |= Pair(&rtMap, "type", "array");
                    VERIFY_CBOR(err);
                    CborEncoder def;
                    err |= cbor_encode_text_stringz(&rtMap, "default");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_create_array(&rtMap, &def, 1);
                    VERIFY_CBOR(err);
                    err |= cbor_encode_text_stringz(&def, rt.c_str());
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&rtMap, &def);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&properties, &rtMap);
                    VERIFY_CBOR(err);
                    CborEncoder ifMap;
                    err |= cbor_encode_text_stringz(&properties, "if");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_create_map(&properties, &ifMap, CborIndefiniteLength);
                    VERIFY_CBOR(err);
                    err |= cbor_encode_text_stringz(&ifMap, "readOnly");
                    VERIFY_CBOR(err);
                    err |= cbor_encode_boolean(&ifMap, true);
                    VERIFY_CBOR(err);
                    err |= Pair(&ifMap, "type", "array");
                    VERIFY_CBOR(err);
                    CborEncoder items;
                    err |= cbor_encode_text_stringz(&ifMap, "items");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_create_map(&ifMap, &items, 2);
                    VERIFY_CBOR(err);
                    err |= Pair(&items, "type", "string");
                    VERIFY_CBOR(err);
                    CborEncoder enumArr;
                    err |= cbor_encode_text_stringz(&items, "enum");
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_create_array(&items, &enumArr, CborIndefiniteLength);
                    VERIFY_CBOR(err);
                    err |= cbor_encode_text_stringz(&enumArr, "oic.if.baseline");
                    VERIFY_CBOR(err);
                    if (access & READ)
                    {
                        err |= cbor_encode_text_stringz(&enumArr, "oic.if.r");
                        VERIFY_CBOR(err);
                    }
                    if (access & READWRITE)
                    {
                        err |= cbor_encode_text_stringz(&enumArr, "oic.if.rw");
                        VERIFY_CBOR(err);
                    }
                    err |= cbor_encoder_close_container(&items, &enumArr);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&ifMap, &items);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&properties, &ifMap);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&definition, &properties);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&definitions, &definition);
                    VERIFY_CBOR(err);
                }
            }
            delete[] props;
            props = NULL;
            size_t numMembers = ifaces[i]->GetMembers(NULL, 0);
            members = new const ajn::InterfaceDescription::Member*[numMembers];
            ifaces[i]->GetMembers(members, numMembers);
            for (size_t j = 0; j < numMembers; ++j)
            {
                std::string rt = GetResourceTypeName(ifaces[i], members[j]->name);
                CborEncoder definition;
                err |= cbor_encode_text_stringz(&definitions, rt.c_str());
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_map(&definitions, &definition, CborIndefiniteLength);
                VERIFY_CBOR(err);
                err |= Pair(&definition, "type", "object");
                VERIFY_CBOR(err);
                CborEncoder properties;
                err |= cbor_encode_text_stringz(&definition, "properties");
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_map(&definition, &properties, CborIndefiniteLength);
                VERIFY_CBOR(err);
                std::string propName = GetPropName(members[j], "validity");
                CborEncoder prop;
                err |= cbor_encode_text_stringz(&properties, propName.c_str());
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_map(&properties, &prop, CborIndefiniteLength);
                VERIFY_CBOR(err);
                if (members[j]->memberType == ajn::MESSAGE_SIGNAL)
                {
                    err |= cbor_encode_text_stringz(&prop, "readOnly");
                    VERIFY_CBOR(err);
                    err |= cbor_encode_boolean(&prop, true);
                    VERIFY_CBOR(err);
                }
                err |= Pair(&prop, "type", "boolean");
                VERIFY_CBOR(err);
                err |= cbor_encoder_close_container(&properties, &prop);
                VERIFY_CBOR(err);
                size_t argN = 0;
                err |= Properties(&properties, ajSoftwareVersion, members[j],
                        members[j]->signature.c_str(), argN,
                        (members[j]->memberType == ajn::MESSAGE_SIGNAL));
                VERIFY_CBOR(err);
                err |= Properties(&properties, ajSoftwareVersion, members[j],
                        members[j]->returnSignature.c_str(), argN, true);
                VERIFY_CBOR(err);
                err |= cbor_encode_text_stringz(&properties, "rt");
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_map(&properties, &prop, CborIndefiniteLength);
                VERIFY_CBOR(err);
                err |= cbor_encode_text_stringz(&prop, "readOnly");
                VERIFY_CBOR(err);
                err |= cbor_encode_boolean(&prop, true);
                VERIFY_CBOR(err);
                err |= Pair(&prop, "type", "array");
                VERIFY_CBOR(err);
                CborEncoder def;
                err |= cbor_encode_text_stringz(&prop, "default");
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_array(&prop, &def, 1);
                VERIFY_CBOR(err);
                err |= cbor_encode_text_stringz(&def, rt.c_str());
                VERIFY_CBOR(err);
                err |= cbor_encoder_close_container(&prop, &def);
                VERIFY_CBOR(err);
                err |= cbor_encoder_close_container(&properties, &prop);
                VERIFY_CBOR(err);
                err |= cbor_encode_text_stringz(&properties, "if");
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_map(&properties, &prop, CborIndefiniteLength);
                VERIFY_CBOR(err);
                err |= cbor_encode_text_stringz(&prop, "readOnly");
                VERIFY_CBOR(err);
                err |= cbor_encode_boolean(&prop, true);
                VERIFY_CBOR(err);
                err |= Pair(&prop, "type", "array");
                VERIFY_CBOR(err);
                CborEncoder items;
                err |= cbor_encode_text_stringz(&prop, "items");
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_map(&prop, &items, 2);
                VERIFY_CBOR(err);
                err |= Pair(&items, "type", "string");
                VERIFY_CBOR(err);
                CborEncoder enumArr;
                err |= cbor_encode_text_stringz(&items, "enum");
                VERIFY_CBOR(err);
                err |= cbor_encoder_create_array(&items, &enumArr, CborIndefiniteLength);
                VERIFY_CBOR(err);
                err |= cbor_encode_text_stringz(&enumArr, "oic.if.baseline");
                VERIFY_CBOR(err);
                if (members[j]->memberType == ajn::MESSAGE_SIGNAL)
                {
                    err |= cbor_encode_text_stringz(&enumArr, "oic.if.r");
                    VERIFY_CBOR(err);
                }
                else
                {
                    err |= cbor_encode_text_stringz(&enumArr, "oic.if.rw");
                    VERIFY_CBOR(err);
                }
                err |= cbor_encoder_close_container(&items, &enumArr);
                VERIFY_CBOR(err);
                err |= cbor_encoder_close_container(&prop, &items);
                VERIFY_CBOR(err);
                err |= cbor_encoder_close_container(&properties, &prop);
                VERIFY_CBOR(err);
                err |= cbor_encoder_close_container(&definition, &properties);
                VERIFY_CBOR(err);
                err |= cbor_encoder_close_container(&definitions, &definition);
                VERIFY_CBOR(err);
            }
            delete[] members;
            members = NULL;
            if (strcmp(ajSoftwareVersion, "v16.10.00") >= 0)
            {
                size_t numAnnotations = ifaces[i]->GetAnnotations();
                names = new qcc::String[numAnnotations];
                values = new qcc::String[numAnnotations];
                ifaces[i]->GetAnnotations(names, values, numAnnotations);
                CborEncoder definition;
                CborEncoder properties;
                qcc::String lastName;
                for (size_t j = 0; j < numAnnotations; ++j)
                {
                    if (names[j].find("org.alljoyn.Bus.Struct.") == 0)
                    {
                        size_t pos = sizeof("org.alljoyn.Bus.Struct.") - 1;
                        size_t dot = names[j].find(".", pos);
                        if (dot == qcc::String::npos)
                        {
                            continue;
                        }
                        qcc::String structName = names[j].substr(pos, dot - pos);
                        pos = dot + sizeof(".Field.") - 1;
                        dot = names[j].find(".", pos);
                        if (dot == qcc::String::npos)
                        {
                            continue;
                        }
                        qcc::String fieldName = names[j].substr(pos, dot - pos);
                        if (structName != lastName)
                        {
                            if (!lastName.empty())
                            {
                                err |= cbor_encoder_close_container(&definition, &properties);
                                VERIFY_CBOR(err);
                                err |= cbor_encoder_close_container(&definitions, &definition);
                                VERIFY_CBOR(err);
                            }
                            err |= cbor_encode_text_stringz(&definitions, structName.c_str());
                            VERIFY_CBOR(err);
                            err |= cbor_encoder_create_map(&definitions, &definition,
                                    CborIndefiniteLength);
                            VERIFY_CBOR(err);
                            err |= Pair(&definition, "type", "object");
                            VERIFY_CBOR(err);
                            err |= cbor_encode_text_stringz(&definition, "properties");
                            VERIFY_CBOR(err);
                            err |= cbor_encoder_create_map(&definition, &properties,
                                    CborIndefiniteLength);
                            VERIFY_CBOR(err);
                            lastName = structName;
                        }
                        CborEncoder prop;
                        err |= cbor_encode_text_stringz(&properties, fieldName.c_str());
                        VERIFY_CBOR(err);
                        err |= cbor_encoder_create_map(&properties, &prop, CborIndefiniteLength);
                        VERIFY_CBOR(err);
                        qcc::String min, max, def;
                        err |= GetJsonType(&prop, values[j].c_str(), min, max, def);
                        VERIFY_CBOR(err);
                        err |= cbor_encoder_close_container(&properties, &prop);
                        VERIFY_CBOR(err);
                    }
                }
                if (!lastName.empty())
                {
                    err |= cbor_encoder_close_container(&definition, &properties);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&definitions, &definition);
                    VERIFY_CBOR(err);
                }
                lastName.clear();
                for (size_t j = 0; j < numAnnotations; ++j)
                {
                    if (names[j].find("org.alljoyn.Bus.Dict.") == 0)
                    {
                        size_t pos = sizeof("org.alljoyn.Bus.Dict.") - 1;
                        size_t dot = names[j].find(".", pos);
                        if (dot == qcc::String::npos)
                        {
                            continue;
                        }
                        qcc::String dictName = names[j].substr(pos, dot - pos);
                        if (dictName != lastName)
                        {
                            err |= cbor_encode_text_stringz(&definitions, dictName.c_str());
                            VERIFY_CBOR(err);
                            err |= cbor_encoder_create_map(&definitions, &definition, 1);
                            VERIFY_CBOR(err);
                            err |= Pair(&definition, "type", "object");
                            VERIFY_CBOR(err);
                            err |= cbor_encoder_close_container(&definitions, &definition);
                            VERIFY_CBOR(err);
                            lastName = dictName;
                        }
                    }
                }
                lastName.clear();
                CborEncoder prop;
                for (size_t j = 0; j < numAnnotations; ++j)
                {
                    if (names[j].find("org.alljoyn.Bus.Enum.") == 0)
                    {
                        size_t pos = sizeof("org.alljoyn.Bus.Enum.") - 1;
                        size_t dot = names[j].find_first_of('.', pos);
                        if (dot == qcc::String::npos)
                        {
                            continue;
                        }
                        qcc::String enumName = names[j].substr(pos, dot - pos);
                        dot = names[j].find_last_of_std('.');
                        qcc::String enumValue = names[j].substr(dot + 1);
                        if (enumName != lastName)
                        {
                            if (!lastName.empty())
                            {
                                err |= cbor_encoder_close_container(&definitions, &prop);
                                VERIFY_CBOR(err);
                                err |= cbor_encoder_close_container(&definitions, &definition);
                                VERIFY_CBOR(err);
                            }
                            err |= cbor_encode_text_stringz(&definitions, enumName.c_str());
                            VERIFY_CBOR(err);
                            err |= cbor_encoder_create_map(&definitions, &definition,
                                    CborIndefiniteLength);
                            VERIFY_CBOR(err);
                            err |= cbor_encode_text_stringz(&definitions, "oneOf");
                            VERIFY_CBOR(err);
                            err |= cbor_encoder_create_array(&definitions, &prop,
                                    CborIndefiniteLength);
                            VERIFY_CBOR(err);
                            lastName = enumName;
                        }
                        CborEncoder enumMap;
                        err |= cbor_encoder_create_map(&prop, &enumMap, CborIndefiniteLength);
                        VERIFY_CBOR(err);
                        CborEncoder enumArr;
                        err |= cbor_encode_text_stringz(&enumMap, "enum");
                        VERIFY_CBOR(err);
                        err |= cbor_encoder_create_array(&enumMap, &enumArr, CborIndefiniteLength);
                        VERIFY_CBOR(err);
                        err |= cbor_encode_text_stringz(&enumArr, values[j].c_str());
                        VERIFY_CBOR(err);
                        err |= cbor_encoder_close_container(&enumMap, &enumArr);
                        VERIFY_CBOR(err);
                        err |= cbor_encode_text_stringz(&enumMap, "title");
                        VERIFY_CBOR(err);
                        err |= cbor_encode_text_stringz(&enumMap, enumValue.c_str());
                        VERIFY_CBOR(err);
                        err |= cbor_encoder_close_container(&prop, &enumMap);
                        VERIFY_CBOR(err);
                    }
                }
                if (!lastName.empty())
                {
                    err |= cbor_encoder_close_container(&definitions, &prop);
                    VERIFY_CBOR(err);
                    err |= cbor_encoder_close_container(&definitions, &definition);
                    VERIFY_CBOR(err);
                }
            }
        }
    }

    err |= cbor_encoder_close_container(cbor, &definitions);
    VERIFY_CBOR(err);

exit:
    delete[] names;
    delete[] values;
    delete[] members;
    delete[] props;
    delete[] ifaces;
    return err;
}

CborError Introspect(ajn::BusAttachment *bus, const char *ajSoftwareVersion, const char *title,
        const char *version, uint8_t *out, size_t *outSize)
{
    int64_t err = CborNoError;
    CborEncoder encoder;
    cbor_encoder_init(&encoder, out, *outSize, 0);
    CborEncoder map;
    err |= cbor_encoder_create_map(&encoder, &map, CborIndefiniteLength);
    VERIFY_CBOR(err);
    err |= Pair(&map, "swagger", "2.0");
    VERIFY_CBOR(err);
    err |= Info(&map, title, version);
    VERIFY_CBOR(err);
    err |= Paths(&map);
    VERIFY_CBOR(err);
    err |= Definitions(&map, bus, ajSoftwareVersion);
    VERIFY_CBOR(err);
    err |= cbor_encoder_close_container(&encoder, &map);
    VERIFY_CBOR(err);

exit:
    if (err == CborErrorOutOfMemory)
    {
        *outSize += cbor_encoder_get_extra_bytes_needed(&encoder);
    }
    else if (err == CborNoError)
    {
        *outSize = cbor_encoder_get_buffer_size(&encoder, out);
    }
    return (CborError)err;
}

static bool SetPropertiesSchema(OCRepPayload *property, OCRepPayloadPropType type,
        OCRepPayload *obj)
{
    OCRepPayload *child = NULL;
    bool success;

    switch (type)
    {
        case OCREP_PROP_NULL:
            success = false;
            break;
        case OCREP_PROP_INT:
            success = OCRepPayloadSetPropString(property, "type", "integer");
            break;
        case OCREP_PROP_DOUBLE:
            success = OCRepPayloadSetPropString(property, "type", "number");
            break;
        case OCREP_PROP_BOOL:
            success = OCRepPayloadSetPropString(property, "type", "boolean");
            break;
        case OCREP_PROP_STRING:
            success = OCRepPayloadSetPropString(property, "type", "string");
            break;
        case OCREP_PROP_BYTE_STRING:
            child = OCRepPayloadCreate();
            success = child &&
                    OCRepPayloadSetPropString(child, "binaryEncoding", "base64") &&
                    OCRepPayloadSetPropObjectAsOwner(property, "media", child) &&
                    OCRepPayloadSetPropString(property, "type", "string");
            if (success)
            {
                child = NULL;
            }
            break;
        case OCREP_PROP_OBJECT:
            child = OCRepPayloadCreate();
            success = child &&
                    SetPropertiesSchema(child, obj) &&
                    OCRepPayloadSetPropObjectAsOwner(property, "properties", child) &&
                    OCRepPayloadSetPropString(property, "type", "object");
            if (success)
            {
                child = NULL;
            }
            break;
        case OCREP_PROP_ARRAY:
            success = false;
            break;
    }
    if (!success)
    {
        goto exit;
    }
    success = true;

exit:
    OCRepPayloadDestroy(child);
    return success;
}

static bool SetPropertiesSchema(OCRepPayload *parent, OCRepPayload *obj)
{
    OCRepPayload *property = NULL;
    OCRepPayload *child = NULL;
    OCRepPayload *array = NULL;
    bool success;

    if (!obj)
    {
        success = true;
        goto exit;
    }
    for (OCRepPayloadValue *value = obj->values; value; value = value->next)
    {
        property = OCRepPayloadCreate();
        if (!property)
        {
            LOG(LOG_ERR, "Failed to create payload");
            success = false;
            goto exit;
        }
        switch (value->type)
        {
            case OCREP_PROP_NULL:
                success = false;
                break;
            case OCREP_PROP_INT:
                success = OCRepPayloadSetPropString(property, "type", "integer");
                break;
            case OCREP_PROP_DOUBLE:
                success = OCRepPayloadSetPropString(property, "type", "number");
                break;
            case OCREP_PROP_BOOL:
                success = OCRepPayloadSetPropString(property, "type", "boolean");
                break;
            case OCREP_PROP_STRING:
                success = OCRepPayloadSetPropString(property, "type", "string");
                break;
            case OCREP_PROP_BYTE_STRING:
                child = OCRepPayloadCreate();
                success = child &&
                        OCRepPayloadSetPropString(child, "binaryEncoding", "base64") &&
                        OCRepPayloadSetPropObjectAsOwner(property, "media", child) &&
                        OCRepPayloadSetPropString(property, "type", "string");
                if (success)
                {
                    child = NULL;
                }
                break;
            case OCREP_PROP_OBJECT:
                child = OCRepPayloadCreate();
                success = child &&
                        SetPropertiesSchema(child, value->obj) &&
                        OCRepPayloadSetPropObjectAsOwner(property, "properties", child) &&
                        OCRepPayloadSetPropString(property, "type", "object");
                if (success)
                {
                    child = NULL;
                }
                break;
            case OCREP_PROP_ARRAY:
                success = true;
                array = property;
                for (size_t i = 0; success && (i < MAX_REP_ARRAY_DEPTH) && value->arr.dimensions[i];
                     ++i)
                {
                    child = OCRepPayloadCreate();
                    success = child &&
                            OCRepPayloadSetPropObjectAsOwner(array, "items", child) &&
                            OCRepPayloadSetPropString(array, "type", "array");
                    if (success)
                    {
                        array = child;
                        child = NULL;
                    }
                }
                if (success)
                {
                    success = SetPropertiesSchema(array, value->arr.type, value->arr.objArray[0]);
                }
                break;
        }
        if (!success)
        {
            goto exit;
        }

        if (!OCRepPayloadSetPropObjectAsOwner(parent, value->name, property))
        {
            success = false;
            goto exit;
        }
        property = NULL;
    }
    success = true;

exit:
    OCRepPayloadDestroy(child);
    OCRepPayloadDestroy(property);
    return success;
}

OCRepPayload *IntrospectDefinition(OCRepPayload *payload, std::string resourceType,
        std::vector<std::string> &interfaces)
{
    OCRepPayload *definition = NULL;
    OCRepPayload *properties = NULL;
    OCRepPayload *rt = NULL;
    size_t rtsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    size_t dimTotal;
    char **rts = NULL;
    std::vector<std::string>::iterator ifIt;
    OCRepPayload *itf = NULL;
    OCRepPayload *items = NULL;
    size_t itfsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    char **itfs = NULL;
    definition = OCRepPayloadCreate();
    if (!definition)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    if (!OCRepPayloadSetPropString(definition, "type", "object"))
    {
        goto error;
    }
    properties = OCRepPayloadCreate();
    if (!properties)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    rt = OCRepPayloadCreate();
    if (!rt)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    if (!OCRepPayloadSetPropBool(rt, "readOnly", true) ||
            !OCRepPayloadSetPropString(rt, "type", "array"))
    {
        goto error;
    }
    rtsDim[0] = 1;
    dimTotal = calcDimTotal(rtsDim);
    rts = (char**) OICCalloc(dimTotal, sizeof(char*));
    if (!rts)
    {
        LOG(LOG_ERR, "Failed to allocate string array");
        goto error;
    }
    rts[0] = OICStrdup(resourceType.c_str());
    if (!OCRepPayloadSetStringArrayAsOwner(rt, "default", rts, rtsDim))
    {
        goto error;
    }
    rts = NULL;
    if (!OCRepPayloadSetPropObjectAsOwner(properties, "rt", rt))
    {
        goto error;
    }
    rt = NULL;
    itf = OCRepPayloadCreate();
    if (!itf)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    if (!OCRepPayloadSetPropBool(itf, "readOnly", true) ||
            !OCRepPayloadSetPropString(itf, "type", "array"))
    {
        goto error;
    }
    items = OCRepPayloadCreate();
    if (!items)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    if (!OCRepPayloadSetPropString(items, "type", "string"))
    {
        goto error;
    }
    itfsDim[0] = interfaces.size();
    dimTotal = calcDimTotal(itfsDim);
    itfs = (char **) OICCalloc(dimTotal, sizeof(char *));
    if (!itfs)
    {
        LOG(LOG_ERR, "Failed to allocate string array");
        goto error;
    }
    ifIt = interfaces.begin();
    for (size_t i = 0; i < dimTotal; ++i, ++ifIt)
    {
        itfs[i] = OICStrdup(ifIt->c_str());
    }
    if (!OCRepPayloadSetStringArrayAsOwner(items, "enum", itfs, itfsDim))
    {
        goto error;
    }
    itfs = NULL;
    if (!OCRepPayloadSetPropObjectAsOwner(itf, "items", items))
    {
        goto error;
    }
    items = NULL;
    if (!OCRepPayloadSetPropObjectAsOwner(properties, "if", itf))
    {
        goto error;
    }
    itf = NULL;
    if (!SetPropertiesSchema(properties, payload))
    {
        goto error;
    }
    if (!OCRepPayloadSetPropObjectAsOwner(definition, "properties", properties))
    {
        goto error;
    }
    properties = NULL;
    return definition;

error:
    OCRepPayloadDestroy(items);
    OCRepPayloadDestroy(itf);
    if (itfs)
    {
        dimTotal = calcDimTotal(itfsDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(itfs[i]);
        }
        OICFree(itfs);
    }
    if (rts)
    {
        dimTotal = calcDimTotal(rtsDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(rts[i]);
        }
        OICFree(rts);
    }
    OCRepPayloadDestroy(rt);
    OCRepPayloadDestroy(properties);
    OCRepPayloadDestroy(definition);
    return NULL;
}

OCRepPayload *IntrospectPath(std::vector<std::string> &resourceTypes,
        std::vector<std::string> &interfaces)
{
    OCRepPayload *path = NULL;
    OCRepPayload *method = NULL;
    size_t dimTotal;
    size_t parametersDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **parameters = NULL;
    size_t itfsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    char **itfs = NULL;
    OCRepPayload *responses = NULL;
    OCRepPayload *code = NULL;
    OCRepPayload *schema = NULL;
    size_t oneOfDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **oneOf = NULL;
    std::string ref;
    path = OCRepPayloadCreate();
    if (!path)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    /* oic.if.baseline is mandatory and it supports post */
    method = OCRepPayloadCreate();
    if (!method)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    parametersDim[0] = 2;
    dimTotal = calcDimTotal(parametersDim);
    parameters = (OCRepPayload **) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    if (!parameters)
    {
        LOG(LOG_ERR, "Failed to allocate object array");
        goto error;
    }
    parameters[0] = OCRepPayloadCreate();
    if (!parameters[0])
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    if (!OCRepPayloadSetPropString(parameters[0], "name", "if") ||
            !OCRepPayloadSetPropString(parameters[0], "in", "query") ||
            !OCRepPayloadSetPropString(parameters[0], "type", "string"))
    {
        goto error;
    }
    itfs = (char **) OICCalloc(interfaces.size(), sizeof(char *));
    if (!itfs)
    {
        LOG(LOG_ERR, "Failed to allocate string array");
        goto error;
    }
    itfsDim[0] = 0;
    for (size_t i = 0; i < interfaces.size(); ++i)
    {
        /* Filter out read-only interfaces from post method */
        std::string &itf = interfaces[i];
        if (itf == "oic.if.ll" || itf == "oic.if.r" || itf == "oic.if.s")
        {
            continue;
        }
        itfs[itfsDim[0]++] = OICStrdup(itf.c_str());
    }
    if (!OCRepPayloadSetStringArrayAsOwner(parameters[0], "enum", itfs, itfsDim))
    {
        goto error;
    }
    itfs = NULL;
    parameters[1] = OCRepPayloadCreate();
    if (!parameters[1])
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    if (!OCRepPayloadSetPropString(parameters[1], "name", "body") ||
            !OCRepPayloadSetPropString(parameters[1], "in", "body"))
    {
        goto error;
    }
    schema = OCRepPayloadCreate();
    if (!schema)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    oneOfDim[0] = resourceTypes.size();
    dimTotal = calcDimTotal(oneOfDim);
    oneOf = (OCRepPayload **) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    if (!oneOf)
    {
        LOG(LOG_ERR, "Failed to allocate object array");
        goto error;
    }
    for (size_t i = 0; i < dimTotal; ++i)
    {
        oneOf[i] = OCRepPayloadCreate();
        if (!oneOf[i])
        {
            LOG(LOG_ERR, "Failed to create payload");
            goto error;
        }
        ref = std::string("#/definitions/") + resourceTypes[i];
        if (!OCRepPayloadSetPropString(oneOf[i], "$ref", ref.c_str()))
        {
            goto error;
        }
    }
    if (!OCRepPayloadSetPropObjectArrayAsOwner(schema, "oneOf", oneOf, oneOfDim))
    {
        goto error;
    }
    oneOf = NULL;
    /* schema will be re-used in "responses" (so no ...AsOwner here) */
    if (!OCRepPayloadSetPropObject(parameters[1], "schema", schema))
    {
        goto error;
    }
    /* parameters will be re-used in "get" (so no ...AsOwner here) */
    if (!OCRepPayloadSetPropObjectArray(method, "parameters", (const OCRepPayload **)parameters,
            parametersDim))
    {
        goto error;
    }
    responses = OCRepPayloadCreate();
    if (!responses)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    code = OCRepPayloadCreate();
    if (!code)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    if (!OCRepPayloadSetPropString(code, "description", ""))
    {
        goto error;
    }
    if (!OCRepPayloadSetPropObjectAsOwner(code, "schema", schema))
    {
        goto error;
    }
    schema = NULL;
    if (!OCRepPayloadSetPropObjectAsOwner(responses, "200", code))
    {
        goto error;
    }
    code = NULL;
    /* responses will be re-used in "get" (so no ...AsOwner here) */
    if (!OCRepPayloadSetPropObject(method, "responses", responses))
    {
        goto error;
    }
    if (!OCRepPayloadSetPropObjectAsOwner(path, "post", method))
    {
        goto error;
    }
    method = NULL;
    method = OCRepPayloadCreate();
    if (!method)
    {
        LOG(LOG_ERR, "Failed to create payload");
        goto error;
    }
    itfs = (char **) OICCalloc(interfaces.size(), sizeof(char *));
    if (!itfs)
    {
        LOG(LOG_ERR, "Failed to allocate string array");
        goto error;
    }
    itfsDim[0] = 0;
    for (size_t i = 0; i < interfaces.size(); ++i)
    {
        /* All interfaces support get method */
        itfs[itfsDim[0]++] = OICStrdup(interfaces[i].c_str());
    }
    if (!OCRepPayloadSetStringArrayAsOwner(parameters[0], "enum", itfs, itfsDim))
    {
        goto error;
    }
    itfs = NULL;
    parametersDim[0] = 1; /* only use "if" parameter */
    if (!OCRepPayloadSetPropObjectArrayAsOwner(method, "parameters", parameters, parametersDim))
    {
        goto error;
    }
    parameters = NULL;
    if (!OCRepPayloadSetPropObject(method, "responses", responses))
    {
        goto error;
    }
    responses = NULL;
    if (!OCRepPayloadSetPropObjectAsOwner(path, "get", method))
    {
        goto error;
    }
    method = NULL;
    return path;

error:
    if (oneOf)
    {
        dimTotal = calcDimTotal(oneOfDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OCRepPayloadDestroy(oneOf[i]);
        }
        OICFree(oneOf);
    }
    OCRepPayloadDestroy(schema);
    OCRepPayloadDestroy(code);
    if (parameters)
    {
        dimTotal = calcDimTotal(parametersDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OCRepPayloadDestroy(parameters[i]);
        }
        OICFree(parameters);
    }
    if (itfs)
    {
        dimTotal = calcDimTotal(itfsDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(itfs[i]);
        }
        OICFree(itfs);
    }
    OCRepPayloadDestroy(responses);
    OCRepPayloadDestroy(method);
    OCRepPayloadDestroy(path);
    return NULL;
}
