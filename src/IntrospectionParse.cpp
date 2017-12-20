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

#include "DeviceConfigurationResource.h"
#include "Interfaces.h"
#include "Log.h"
#include "Name.h"
#include "Payload.h"
#include "PlatformConfigurationResource.h"
#include "Resource.h"
#include "VirtualBusAttachment.h"
#include "VirtualBusObject.h"
#include "VirtualConfigBusObject.h"
#include "ocpayload.h"
#include "oic_malloc.h"
#include <alljoyn/InterfaceDescription.h>
#include <math.h>

static const std::string EnumPrefix = "org.alljoyn.Bus.Enum.";
static const std::string DictPrefix = "org.alljoyn.Bus.Dict.";
static const std::string StructPrefix = "org.alljoyn.Bus.Struct.";

static const char *GetRefDefinition(const char *ref)
{
    static const char prefix[] = "#/definitions/";
    if (!strncmp(ref, prefix, sizeof(prefix) - 1))
    {
        return &ref[sizeof(prefix) - 1];
    }
    return NULL;
}

/*
 * @param[in] schema a schema definition object.
 * @param[in] annotations map from definition name to AJ annotations
 * @param[out] type the payload property type.  OCREP_PROP_NULL to use the natural type of the
 *                  returned signature.
 *
 * @return a pair<D-Bus signature, org.alljoyn.Bus.Type.Name>
 */
