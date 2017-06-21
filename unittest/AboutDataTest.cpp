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

#include "AboutData.h"
#include "DeviceConfigurationResource.h"
#include "DeviceResource.h"
#include "Hash.h"
#include "PlatformConfigurationResource.h"
#include "PlatformResource.h"
#include "Plugin.h"
#include "ocstack.h"
#include "oic_malloc.h"

class AboutDataTest : public AJOCSetUp
{
public:
    virtual ~AboutDataTest() { }
};

TEST_F(AboutDataTest, IsValid)
{
    AboutData aboutData(NULL);
    EXPECT_TRUE(aboutData.IsValid());
}

TEST_F(AboutDataTest, SetFieldsFromDevice)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* di => AppId */
    const char *di = "7d529297-6f9f-83e8-aec0-72dd5392b584";
    const uint8_t appId[] = { 0x7d, 0x52, 0x92, 0x97, 0x6f, 0x9f, 0x83, 0xe8,
                              0xae, 0xc0, 0x72, 0xdd, 0x53, 0x92, 0xb5, 0x84 };
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEVICE_ID, di));
    /* d.dmn => Manufacturer (localized) */
    LocalizedString manufacturers[] = {
        { "en", "en-manufacturer" },
        { "fr", "fr-manufacturer" }
    };
    size_t dmnDim[MAX_REP_ARRAY_DEPTH] = { sizeof(manufacturers) / sizeof(manufacturers[0]), 0, 0 };
    size_t dimTotal = calcDimTotal(dmnDim);
    OCRepPayload **dmn = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        dmn[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(dmn[i], "language", manufacturers[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(dmn[i], "value", manufacturers[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_DEVICE_MFG_NAME, dmn, dmnDim));
    /* d.dmno => ModelNumber */
    const char *modelNumber = "model-number";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEVICE_MODEL_NUM, modelNumber));
    /* d.ld => Description (localized) */
    LocalizedString descriptions[] = {
        { "en", "en-description" },
        { "fr", "fr-description" }
    };
    size_t ldDim[MAX_REP_ARRAY_DEPTH] = { sizeof(descriptions) / sizeof(descriptions[0]), 0, 0 };
    dimTotal = calcDimTotal(ldDim);
    OCRepPayload **ld = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        ld[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(ld[i], "language", descriptions[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(ld[i], "value", descriptions[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_DEVICE_DESCRIPTION, ld, ldDim));
    /* d.piid => org.openconnectivity.piid */
    const char *piid = "c208d3b0-169b-4ace-bf5a-54ad2d6549f7";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, piid));
    /* d.sv => SoftwareVersion */
    const char *softwareVersion = "software-version";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_SOFTWARE_VERSION, softwareVersion));
    /* vendor-defined => vendor-defined with leading x removed */
    const char *vendorProperty = "x.org.iotivity.property";
    const char *vendorValue = "value";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "x.org.iotivity.property", "value"));

    AboutData aboutData(NULL);
    aboutData.Set(payload);
    EXPECT_TRUE(aboutData.IsValid());

    size_t nb;
    uint8_t *b;
    EXPECT_EQ(ER_OK, aboutData.GetAppId(&b, &nb));
    EXPECT_EQ(sizeof(appId), nb);
    EXPECT_EQ(0, memcmp(appId, b, nb));
    char *s;
    for (size_t i = 0; i < sizeof(manufacturers) / sizeof(manufacturers[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.GetManufacturer(&s, manufacturers[i].language));
        EXPECT_STREQ(manufacturers[i].value, s);
    }
    EXPECT_EQ(ER_OK, aboutData.GetModelNumber(&s));
    EXPECT_STREQ(modelNumber, s);
    for (size_t i = 0; i < sizeof(descriptions) / sizeof(descriptions[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.GetDescription(&s, descriptions[i].language));
        EXPECT_STREQ(descriptions[i].value, s);
    }
    ajn::MsgArg *msgArg;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.piid", msgArg));
    EXPECT_STREQ(piid, msgArg->v_string.str);
    EXPECT_EQ(ER_OK, aboutData.GetSoftwareVersion(&s));
    EXPECT_STREQ(softwareVersion, s);
    EXPECT_EQ(ER_OK, aboutData.GetField(&vendorProperty[2], msgArg));
    EXPECT_STREQ(vendorValue, msgArg->v_string.str);

    OCRepPayloadDestroy(payload);
}

TEST_F(AboutDataTest, SetFieldsFromPlatform)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* p.mndt => DateOfManufacture */
    const char *dateOfManufacture = "date-of-manufacture";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_MFG_DATE, dateOfManufacture));
    /* p.mnfv => org.openconnectivity.mnfv */
    const char *firmwareVersion = "firmware-version";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_FIRMWARE_VERSION, firmwareVersion));
    /* p.mnhw => HardwareVersion */
    const char *hardwareVersion = "hardware-version";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_HARDWARE_VERSION, hardwareVersion));
    /* p.mnml => org.openconnectivity.mnml */
    const char *manufacturerUrl = "manufacturer-url";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_MFG_URL, manufacturerUrl));
    /* p.mnos => org.openconnectivity.mnos */
    const char *osVersion = "os-version";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_OS_VERSION, osVersion));
    /* p.mnpv => org.openconnectivity.mnpv */
    const char *platformVersion = "platform-version";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_PLATFORM_VERSION, platformVersion));
    /* p.mnsl => SupportUrl */
    const char *supportUrl = "support-url";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_SUPPORT_URL, supportUrl));
    /* p.pi => DeviceId */
    const char *platformId = "platform-id";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_PLATFORM_ID, platformId));
    /* p.st => org.openconnectivity.st */
    const char *systemTime = "system-time";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_SYSTEM_TIME, systemTime));
    /* vendor-defined => vendor-defined with leading x removed */
    const char *vendorProperty = "x.org.iotivity.property";
    const char *vendorValue = "value";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "x.org.iotivity.property", "value"));

    AboutData aboutData(NULL);
    aboutData.Set(payload);
    EXPECT_TRUE(aboutData.IsValid());

    char *s;
    EXPECT_EQ(ER_OK, aboutData.GetDateOfManufacture(&s));
    EXPECT_STREQ(dateOfManufacture, s);
    ajn::MsgArg *msgArg;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnfv", msgArg));
    EXPECT_STREQ(firmwareVersion, msgArg->v_string.str);
    EXPECT_EQ(ER_OK, aboutData.GetHardwareVersion(&s));
    EXPECT_STREQ(hardwareVersion, s);
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnml", msgArg));
    EXPECT_STREQ(manufacturerUrl, msgArg->v_string.str);
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnos", msgArg));
    EXPECT_STREQ(osVersion, msgArg->v_string.str);
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.mnpv", msgArg));
    EXPECT_STREQ(platformVersion, msgArg->v_string.str);
    EXPECT_EQ(ER_OK, aboutData.GetSupportUrl(&s));
    EXPECT_STREQ(supportUrl, s);
    EXPECT_EQ(ER_OK, aboutData.GetDeviceId(&s));
    EXPECT_STREQ(platformId, s);
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.st", msgArg));
    EXPECT_STREQ(systemTime, msgArg->v_string.str);
    EXPECT_EQ(ER_OK, aboutData.GetField(&vendorProperty[2], msgArg));
    EXPECT_STREQ(vendorValue, msgArg->v_string.str);

    OCRepPayloadDestroy(payload);
}

