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

#include "Name.h"
#include "Payload.h"
#include "Signature.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "cJSON.h"

static OCStackResult Info(std::ostream &os, const char *title, const char *version)
{
    os << "\"info\":{"
       << "\"title\":\"" << title << "\","
       << "\"version\":\"" << version << "\""
       << "}";
    return OC_STACK_OK;
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

static OCStackResult QueryParameters(std::ostream &os, OCResourceHandle h)
{
    os << "{"
       << "\"name\":\"if\","
       << "\"in\":\"query\","
       << "\"type\":\"string\","
       << "\"enum\":[";
    uint8_t ni;
    OCStackResult result = OCGetNumberOfResourceInterfaces(h, &ni);
    if (result != OC_STACK_OK)
    {
        return result;
    }
    for (uint8_t j = 0; j < ni; ++j)
    {
        os << (j ? ",\"" : "\"") << OCGetResourceInterfaceName(h, j) << "\"";
    }
    os << "]" /* enum */
       << "}";
    return OC_STACK_OK;
}

static OCStackResult Schema(std::ostream &os, OCResourceHandle h)
{
    os << "\"schema\":{"
       << "\"oneOf\":[";
    uint8_t nrt;
    OCStackResult result = OCGetNumberOfResourceTypes(h, &nrt);
    if (result != OC_STACK_OK)
    {
        return result;
    }
    for (uint8_t j = 0; j < nrt; ++j)
    {
        os << (j ? ",{" : "{") << "\"$ref\":\"#/definitions/" << OCGetResourceTypeName(h, j) << "\"}";
    }
    os << "]" /* oneOf */
       << "}";
    return OC_STACK_OK;
}

static OCStackResult Paths(std::ostream &os)
{
    os << "\"paths\":{";
    uint8_t nr;
    OCStackResult result = OCGetNumberOfResources(&nr);
    if (result != OC_STACK_OK)
    {
        return result;
    }
    int comma = 0;
    for (uint8_t i = 0; i < nr; ++i)
    {
        OCResourceHandle h = OCGetResourceHandle(i);
        const char *uri = OCGetResourceUri(h);
        if (!strcmp(uri, OC_RSRVD_WELL_KNOWN_URI) ||
                !strcmp(uri, OC_RSRVD_DEVICE_URI) ||
                !strcmp(uri, OC_RSRVD_PLATFORM_URI) ||
                !strncmp(uri, "/oic/sec", 8))
        {
            // TODO skip unless there are vendor specific properties
            continue;
        }
        os << (comma++ ? ",\"" : "\"") << uri << "\":{";

        os << "\"get\":{";
        os << "\"parameters\":[";
        result = QueryParameters(os, h);
        if (result != OC_STACK_OK)
        {
            return result;
        }
        os << "],"; /* parameters */
        os << "\"responses\":{"
           << "\"200\":{"
           << "\"description\":\"\",";
        result = Schema(os, h);
        if (result != OC_STACK_OK)
        {
            return result;
        }
        os << "}" /* 200 */
           << "}" /* responses */
           << "}"; /* get */

        if (ImplementsPost(h))
        {
            os << ",\"post\":{";
            os << "\"parameters\":[";
            result = QueryParameters(os, h);
            if (result != OC_STACK_OK)
            {
                return result;
            }
            os << "," /* query */
               << "{"
               << "\"name\":\"body\","
               << "\"in\":\"body\",";
            result = Schema(os, h);
            if (result != OC_STACK_OK)
            {
                return result;
            }
            os << "}"
               << "]," /* parameters */
               << "\"responses\":{"
               << "\"200\":{"
               << "\"description\":\"\",";
            result = Schema(os, h);
            if (result != OC_STACK_OK)
            {
                return result;
            }
            os << "}" /* 200 */
               << "}" /* responses */
               << "}"; /* post */
        }

        os << "}"; /* path */
    }
    os << "}";
    return OC_STACK_OK;
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

static void GetJsonType(std::ostream &os, const char *ajType,
        const qcc::String &typeMin, const qcc::String &typeMax, const qcc::String &typeDefault)
{
    switch (ajType[0])
    {
        case ajn::ALLJOYN_BOOLEAN:
            os << "\"type\":\"boolean\"";
            if (!typeDefault.empty())
            {
                os << ",\"default\":" << typeDefault;
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
                    os << "\"type\":\"integer\",\"minimum\":" << min << ",\"maximum\":" << max;
                }
                else
                {
                    os << "\"type\":\"string\",\"format\":\"uint64\"";
                }
                if (!typeDefault.empty())
                {
                    os << ",\"default\":" << typeDefault;
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
                    os << "\"type\":\"integer\",\"minimum\":" << min << ",\"maximum\":" << max;
                }
                else
                {
                    os << "\"type\":\"string\",\"format\":\"int64\"";
                }
                if (!typeDefault.empty())
                {
                    os << ",\"default\":" << typeDefault;
                }
            }
            break;
        case ajn::ALLJOYN_DOUBLE:
            os << "\"type\":\"number\"";
            if (!typeDefault.empty())
            {
                os << ",\"default\":" << typeDefault;
            }
            break;
        case ajn::ALLJOYN_STRING:
        case ajn::ALLJOYN_OBJECT_PATH:
        case ajn::ALLJOYN_SIGNATURE:
            os << "\"type\":\"string\"";
            if (!typeDefault.empty())
            {
                os << ",\"default\":\"" << typeDefault << "\"";
            }
            break;
        case ajn::ALLJOYN_ARRAY:
            switch (ajType[1])
            {
                case ajn::ALLJOYN_BYTE:
                    os << "\"type\":\"string\",\"media\":{\"binaryEncoding\":\"base64\"}";
                    break;
                case ajn::ALLJOYN_VARIANT:
                    os << "\"type\":\"undefined\"";
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    os << "\"type\":\"object\"";
                    break;
                default:
                    os << "\"type\":\"array\","
                       << "\"items\":{";
                    qcc::String min, max, def;
                    GetJsonType(os, &ajType[1], min, max, def);
                    os << "}";
                    break;
            }
            break;
        case ajn::ALLJOYN_STRUCT_OPEN:
            os << "\"type\":\"array\"";
            break;
        case ajn::ALLJOYN_VARIANT:
            os << "\"anyOf\":["
               << "{\"type\":\"string\"},"
               << "{\"type\":\"integer\"},"
               << "{\"type\":\"number\"},"
               << "{\"type\":\"object\"},"
               << "{\"type\":\"array\"},"
               << "{\"type\":\"boolean\"}"
               << "]";
            break;
        case '[':
            {
                std::string def = ajType;
                def = def.substr(1, def.size() - 2);
                os << "\"$ref\":\"#/definitions/" << def << "\"";
            }
            break;
        default:
            os << "\"type\":\"undefined\"";
            break;
    }
}

static OCStackResult Property(std::ostream &os, std::string propName, qcc::String description,
        bool readOnly, qcc::String sig, qcc::String min, qcc::String max, qcc::String def)
{
    os << "\"" << propName << "\":{";
    if (!description.empty())
    {
        os << "\"description\":\"" << description << "\",";
    }
    if (readOnly)
    {
        os << "\"readOnly\":true,";
    }
    GetJsonType(os, sig.c_str(), min, max, def);
    os << "}";
    return OC_STACK_OK;
}

static OCStackResult Properties(std::ostream &os, const char *ajSoftwareVersion,
        const ajn::InterfaceDescription::Member *member, const char *memberSignature, size_t &argN,
        bool readOnly)
{
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
        os << (k ? "," : "");
        OCStackResult result = Property(os, propName, member->description, readOnly, sig, min, max,
                def);
        if (result != OC_STACK_OK)
        {
            return result;
        }
    }
    return OC_STACK_OK;
}

