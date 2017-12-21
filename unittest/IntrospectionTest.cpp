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

#include "UnitTest.h"

#include "Introspection.h"
#include "VirtualBusAttachment.h"
#include "ocpayloadcbor.h"
#include "ocstack.h"

/*
 * 7.2.2.1 Translation of the introspection itself
 */

class Introspection : public AJOCSetUp
{
public:
    OCResourceHandle m_handle;
    DiscoverContext *m_context;
    VirtualBusAttachment *m_bus;
    virtual ~Introspection() { }
    virtual void SetUp()
    {
        AJOCSetUp::SetUp();
        EXPECT_EQ(OC_STACK_OK, OCCreateResource(&m_handle, "x.org.iotivity.rt", NULL, "/resource",
                NULL, NULL, OC_DISCOVERABLE));

        m_context = new DiscoverContext();
        Callback discoverCB(Discover, m_context);
        EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "/oic/res", NULL, 0, CT_DEFAULT,
                OC_HIGH_QOS, discoverCB, NULL, 0));
        EXPECT_EQ(OC_STACK_OK, discoverCB.Wait(1000));

        m_bus = VirtualBusAttachment::Create(m_context->m_device->m_di.c_str(), NULL, false);
    }
    virtual void TearDown()
    {
        delete m_bus;
        delete m_context;
        OCDeleteResource(m_handle);
        AJOCSetUp::TearDown();
    }
};

TEST_F(Introspection, TheTranslatorShallPreserveAsMuchOfTheOriginalInformationAsCanBeRepresentedInTheTranslatedFormat)
{
    FAIL();
}

static void VerifyMinimumAndMaximum(OCRepPayload *properties, const char *propertyName,
        int64_t minimum, int64_t maximum)
{
    OCRepPayload *property = NULL;
    std::string name = std::string("x.org.iotivity.-interface.false.") + propertyName;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, name.c_str(), &property));
    if (property)
    {
        char *s;
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("integer", s);
        int64_t min, max;
        EXPECT_TRUE(OCRepPayloadGetPropInt(property, "minimum", &min));
        EXPECT_TRUE(OCRepPayloadGetPropInt(property, "maximum", &max));
        EXPECT_EQ(minimum, min);
        EXPECT_EQ(maximum, max);
    }
}