TEST_F(AboutDataTest, SetFieldsFromDeviceConfiguration)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* con.dl => DefaultLanguage */
    const char *defaultLanguage = "en";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, defaultLanguage));
    /* con.ln => AppName (localized), SupportedLanguages */
    LocalizedString names[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    size_t lnDim[MAX_REP_ARRAY_DEPTH] = { sizeof(names) / sizeof(names[0]), 0, 0 };
    size_t dimTotal = calcDimTotal(lnDim);
    OCRepPayload **ln = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        ln[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "language", names[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "value", names[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, ln, lnDim));

    AboutData aboutData(defaultLanguage);
    aboutData.Set(payload);
    EXPECT_TRUE(aboutData.IsValid());

    char *s;
    EXPECT_EQ(ER_OK, aboutData.GetDefaultLanguage(&s));
    EXPECT_STREQ(defaultLanguage, s);
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.GetAppName(&s, names[i].language));
        EXPECT_STREQ(names[i].value, s);
    }
    size_t ns = sizeof(names) / sizeof(names[0]);
    EXPECT_EQ(ns, aboutData.GetSupportedLanguages());
    const char *ss[ns];
    EXPECT_EQ(ns, aboutData.GetSupportedLanguages(ss, ns));
    for (size_t i = 0; i < ns; ++i)
    {
        EXPECT_STREQ(names[i].language, ss[i]);
    }

    OCRepPayloadDestroy(payload);
}