static std::pair<std::string, std::string> GetSignature(OCRepPayload *schema,
        std::map<std::string, Annotations> &annotations, OCRepPayloadPropType *type = NULL)
{
    if (type)
    {
        *type = OCREP_PROP_NULL;
    }
    char **typeArr = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
    char *str = NULL;
    const char *ref = NULL;
    std::pair<std::string, std::string> sig;
    if (OCRepPayloadGetStringArray(schema, "type", &typeArr, dim))
    {
        sig.first = "v";
    }
    else if (OCRepPayloadGetPropString(schema, "$ref", &str) &&
            (ref = GetRefDefinition(str)))
    {
        Annotations &as = annotations[ref];
        if (!as.empty())
        {
            std::string &aName = as[0].first;
            if (aName.compare(0, EnumPrefix.size(), EnumPrefix) == 0)
            {
                sig.first = "x";
            }
            else if (aName.compare(0, DictPrefix.size(), DictPrefix) == 0)
            {
                sig.first = "a{" + as[0].second + as[1].second + "}";
            }
            else if (aName.compare(0, StructPrefix.size(), StructPrefix) == 0)
            {
                sig.first = "(";
                for (Annotation a : as)
                {
                    sig.first += a.second;
                }
                sig.first += ")";
            }
            sig.second = std::string("[") + ref + "]";
        }
    }
    else if (OCRepPayloadGetPropString(schema, "type", &str))
    {
        if (!strcmp(str, "boolean"))
        {
            sig.first = "b";
        }
        else if (!strcmp(str, "integer"))
        {
            double min = MIN_SAFE_INTEGER;
            double max = MAX_SAFE_INTEGER;
            OCRepPayloadGetPropDouble(schema, "minimum", &min);
            OCRepPayloadGetPropDouble(schema, "maximum", &max);
            if (min >= 0 && max <= UINT8_MAX)
            {
                sig.first = "y";
            }
            else if (min >= 0 && max <= UINT16_MAX)
            {
                sig.first = "q";
            }
            else if (min >= INT16_MIN && max <= INT16_MAX)
            {
                sig.first = "n";
            }
            else if (min >= 0 && max <= UINT32_MAX)
            {
                sig.first = "u";
            }
            else if (min >= INT32_MIN && max <= INT32_MAX)
            {
                sig.first = "i";
            }
            else if (min >= 0)
            {
                sig.first = "t";
            }
            else
            {
                sig.first = "x";
            }
        }
        else if (!strcmp(str, "number"))
        {
            sig.first = "d";
        }
        else if (!strcmp(str, "string"))
        {
            char *pattern = NULL;
            char *format = NULL;
            OCRepPayload *media = NULL;
            char *encoding = NULL;
            OCRepPayloadGetPropString(schema, "pattern", &pattern);
            OCRepPayloadGetPropString(schema, "format", &format);
            OCRepPayloadGetPropObject(schema, "media", &media);
            if ((pattern && !strcmp(pattern, "^0([1-9][0-9]{0,19})$")) ||
                    (format && !strcmp(format, "uint64")))
            {
                sig.first = "t";
                if (type)
                {
                    *type = OCREP_PROP_STRING;
                }
            }
            else if ((pattern && !strcmp(pattern, "^0(-?[1-9][0-9]{0,18})$")) ||
                    (format && !strcmp(format, "int64")))
            {
                sig.first = "x";
                if (type)
                {
                    *type = OCREP_PROP_STRING;
                }
            }
            else if ((pattern && !strcmp(pattern, "^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$")) ||
                    (media && OCRepPayloadGetPropString(media, "binaryEncoding", &encoding) &&
                            !strcmp(encoding, "base64")))
            {
                sig.first = "ay";
            }
            else
            {
                sig.first = "s";
            }
            OICFree(encoding);
            OCRepPayloadDestroy(media);
            OICFree(format);
        }
        else if (!strcmp(str, "object"))
        {
            sig.first = "a{sv}";
            OCRepPayload *properties = NULL;
            if (OCRepPayloadGetPropObject(schema, "properties", &properties))
            {
                std::string dictName = Types::GenerateAnonymousName();
                for (OCRepPayloadValue *property = properties->values; property;
                     property = property->next)
                {
                    if (property->type != OCREP_PROP_OBJECT)
                    {
                        LOG(LOG_INFO, "%s property unknown type %d, skipping", property->name,
                                property->type);
                        continue;
                    }
                    OCRepPayloadPropType propType;
                    std::pair<std::string, std::string> propSig = GetSignature(property->obj,
                            annotations, &propType);
                    /* Use property-name directly here since it is a dictionary key, not an AJ property */
                    Types::m_dicts[dictName][property->name] =
                            Types::Value(!propSig.second.empty() ? propSig.second : propSig.first,
                                    propType);
                }
                sig.second = dictName;
            }
            OCRepPayloadDestroy(properties);
        }
        else if (!strcmp(str, "array"))
        {
            OCRepPayload *items = NULL;
            OCRepPayload **itemsArr = NULL;
            size_t itemsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
            if (OCRepPayloadGetPropObject(schema, "items", &items))
            {
                std::pair<std::string, std::string> itemSig = GetSignature(items, annotations);
                sig.first = "a" + itemSig.first;
                if (!itemSig.second.empty())
                {
                    sig.second = "a" + itemSig.second;
                }
                OCRepPayloadDestroy(items);
            }
            else if (OCRepPayloadGetPropObjectArray(schema, "items", &itemsArr, itemsDim))
            {
                /*
                 * Potentially have a struct here, confirm first that there are a fixed number of
                 * items.
                  */
                double minItems, maxItems;
                size_t dimTotal = calcDimTotal(itemsDim);
                if (OCRepPayloadGetPropDouble(schema, "minItems", &minItems) &&
                        OCRepPayloadGetPropDouble(schema, "maxItems", &maxItems) &&
                        ((minItems == maxItems) || (maxItems == dimTotal)))
                {
                    sig.first = "(";
                    sig.second = "(";
                    for (size_t i = 0; i < dimTotal; ++i)
                    {
                        std::pair<std::string, std::string> itemSig = GetSignature(itemsArr[i],
                                annotations);
                        sig.first += itemSig.first;
                        sig.second += !itemSig.second.empty() ? itemSig.second : itemSig.first;
                    }
                    sig.first += ")";
                    sig.second += ")";
                    if (sig.second == sig.first)
                    {
                        sig.second.clear();
                    }
                }
                else
                {
                    sig.first = "av";
                }
                for (size_t i = 0; i < dimTotal; ++i)
                {
                    OCRepPayloadDestroy(itemsArr[i]);
                }
                OICFree(itemsArr);
            }
            else
            {
                sig.first = "av";
            }
        }
        else
        {
            LOG(LOG_INFO, "Unhandled type %s", str);
        }
    }
    else
    {
        LOG(LOG_INFO, "Missing \"$ref\" or \"type\" property");
    }
    size_t dimTotal = calcDimTotal(dim);
    for (size_t i = 0; i < dimTotal; ++i)
    {
        OICFree(typeArr[i]);
    }
    OICFree(typeArr);
    OICFree(str);
    return sig;
}