TEST_F(Introspection, NumericTypes)
{
    const char *introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"byte\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 255 },"
            "        \"uint16\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 65535 },"
            "        \"int16\": { \"type\": \"integer\", \"minimum\": -32768, \"maximum\": 32767 },"
            "        \"uint32\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 4294967295 },"
            "        \"int32\": { \"type\": \"integer\", \"minimum\": -2147483648, \"maximum\": 2147483647 },"
            "        \"uint64\": { \"type\": \"integer\", \"minimum\": 0 },"
            "        \"int64\": { \"type\": \"integer\" },"
            "        \"double\": { \"type\": \"number\" },"
            "        \"uint64large\": { \"type\": \"string\", \"pattern\": \"^0([1-9][0-9]{0,19})$\" },"
            "        \"int64large\": { \"type\": \"string\", \"pattern\": \"^0(-?[1-9][0-9]{0,18})$\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.iotivity.rt");
    EXPECT_TRUE(iface != NULL);

    const ajn::InterfaceDescription::Property *prop;
    prop = iface->GetProperty("byte");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("y", prop->signature.c_str());
    prop = iface->GetProperty("uint16");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("q", prop->signature.c_str());
    prop = iface->GetProperty("int16");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("n", prop->signature.c_str());
    prop = iface->GetProperty("uint32");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("u", prop->signature.c_str());
    prop = iface->GetProperty("int32");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("i", prop->signature.c_str());
    prop = iface->GetProperty("uint64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("t", prop->signature.c_str());
    prop = iface->GetProperty("int64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("x", prop->signature.c_str());
    prop = iface->GetProperty("double");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("d", prop->signature.c_str());
    prop = iface->GetProperty("uint64large");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("t", prop->signature.c_str());
    prop = iface->GetProperty("int64large");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("x", prop->signature.c_str());

    static const char *introspectionXml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='byte' type='y' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='0'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='255'/>"
            "  </property>"
            "  <property name='uint16' type='q' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='0'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='65535'/>"
            "  </property>"
            "  <property name='int16' type='n' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-32768'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='32767'/>"
            "  </property>"
            "  <property name='uint32' type='u' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='0'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='4294967295'/>"
            "  </property>"
            "  <property name='int32' type='i' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-2147483648'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='2147483647'/>"
            "  </property>"
            "  <property name='uint64small' type='t' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "  </property>"
            "  <property name='uint64large' type='t' access='read'/>"
            "  <property name='int64small' type='x' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "  </property>"
            "  <property name='int64large' type='x' access='read'/>"
            "  <property name='double' type='d' access='read'/>"
            "</interface>";
    EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(introspectionXml));
    uint8_t out[8192];
    size_t outSize = 8192;
    EXPECT_EQ(CborNoError, Introspect(m_bus, "v16.10.00", "TITLE", "VERSION", out, &outSize));
    OCPayload *p;
    EXPECT_EQ(OC_STACK_OK, OCParsePayload(&p, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
            out, outSize));
    OCRepPayload *payload = (OCRepPayload *) p;
    OCRepPayload *definitions;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "definitions", &definitions));
    OCRepPayload *definition;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "x.org.iotivity.-interface.false",
            &definition));
    OCRepPayload *properties;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));

    VerifyMinimumAndMaximum(properties, "byte", 0, 255);
    VerifyMinimumAndMaximum(properties, "uint16", 0, 65535);
    VerifyMinimumAndMaximum(properties, "int16", -32768, 32767);
    VerifyMinimumAndMaximum(properties, "uint32", 0, 4294967295);
    VerifyMinimumAndMaximum(properties, "int32", -2147483648, 2147483647);

    VerifyMinimumAndMaximum(properties, "uint64small", 0, 9007199254740992);
    char *s;
    OCRepPayload *property = NULL;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.uint64large",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "pattern", &s));
        EXPECT_STREQ("^0([1-9][0-9]{0,19})$", s);
    }

    VerifyMinimumAndMaximum(properties, "int64small", -9007199254740992, 9007199254740992);
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.int64large",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "pattern", &s));
        EXPECT_STREQ("^0(-?[1-9][0-9]{0,18})$", s);
    }

    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.double",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("number", s);
    }
}

TEST_F(Introspection, TextStringsAndByteArrays)
{
    const char *introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"string\": { \"type\": \"string\" },"
            "        \"stringbase64\": { \"type\": \"string\", \"media\": { \"binaryEncoding\": \"base64\" } },"
            "        \"stringpattern\": { \"type\": \"string\", \"pattern\": \"^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.iotivity.rt");
    EXPECT_TRUE(iface != NULL);

    const ajn::InterfaceDescription::Property *prop;
    prop = iface->GetProperty("string");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("s", prop->signature.c_str());
    prop = iface->GetProperty("stringbase64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("ay", prop->signature.c_str());
    prop = iface->GetProperty("stringpattern");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("ay", prop->signature.c_str());

    static const char *introspectionXml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='string' type='s' access='read'/>"
            "  <property name='arrayOfByte' type='ay' access='read'/>"
            "</interface>";
    EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(introspectionXml));
    uint8_t out[8192];
    size_t outSize = 8192;
    EXPECT_EQ(CborNoError, Introspect(m_bus, "v16.10.00", "TITLE", "VERSION", out, &outSize));
    OCPayload *p;
    EXPECT_EQ(OC_STACK_OK, OCParsePayload(&p, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
            out, outSize));
    OCRepPayload *payload = (OCRepPayload *) p;
    OCRepPayload *definitions;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "definitions", &definitions));
    OCRepPayload *definition;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "x.org.iotivity.-interface.false",
            &definition));
    OCRepPayload *properties;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));

    OCRepPayload *property;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.string",
            &property));
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
    EXPECT_STREQ("string", s);

    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.arrayOfByte",
            &property));
    EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
    EXPECT_STREQ("string", s);
    OCRepPayload *media;
    EXPECT_TRUE(OCRepPayloadGetPropObject(property, "media", &media));
    EXPECT_TRUE(OCRepPayloadGetPropString(media, "binaryEncoding", &s));
    EXPECT_STREQ("base64", s);
}