TEST_F(AboutDataTest, SetFieldsFromPlatformConfiguration)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* con.p.mnpn => DeviceName (localized) */
    LocalizedString names[] = {
        { "en", "en-device-name" },
        { "fr", "fr-device-name" }
    };
    size_t mnpnDim[MAX_REP_ARRAY_DEPTH] = { sizeof(names) / sizeof(names[0]), 0, 0 };
    size_t dimTotal = calcDimTotal(mnpnDim);
    OCRepPayload **mnpn = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        mnpn[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[i], "language", names[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[i], "value", names[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_PLATFORM_NAME, mnpn, mnpnDim));

    AboutData aboutData(NULL);
    aboutData.Set(payload);
    EXPECT_TRUE(aboutData.IsValid());

    char *s;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.GetDeviceName(&s, names[i].language));
        EXPECT_STREQ(names[i].value, s);
    }

    OCRepPayloadDestroy(payload);
}

TEST_F(AboutDataTest, UseNameWhenLocalizedNamesNotPresent)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* n => AppName (localized) */
    const char *name = "name";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEVICE_NAME, name));

    AboutData aboutData(NULL);
    aboutData.Set(payload);
    EXPECT_TRUE(aboutData.IsValid());

    char *s;
    EXPECT_EQ(ER_OK, aboutData.GetAppName(&s));
    EXPECT_STREQ(name, s);

    OCRepPayloadDestroy(payload);
}

TEST_F(AboutDataTest, UseLocalizedNamesWhenNamePresent)
{
    OCRepPayload *device = OCRepPayloadCreate();
    /* n => AppName (localized) */
    const char *name = "name";
    EXPECT_TRUE(OCRepPayloadSetPropString(device, OC_RSRVD_DEVICE_NAME, name));
    OCRepPayload *config = OCRepPayloadCreate();
    /* con.ln => AppName (localized), SupportedLanguages */
    LocalizedString names[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    size_t lnDim[MAX_REP_ARRAY_DEPTH] = { sizeof(names) / sizeof(names[0]), 0, 0 };
    size_t dimTotal = calcDimTotal(lnDim);
    OCRepPayload **ln = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        ln[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "language", names[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "value", names[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(config, OC_RSRVD_DEVICE_NAME_LOCALIZED, ln, lnDim));

    AboutData aboutData(NULL);
    aboutData.Set(device);
    aboutData.Set(config);
    EXPECT_TRUE(aboutData.IsValid());

    size_t ns = sizeof(names) / sizeof(names[0]);
    EXPECT_EQ(ns, aboutData.GetSupportedLanguages());
    char *s;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.GetAppName(&s, names[i].language));
        EXPECT_STREQ(names[i].value, s);
    }

    OCRepPayloadDestroy(config);
    OCRepPayloadDestroy(device);
}

class ConfigDataTest : public AJOCSetUp
{
public:
    virtual ~ConfigDataTest() { }
};

TEST_F(ConfigDataTest, SetFieldsFromDeviceConfiguration)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* dl => DefaultLanguage */
    const char *defaultLanguage = "en";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, defaultLanguage));
    /* loc => org.openconnectivity.loc */
    double location[] = { -1.0, 1.0 };
    size_t locationDim[MAX_REP_ARRAY_DEPTH] = { sizeof(location) / sizeof(location[0]), 0, 0 };
    EXPECT_TRUE(OCRepPayloadSetDoubleArray(payload, OC_RSRVD_LOCATION, location, locationDim));
    /* locn => org.openconnectivity.locn */
    const char *locationName = "location-name";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_LOCATION_NAME, locationName));
    /* c => org.openconnectivity.c */
    const char *currency = "currency";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_CURRENCY, currency));
    /* r => org.openconnectivity.r */
    const char *region = "region";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_REGION, region));
    /* vendor-defined => */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    const char *vendorValue = "value";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, vendorProperty, vendorValue));
    // TODO other OC types

    AboutData aboutData(NULL);
    aboutData.Set(payload);
    EXPECT_TRUE(aboutData.IsValid());

    char *s;
    EXPECT_EQ(ER_OK, aboutData.GetDefaultLanguage(&s));
    EXPECT_STREQ(defaultLanguage, s);
    ajn::MsgArg *arg;
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.loc", arg));
    double *ds;
    size_t nd;
    EXPECT_EQ(ER_OK, arg->Get("ad", &nd, &ds));
    EXPECT_EQ(sizeof(location) / sizeof(location[0]), nd);
    for (size_t i = 0; i < sizeof(location) / sizeof(location[0]); ++i)
    {
        EXPECT_EQ(location[i], ds[i]);
    }
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.locn", arg));
    EXPECT_EQ(ER_OK, arg->Get("s", &s));
    EXPECT_STREQ(locationName, s);
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.c", arg));
    EXPECT_EQ(ER_OK, arg->Get("s", &s));
    EXPECT_STREQ(currency, s);
    EXPECT_EQ(ER_OK, aboutData.GetField("org.openconnectivity.r", arg));
    EXPECT_EQ(ER_OK, arg->Get("s", &s));
    EXPECT_STREQ(region, s);
    EXPECT_EQ(ER_OK, aboutData.GetField(vendorField, arg));
    EXPECT_EQ(ER_OK, arg->Get("s", &s));
    EXPECT_STREQ(vendorValue, s);
}