/*
 * @param[in] schema a schema definition object.
 * @param[in] annotations map from definition name to AJ annotations.
 * @param[in,out] iface the AJ iface to add annotations to.
 */
static void AddAnnotations(const char *name, OCRepPayload *schema,
        std::map<std::string, Annotations> &annotations, ajn::InterfaceDescription *iface)
{
    char *str = NULL;
    const char *ref = NULL;
    if (OCRepPayloadGetPropString(schema, "$ref", &str) &&
            (ref = GetRefDefinition(str)))
    {
        for (Annotation &a : annotations[ref])
        {
            iface->AddAnnotation(a.first, a.second);
        }
    }
    else if (OCRepPayloadGetPropString(schema, "type", &str))
    {
        if (!strcmp(str, "array"))
        {
            OCRepPayload *items = NULL;
            if (OCRepPayloadGetPropObject(schema, "items", &items))
            {
                AddAnnotations(name, items, annotations, iface);
                OCRepPayloadDestroy(items);
            }
        }
        else
        {
            double d;
            if (OCRepPayloadGetPropDouble(schema, "default", &d) &&
                    (floor(d) == d))
            {
                iface->AddPropertyAnnotation(name, "org.alljoyn.Bus.Type.Default",
                        (d > 0) ? std::to_string((uint64_t) d) : std::to_string((int64_t) d));
            }
            double min = MIN_SAFE_INTEGER;
            double max = MAX_SAFE_INTEGER;
            if ((OCRepPayloadGetPropDouble(schema, "maximum", &max) && (floor(max) == max)) ||
                    !strcmp(str, "integer"))
            {
                iface->AddPropertyAnnotation(name, "org.alljoyn.Bus.Type.Max",
                        (max > 0) ? std::to_string((uint64_t) max) : std::to_string((int64_t) max));
            }
            if ((OCRepPayloadGetPropDouble(schema, "minimum", &min) && (floor(min) == min)) ||
                    !strcmp(str, "integer"))
            {
                iface->AddPropertyAnnotation(name, "org.alljoyn.Bus.Type.Min",
                        (min > 0) ? std::to_string((uint64_t) min) : std::to_string((int64_t) min));
            }
        }
    }
    OICFree(str);
}

/*
 * @param[in] property a schema definition pair.
 * @param[in] isObservable true when the property is observable.
 * @param[in] annotations map from definition name to AJ annotations.
 * @param[in,out] iface the AJ iface to add annotations to.
 */
static void AddProperty(OCRepPayloadValue *property, bool isObservable,
        std::map<std::string, Annotations> &annotations, ajn::InterfaceDescription *iface)
{
    std::pair<std::string, std::string> sig = GetSignature(property->obj, annotations);
    if (sig.first.empty())
    {
        LOG(LOG_INFO, "%s property unknown type, skipping", property->name);
        return;
    }
    uint8_t access = ajn::PROP_ACCESS_RW;
    bool readOnly;
    if (OCRepPayloadGetPropBool(property->obj, "readOnly", &readOnly) && readOnly)
    {
        access = ajn::PROP_ACCESS_READ;
    }
    std::string propName = ToAJPropName(property->name);
    iface->AddProperty(propName.c_str(), sig.first.c_str(), access);
    std::string dictName = std::string("[") + iface->GetName() + ".Properties" + "]";
    if (sig.second.empty())
    {
        Types::m_dicts[dictName][propName] = Types::Value(sig.first);
    }
    else
    {
        iface->AddPropertyAnnotation(propName, "org.alljoyn.Bus.Type.Name", sig.second);
        Types::m_dicts[dictName][propName] = Types::Value(sig.second);
    }
    AddAnnotations(propName.c_str(), property->obj, annotations, iface);
    if (isObservable)
    {
        /* "false" is the default value */
        iface->AddPropertyAnnotation(propName,
                ajn::org::freedesktop::DBus::AnnotateEmitsChanged, "true");
    }
}