TEST_F(Introspection, ObjectPathsAndSignatures)
{
    static const char *introspectionXml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='objectPath' type='o' access='read'/>"
            "  <property name='signature' type='g' access='read'/>"
            "</interface>";
    EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(introspectionXml));
    uint8_t out[8192];
    size_t outSize = 8192;
    EXPECT_EQ(CborNoError, Introspect(m_bus, "v16.10.00", "TITLE", "VERSION", out, &outSize));
    OCPayload *p;
    EXPECT_EQ(OC_STACK_OK, OCParsePayload(&p, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
            out, outSize));
    OCRepPayload *payload = (OCRepPayload *) p;
    OCRepPayload *definitions;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "definitions", &definitions));
    OCRepPayload *definition;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "x.org.iotivity.-interface.false",
            &definition));
    OCRepPayload *properties;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));

    char *s;
    OCRepPayload *property = NULL;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.objectPath",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
    }

    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.signature",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
    }
}

TEST_F(Introspection, Structures)
{
    static const char *introspectionXml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='struct' type='(is)' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='[StructName]'/>"
            "  </property>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.int.Type' value='i'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.string.Type' value='s'/>"
            "</interface>";
    EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(introspectionXml));
    uint8_t out[8192];
    size_t outSize = 8192;
    EXPECT_EQ(CborNoError, Introspect(m_bus, "v16.10.00", "TITLE", "VERSION", out, &outSize));
    OCPayload *p;
    EXPECT_EQ(OC_STACK_OK, OCParsePayload(&p, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
            out, outSize));
    OCRepPayload *payload = (OCRepPayload *) p;
    OCRepPayload *definitions;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "definitions", &definitions));
    OCRepPayload *definition;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "StructName", &definition));
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(definition, "type", &s));
    EXPECT_STREQ("object", s);
    OCRepPayload *properties;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));
    OCRepPayload *property;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "int", &property));
    EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
    EXPECT_STREQ("integer", s);
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "string", &property));
    EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
    EXPECT_STREQ("string", s);
}

static void VerifyArrayMinimumAndMaximum(OCRepPayload *properties, const char *propertyName,
        int64_t minimum, int64_t maximum)
{
    OCRepPayload *property = NULL;
    std::string name = std::string("x.org.iotivity.-interface.false.") + propertyName;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, name.c_str(), &property));
    char *s = NULL;
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        OCRepPayload *items = NULL;
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("integer", s);
        int64_t min, max;
        EXPECT_TRUE(OCRepPayloadGetPropInt(items, "minimum", &min));
        EXPECT_TRUE(OCRepPayloadGetPropInt(items, "maximum", &max));
        EXPECT_EQ(minimum, min);
        EXPECT_EQ(maximum, max);
    }
}