static OCStackResult Definitions(std::ostream &os, ajn::BusAttachment *bus,
        const char *ajSoftwareVersion)
{
    os << "\"definitions\":{";

    OCStackResult result = OC_STACK_OK;
    const ajn::InterfaceDescription **ifaces = NULL;
    const ajn::InterfaceDescription::Property **props = NULL;
    const ajn::InterfaceDescription::Member **members = NULL;
    qcc::String *names = NULL;
    qcc::String *values = NULL;

    size_t numIfaces = bus->GetInterfaces(NULL, 0);
    ifaces = new const ajn::InterfaceDescription*[numIfaces];
    bus->GetInterfaces(ifaces, numIfaces);
    int comma = 0;
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
        static const char *emitsChangedValues[] = { "const", "false", "true", "invalidates", NULL };
        for (const char **emitsChanged = emitsChangedValues; *emitsChanged; ++emitsChanged)
        {
            int fieldComma = 0;
            bool hasProps = false;
            uint8_t access = NONE;
            std::string rt = GetResourceTypeName(ifaces[i], *emitsChanged);
            for (size_t j = 0; j < numProps; ++j)
            {
                qcc::String value = (props[j]->name == "Version") ? "const" : "false";
                props[j]->GetAnnotation(::ajn::org::freedesktop::DBus::AnnotateEmitsChanged, value);
                if (value != *emitsChanged)
                {
                    continue;
                }
                if (!hasProps)
                {
                    os << (comma++ ? ",\"" : "\"") << rt << "\":{"
                       << "\"type\":\"object\","
                       << "\"properties\":{";
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
                os << (fieldComma++ ? "," : "");
                result = Property(os, propName, props[j]->description,
                        (props[j]->access == ajn::PROP_ACCESS_READ), signature, min, max, def);
                if (result != OC_STACK_OK)
                {
                    goto exit;
                }
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
                os << ",\"rt\":{"
                   << "\"readOnly\":true,"
                   << "\"type\":\"array\","
                   << "\"default\":[\"" << rt << "\"]"
                   << "}";
                os << ",\"if\":{"
                   << "\"readOnly\":true,"
                   << "\"type\":\"array\","
                   << "\"items\":{"
                   << "\"type\":\"string\","
                   << "\"enum\":["
                   << "\"oic.if.baseline\"";
                if (access & READ)
                {
                    os << ",\"oic.if.r\"";
                }
                if (access & READWRITE)
                {
                    os << ",\"oic.if.rw\"";
                }
                os << "]" /* enum */
                   << "}" /* items */
                   << "}"; /* if */
                os << "}" /* properties */
                   << "}";
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
            os << (comma++ ? ",\"" : "\"") << rt << "\":{"
               << "\"type\":\"object\","
               << "\"properties\":{";
            std::string propName = GetPropName(members[j], "validity");
            os << "\"" << propName << "\":{";
            if (members[j]->memberType == ajn::MESSAGE_SIGNAL)
            {
                os << "\"readOnly\":true,";
            }
            os << "\"type\":\"boolean\""
               << "},";
            size_t argN = 0;
            result = Properties(os, ajSoftwareVersion, members[j], members[j]->signature.c_str(),
                    argN, (members[j]->memberType == ajn::MESSAGE_SIGNAL));
            if (result != OC_STACK_OK)
            {
                goto exit;
            }
            os << (members[j]->signature.empty() ? "" : ",");
            result = Properties(os, ajSoftwareVersion, members[j],
                    members[j]->returnSignature.c_str(), argN, true);
            if (result != OC_STACK_OK)
            {
                goto exit;
            }
            os << (members[j]->returnSignature.empty() ? "" : ",");
            os << "\"rt\":{"
               << "\"readOnly\":true,"
               << "\"type\":\"array\","
               << "\"default\":[\"" << rt << "\"]"
               << "},";
            os << "\"if\":{"
               << "\"readOnly\":true,"
               << "\"type\":\"array\","
               << "\"items\":{"
               << "\"type\":\"string\","
               << "\"enum\":["
               << "\"oic.if.baseline\"";
            if (members[j]->memberType == ajn::MESSAGE_SIGNAL)
            {
                os << ",\"oic.if.r\"";
            }
            else
            {
                os << ",\"oic.if.rw\"";
            }
            os << "]" /* enum */
               << "}" /* items */
               << "}"; /* if */

            os << "}" /* properties */
               << "}";
        }
        delete[] members;
        members = NULL;
        if (strcmp(ajSoftwareVersion, "v16.10.00") >= 0)
        {
            size_t numAnnotations = ifaces[i]->GetAnnotations();
            names = new qcc::String[numAnnotations];
            values = new qcc::String[numAnnotations];
            ifaces[i]->GetAnnotations(names, values, numAnnotations);
            qcc::String lastName;
            int fieldComma = 0;
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
                            os << "}"
                               << "}";
                        }
                        os << (comma++ ? ",\"" : "\"") << structName << "\":{"
                           << "\"type\":\"object\","
                           << "\"properties\":{";
                        lastName = structName;
                        fieldComma = 0;
                    }
                    os << (fieldComma++ ? ",\"" : "\"") << fieldName << "\":{";
                    qcc::String min, max, def;
                    GetJsonType(os, values[j].c_str(), min, max, def);
                    os << "}";
                }
            }
            if (!lastName.empty())
            {
                os << "}"
                   << "}";
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
                        os << (comma++ ? ",\"" : "\"") << dictName << "\":{"
                           << "\"type\":\"object\""
                           << "}";
                        lastName = dictName;
                    }
                }
            }
            lastName.clear();
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
                            os << "]"
                               << "}";
                        }
                        os << (comma++ ? ",\"" : "\"") << enumName << "\":{"
                           << "\"oneOf\":[";
                        lastName = enumName;
                        fieldComma = 0;
                    }
                    os << (fieldComma++ ? "," : "") << "{"
                       << "\"enum\":[" << values[j] << "],"
                       << "\"title\":\"" << enumValue << "\""
                       << "}";
                }
            }
            if (!lastName.empty())
            {
                os << "]"
                   << "}";
            }
        }
    }

    result = OC_STACK_OK;