/*
 * @param[in] refValue the value of a "$ref" property.
 * @param[in] ajNames a map from definition name to interface name.
 * @param[in,out] obj the AJ bus object to add interfaces to.
 */
static void AddInterface(const char *refValue, std::map<std::string, std::string> &ajNames,
        VirtualBusObject *obj)
{
    const char *def = GetRefDefinition(refValue);
    if (def && ajNames.find(def) != ajNames.end())
    {
        const char *rt = ajNames[def].c_str();
        if (TranslateResourceType(rt))
        {
            obj->AddInterface(rt);
        }
    }
    else
    {
        LOG(LOG_INFO, "Missing definition of %s", def);
    }
}

/*
 * @param[in] schema a schema definition object.
 * @param[in] ajNames a map from definition name to interface name.
 * @param[in,out] obj the AJ bus object to add interfaces to.
 */
static void AddInterface(OCRepPayload *schema, std::map<std::string, std::string> &ajNames,
        VirtualBusObject *obj)
{
    if (!schema->values)
    {
        return;
    }
    if (!strcmp(schema->values->name, "$ref") &&
            schema->values->type == OCREP_PROP_STRING)
    {
        AddInterface(schema->values->str, ajNames, obj);
    }
    else if (!strcmp(schema->values->name, "oneOf") &&
            schema->values->type == OCREP_PROP_ARRAY &&
            schema->values->arr.type == OCREP_PROP_OBJECT)
    {
        OCRepPayload **objArray = schema->values->arr.objArray;
        for (size_t j = 0; j < schema->values->arr.dimensions[0]; ++j)
        {
            AddInterface(objArray[j], ajNames, obj);
        }
    }
}

/*
 * @param[in] definitions definitions of OC introspection data.
 * @param[out] annotations map from definition name to AJ annotations
 */
static void ParseAnnotations(const OCRepPayload *definitions,
        std::map<std::string, Annotations> &annotations)
{
    for (OCRepPayloadValue *definition = definitions->values; definition;
         definition = definition->next)
    {
        if (definition->type != OCREP_PROP_OBJECT)
        {
            LOG(LOG_INFO, "%s unknown type %d, skipping", definition->name, definition->type);
            continue;
        }
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
        OCRepPayload **oneOf = NULL;
        char *type = NULL;
        if (OCRepPayloadGetPropObjectArray(definition->obj, "oneOf", &oneOf, dim))
        {
            size_t dimTotal = calcDimTotal(dim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                char *enumName = NULL;
                double *enumValue = NULL;
                size_t dimEnumValue[MAX_REP_ARRAY_DEPTH] = { 0 };
                if (OCRepPayloadGetPropString(oneOf[i], "title", &enumName) &&
                        OCRepPayloadGetDoubleArray(oneOf[i], "enum", &enumValue, dimEnumValue) &&
                        (calcDimTotal(dimEnumValue) == 1))
                {
                    std::string Enum = EnumPrefix + definition->name;
                    /* Assume enum value is in range of an int64_t */
                    annotations[definition->name].push_back(Annotation(Enum + ".Value." + enumName,
                            std::to_string((int64_t) enumValue[0])));
                }
                OICFree(enumValue);
                OICFree(enumName);
            }
        }
        else if (OCRepPayloadGetPropString(definition->obj, "type", &type) &&
                !strcmp(type, "object"))
        {
            OCRepPayload *properties = NULL;
            OCRepPayload *rt = NULL;
            if (!OCRepPayloadGetPropObject(definition->obj, "properties", &properties))
            {
                std::string Dict = DictPrefix + definition->name;
                annotations[definition->name].push_back(Annotation(Dict + ".Key.Type", "s"));
                annotations[definition->name].push_back(Annotation(Dict + ".Value.Type", "v"));
            }
            else if (!OCRepPayloadGetPropObject(properties, "rt", &rt))
            {
                for (OCRepPayloadValue *property = properties->values; property;
                     property = property->next)
                {
                    if (property->type != OCREP_PROP_OBJECT)
                    {
                        LOG(LOG_INFO, "%s property unknown type %d, skipping", property->name,
                                property->type);
                        continue;
                    }
                    std::string Struct = StructPrefix + definition->name;
                    std::string fieldName = ToAJPropName(property->name);
                    std::string fieldSig = GetSignature(property->obj, annotations).first;
                    annotations[definition->name].push_back(Annotation(Struct + ".Field." +
                            fieldName + ".Type", fieldSig));
                    std::string structName = std::string("[") + definition->name + "]";
                    auto it = std::find_if(Types::m_structs[structName].begin(), Types::m_structs[structName].end(),
                            [fieldName](Types::Field &field) -> bool { return field.m_name == fieldName; });
                    if (it == Types::m_structs[structName].end())
                    {
                        Types::m_structs[structName].push_back(Types::Field(fieldName, fieldSig));
                    }
                }
            }
            OCRepPayloadDestroy(rt);
            OCRepPayloadDestroy(properties);
        }
        else
        {
            LOG(LOG_INFO, "%s unsupported \"type\" value \"%s\", skipping", definition->name, type);
        }
        OICFree(type);
        if (oneOf)
        {
            size_t dimTotal = calcDimTotal(dim);
            for(size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(oneOf[i]);
            }
        }
        OICFree(oneOf);
    }
}