TEST_F(Introspection, Arrays)
{
    const char *introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"byte\": { \"type\": \"array\", \"items\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 255 } },"
            "        \"uint16\": { \"type\": \"array\", \"items\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 65535 } },"
            "        \"int16\": { \"type\": \"array\", \"items\": { \"type\": \"integer\", \"minimum\": -32768, \"maximum\": 32767 } },"
            "        \"uint32\": { \"type\": \"array\", \"items\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 4294967295 } },"
            "        \"int32\": { \"type\": \"array\", \"items\": { \"type\": \"integer\", \"minimum\": -2147483648, \"maximum\": 2147483647 } },"
            "        \"uint64\": { \"type\": \"array\", \"items\": { \"type\": \"integer\", \"minimum\": 0 } },"
            "        \"int64\": { \"type\": \"array\", \"items\": { \"type\": \"integer\" } },"
            "        \"double\": { \"type\": \"array\", \"items\": { \"type\": \"number\" } },"
            "        \"uint64large\": { \"type\": \"array\", \"items\": { \"type\": \"string\", \"pattern\": \"^0([1-9][0-9]{0,19})$\" } },"
            "        \"int64large\": { \"type\": \"array\", \"items\": { \"type\": \"string\", \"pattern\": \"^0(-?[1-9][0-9]{0,18})$\" } },"
            "        \"string\": { \"type\": \"array\", \"items\": { \"type\": \"string\" } },"
            "        \"stringbase64\": { \"type\": \"array\", \"items\": { \"type\": \"string\", \"media\": { \"binaryEncoding\": \"base64\" } } },"
            "        \"stringpattern\": { \"type\": \"array\", \"items\": { \"type\": \"string\", \"pattern\": \"^[a-fA-F0-9]{8}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{4}-[a-fA-F0-9]{12}$\" } },"
            "        \"variant\": { \"type\": \"array\", \"items\": { \"type\": [ \"integer\", \"string\" ] } },"
            "        \"struct0\": { \"type\": \"array\", \"items\": { \"type\": \"array\", \"items\": [ { \"type\": \"integer\", \"minimum\": -2147483648, \"maximum\": 2147483647 }, { \"type\": \"string\" } ], \"minItems\": 2, \"maxItems\": 2 } },"
            "        \"struct1\": { \"type\": \"array\", \"items\": { \"type\": \"object\", \"properties\": { \"int\": { \"type\": \"integer\" }, \"string\": { \"type\": \"string\" } } } },"
            "        \"dict\": { \"type\": \"array\", \"items\": { \"type\": \"object\" } },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.iotivity.rt");
    EXPECT_TRUE(iface != NULL);

    const ajn::InterfaceDescription::Property *prop;
    prop = iface->GetProperty("byte");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("ay", prop->signature.c_str());
    prop = iface->GetProperty("uint16");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("aq", prop->signature.c_str());
    prop = iface->GetProperty("int16");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("an", prop->signature.c_str());
    prop = iface->GetProperty("uint32");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("au", prop->signature.c_str());
    prop = iface->GetProperty("int32");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("ai", prop->signature.c_str());
    prop = iface->GetProperty("uint64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("at", prop->signature.c_str());
    prop = iface->GetProperty("int64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("ax", prop->signature.c_str());
    prop = iface->GetProperty("double");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("ad", prop->signature.c_str());
    prop = iface->GetProperty("uint64large");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("at", prop->signature.c_str());
    prop = iface->GetProperty("int64large");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("ax", prop->signature.c_str());
    prop = iface->GetProperty("string");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("as", prop->signature.c_str());
    prop = iface->GetProperty("stringbase64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("aay", prop->signature.c_str());
    prop = iface->GetProperty("stringpattern");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("aay", prop->signature.c_str());
    prop = iface->GetProperty("variant");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("av", prop->signature.c_str());
    prop = iface->GetProperty("struct0");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("a(is)", prop->signature.c_str());
    prop = iface->GetProperty("struct1");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("aa{sv}", prop->signature.c_str());
    prop = iface->GetProperty("dict");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("aa{sv}", prop->signature.c_str());

    static const char *introspectionXml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='byte' type='ay' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='0'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='255'/>"
            "  </property>"
            "  <property name='uint16' type='aq' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='0'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='65535'/>"
            "  </property>"
            "  <property name='int16' type='an' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-32768'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='32767'/>"
            "  </property>"
            "  <property name='uint32' type='au' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='0'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='4294967295'/>"
            "  </property>"
            "  <property name='int32' type='ai' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-2147483648'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='2147483647'/>"
            "  </property>"
            "  <property name='uint64small' type='at' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "  </property>"
            "  <property name='uint64large' type='at' access='read'/>"
            "  <property name='int64small' type='ax' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "  </property>"
            "  <property name='int64large' type='ax' access='read'/>"
            "  <property name='double' type='ad' access='read'/>"
            "  <property name='string' type='as' access='read'/>"
            "  <property name='variant' type='av' access='read'/>"
            "  <property name='objectpath' type='ao' access='read'/>"
            "  <property name='signature' type='ag' access='read'/>"
            "  <property name='struct0' type='a(is)' access='read'/>"
            "  <property name='struct1' type='a(is)' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='[StructName]'/>"
            "  </property>"
            "  <property name='dict' type='aa{sv}' access='read'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.int.Type' value='i'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.string.Type' value='s'/>"
            "</interface>";
    EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(introspectionXml));
    uint8_t out[8192];
    size_t outSize = 8192;
    EXPECT_EQ(CborNoError, Introspect(m_bus, "v16.10.00", "TITLE", "VERSION", out, &outSize));
    OCPayload *p;
    EXPECT_EQ(OC_STACK_OK, OCParsePayload(&p, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
            out, outSize));
    OCRepPayload *payload = (OCRepPayload *) p;
    OCRepPayload *definitions;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "definitions", &definitions));
    OCRepPayload *definition;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "x.org.iotivity.-interface.false",
            &definition));
    OCRepPayload *properties;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));

    OCRepPayload *property;
    char *s;
    int64_t i;
    OCRepPayload *items;
    size_t dim[MAX_REP_ARRAY_DEPTH];
    char **typesArr;
    OCRepPayload **itemsArr;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.byte",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "media", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "binaryEncoding", &s));
        EXPECT_STREQ("base64", s);
    }
    VerifyArrayMinimumAndMaximum(properties, "uint16", 0, 65535);
    VerifyArrayMinimumAndMaximum(properties, "int16", -32768, 32767);
    VerifyArrayMinimumAndMaximum(properties, "uint32", 0, 4294967295);
    VerifyArrayMinimumAndMaximum(properties, "int32", -2147483648, 2147483647);
    VerifyArrayMinimumAndMaximum(properties, "uint64small", 0, 9007199254740992);
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.uint64large",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "pattern", &s));
        EXPECT_STREQ("^0([1-9][0-9]{0,19})$", s);
    }
    VerifyArrayMinimumAndMaximum(properties, "int64small", -9007199254740992, 9007199254740992);
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.int64large",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "pattern", &s));
        EXPECT_STREQ("^0(-?[1-9][0-9]{0,18})$", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.double",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("number", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.string",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("string", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.variant",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetStringArray(items, "type", &typesArr, dim));
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.objectpath",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("string", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.signature",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("string", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.struct0",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObjectArray(items, "items", &itemsArr, dim));
        EXPECT_EQ(2u, calcDimTotal(dim));
        EXPECT_TRUE(OCRepPayloadGetPropString(itemsArr[0], "type", &s));
        EXPECT_STREQ("integer", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(itemsArr[1], "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropInt(items, "minItems", &i));
        EXPECT_EQ(2, i);
        EXPECT_TRUE(OCRepPayloadGetPropInt(items, "maxItems", &i));
        EXPECT_EQ(2, i);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.struct1",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "$ref", &s));
        EXPECT_STREQ("#/definitions/StructName", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "StructName", &definition));
        EXPECT_TRUE(OCRepPayloadGetPropString(definition, "type", &s));
        EXPECT_STREQ("object", s);
        OCRepPayload *ps;
        EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &ps));
        EXPECT_TRUE(OCRepPayloadGetPropObject(ps, "int", &property));
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("integer", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(ps, "string", &property));
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.dict",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("object", s);
    }
}

