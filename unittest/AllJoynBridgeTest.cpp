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
#include "Interfaces.h"
#include "Introspection.h"
#include "Name.h"
#include "Payload.h"
#include "PlatformConfigurationResource.h"
#include "PlatformResource.h"
#include "Plugin.h"
#include "Resource.h"
#include "VirtualConfigBusObject.h"
#include "VirtualConfigurationResource.h"
#include "VirtualDevice.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "oic_time.h"
#include <alljoyn/AboutData.h>
#include <alljoyn/BusAttachment.h>
#include <thread>

class MethodCall : public ajn::MessageReceiver
{
public:
    MethodCall(ajn::BusAttachment *bus, ajn::ProxyBusObject *proxyObj)
        : m_proxyObj(proxyObj), m_reply(*bus), m_called(false) { }
    QStatus Call(const char *iface, const char *method, const ajn::MsgArg *args, size_t numArgs)
    {
        return m_proxyObj->MethodCallAsync(iface, method, this,
                static_cast<MessageReceiver::ReplyHandler>(&MethodCall::ReplyHandler), args,
                numArgs);
    }
    OCStackResult Wait(long waitMs)
    {
        uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
        while (!m_called)
        {
            uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
            long elapsed = (long)(currTime - startTime);
            if (elapsed > waitMs)
            {
                break;
            }
            OCProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        if (!m_called)
        {
            return OC_STACK_TIMEOUT;
        }
        if (m_reply->GetType() != ajn::MESSAGE_METHOD_RET)
        {
            return OC_STACK_ERROR;
        }
        return OC_STACK_OK;
    }
    ajn::Message &Reply() { return m_reply; }
private:
    ajn::ProxyBusObject *m_proxyObj;
    ajn::Message m_reply;
    bool m_called;
    void ReplyHandler(ajn::Message &reply, void *context)
    {
        (void) context;
        m_reply = reply;
        m_called = true;
    }
};

static void Wait(long waitMs)
{
    uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
    for (;;)
    {
        uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
        long elapsed = (long)(currTime - startTime);
        if (elapsed > waitMs)
        {
            break;
        }
        OCProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
}

TEST(IsValidErrorNameTest, Check)
{
    const char *endp;

    EXPECT_TRUE(IsValidErrorName("a.b", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("A.b", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("_.b", &endp) && (*endp == '\0'));
    EXPECT_FALSE(IsValidErrorName("0.b", &endp));

    EXPECT_TRUE(IsValidErrorName("aa.bb", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("aA.bB", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("a_.b_", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("a0.b0", &endp) && (*endp == '\0'));

    EXPECT_FALSE(IsValidErrorName("", &endp));
    EXPECT_FALSE(IsValidErrorName(".", &endp));
    EXPECT_FALSE(IsValidErrorName("a.", &endp));
    EXPECT_FALSE(IsValidErrorName("a..b", &endp));

    EXPECT_TRUE(IsValidErrorName("a.b ", &endp) && (*endp == ' '));
    EXPECT_TRUE(IsValidErrorName("a.b:", &endp) && (*endp == ':'));
}

class AboutDataTest : public AJOCSetUp { };

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

class ConfigDataTest : public AJOCSetUp { };

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
    /* TODO other OC types */

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
    /* TODO other OC types */

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

class DeviceProperties : public AJOCSetUp { };

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
    /* TODO Verify vendorValue when get passes */
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
    /* TODO Verify vendorValues when get passes */
}

class PlatformProperties : public AJOCSetUp { };

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

class DeviceConfigurationProperties : public AJOCSetUp { };

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
    /* TODO Verify vendorValue when get passes */
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
    /* TODO Verify vendorValues when get passes */
}

class PlatformConfigurationProperties : public AJOCSetUp { };

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

class VirtualProducer
    : public ajn::SessionPortListener, public AJOCSetUp
{
protected:
    ajn::BusAttachment *m_bus;
    ajn::SessionPort m_port;
    ajn::SessionOpts m_opts;
    virtual void SetUp()
    {
        AJOCSetUp::SetUp();
        m_bus = new ajn::BusAttachment("Consumer");
        EXPECT_EQ(ER_OK, m_bus->Start());
        EXPECT_EQ(ER_OK, m_bus->Connect());
        m_port = ajn::SESSION_PORT_ANY;
        EXPECT_EQ(ER_OK, m_bus->BindSessionPort(m_port, m_opts, *this));
    }
    virtual void TearDown()
    {
        delete m_bus;
        AJOCSetUp::TearDown();
    }
    virtual bool AcceptSessionJoiner(ajn::SessionPort port, const char *name,
            const ajn::SessionOpts& opts)
    {
        (void) port;
        (void) name;
        (void) opts;
        return true;
    }
};

class TestResource
{
public:
    void DoResponse(OCEntityHandlerRequest *request, OCRepPayload *payload)
    {
        OCEntityHandlerResponse response;
        memset(&response, 0, sizeof(response));
        response.requestHandle = request->requestHandle;
        response.resourceHandle = request->resource;
        response.ehResult = OC_EH_OK;
        response.payload = reinterpret_cast<OCPayload *>(payload);
        EXPECT_EQ(OC_STACK_OK, OCDoResponse(&response));
    }
};

class ConfigurationResource : public TestResource
{
public:
    ConfigurationResource(bool createResources = true, bool separateResources = false,
            ExpectedProperties *properties = NULL)
        : m_includeOptionalProperties(false), m_con(NULL), m_conp(NULL), m_mnt(NULL),
          m_properties(properties)
    {
        if (createResources)
        {
            /*
             * TODO discovery with large /oic/res responses doesn't work over localhost, so only the
             * first resource is OC_DISCOVERABLE and the tests derive the others.
             */
            // TODO this may not be true when discover /oic/res instead of 127.0.0.1/oic/res
            EXPECT_EQ(OC_STACK_OK, CreateResource(&m_con, "/con", "oic.wk.con", "oic.if.rw",
                    ConfigurationResource::ConfigurationHandler, this, OC_DISCOVERABLE));
            if (separateResources)
            {
                EXPECT_EQ(OC_STACK_OK, CreateResource(&m_conp, "/con/p", "oic.wk.con", "oic.if.rw",
                        ConfigurationResource::ConfigurationHandler, this, 0));
            }
            else
            {
                EXPECT_EQ(OC_STACK_OK, OCBindResourceTypeToResource(m_con, "oic.wk.con.p"));
            }
            EXPECT_EQ(OC_STACK_OK, CreateResource(&m_mnt, "/oic/mnt", "oic.wk.mnt", "oic.if.rw",
                    ConfigurationResource::MaintenanceHandler, this, 0));
            EXPECT_EQ(OC_STACK_OK, OCBindResourceInterfaceToResource(m_mnt, "oic.if.r"));
        }
    }
    ~ConfigurationResource()
    {
        OCDeleteResource(m_mnt);
        OCDeleteResource(m_conp);
        OCDeleteResource(m_con);
    }
    void SetMandatoryProperties() { m_includeOptionalProperties = false; }
    void SetAllProperties() { m_includeOptionalProperties = true; }
    void SetMandatoryDeviceProperties(OCRepPayload *payload)
    {
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "n", "en-name"));
    }
    void SetOptionalDeviceProperties(OCRepPayload *payload)
    {
        double loc[2] = { -1.0, 1.0 };
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
        EXPECT_TRUE(OCRepPayloadSetDoubleArray(payload, OC_RSRVD_LOCATION, loc, dim));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_LOCATION_NAME, "locn"));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_CURRENCY, "c"));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_REGION, "r"));
        OCRepPayload **ln = (OCRepPayload **) OICCalloc(2, sizeof(OCRepPayload*));
        ln[0] = OCRepPayloadCreate();
        EXPECT_TRUE(ln[0] != NULL);
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[0], "language", "en"));
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[0], "value", "en-name"));
        ln[1] = OCRepPayloadCreate();
        EXPECT_TRUE(ln[1] != NULL);
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[1], "language", "fr"));
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[1], "value", "fr-name"));
        EXPECT_TRUE(OCRepPayloadSetPropObjectArray(payload, "ln", (const OCRepPayload **) ln, dim));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "dl", "en"));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "x.org.iotivity.property", "value"));
    }
    void SetOptionalPlatformProperties(OCRepPayload *payload)
    {
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
        OCRepPayload **mnpn = (OCRepPayload **) OICCalloc(2, sizeof(OCRepPayload*));
        mnpn[0] = OCRepPayloadCreate();
        EXPECT_TRUE(mnpn[0] != NULL);
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[0], "language", "en"));
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[0], "value", "en-name"));
        mnpn[1] = OCRepPayloadCreate();
        EXPECT_TRUE(mnpn[1] != NULL);
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[1], "language", "fr"));
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[1], "value", "fr-name"));
        EXPECT_TRUE(OCRepPayloadSetPropObjectArray(payload, "mnpn", (const OCRepPayload **) mnpn, dim));
    }
    void SetMandatoryMaintenanceProperties(OCRepPayload *payload)
    {
        EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "fr", false));
        EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "rb", false));
    }
    void VerifyDeviceProperties(OCRepPayload *payload)
    {
        char *s;
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, &s));
        EXPECT_STREQ(m_properties->dl, s);
        size_t locDim[MAX_REP_ARRAY_DEPTH];
        double *locArr;
        EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, OC_RSRVD_LOCATION, &locArr, locDim));
        EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
        EXPECT_EQ(m_properties->loc[0], locArr[0]);
        EXPECT_EQ(m_properties->loc[1], locArr[1]);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_LOCATION_NAME, &s));
        EXPECT_STREQ(m_properties->locn, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_CURRENCY, &s));
        EXPECT_STREQ(m_properties->c, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_REGION, &s));
        EXPECT_STREQ(m_properties->r, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, m_properties->vendorProperty, &s));
        EXPECT_STREQ(m_properties->vendorValue, s);
    }
    void VerifyPlatformProperties(OCRepPayload *payload)
    {
        size_t lnDim[MAX_REP_ARRAY_DEPTH];
        OCRepPayload **lnArr;
        EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, "mnpn", &lnArr, lnDim));
        EXPECT_EQ(m_properties->nmnpn, calcDimTotal(lnDim));
        for (size_t i = 0; i < m_properties->nmnpn; ++i)
        {
            char *s;
            EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "language", &s));
            EXPECT_STREQ(m_properties->mnpn[i].language, s);
            EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "value", &s));
            EXPECT_STREQ(m_properties->mnpn[i].value, s);
        }
    }
    static OCEntityHandlerResult ConfigurationHandler(OCEntityHandlerFlag flag,
            OCEntityHandlerRequest *request, void *callbackParam)
    {
        (void) flag;
        ConfigurationResource *thiz = (ConfigurationResource *) callbackParam;
        switch (request->method)
        {
            case OC_REST_GET:
                {
                    OCRepPayload *payload = CreatePayload(request->resource, request->query);
                    EXPECT_TRUE(payload != NULL);
                    if (request->resource == thiz->m_con)
                    {
                        thiz->SetMandatoryDeviceProperties(payload);
                    }
                    if (thiz->m_includeOptionalProperties)
                    {
                        if (request->resource == thiz->m_con)
                        {
                            thiz->SetOptionalDeviceProperties(payload);
                        }
                        if (!thiz->m_conp || request->resource == thiz->m_conp)
                        {
                            thiz->SetOptionalPlatformProperties(payload);
                        }
                    }
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            case OC_REST_POST:
                {
                    OCRepPayload *payload = (OCRepPayload *) request->payload;
                    if (request->resource == thiz->m_con)
                    {
                        thiz->VerifyDeviceProperties(payload);
                    }
                    if (!thiz->m_conp || request->resource == thiz->m_conp)
                    {
                        thiz->VerifyPlatformProperties(payload);
                    }
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            default:
                return OC_EH_METHOD_NOT_ALLOWED;
        }
    }
    static OCEntityHandlerResult MaintenanceHandler(OCEntityHandlerFlag flag,
            OCEntityHandlerRequest *request, void *callbackParam)
    {
        (void) flag;
        ConfigurationResource *thiz = (ConfigurationResource *) callbackParam;
        switch (request->method)
        {
            case OC_REST_GET:
                {
                    OCRepPayload *payload = CreatePayload(request->resource, request->query);
                    EXPECT_TRUE(payload != NULL);
                    thiz->SetMandatoryMaintenanceProperties(payload);
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            default:
                return OC_EH_METHOD_NOT_ALLOWED;
        }
    }
    bool m_includeOptionalProperties;
    OCResourceHandle m_con;
    OCResourceHandle m_conp;
    OCResourceHandle m_mnt;
    ExpectedProperties *m_properties;
};

class DeviceConfigurationResource : public ConfigurationResource
{
public:
    DeviceConfigurationResource() : ConfigurationResource(false)
    {
        EXPECT_EQ(OC_STACK_OK, CreateResource(&m_con, "/con", "oic.wk.con", "oic.if.rw",
                DeviceConfigurationResource::EntityHandler, this, OC_DISCOVERABLE));
    }
    DeviceConfigurationResource(ExpectedProperties *properties)
        : ConfigurationResource(false, false, properties)
    {
        EXPECT_EQ(OC_STACK_OK, CreateResource(&m_con, "/con", "oic.wk.con", "oic.if.rw",
                DeviceConfigurationResource::EntityHandler, this, OC_DISCOVERABLE));
    }
    static OCEntityHandlerResult EntityHandler(OCEntityHandlerFlag flag,
            OCEntityHandlerRequest *request, void *callbackParam)
    {
        (void) flag;
        DeviceConfigurationResource *thiz = (DeviceConfigurationResource *) callbackParam;
        switch (request->method)
        {
            case OC_REST_GET:
                {
                    OCRepPayload *payload = CreatePayload(request->resource, request->query);
                    EXPECT_TRUE(payload != NULL);
                    thiz->SetMandatoryDeviceProperties(payload);
                    if (thiz->m_includeOptionalProperties)
                    {
                        thiz->SetOptionalDeviceProperties(payload);
                    }
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            case OC_REST_POST:
                {
                    OCRepPayload *payload = (OCRepPayload *) request->payload;
                    thiz->VerifyDeviceProperties(payload);
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            default:
                return OC_EH_METHOD_NOT_ALLOWED;
        }
    }
};

class PlatformConfigurationResource : public ConfigurationResource
{
public:
    PlatformConfigurationResource() : ConfigurationResource(false)
    {
        EXPECT_EQ(OC_STACK_OK, CreateResource(&m_conp, "/con/p", "oic.wk.con.p", "oic.if.rw",
                PlatformConfigurationResource::EntityHandler, this, OC_DISCOVERABLE));
    }
    PlatformConfigurationResource(ExpectedProperties *properties)
        : ConfigurationResource(false, false, properties)
    {
        EXPECT_EQ(OC_STACK_OK, CreateResource(&m_conp, "/con/p", "oic.wk.con.p", "oic.if.rw",
                PlatformConfigurationResource::EntityHandler, this, OC_DISCOVERABLE));
    }
    static OCEntityHandlerResult EntityHandler(OCEntityHandlerFlag flag,
            OCEntityHandlerRequest *request, void *callbackParam)
    {
        (void) flag;
        PlatformConfigurationResource *thiz = (PlatformConfigurationResource *) callbackParam;
        switch (request->method)
        {
            case OC_REST_GET:
                {
                    OCRepPayload *payload = CreatePayload(request->resource, request->query);
                    EXPECT_TRUE(payload != NULL);
                    if (thiz->m_includeOptionalProperties)
                    {
                        thiz->SetOptionalPlatformProperties(payload);
                    }
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            case OC_REST_POST:
                {
                    OCRepPayload *payload = (OCRepPayload *) request->payload;
                    thiz->VerifyPlatformProperties(payload);
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            default:
                return OC_EH_METHOD_NOT_ALLOWED;
        }
    }
};

TEST_F(VirtualProducer, DeviceConfigurationProperties)
{
    DeviceConfigurationResource deviceConfiguration;

    DiscoverContext context("/con");
    Callback discoverDeviceCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.freedesktop.DBus.Properties */
    ajn::MsgArg property;
    EXPECT_EQ(ER_OK, proxyObj->GetProperty("org.alljoyn.Config", "Version", property));
    EXPECT_EQ(1, property.v_variant.val->v_uint16);

    EXPECT_NE(ER_OK, proxyObj->SetProperty("org.alljoyn.Config", "Version", property));

    ajn::MsgArg properties;
    EXPECT_EQ(ER_OK, proxyObj->GetAllProperties("org.alljoyn.Config", properties));
    uint16_t version;
    EXPECT_EQ(1u, properties.v_array.GetNumElements());
    EXPECT_EQ(ER_OK, properties.GetElement("{sq}", "Version", &version));
    EXPECT_EQ(1, version);

    /* org.alljoyn.Config.GetConfigurations */

    /* Mandatory properties */
    deviceConfiguration.SetMandatoryProperties();
    ajn::MsgArg arg("s", "");
    MethodCall get(m_bus, proxyObj);
    EXPECT_EQ(ER_OK, get.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
    EXPECT_EQ(OC_STACK_OK, get.Wait(100));
    size_t numArgs;
    const ajn::MsgArg *args;
    get.Reply()->GetArgs(numArgs, args);
    EXPECT_EQ(1u, numArgs);
    EXPECT_EQ(0u, args[0].v_array.GetNumElements());

    /* All properties */
    deviceConfiguration.SetAllProperties();
    MethodCall getAll(m_bus, proxyObj);
    EXPECT_EQ(ER_OK, getAll.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
    EXPECT_EQ(OC_STACK_OK, getAll.Wait(100));
    getAll.Reply()->GetArgs(numArgs, args);
    EXPECT_EQ(1u, numArgs);
    EXPECT_EQ(8u, args[0].v_array.GetNumElements());
    char *s = NULL;
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "DefaultLanguage", &s));
    EXPECT_STREQ("en", s);
    size_t n;
    double *ds = NULL;
    EXPECT_EQ(ER_OK, args[0].GetElement("{sad}", "org.openconnectivity.loc", &n, &ds));
    EXPECT_EQ(2u, n);
    EXPECT_EQ(-1, ds[0]);
    EXPECT_EQ(1, ds[1]);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.openconnectivity.locn", &s));
    EXPECT_STREQ("locn", s);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.openconnectivity.c", &s));
    EXPECT_STREQ("c", s);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.openconnectivity.r", &s));
    EXPECT_STREQ("r", s);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.iotivity.property", &s));
    EXPECT_STREQ("value", s);

    delete proxyObj;
    delete obj;
}

TEST_F(VirtualProducer, PlatformConfigurationProperties)
{
    PlatformConfigurationResource platformConfiguration;

    DiscoverContext context("/con/p");
    Callback discoverPlatformCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverPlatformCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverPlatformCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.GetConfigurations */

    /* All properties (there are no mandatory properties) */
    platformConfiguration.SetAllProperties();
    ajn::MsgArg lang("s", "");
    MethodCall getAll(m_bus, proxyObj);
    EXPECT_EQ(ER_OK, getAll.Call("org.alljoyn.Config", "GetConfigurations", &lang, 1));
    EXPECT_EQ(OC_STACK_OK, getAll.Wait(100));
    size_t numArgs;
    const ajn::MsgArg *args;
    getAll.Reply()->GetArgs(numArgs, args);
    EXPECT_EQ(1u, numArgs);
    EXPECT_EQ(1u, args[0].v_array.GetNumElements());
    char *s = NULL;
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "DeviceName", &s));
    EXPECT_STREQ("en-name", s);

    delete proxyObj;
    delete obj;
}

static void ConfigurationPropertiesVerify(ConfigurationResource *configuration,
        ajn::BusAttachment *bus, ajn::ProxyBusObject *proxyObj)
{
    /* org.alljoyn.Config.GetConfigurations */

    /* Mandatory properties */
    configuration->SetMandatoryProperties();
    ajn::MsgArg arg("s", "");
    MethodCall get(bus, proxyObj);
    EXPECT_EQ(ER_OK, get.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
    EXPECT_EQ(OC_STACK_OK, get.Wait(100));
    size_t numArgs;
    const ajn::MsgArg *args;
    get.Reply()->GetArgs(numArgs, args);
    EXPECT_EQ(1u, numArgs);
    EXPECT_EQ(0u, args[0].v_array.GetNumElements());

    /* All properties */
    configuration->SetAllProperties();
    MethodCall getAll(bus, proxyObj);
    EXPECT_EQ(ER_OK, getAll.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
    EXPECT_EQ(OC_STACK_OK, getAll.Wait(100));
    getAll.Reply()->GetArgs(numArgs, args);
    EXPECT_EQ(1u, numArgs);
    EXPECT_EQ(9u, args[0].v_array.GetNumElements());
    char *s = NULL;
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "DefaultLanguage", &s));
    EXPECT_STREQ("en", s);
    size_t n;
    double *ds = NULL;
    EXPECT_EQ(ER_OK, args[0].GetElement("{sad}", "org.openconnectivity.loc", &n, &ds));
    EXPECT_EQ(2u, n);
    EXPECT_EQ(-1, ds[0]);
    EXPECT_EQ(1, ds[1]);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.openconnectivity.locn", &s));
    EXPECT_STREQ("locn", s);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.openconnectivity.c", &s));
    EXPECT_STREQ("c", s);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.openconnectivity.r", &s));
    EXPECT_STREQ("r", s);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "org.iotivity.property", &s));
    EXPECT_STREQ("value", s);
    EXPECT_EQ(ER_OK, args[0].GetElement("{ss}", "DeviceName", &s));
    EXPECT_STREQ("en-name", s);
}

TEST_F(VirtualProducer, ConfigurationPropertiesSameResource)
{
    ConfigurationResource configuration;

    DiscoverContext context("/con");
    Callback discoverDeviceCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);

    context.m_resource->m_uri = "/oic/mnt";
    context.m_resource->m_ifs.clear();
    context.m_resource->m_ifs.push_back("oic.if.rw");
    context.m_resource->m_ifs.push_back("oic.if.r");
    context.m_resource->m_ifs.push_back("oic.if.baseline");
    context.m_resource->m_rts.clear();
    context.m_resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*context.m_resource);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    ConfigurationPropertiesVerify(&configuration, m_bus, proxyObj);

    delete proxyObj;
    delete obj;
}