/*
 * @param[in] definitions the definitions of the OC introspection data.
 * @param[in] annotations a map from definition name to AJ annotations.
 * @param[in] isObservable a map from rt name to observable flag.
 * @param[in,out] bus the bus to create AJ interfaces on.
 * @param[out] ajNames a map from definition name to interface name.
 */
static void ParseInterfaces(const OCRepPayload *definitions,
        std::map<std::string, Annotations> &annotations, std::map<std::string, bool> &isObservable,
        ajn::BusAttachment *bus, std::map<std::string, std::string> &ajNames)
{
    for (OCRepPayloadValue *definition = definitions->values; definition;
         definition = definition->next)
    {
        if (definition->type != OCREP_PROP_OBJECT)
        {
            LOG(LOG_INFO, "%s unknown type %d, skipping", definition->name, definition->type);
            continue;
        }
        OCRepPayload *properties = NULL;
        OCRepPayload *rt = NULL;
        char **rts = NULL;
        size_t rtsDim[MAX_REP_ARRAY_DEPTH] = { 0 };
        std::string ifaceName;
        ajn::InterfaceDescription *iface = NULL;
        if (!OCRepPayloadGetPropObject(definition->obj, "properties", &properties))
        {
            LOG(LOG_INFO, "%s missing \"properties\", skipping", definition->name);
            goto next_iface;
        }
        if (!OCRepPayloadGetPropObject(properties, "rt", &rt))
        {
            LOG(LOG_INFO, "%s missing \"rt\" property, skipping", definition->name);
            goto next_iface;
        }
        if (!OCRepPayloadGetStringArray(rt, "default", &rts, rtsDim) || !rtsDim[0])
        {
            LOG(LOG_INFO, "%s missing or empty \"default\" property, skipping", definition->name);
            goto next_iface;
        }
        if (IsResourceTypeInWellDefinedSet(rts[0]))
        {
            LOG(LOG_INFO, "Skipping well-defined %s resource type", rts[0]);
            goto next_iface;
        }
        ifaceName = ToAJName(rts[0]);
        ajNames[definition->name] = ifaceName;
        bus->CreateInterface(ifaceName.c_str(), iface, ajn::AJ_IFC_SECURITY_INHERIT);
        if (!iface)
        {
            LOG(LOG_ERR, "CreateInterface %s failed", ifaceName.c_str());
            goto next_iface;
        }
        LOG(LOG_INFO, "Created interface %s", ifaceName.c_str());
        if (strstr(ifaceName.c_str(), "oic.d.") == ifaceName.c_str())
        {
            /* Device types are translated as empty interfaces */
            iface->Activate();
            goto next_iface;
        }
        for (OCRepPayloadValue *property = properties->values; property; property = property->next)
        {
            if (property->type != OCREP_PROP_OBJECT)
            {
                LOG(LOG_INFO, "%s property unknown type %d, skipping", property->name,
                        property->type);
                continue;
            }
            if (!strcmp(property->name, "rt"))
            {
                /* "rt" property is special-cased above */
                continue;
            }
            else if (!strcmp(property->name, "if") ||
                    !strcmp(property->name, "p") ||
                    !strcmp(property->name, "n") ||
                    !strcmp(property->name, "id"))
            {
                /* Ignore baseline properties */
                continue;
            }
            AddProperty(property, isObservable[rts[0]], annotations, iface);
        }
        iface->Activate();
    next_iface:
        size_t dimTotal = calcDimTotal(rtsDim);
        for (size_t i = 0; i < dimTotal; ++i)
        {
            OICFree(rts[i]);
        }
        OICFree(rts);
        rts = NULL;
        OCRepPayloadDestroy(rt);
        rt = NULL;
        OCRepPayloadDestroy(properties);
        properties = NULL;
    }
}