TEST_F(Introspection, Examples)
{
    const char *introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"int32\": { \"type\": \"integer\" },"
            "        \"int64\": { \"type\": \"integer\", \"minimum\": -1099511627776, \"maximum\": 1099511627776 },"
            "        \"uint64\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 281474976710656 },"
            "        \"double\": { \"type\": \"number\" },"
            "        \"arrayOfUint64\": { \"type\": \"array\", \"items\": { \"type\": \"integer\", \"minimum\": 0, \"maximum\": 70368744177664 } },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.iotivity.rt");
    EXPECT_TRUE(iface != NULL);

    qcc::String annotation;
    const ajn::InterfaceDescription::Property *prop;
    prop = iface->GetProperty("int32");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("i", prop->signature.c_str());
    prop = iface->GetProperty("int64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("x", prop->signature.c_str());
    EXPECT_TRUE(prop->GetAnnotation("org.alljoyn.Bus.Type.Min", annotation));
    EXPECT_STREQ("-1099511627776", annotation.c_str());
    EXPECT_TRUE(prop->GetAnnotation("org.alljoyn.Bus.Type.Max", annotation));
    EXPECT_STREQ("1099511627776", annotation.c_str());
    prop = iface->GetProperty("uint64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("t", prop->signature.c_str());
    EXPECT_TRUE(prop->GetAnnotation("org.alljoyn.Bus.Type.Max", annotation));
    EXPECT_STREQ("281474976710656", annotation.c_str());
    prop = iface->GetProperty("double");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("d", prop->signature.c_str());
    prop = iface->GetProperty("arrayOfUint64");
    EXPECT_TRUE(prop != NULL);
    EXPECT_STREQ("at", prop->signature.c_str());
    EXPECT_TRUE(prop->GetAnnotation("org.alljoyn.Bus.Type.Max", annotation));
    EXPECT_STREQ("70368744177664", annotation.c_str());

    static const char *introspectionXml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='uint32' type='u' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='0'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='4294967295'/>"
            "  </property>"
            "  <property name='int64' type='x' access='read'/>"
            "  <property name='uint64' type='t' access='read'/>"
            "  <property name='string' type='s' access='read'/>"
            "  <property name='objectPath' type='o' access='read'/>"
            "  <property name='signature' type='g' access='read'/>"
            "  <property name='arrayOfByte' type='ay' access='read'/>"
            "  <property name='variant' type='v' access='read'/>"
            "  <property name='arrayOfInt32' type='ai' access='read'/>"
            "  <property name='arrayOfInt64' type='ax' access='read'/>"
            "  <property name='struct' type='(ii)' access='read'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='[Point]'/>"
            "  </property>"
            "  <annotation name='org.alljoyn.Bus.Struct.Point.Field.x.Type' value='i'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.Point.Field.y.Type' value='i'/>"
            "</interface>";
    EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(introspectionXml));
    uint8_t out[8192];
    size_t outSize = 8192;
    EXPECT_EQ(CborNoError, Introspect(m_bus, "v16.10.00", "TITLE", "VERSION", out, &outSize));
    OCPayload *p;
    EXPECT_EQ(OC_STACK_OK, OCParsePayload(&p, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
            out, outSize));
    OCRepPayload *payload = (OCRepPayload *) p;
    OCRepPayload *definitions;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "definitions", &definitions));
    OCRepPayload *definition;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "x.org.iotivity.-interface.false",
            &definition));
    OCRepPayload *properties;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));

    OCRepPayload *property = NULL;
    char *s;
    double d;
    char **ss;
    size_t dim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload *items;

    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.uint32",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("integer", s);
        EXPECT_TRUE(OCRepPayloadGetPropDouble(property, "minimum", &d));
        EXPECT_EQ(0, d);
        EXPECT_TRUE(OCRepPayloadGetPropDouble(property, "maximum", &d));
        EXPECT_EQ(4294967295, d);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.int64",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "format", &s));
        EXPECT_STREQ("int64", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.uint64",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "format", &s));
        EXPECT_STREQ("uint64", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.string",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.objectPath",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.signature",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.arrayOfByte",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "media", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "binaryEncoding", &s));
        EXPECT_STREQ("base64", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.variant",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetStringArray(property, "type", &ss, dim));
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.arrayOfInt32",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("integer", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.arrayOfInt64",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("array", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(property, "items", &items));
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "type", &s));
        EXPECT_STREQ("string", s);
        EXPECT_TRUE(OCRepPayloadGetPropString(items, "format", &s));
        EXPECT_STREQ("int64", s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.struct",
            &property));
    if (property)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "$ref", &s));
        EXPECT_STREQ("#/definitions/Point", s);
        OCRepPayload *ps;
        EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "Point", &definition));
        EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &ps));
        EXPECT_TRUE(OCRepPayloadGetPropObject(ps, "x", &property));
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("integer", s);
        EXPECT_TRUE(OCRepPayloadGetPropObject(ps, "y", &property));
        EXPECT_TRUE(OCRepPayloadGetPropString(property, "type", &s));
        EXPECT_STREQ("integer", s);
    }
}