TEST_F(VirtualProducer, ConfigurationPropertiesDifferentResources)
{
    ConfigurationResource configuration(true, true);

    DiscoverContext context("/con");
    Callback discoverDeviceCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);

    context.m_resource->m_uri = "/con/p";
    context.m_resource->m_ifs.clear();
    context.m_resource->m_ifs.push_back("oic.if.rw");
    context.m_resource->m_ifs.push_back("oic.if.baseline");
    context.m_resource->m_rts.clear();
    context.m_resource->m_rts.push_back("oic.wk.con.p");
    obj->AddResource(*context.m_resource);

    context.m_resource->m_uri = "/oic/mnt";
    context.m_resource->m_ifs.clear();
    context.m_resource->m_ifs.push_back("oic.if.rw");
    context.m_resource->m_ifs.push_back("oic.if.r");
    context.m_resource->m_ifs.push_back("oic.if.baseline");
    context.m_resource->m_rts.clear();
    context.m_resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*context.m_resource);

    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    ConfigurationPropertiesVerify(&configuration, m_bus, proxyObj);

    delete proxyObj;
    delete obj;
}

TEST_F(VirtualProducer, UpdateDeviceConfigurationProperties)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    properties.dl = "en";
    properties.loc[0] = -1;
    properties.loc[1] = 1;
    properties.locn = "locn";
    properties.c = "c";
    properties.r = "r";
    properties.vendorProperty = "x.org.iotivity.property";
    properties.vendorValue = "value";
    DeviceConfigurationResource deviceConfiguration(&properties);

    DiscoverContext context("/con");
    Callback discoverDeviceCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(m_bus, proxyObj);
    ajn::MsgArg elems[8];
    ajn::MsgArg *elem = elems;
    elem->Set("{sv}", "DefaultLanguage", new ajn::MsgArg("s", properties.dl));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.loc", new ajn::MsgArg("ad", 2, properties.loc));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.locn", new ajn::MsgArg("s", properties.locn));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.c", new ajn::MsgArg("s", properties.c));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.r", new ajn::MsgArg("s", properties.r));
    ++elem;
    elem->Set("{sv}", &properties.vendorProperty[2], new ajn::MsgArg("s", properties.vendorValue));
    ++elem;
    ajn::MsgArg args[2];
    args[0].Set("s", "");
    args[1].Set("a{sv}", elem - elems, elems);
    EXPECT_EQ(ER_OK, update.Call("org.alljoyn.Config", "UpdateConfigurations", args, 2));
    EXPECT_EQ(OC_STACK_OK, update.Wait(100));

    delete proxyObj;
    delete obj;
}