exit:
    delete[] names;
    delete[] values;
    delete[] members;
    delete[] props;
    delete[] ifaces;

    os << "}";
    return result;
}

OCStackResult Introspect(std::ostream &os, ajn::BusAttachment *bus, const char *ajSoftwareVersion,
        const char *title, const char *version)
{
    os << "{\"swagger\":\"2.0\",";
    OCStackResult result = Info(os, title, version);
    if (result != OC_STACK_OK)
    {
        return result;
    }
    os << ",";
    result = Paths(os);
    if (result != OC_STACK_OK)
    {
        return result;
    }
    os << ",";
    result = Definitions(os, bus, ajSoftwareVersion);
    if (result != OC_STACK_OK)
    {
        return result;
    }
    os << "}";
    return OC_STACK_OK;
}

/* Only single-dimension homogenous arrays are supported for now */
static int JsonArrayType(cJSON *json, size_t dim[MAX_REP_ARRAY_DEPTH])
{
    dim[0] = 0;
    cJSON *element = json->child;
    if (!element)
    {
        return cJSON_NULL;
    }
    int type = element->type;
    while (element)
    {
        if (element->type != type)
        {
            return 0;
        }
        ++dim[0];
        element = element->next;
    }
    return type;
}

static OCStackResult ParseJsonItem(OCRepPayload *outPayload, cJSON *json)
{
    bool success = true;
    while (success && json)
    {
        switch (json->type)
        {
            case cJSON_False:
                success = OCRepPayloadSetPropBool(outPayload, json->string, false);
                break;
            case cJSON_True:
                success = OCRepPayloadSetPropBool(outPayload, json->string, true);
                break;
            case cJSON_NULL:
                success = false;
                break;
            case cJSON_Number:
                success = OCRepPayloadSetPropDouble(outPayload, json->string, json->valuedouble);
                break;
            case cJSON_String:
                success = OCRepPayloadSetPropString(outPayload, json->string, json->valuestring);
                break;
            case cJSON_Array:
                {
                    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
                    int type = JsonArrayType(json, dim);
                    size_t dimTotal = calcDimTotal(dim);
                    switch (type)
                    {
                        case cJSON_Number:
                            {
                                double *array = (double *) OICCalloc(dimTotal, sizeof(double));
                                if (!array)
                                {
                                    success = false;
                                    break;
                                }
                                cJSON *element = json->child;
                                for (size_t i = 0; i < dimTotal; ++i)
                                {
                                    array[i] = element->valuedouble;
                                    element = element->next;
                                }
                                success = OCRepPayloadSetDoubleArrayAsOwner(outPayload,
                                        json->string, array, dim);
                                if (!success)
                                {
                                    OICFree(array);
                                }
                            }
                            break;
                        case cJSON_String:
                            {
                                char **array = (char **) OICCalloc(dimTotal, sizeof(char*));
                                if (!array)
                                {
                                    success = false;
                                    break;
                                }
                                cJSON *element = json->child;
                                for (size_t i = 0; i < dimTotal; ++i)
                                {
                                    array[i] = OICStrdup(element->valuestring);
                                    if (!array[i])
                                    {
                                        success = false;
                                        break;
                                    }
                                    element = element->next;
                                }
                                if (success)
                                {
                                    success = OCRepPayloadSetStringArrayAsOwner(outPayload,
                                            json->string, array, dim);
                                }
                                if (!success)
                                {
                                    for (size_t i = 0; i < dimTotal; ++i)
                                    {
                                        OICFree(array[i]);
                                    }
                                    OICFree(array);
                                }
                            }
                            break;
                        case cJSON_Object:
                            {
                                OCRepPayload **array = (OCRepPayload **) OICCalloc(dimTotal,
                                        sizeof(OCRepPayload*));
                                if (!array)
                                {
                                    success = false;
                                    break;
                                }
                                cJSON *element = json->child;
                                for (size_t i = 0; i < dimTotal; ++i)
                                {
                                    array[i] = OCRepPayloadCreate();
                                    if (!array[i])
                                    {
                                        success = false;
                                        break;
                                    }
                                    OCStackResult result = ParseJsonItem(array[i], element->child);
                                    if (result != OC_STACK_OK)
                                    {
                                        success = false;
                                        break;
                                    }
                                    element = element->next;
                                }
                                if (success)
                                {
                                    success = OCRepPayloadSetPropObjectArrayAsOwner(outPayload,
                                            json->string, array, dim);
                                }
                                if (!success)
                                {
                                    for (size_t i = 0; i < dimTotal; ++i)
                                    {
                                        OCRepPayloadDestroy(array[i]);
                                    }
                                    OICFree(array);
                                }
                            }
                            break;
                        default:
                            /* Only number, string, and object arrays are supported for now */
                            assert(0);
                            success = false;
                            break;
                    }
                }
                break;
            case cJSON_Object:
                {
                    OCRepPayload *objPayload = OCRepPayloadCreate();
                    if (!objPayload)
                    {
                        success = false;
                        break;
                    }
                    cJSON *obj = json->child;
                    OCStackResult result = ParseJsonItem(objPayload, obj);
                    if (result != OC_STACK_OK)
                    {
                        success = false;
                        break;
                    }
                    success = OCRepPayloadSetPropObjectAsOwner(outPayload, json->string, objPayload);
                }
                break;
        }
        json = json->next;
    }
    return success ? OC_STACK_OK : OC_STACK_ERROR;
}