TEST_F(Introspection, SecureModeResource)
{
    FAIL();
}

TEST_F(Introspection, AllJoynObjectResource)
{
    FAIL();
}

TEST_F(Introspection, InvalidIntrospectionDoesNotParse)
{
    const char *introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": []"
            "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_FALSE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
}

TEST_F(Introspection, PropertyNamesAreEscaped)
{
    const char *introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/x.org.iotivity.rt\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"x.org.iotivity.rt\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"oneTwo\": { \"type\": \"integer\" },"
            "        \"one.two\": { \"type\": \"integer\" },"
            "        \"one-two\": { \"type\": \"integer\" },"
            "        \"struct\": { \"$ref\": \"#/definitions/StructName\" },"
            "        \"dict\": { \"type\": \"object\", \"properties\": { \"one.two\": { \"type\": \"integer\" }, \"one-two\": { \"type\": \"string\" } } },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"x.org.iotivity.rt\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    },"
            "    \"StructName\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"one.two\": { \"type\": \"integer\" },"
            "        \"one-two\": { \"type\": \"string\" }"
            "      }"
            "    }"
            "  }"
            "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface("org.iotivity.rt");
    EXPECT_TRUE(iface != NULL);

    EXPECT_TRUE(iface->GetProperty("oneTwo") != NULL);
    EXPECT_TRUE(iface->GetProperty("one_dtwo") != NULL);
    EXPECT_TRUE(iface->GetProperty("one_htwo") != NULL);

    qcc::String value;
    EXPECT_TRUE(iface->GetAnnotation("org.alljoyn.Bus.Struct.StructName.Field.one_dtwo.Type", value));
    EXPECT_TRUE(iface->GetAnnotation("org.alljoyn.Bus.Struct.StructName.Field.one_htwo.Type", value));

    static const char *introspectionXml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='dict' type='a{sv}' access='readwrite'/>"
            "  <property name='oneTwo' type='x' access='readwrite'/>"
            "  <property name='one_dtwo' type='x' access='readwrite'/>"
            "  <property name='one_htwo' type='x' access='readwrite'/>"
            "  <property name='struct' type='(xs)' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='[StructName]'/>"
            "  </property>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.one_dtwo.Type' value='x'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.one_htwo.Type' value='s'/>"
            "</interface>";
    EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(introspectionXml));
    uint8_t out[8192];
    size_t outSize = 8192;
    EXPECT_EQ(CborNoError, Introspect(m_bus, "v16.10.00", "TITLE", "VERSION", out, &outSize));
    OCPayload *p;
    EXPECT_EQ(OC_STACK_OK, OCParsePayload(&p, OC_FORMAT_CBOR, PAYLOAD_TYPE_REPRESENTATION,
            out, outSize));
    OCRepPayload *payload = (OCRepPayload *) p;
    OCRepPayload *definitions;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "definitions", &definitions));
    OCRepPayload *definition;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "x.org.iotivity.-interface.false",
            &definition));
    OCRepPayload *properties;
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));
    OCRepPayload *property;
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.dict",
            &property));
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.oneTwo",
            &property));
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.one.two",
            &property));
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.one-two",
            &property));
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "x.org.iotivity.-interface.false.struct",
            &property));

    EXPECT_TRUE(OCRepPayloadGetPropObject(definitions, "StructName",
            &definition));
    EXPECT_TRUE(OCRepPayloadGetPropObject(definition, "properties", &properties));
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "one.two",
            &property));
    EXPECT_TRUE(OCRepPayloadGetPropObject(properties, "one-two",
            &property));
}