TEST_F(VirtualProducer, UpdatePlatformConfigurationProperties)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    LocalizedString deviceNames[] = {
        { "en", "en-device-name" },
    };
    properties.mnpn = deviceNames;
    properties.nmnpn = sizeof(deviceNames) / sizeof(deviceNames[0]);
    PlatformConfigurationResource platformConfiguration(&properties);

    DiscoverContext context("/con/p");
    Callback discoverPlatformCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverPlatformCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverPlatformCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(m_bus, proxyObj);
    ajn::MsgArg elems[1];
    ajn::MsgArg *elem = elems;
    elem->Set("{sv}", "DeviceName", new ajn::MsgArg("s", deviceNames[0].value));
    ++elem;
    ajn::MsgArg args[2];
    args[0].Set("s", deviceNames[0].language);
    args[1].Set("a{sv}", elem - elems, elems);
    EXPECT_EQ(ER_OK, update.Call("org.alljoyn.Config", "UpdateConfigurations", args, 2));
    EXPECT_EQ(OC_STACK_OK, update.Wait(100));

    delete proxyObj;
    delete obj;
}

TEST_F(VirtualProducer, UpdateConfigurationPropertiesSameResource)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    properties.dl = "en";
    properties.loc[0] = -1;
    properties.loc[1] = 1;
    properties.locn = "locn";
    properties.c = "c";
    properties.r = "r";
    properties.vendorProperty = "x.org.iotivity.property";
    properties.vendorValue = "value";
    LocalizedString deviceNames[] = {
        { "en", "en-device-name" },
    };
    properties.mnpn = deviceNames;
    properties.nmnpn = sizeof(deviceNames) / sizeof(deviceNames[0]);
    ConfigurationResource configuration(true, false, &properties);

    DiscoverContext context("/con");
    Callback discoverDeviceCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);

    context.m_resource->m_uri = "/oic/mnt";
    context.m_resource->m_ifs.clear();
    context.m_resource->m_ifs.push_back("oic.if.rw");
    context.m_resource->m_ifs.push_back("oic.if.r");
    context.m_resource->m_ifs.push_back("oic.if.baseline");
    context.m_resource->m_rts.clear();
    context.m_resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*context.m_resource);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(m_bus, proxyObj);
    ajn::MsgArg elems[9];
    ajn::MsgArg *elem = elems;
    elem->Set("{sv}", "DefaultLanguage", new ajn::MsgArg("s", properties.dl));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.loc", new ajn::MsgArg("ad", 2, properties.loc));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.locn", new ajn::MsgArg("s", properties.locn));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.c", new ajn::MsgArg("s", properties.c));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.r", new ajn::MsgArg("s", properties.r));
    ++elem;
    elem->Set("{sv}", &properties.vendorProperty[2], new ajn::MsgArg("s", properties.vendorValue));
    ++elem;
    elem->Set("{sv}", "DeviceName", new ajn::MsgArg("s", deviceNames[0].value));
    ++elem;
    ajn::MsgArg args[2];
    args[0].Set("s", deviceNames[0].language);
    args[1].Set("a{sv}", elem - elems, elems);
    EXPECT_EQ(ER_OK, update.Call("org.alljoyn.Config", "UpdateConfigurations", args, 2));
    EXPECT_EQ(OC_STACK_OK, update.Wait(100));

    delete proxyObj;
    delete obj;
}