TEST_F(ConfigDataTest, SetFieldsFromPlatformConfiguration)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* mnpn => DeviceName */
    LocalizedString names[] = {
        { "en", "en-device-name" },
        { "fr", "fr-device-name" }
    };
    size_t mnpnDim[MAX_REP_ARRAY_DEPTH] = { sizeof(names) / sizeof(names[0]), 0, 0 };
    size_t dimTotal = calcDimTotal(mnpnDim);
    OCRepPayload **mnpn = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        mnpn[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[i], "language", names[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[i], "value", names[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_PLATFORM_NAME, mnpn, mnpnDim));
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    const char *vendorValue = "value";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, vendorProperty, vendorValue));
    // TODO other OC types

    AboutData aboutData("en");
    aboutData.Set(payload);
    EXPECT_TRUE(aboutData.IsValid());

    char *s;
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.GetDeviceName(&s, names[i].language));
        EXPECT_STREQ(names[i].value, s);
    }
    ajn::MsgArg *arg;
    EXPECT_EQ(ER_OK, aboutData.GetField(vendorField, arg));
    EXPECT_EQ(ER_OK, arg->Get("s", &s));
    EXPECT_STREQ(vendorValue, s);
}

TEST_F(ConfigDataTest, WithoutDefaultOrSupportedLanguages)
{
    const char *name = "app-name";
    ajn::MsgArg entry;
    entry.typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry.v_dictEntry.key = new ajn::MsgArg("s", "AppName");
    entry.v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", name));
    entry.SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    ajn::MsgArg dict;
    dict.typeId = ajn::ALLJOYN_ARRAY;
    dict.v_array.SetElements("{sv}", 1, &entry);

    const char *lang = "";
    AboutData aboutData(&dict, lang);
    char *s;
    EXPECT_EQ(ER_OK, aboutData.GetAppName(&s, lang));
    EXPECT_STREQ(name, s);

    ajn::MsgArg arg, elem;
    EXPECT_EQ(ER_OK, aboutData.GetConfigData(&arg, lang));
    EXPECT_NE(ER_OK, arg.GetElement("{sv}", "DefaultLanguage", &elem));
    EXPECT_NE(ER_OK, arg.GetElement("{sv}", "SupportedLanguages", &elem));
}

class DeviceProperties : public AJOCSetUp
{
public:
    virtual ~DeviceProperties() { }
};