TEST_F(Introspection, SimulatorIntrospectionParses)
{
    const char *introspectionJson =
        "{"
        "  \"swagger\": \"2.0\","
        "  \"info\": {"
        "    \"title\": \"Swagger for simulator\","
        "    \"version\": \"1.0\""
        "  },"
        "  \"paths\": {"
        "    \"/com/example/OCF/Types\": {"
        "      \"get\": {"
        "        \"responses\": {"
        "          \"200\": {"
        "            \"description\": \"\","
        "            \"schema\": {"
        "              \"$ref\": \"#/definitions/example\""
        "            }"
        "          }"
        "        },"
        "        \"parameters\": ["
        "          {"
        "            \"name\": \"if\","
        "            \"description\": \"\","
        "            \"enum\": ["
        "              \"oic.if.r\","
        "              \"oic.if.baseline\""
        "            ],"
        "            \"type\": \"string\","
        "            \"in\": \"query\""
        "          }"
        "        ]"
        "      }"
        "    }"
        "  },"
        "  \"definitions\": {"
        "    \"example\": {"
        "      \"type\": \"object\"," /* This is not currently present in the introspection data */
        "      \"properties\": {"
        "        \"myBoolean\": {"
        "          \"type\": \"boolean\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myObject\": {"
        "          \"type\": \"object\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myInt64\": {"
        "          \"type\": \"string\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"if\": {"
        "          \"type\": \"array\","
        "          \"items\": {"
        "            \"type\": \"string\""
        "          },"
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myDate\": {"
        "          \"type\": \"string\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myArray\": {"
        "          \"type\": \"array\","
        "          \"items\": {"
        "            \"type\": \"string\""
        "          },"
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"rt\": {"
        "          \"readOnly\": true,"
        "          \"description\": \"\","
        "          \"default\": ["
        "            \"x.com.example.-m----w.m---w.x--9-1\""
        "          ],"
        "          \"type\": \"array\","
        "          \"items\": {"
        "            \"type\": \"string\""
        "          }"
        "        },"
        "        \"myNumber\": {"
        "          \"type\": \"number\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myLanguageTag\": {"
        "          \"type\": \"string\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myUUID\": {"
        "          \"type\": \"string\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myUint64\": {"
        "          \"type\": \"string\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myCVS\": {"
        "          \"type\": \"string\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myInteger\": {"
        "          \"type\": \"integer\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        },"
        "        \"myString\": {"
        "          \"type\": \"string\","
        "          \"description\": \"\","
        "          \"readOnly\": true"
        "        }"
        "      }"
        "    }"
        "  }"
        "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
}