/*
 * @param[in] path a path of the OC introspection data.
 * @param[in] ajNames a map from definition name to interface name.
 * @param[in,out] obj the AJ bus object to add interfaces to.
 *
 * @note obj must be a VirtualBusObject (and not an ajn::BusObject) as AddInterface is protected.
 */
static void ParsePath(OCRepPayload *path, std::map<std::string, std::string> &ajNames,
        VirtualBusObject *obj)
{
    for (OCRepPayloadValue *method = path->values; method; method = method->next)
    {
        if (method->type != OCREP_PROP_OBJECT)
        {
            LOG(LOG_INFO, "%s method unknown type %d, skipping", method->name,
                    method->type);
            continue;
        }
        for (OCRepPayloadValue *value = method->obj->values; value; value = value->next)
        {
            if (!strcmp(value->name, "parameters"))
            {
                if (value->type != OCREP_PROP_ARRAY || value->arr.type != OCREP_PROP_OBJECT)
                {
                    LOG(LOG_INFO, "%s unknown type %d, skipping", value->name,
                            value->type);
                    continue;
                }
                for (size_t i = 0; i < value->arr.dimensions[0]; ++i)
                {
                    OCRepPayload *schema = NULL;
                    if (OCRepPayloadGetPropObject(value->arr.objArray[i], "schema",
                            &schema))
                    {
                        AddInterface(schema, ajNames, obj);
                    }
                    OCRepPayloadDestroy(schema);
                    schema = NULL;
                }
            }
            else if (!strcmp(value->name, "responses"))
            {
                if (value->type != OCREP_PROP_OBJECT)
                {
                    LOG(LOG_INFO, "%s unknown type %d, skipping", value->name,
                            value->type);
                    continue;
                }
                for (OCRepPayloadValue *code = value->obj->values; code; code = code->next)
                {
                    if (code->type != OCREP_PROP_OBJECT)
                    {
                        LOG(LOG_INFO, "%s code unknown type %d, skipping", code->name,
                                code->type);
                        continue;
                    }
                    for (OCRepPayloadValue *codeValue = code->obj->values; codeValue;
                         codeValue = codeValue->next)
                    {
                        if (!strcmp(codeValue->name, "schema") &&
                                codeValue->type == OCREP_PROP_OBJECT)
                        {
                            AddInterface(codeValue->obj, ajNames, obj);
                        }
                    }
                }
            }
        }
    }
}