TEST_F(VirtualProducer, UpdateConfigurationPropertiesDifferentResource)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    properties.dl = "en";
    properties.loc[0] = -1;
    properties.loc[1] = 1;
    properties.locn = "locn";
    properties.c = "c";
    properties.r = "r";
    properties.vendorProperty = "x.org.iotivity.property";
    properties.vendorValue = "value";
    LocalizedString deviceNames[] = {
        { "en", "en-device-name" },
    };
    properties.mnpn = deviceNames;
    properties.nmnpn = sizeof(deviceNames) / sizeof(deviceNames[0]);
    ConfigurationResource configuration(true, true, &properties);

    DiscoverContext context("/con");
    Callback discoverDeviceCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(100));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(m_bus, *context.m_resource);

    context.m_resource->m_uri = "/con/p";
    context.m_resource->m_ifs.clear();
    context.m_resource->m_ifs.push_back("oic.if.rw");
    context.m_resource->m_ifs.push_back("oic.if.baseline");
    context.m_resource->m_rts.clear();
    context.m_resource->m_rts.push_back("oic.wk.con.p");
    obj->AddResource(*context.m_resource);

    context.m_resource->m_uri = "/oic/mnt";
    context.m_resource->m_ifs.clear();
    context.m_resource->m_ifs.push_back("oic.if.rw");
    context.m_resource->m_ifs.push_back("oic.if.r");
    context.m_resource->m_ifs.push_back("oic.if.baseline");
    context.m_resource->m_rts.clear();
    context.m_resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*context.m_resource);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(m_bus, proxyObj);
    ajn::MsgArg elems[9];
    ajn::MsgArg *elem = elems;
    elem->Set("{sv}", "DefaultLanguage", new ajn::MsgArg("s", properties.dl));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.loc", new ajn::MsgArg("ad", 2, properties.loc));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.locn", new ajn::MsgArg("s", properties.locn));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.c", new ajn::MsgArg("s", properties.c));
    ++elem;
    elem->Set("{sv}", "org.openconnectivity.r", new ajn::MsgArg("s", properties.r));
    ++elem;
    elem->Set("{sv}", &properties.vendorProperty[2], new ajn::MsgArg("s", properties.vendorValue));
    ++elem;
    elem->Set("{sv}", "DeviceName", new ajn::MsgArg("s", deviceNames[0].value));
    ++elem;
    ajn::MsgArg args[2];
    args[0].Set("s", deviceNames[0].language);
    args[1].Set("a{sv}", elem - elems, elems);
    EXPECT_EQ(ER_OK, update.Call("org.alljoyn.Config", "UpdateConfigurations", args, 2));
    EXPECT_EQ(OC_STACK_OK, update.Wait(100));

    delete proxyObj;
    delete obj;
}