TEST_F(DeviceProperties, SetFromAboutData)
{
    AboutData aboutData("");
    /* AppName => n */
    const char *appName = "app-name";
    EXPECT_EQ(ER_OK, aboutData.SetAppName(appName));
    /* org.openconnectivity.piid => piid */
    const char *piid = "10f70cc4-2398-41f5-8062-4c1facbfc41b";
    EXPECT_EQ(ER_OK, aboutData.SetProtocolIndependentId(piid));
    const char *peerGuid = "10f70cc4239841f580624c1facbfc41b";
    const char *deviceId = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(deviceId));
    const uint8_t appId[] = { 0x46, 0xe8, 0x0b, 0xf8, 0x9f, 0xf5, 0x47, 0x8a,
                              0xbe, 0x9f, 0x7f, 0xa3, 0x4a, 0xdc, 0x49, 0x7b };
    EXPECT_EQ(ER_OK, aboutData.SetAppId(appId, sizeof(appId) / sizeof(appId[0])));
    /* Version(s) => dmv */
    const char *dmvs[] = { "org.iotivity.A.1", "org.iotivity.B.2", "org.iotivity.C.3", "org.iotivity.D.4" };
    const char *ifs[] = {
        "<interface name='org.iotivity.A'/>",
        "<interface name='org.iotivity.B'>"
        "  <annotation name='org.gtk.GDBus.Since' value='2'/>"
        "</interface>",
        "<interface name='org.iotivity.C'>"
        "  <annotation name='org.gtk.GDBus.Since' value='3'/>"
        "</interface>",
        "<interface name='org.iotivity.D'>"
        "  <annotation name='org.gtk.GDBus.Since' value='4'/>"
        "</interface>",
    };
    ajn::BusAttachment *bus = new ajn::BusAttachment("DeviceProperties.SetFromAboutData");
    for (size_t i = 0; i < sizeof(ifs) / sizeof(ifs[0]); ++i)
    {
        EXPECT_EQ(ER_OK, bus->CreateInterfacesFromXml(ifs[i]));
    }
    const char *ifsOne[] = { "org.iotivity.A", "org.iotivity.B" };
    const char *ifsTwo[] = { "org.iotivity.B", "org.iotivity.C", "org.iotivity.D" };
    ajn::MsgArg os[2];
    os[0].Set("(oas)", "/one", sizeof(ifsOne) / sizeof(ifsOne[0]), ifsOne);
    os[1].Set("(oas)", "/two", sizeof(ifsTwo) / sizeof(ifsTwo[0]), ifsTwo);
    ajn::MsgArg odArg;
    EXPECT_EQ(ER_OK, odArg.Set("a(oas)", sizeof(os) / sizeof(os[0]), os));
    ajn::AboutObjectDescription objectDescription(odArg);
    /* Description => ld */
    LocalizedString descriptions[] = {
        { "en", "en-description" },
        { "fr", "fr-description" }
    };
    for (size_t i = 0; i < sizeof(descriptions) / sizeof(descriptions[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetDescription(descriptions[i].value, descriptions[i].language));
    }
    /* SoftwareVersion => sv */
    const char *softwareVersion = "software-version";
    EXPECT_EQ(ER_OK, aboutData.SetSoftwareVersion(softwareVersion));
    /* Manufacturer => dmn */
    LocalizedString manufacturers[] = {
        { "en", "en-manufacturer" },
        { "fr", "fr-manufacturer" }
    };
    for (size_t i = 0; i < sizeof(manufacturers) / sizeof(manufacturers[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetManufacturer(manufacturers[i].value, manufacturers[i].language));
    }
    /* ModelNumber => dmno */
    const char *modelNumber = "model-number";
    EXPECT_EQ(ER_OK, aboutData.SetModelNumber(modelNumber));
    /* vendor-defined => x. */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    const char *vendorValue = "value";
    ajn::MsgArg vendorArg("s", vendorValue);
    EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg));

    EXPECT_EQ(OC_STACK_OK, SetDeviceProperties(bus, &objectDescription, &aboutData, peerGuid));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, (void**) &s));
    EXPECT_STREQ(appName, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, (void**) &s));
    EXPECT_STREQ(piid, s);
    OCStringLL *ll;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DATA_MODEL_VERSION, (void**) &ll));
    for (size_t i = 0; i < sizeof(dmvs) / sizeof(dmvs[0]); ++i)
    {
        EXPECT_STREQ(dmvs[i], ll->value);
        ll = ll->next;
    }
    EXPECT_TRUE(ll == NULL);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_DESCRIPTION, (void**) &ll));
    for (size_t i = 0; i < sizeof(descriptions) / sizeof(descriptions[0]); ++i)
    {
        EXPECT_STREQ(descriptions[i].language, ll->value);
        ll = ll->next;
        EXPECT_STREQ(descriptions[i].value, ll->value);
        ll = ll->next;
    }
    EXPECT_TRUE(ll == NULL);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SOFTWARE_VERSION, (void**) &s));
    EXPECT_STREQ(softwareVersion, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MFG_NAME, (void**) &ll));
    for (size_t i = 0; i < sizeof(manufacturers) / sizeof(manufacturers[0]); ++i)
    {
        EXPECT_STREQ(manufacturers[i].language, ll->value);
        ll = ll->next;
        EXPECT_STREQ(manufacturers[i].value, ll->value);
        ll = ll->next;
    }
    EXPECT_TRUE(ll == NULL);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MODEL_NUM, (void**) &s));
    EXPECT_STREQ(modelNumber, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, vendorProperty, (void**) &s));
    EXPECT_STREQ(vendorValue, s);

    delete bus;
}