bool ParseIntrospectionPayload(Device *device, VirtualBusAttachment *bus,
        const OCRepPayload *payload)
{
    OCRepPayload *definitions = NULL;
    OCRepPayload *paths = NULL;
    Resource *resource;
    std::map<std::string, bool> isObservable;
    std::map<std::string, Annotations> annotations;
    std::map<std::string, std::string> ajNames;
    QStatus status;
    bool success = false;

    /*
     * Figure out which resource types are observable so that the properties can be annotated with
     * the correct EmitsChanged value.  On the OC side it is resources that are observable, not
     * resource types.  So in the worst case where a resource type is used by two resources, one of
     * which is observable and one which is not, we set EmitsChanged to false.
     */
    for (auto &r : device->m_resources)
    {
        for (auto &rt : r.m_rts)
        {
            if (isObservable.find(rt) == isObservable.end())
            {
                isObservable[rt] = r.m_isObservable;
            }
            else
            {
                isObservable[rt] = isObservable[rt] && r.m_isObservable;
            }
        }
    }

    /*
     * Create AJ interfaces from OC definitions.  Look for annotations first since they will be
     * needed by interface definitions.
     */
    if (!OCRepPayloadGetPropObject(payload, "definitions", &definitions))
    {
        goto exit;
    }
    ParseAnnotations(definitions, annotations);
    ParseInterfaces(definitions, annotations, isObservable, bus, ajNames);
    OCRepPayloadDestroy(definitions);
    definitions = NULL;

    /*
     * Create virtual bus objects from OC paths.
     */
    if (!OCRepPayloadGetPropObject(payload, "paths", &paths))
    {
        goto exit;
    }
    for (std::vector<Resource>::iterator it = device->m_resources.begin();
         it != device->m_resources.end(); ++it)
    {
        const char *uri = it->m_uri.c_str();
        OCRepPayload *path = NULL;
        if (!OCRepPayloadGetPropObject(paths, uri, &path))
        {
            continue;
        }
        if (!strcmp(uri, "/oic/d") || !strcmp(uri, "/oic/p"))
        {
            /* These resources are not translated on-the-fly */
        }
        else if (HasResourceType(it->m_rts, OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION) ||
                HasResourceType(it->m_rts, OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION) ||
                !strcmp(uri, "/oic/mnt"))
        {
            VirtualBusObject *obj = bus->GetConfigBusObject();
            if (!obj)
            {
                obj = new VirtualConfigBusObject(bus, *it);
                status = bus->RegisterBusObject(obj);
                if (status != ER_OK)
                {
                    delete obj;
                }
            }
            else
            {
                obj->AddResource(*it);
            }
        }
        else
        {
            VirtualBusObject *obj = new VirtualBusObject(bus, *it);
            ParsePath(path, ajNames, obj);
            for (std::vector<Resource>::iterator jt = it->m_resources.begin();
                 jt != it->m_resources.end(); ++jt)
            {
                OCRepPayload *childPath = NULL;
                if (!OCRepPayloadGetPropObject(paths, jt->m_uri.c_str(), &childPath))
                {
                    continue;
                }
                ParsePath(childPath, ajNames, obj);
                OCRepPayloadDestroy(childPath);
            }
            status = bus->RegisterBusObject(obj, it->IsSecure());
            if (status != ER_OK)
            {
                delete obj;
            }
        }
        OCRepPayloadDestroy(path);
    }
    OCRepPayloadDestroy(paths);
    paths = NULL;

    /* Done */
    resource = device->GetResourceUri(OC_RSRVD_DEVICE_URI);
    if (resource)
    {
        VirtualBusObject *obj = new VirtualBusObject(bus, *resource);
        for (auto &rt : resource->m_rts)
        {
            if (TranslateResourceType(rt.c_str()))
            {
                obj->AddInterface(ToAJName(rt).c_str(), true);
            }
        }
        obj->AddInterface(ToAJName("oic.d.virtual").c_str(), true);
        status = bus->RegisterBusObject(obj);
        if (status != ER_OK)
        {
            delete obj;
        }
    }
    success = true;

exit:
    OCRepPayloadDestroy(paths);
    OCRepPayloadDestroy(definitions);
    return success;
}