class BinarySwitchResource : public TestResource
{
public:
    BinarySwitchResource(const char *path, uint8_t props)
        : m_value(false)
    {
        EXPECT_EQ(OC_STACK_OK, CreateResource(&m_handle, path, "oic.r.switch.binary", "oic.if.a",
                BinarySwitchResource::EntityHandler, this, props));
    }
    void Notify()
    {
        EXPECT_EQ(OC_STACK_OK, OCNotifyAllObservers(m_handle, OC_HIGH_QOS));
    }
    static OCEntityHandlerResult EntityHandler(OCEntityHandlerFlag flag,
            OCEntityHandlerRequest *request, void *callbackParam)
    {
        (void) flag;
        BinarySwitchResource *thiz = (BinarySwitchResource *) callbackParam;
        switch (request->method)
        {
            case OC_REST_GET:
                {
                    OCRepPayload *payload = CreatePayload(request->resource, request->query);
                    EXPECT_TRUE(payload != NULL);
                    EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "value", thiz->m_value));
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            case OC_REST_POST:
                {
                    OCRepPayload *payload = (OCRepPayload *) request->payload;
                    EXPECT_TRUE(payload != NULL);
                    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "value", &thiz->m_value));
                    thiz->DoResponse(request, payload);
                    return OC_EH_OK;
                }
            default:
                return OC_EH_METHOD_NOT_ALLOWED;
        }
    }
    OCResourceHandle m_handle;
    bool m_value;
};