static OCStackResult ParseJsonPayload(OCPayload** outPayload, const char* payload)
{
    OCStackResult result = OC_STACK_INVALID_PARAM;
    cJSON *json;

    *outPayload = NULL;
    json = cJSON_Parse(payload);
    if (!json)
    {
        goto exit;
    }
    if (json->type != cJSON_Object)
    {
        goto exit;
    }
    *outPayload = (OCPayload *) OCRepPayloadCreate();
    if (!*outPayload)
    {
        goto exit;
    }
    result = ParseJsonItem((OCRepPayload *) *outPayload, json->child);
exit:
    if (json)
    {
        cJSON_Delete(json);
    }
    if (result != OC_STACK_OK)
    {
        OCRepPayloadDestroy((OCRepPayload *) *outPayload);
        *outPayload = NULL;
    }
    return result;
}

OCStackResult ParsePayload(OCPayload** outPayload, OCPayloadFormat format, OCPayloadType type,
        const uint8_t* payload, size_t payloadSize)
{
    (void) type;
    (void) payloadSize;
    OCStackResult result;

    switch (format)
    {
        case OC_FORMAT_JSON:
            result = ParseJsonPayload(outPayload, (const char *) payload);
            break;
        default:
            result = OC_STACK_INVALID_PARAM;
            break;
    }

    return result;
}