TEST_F(DeviceProperties, NonStringVendorField)
{
    AboutData aboutData("");
    /* vendor-defined => x. */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    int32_t vendorValue = 1;
    ajn::MsgArg vendorArg("i", vendorValue);
    EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg));

    EXPECT_EQ(OC_STACK_OK, SetDeviceProperties(NULL, NULL, &aboutData, NULL));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, vendorProperty, (void**) &s));
    // TODO Verify vendorValue when get passes
}

TEST_F(DeviceProperties, LocalizedVendorField)
{
    AboutData aboutData("");
    /* vendor-defined => x. */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    LocalizedString vendorValues[] = {
        { "en", "en-vendor" },
        { "fr", "fr-vendor" }
    };
    for (size_t i = 0; i < sizeof(vendorValues) / sizeof(vendorValues[0]); ++i)
    {
        ajn::MsgArg vendorArg("s", vendorValues[i].value);
        EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg, vendorValues[i].language));
    }

    EXPECT_EQ(OC_STACK_OK, SetDeviceProperties(NULL, NULL, &aboutData, NULL));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, vendorProperty, (void**) &s));
    // TODO Verify vendorValues when get passes
}

class PlatformProperties : public AJOCSetUp
{
public:
    virtual ~PlatformProperties() { }
};

TEST_F(PlatformProperties, SetFromAboutData)
{
    AboutData aboutData("fr");
    /* DeviceId, if it is UUID => pi */
    const char *deviceId = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(deviceId));
    /* Manufacturer in DefaultLanguage and truncated to 16 bytes => mnmn */
    LocalizedString manufacturers[] = {
        { "en", "en-manufacturer-name" },
        { "fr", "fr-manufacturer-name" }
    };
    for (size_t i = 0; i < sizeof(manufacturers) / sizeof(manufacturers[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetManufacturer(manufacturers[i].value, manufacturers[i].language));
    }
    /* org.openconnectivity.mnml => mnml */
    const char *manufacturerUrl = "manufacturer-url";
    EXPECT_EQ(ER_OK, aboutData.SetManufacturerUrl(manufacturerUrl));
    /* ModelNumber => mnmo */
    const char *modelNumber = "model-number";
    EXPECT_EQ(ER_OK, aboutData.SetModelNumber(modelNumber));
    /* DateOfManufacture => mndt */
    const char *dateOfManufacture = "date-of-manufacture";
    EXPECT_EQ(ER_OK, aboutData.SetDateOfManufacture(dateOfManufacture));
    /* org.openconnectivity.mnpv => mnpv */
    const char *platformVersion = "platform-version";
    EXPECT_EQ(ER_OK, aboutData.SetPlatformVersion(platformVersion));
    /* org.openconnectivity.mnos => mnos */
    const char *osVersion = "os-version";
    EXPECT_EQ(ER_OK, aboutData.SetOperatingSystemVersion(osVersion));
    /* HardwareVersion => mnhw */
    const char *hardwareVersion = "hardware-version";
    EXPECT_EQ(ER_OK, aboutData.SetHardwareVersion(hardwareVersion));
    /* org.openconnectivity.mnfv => mnfv */
    const char *firmwareVersion = "firmware-version";
    EXPECT_EQ(ER_OK, aboutData.SetFirmwareVersion(firmwareVersion));
    /* SupportUrl => mnsl */
    const char *supportUrl = "support-url";
    EXPECT_EQ(ER_OK, aboutData.SetSupportUrl(supportUrl));
    /* org.openconnectivity.st => st */
    const char *systemTime = "system-time";
    EXPECT_EQ(ER_OK, aboutData.SetSystemTime(systemTime));

    EXPECT_EQ(OC_STACK_OK, SetPlatformProperties(&aboutData));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, (void**) &s));
    EXPECT_STREQ(deviceId, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_NAME, (void**) &s));
    EXPECT_EQ((size_t) 16, strlen(s));
    EXPECT_EQ(0, strncmp(manufacturers[1].value, s, 16));
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_URL, (void**) &s));
    EXPECT_STREQ(manufacturerUrl, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MODEL_NUM, (void**) &s));
    EXPECT_STREQ(modelNumber, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, (void**) &s));
    EXPECT_STREQ(dateOfManufacture, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_VERSION, (void**) &s));
    EXPECT_STREQ(platformVersion, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_OS_VERSION, (void**) &s));
    EXPECT_STREQ(osVersion, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, (void**) &s));
    EXPECT_STREQ(hardwareVersion, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_FIRMWARE_VERSION, (void**) &s));
    EXPECT_STREQ(firmwareVersion, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, (void**) &s));
    EXPECT_STREQ(supportUrl, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SYSTEM_TIME, (void**) &s));
    EXPECT_STREQ(systemTime, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_VID, (void**) &s));
    EXPECT_STREQ(deviceId, s);
}