class PropertiesChangedListener : public ajn::ProxyBusObject::PropertiesChangedListener
{
public:
    size_t m_calls;
    PropertiesChangedListener() : m_calls(0) { }
    virtual ~PropertiesChangedListener() { }
    void PropertiesChanged(ajn::ProxyBusObject& proxyObj, const char* ifaceName,
            const ajn::MsgArg& changed, const ajn::MsgArg& invalidated, void* context)
    {
        (void) proxyObj;
        (void) ifaceName;
        (void) changed;
        (void) invalidated;
        (void) context;
        ++m_calls;
    }
};

TEST_F(VirtualProducer, Observe)
{
    BinarySwitchResource r0("/r0", OC_DISCOVERABLE | OC_OBSERVABLE);
    BinarySwitchResource r1("/r1", OC_OBSERVABLE);

    DiscoverContext context("/r0");
    Callback discoverCB(Discover, &context);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverCB.Wait(100));
    VirtualBusObject *obj = new VirtualBusObject(m_bus, *context.m_resource);
    context.m_resource->m_uri = "/r1";
    obj->AddResource(*context.m_resource);
    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "value", false));
    OCRepPayload *definitions = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropObject(definitions, "oic.r.switch.binary",
            IntrospectDefinition(payload, context.m_resource->m_rts[0], context.m_resource->m_ifs)));
    std::map<std::string, Annotations> annotations;
    ParseAnnotations(definitions, annotations);
    std::map<std::string, bool> isObservable;
    isObservable["oic.r.switch.binary"] = true;
    std::map<std::string, std::string> ajNames;
    ParseInterfaces(definitions, annotations, isObservable, m_bus, ajNames);
    OCRepPayload *path = IntrospectPath(context.m_resource->m_rts, context.m_resource->m_ifs);
    ParsePath(path, ajNames, obj);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*obj));

    ajn::SessionId id;
    ajn::SessionOpts opts;
    EXPECT_EQ(ER_OK, m_bus->JoinSession(m_bus->GetUniqueName().c_str(), m_port, NULL, id, opts));
    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*m_bus, m_bus->GetUniqueName().c_str(), "/r0", id);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());
    PropertiesChangedListener listener;
    EXPECT_EQ(ER_OK, proxyObj->RegisterPropertiesChangedListener("oic.r.switch.binary", NULL, 0, listener, NULL));
    obj->Observe();
    Wait(100); /* Must wait for Observe to be processed */

    listener.m_calls = 0;
    r0.Notify();
    Wait(100);
    EXPECT_EQ(1u, listener.m_calls);
    r1.Notify();
    Wait(100);
    EXPECT_EQ(2u, listener.m_calls);
}