TEST_F(Introspection, WellDefinedResourceTypesIgnored)
{
    const char *introspectionJson =
            "{"
            "  \"swagger\": \"2.0\","
            "  \"info\": { \"title\": \"TITLE\", \"version\": \"VERSION\" },"
            "  \"paths\": {"
            "    \"/resource\": {"
            "      \"get\": {"
            "        \"parameters\": [ { \"name\": \"if\", \"in\": \"query\", \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } ],"
            "        \"responses\": { \"200\": { \"description\": \"\", \"schema\": { \"oneOf\": [ { \"$ref\": \"#/definitions/oic.r.switch.binary\" } ] } } }"
            "      }"
            "    }"
            "  },"
            "  \"definitions\": {"
            "    \"oic.r.switch.binary\": {"
            "      \"type\": \"object\","
            "      \"properties\": {"
            "        \"value\": { \"type\": \"boolean\" },"
            "        \"rt\": { \"readOnly\": true, \"type\": \"array\", \"default\": [ \"oic.r.switch.binary\" ] },"
            "        \"if\": { \"readOnly\": true, \"type\": \"array\", \"items\": { \"type\": \"string\", \"enum\": [ \"oic.if.baseline\" ] } }"
            "      }"
            "    }"
            "  }"
            "}";
    OCRepPayload *introspectionData;
    EXPECT_EQ(OC_STACK_OK, ParseJsonPayload(&introspectionData, introspectionJson));
    EXPECT_TRUE(ParseIntrospectionPayload(m_context->m_device, m_bus, introspectionData));
    const ajn::InterfaceDescription *iface = m_bus->GetInterface("oic.r.switch.binary");
    EXPECT_TRUE(iface == NULL);
}