TEST_F(PlatformProperties, UseHashForPlatformId)
{
    AboutData aboutData("");
    /* Hash(DeviceId), if DeviceId is not UUID => pi */
    const char *deviceId = "device-id";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(deviceId));
    OCUUIdentity id;
    Hash(&id, deviceId, NULL, 0);
    char pi[UUID_STRING_SIZE];
    OCConvertUuidToString(id.id, pi);

    EXPECT_EQ(OC_STACK_OK, SetPlatformProperties(&aboutData));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, (void**) &s));
    EXPECT_STREQ(pi, s);
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_VID, (void**) &s));
    EXPECT_STREQ(deviceId, s);
}

class DeviceConfigurationProperties : public AJOCSetUp
{
public:
    virtual ~DeviceConfigurationProperties() { }
};

TEST_F(DeviceConfigurationProperties, SetFromAboutData)
{
    AboutData aboutData("fr");
    /* AppName => n */
    LocalizedString names[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(names[i].value, names[i].language));
    }
    /* org.openconnectivity.loc => loc */
    double latitude = -1.0;
    double longitude = 1.0;
    EXPECT_EQ(ER_OK, aboutData.SetLocation(latitude, longitude));
    /* org.openconnectivity.locn => locn */
    const char *locationName = "location-name";
    EXPECT_EQ(ER_OK, aboutData.SetLocationName(locationName));
    /* org.openconnectivity.c => c */
    const char *currency = "currency";
    EXPECT_EQ(ER_OK, aboutData.SetCurrency(currency));
    /* org.openconnectivity.r => r */
    const char *region = "region";
    EXPECT_EQ(ER_OK, aboutData.SetRegion(region));
    /* AppName => ln */
    /* DefaultLanguage => dl */
    /* vendor-defined => x. */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    const char *vendorValue = "value";
    ajn::MsgArg vendorArg("s", vendorValue);
    EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg));

    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_EQ(OC_STACK_OK, SetDeviceConfigurationProperties(payload, &aboutData));

    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
    EXPECT_STREQ(names[1].value, s);
    size_t locDim[MAX_REP_ARRAY_DEPTH];
    double *locArr;
    EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, OC_RSRVD_LOCATION, &locArr, locDim));
    EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
    EXPECT_EQ(latitude, locArr[0]);
    EXPECT_EQ(longitude, locArr[1]);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_LOCATION_NAME, &s));
    EXPECT_STREQ(locationName, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_CURRENCY, &s));
    EXPECT_STREQ(currency, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_REGION, &s));
    EXPECT_STREQ(region, s);
    size_t lnDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **lnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, "ln", &lnArr, lnDim));
    EXPECT_EQ(sizeof(names) / sizeof(names[0]), calcDimTotal(lnDim));
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "language", &s));
        EXPECT_STREQ(names[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "value", &s));
        EXPECT_STREQ(names[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "dl", &s));
    EXPECT_STREQ(names[1].language, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, vendorProperty, &s));
    EXPECT_STREQ(vendorValue, s);
}

TEST_F(DeviceConfigurationProperties, SetFromMsgArg)
{
    ajn::MsgArg entries[6];
    ajn::MsgArg *entry = entries;
    const char *name = "app-name";
    entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry->v_dictEntry.key = new ajn::MsgArg("s", "AppName");
    entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", name));
    entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    ++entry;
    double loc[2] = { -1.0, 1.0 };
    entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.loc");
    entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("ad", 2, loc));
    entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    ++entry;
    const char *locn = "locn";
    entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.locn");
    entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", locn));
    entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    ++entry;
    const char *c = "c";
    entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.c");
    entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", c));
    entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    ++entry;
    const char *r = "r";
    entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.r");
    entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", r));
    entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    ++entry;
    const char *vendorField = "org.iotivity.Field";
    const char *vendorValue = "vendor-value";
    entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry->v_dictEntry.key = new ajn::MsgArg("s", vendorField);
    entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", vendorValue));
    entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    ++entry;
    ajn::MsgArg dict;
    dict.typeId = ajn::ALLJOYN_ARRAY;
    dict.v_array.SetElements("{sv}", entry - entries, entries);

    const char *lang = "";
    AboutData aboutData(&dict, lang);

    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_EQ(OC_STACK_OK, SetDeviceConfigurationProperties(payload, &aboutData));

    /* Verify */
    size_t numValues = 0;
    for (OCRepPayloadValue *value = payload->values; value; value = value->next)
    {
        ++numValues;
    }
    EXPECT_EQ((size_t) (entry - entries) + 1 /* ln from AppName */, numValues);
    char *s = NULL;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
    EXPECT_STREQ(name, s);
    size_t locDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    double *locArr = NULL;
    EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, OC_RSRVD_LOCATION, &locArr, locDim));
    EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
    EXPECT_EQ(loc[0], locArr[0]);
    EXPECT_EQ(loc[1], locArr[1]);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_LOCATION_NAME, &s));
    EXPECT_STREQ(locn, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_CURRENCY, &s));
    EXPECT_STREQ(c, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_REGION, &s));
    EXPECT_STREQ(r, s);
    size_t lnDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **lnArr = NULL;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, "ln", &lnArr, lnDim));
    EXPECT_EQ(1u, calcDimTotal(lnDim));
    EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[0], "language", &s));
    EXPECT_STREQ("", s);
    EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[0], "value", &s));
    EXPECT_STREQ(name, s);
}

TEST_F(DeviceConfigurationProperties, NonStringVendorField)
{
    AboutData aboutData("");
    /* vendor-defined => x. */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    int32_t vendorValue = 1;
    ajn::MsgArg vendorArg("i", vendorValue);
    EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg));

    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_EQ(OC_STACK_OK, SetDeviceConfigurationProperties(payload, &aboutData));

    int64_t i;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, vendorProperty, &i));
    // TODO Verify vendorValue when get passes
}

TEST_F(DeviceConfigurationProperties, LocalizedVendorField)
{
    AboutData aboutData("");
    /* vendor-defined => x. */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    LocalizedString vendorValues[] = {
        { "en", "en-vendor" },
        { "fr", "fr-vendor" }
    };
    for (size_t i = 0; i < sizeof(vendorValues) / sizeof(vendorValues[0]); ++i)
    {
        ajn::MsgArg vendorArg("s", vendorValues[i].value);
        EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg, vendorValues[i].language));
    }

    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_EQ(OC_STACK_OK, SetDeviceConfigurationProperties(payload, &aboutData));

    OCRepPayload **arr;
    size_t dim[MAX_REP_ARRAY_DEPTH];
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, vendorProperty, &arr, dim));
    // TODO Verify vendorValues when get passes
}

class PlatformConfigurationProperties : public AJOCSetUp
{
public:
    virtual ~PlatformConfigurationProperties() { }
};

TEST_F(PlatformConfigurationProperties, SetFromAboutData)
{
    AboutData aboutData("fr");
    /* DeviceName => n */
    LocalizedString names[] = {
        { "en", "en-device-name" },
        { "fr", "fr-device-name" }
    };
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetDeviceName(names[i].value, names[i].language));
    }

    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_EQ(OC_STACK_OK, SetPlatformConfigurationProperties(payload, &aboutData));

    size_t dim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **arr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, "mnpn", &arr, dim));
    EXPECT_EQ(sizeof(names) / sizeof(names[0]), calcDimTotal(dim));
    for (size_t i = 0; i < sizeof(names) / sizeof(names[0]); ++i)
    {
        char *s;
        EXPECT_TRUE(OCRepPayloadGetPropString(arr[i], "language", &s));
        EXPECT_STREQ(names[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(arr[i], "value", &s));
        EXPECT_STREQ(names[i].value, s);
    }
}
