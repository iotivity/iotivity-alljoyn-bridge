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

#include <gtest/gtest.h>

#include "AboutData.h"
#include "DeviceConfigurationResource.h"
#include "DeviceResource.h"
#include "Interfaces.h"
#include "Hash.h"
#include "Name.h"
#include "Payload.h"
#include "PlatformConfigurationResource.h"
#include "PlatformResource.h"
#include "Plugin.h"
#include "Resource.h"
#include "VirtualConfigurationResource.h"
#include "VirtualDevice.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include <alljoyn/AboutData.h>
#include <alljoyn/BusAttachment.h>
#include <alljoyn/Init.h>

#include "gtest_helper.h"
/* TODO fold addition of context to Callback back into gtest_helper.h */
class Callback
{
public:
    Callback(OCClientResponseHandler cb, void *context = NULL) : m_cb(cb), m_context(context), m_called(false)
    {
        m_cbData.cb = &Callback::handler;
        m_cbData.cd = NULL;
        m_cbData.context = this;
    }
    OCStackResult Wait(long waitTime)
    {
        uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
        while (!m_called)
        {
            uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
            long elapsed = (long)((currTime - startTime) / MS_PER_SEC);
            if (elapsed > waitTime)
            {
                break;
            }
            OCProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return m_called ? OC_STACK_OK : OC_STACK_TIMEOUT;
    }
    operator OCCallbackData *()
    {
        return &m_cbData;
    }
private:
    OCCallbackData m_cbData;
    OCClientResponseHandler m_cb;
    void *m_context;
    bool m_called;
    static OCStackApplicationResult handler(void *ctx, OCDoHandle handle, OCClientResponse *clientResponse)
    {
        Callback *callback = (Callback *) ctx;
        OCStackApplicationResult result = callback->m_cb(callback->m_context, handle, clientResponse);
        callback->m_called = true;
        return result;
    }
};

class CreateCallback
{
public:
    CreateCallback() : m_called(false) { }
    OCStackResult Wait(long waitTime)
    {
        uint64_t startTime = OICGetCurrentTime(TIME_IN_MS);
        while (!m_called)
        {
            uint64_t currTime = OICGetCurrentTime(TIME_IN_MS);
            long elapsed = (long)((currTime - startTime) / MS_PER_SEC);
            if (elapsed > waitTime)
            {
                break;
            }
            OCProcess();
            std::this_thread::sleep_for(std::chrono::milliseconds(10));
        }
        return m_called ? OC_STACK_OK : OC_STACK_TIMEOUT;
    }
    operator VirtualResource::CreateCB ()
    {
        return cb;
    }
private:
    bool m_called;
    static void cb(void *ctx)
    {
        CreateCallback *callback = (CreateCallback *) ctx;
        callback->m_called = true;
    }
};

class NameTranslationTest : public ::testing::TestWithParam<const char *> { };

TEST_P(NameTranslationTest, RoundTrip)
{
    const char *ajName = GetParam();
    EXPECT_STREQ(ajName, ToAJName(ToOCName(ajName)).c_str());
}

INSTANTIATE_TEST_CASE_P(FromSpec,
                        NameTranslationTest,
                        ::testing::Values("example.Widget",
                                "example.my_widget",
                                "example.My_Widget",
                                "xn_p1ai.example",
                                "xn__90ae.example",
                                "example.myName_1"));

INSTANTIATE_TEST_CASE_P(Extras,
                        NameTranslationTest,
                        ::testing::Values("oneTwoThree",
                                "One_Two_Three",
                                "",
                                "x",
                                "example.foo_",
                                "example.foo__"));

TEST(NameTranslationTest, BoundsCheck)
{
    EXPECT_STREQ("", ToAJName("").c_str());
    EXPECT_STREQ("x", ToAJName("x").c_str());
    EXPECT_STREQ("example.foo_", ToAJName("x.example.foo-").c_str());
    EXPECT_STREQ("example.foo__", ToAJName("x.example.foo--").c_str());
    EXPECT_STREQ("example.foo__", ToAJName("x.example.foo---").c_str());
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

class AJOCSetUp : public testing::Test
{
protected:
    virtual void SetUp()
    {
        EXPECT_EQ(OC_STACK_OK, OCInit2(OC_SERVER, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS, OC_ADAPTER_IP));
        EXPECT_EQ(ER_OK, AllJoynInit());
        EXPECT_EQ(ER_OK, AllJoynRouterInit());
    }
    virtual void TearDown()
    {
        /*
         * TODO AllJoyn has a bug where AllJoynShutdown() does not clear all the state needed to
         * call AllJoynInit() again (an assert fires: alljoyn_core/src/XmlRulesValidator.cc:98:
         * static void ajn::XmlRulesValidator::MethodsValidator::Init(): Assertion `nullptr ==
         * s_actionsMap' failed.)
         */
        //EXPECT_EQ(ER_OK, AllJoynShutdown());
        EXPECT_EQ(OC_STACK_OK, OCStop());
    }
};

class AboutDataTest : public AJOCSetUp { };

TEST_F(AboutDataTest, IsValid)
{
    AboutData aboutData(NULL);
    EXPECT_TRUE(aboutData.IsValid());
}

struct LocalizedString
{
    const char *language;
    const char *value;
};

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
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_DEVICE, payload);
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
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_PLATFORM, payload);
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
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION, payload);
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
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION, payload);
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
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_DEVICE, payload);
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
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_DEVICE, device);
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION, config);
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
    EXPECT_TRUE(OCRepPayloadSetDoubleArray(payload, "loc", location, locationDim));
    /* locn => org.openconnectivity.locn */
    const char *locationName = "location-name";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "locn", locationName));
    /* c => org.openconnectivity.c */
    const char *currency = "currency";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "c", currency));
    /* r => org.openconnectivity.r */
    const char *region = "region";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "r", region));
    /* vendor-defined => */
    const char *vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    const char *vendorValue = "value";
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, vendorProperty, vendorValue));
    /* TODO other OC types */

    AboutData aboutData(NULL);
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION, payload);
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
    aboutData.Set(OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION, payload);
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

TEST_F(DeviceProperties, UsePeerGuidForProtocolIndependentId)
{
    AboutData aboutData("");
    /* peer GUID (if org.openconnectivity.piid does not exist) => piid */
    const char *piid = "10f70cc4-2398-41f5-8062-4c1facbfc41b";
    const char *peerGuid = "10f70cc4239841f580624c1facbfc41b";
    const char *deviceId = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(deviceId));
    const uint8_t appId[] = { 0x46, 0xe8, 0x0b, 0xf8, 0x9f, 0xf5, 0x47, 0x8a,
                              0xbe, 0x9f, 0x7f, 0xa3, 0x4a, 0xdc, 0x49, 0x7b };
    EXPECT_EQ(ER_OK, aboutData.SetAppId(appId, sizeof(appId) / sizeof(appId[0])));

    EXPECT_EQ(OC_STACK_OK, SetDeviceProperties(NULL, NULL, &aboutData, peerGuid));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, (void**) &s));
    EXPECT_STREQ(piid, s);
}

TEST_F(DeviceProperties, UseHashForProtocolIndependentId)
{
    AboutData aboutData("");
    /* Hash(DeviceId, AppId) (if org.openconnectivity.piid and peer GUID do not exist) => piid */
    const char *deviceId = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(deviceId));
    const uint8_t appId[] = { 0x46, 0xe8, 0x0b, 0xf8, 0x9f, 0xf5, 0x47, 0x8a,
                              0xbe, 0x9f, 0x7f, 0xa3, 0x4a, 0xdc, 0x49, 0x7b };
    EXPECT_EQ(ER_OK, aboutData.SetAppId(appId, sizeof(appId) / sizeof(appId[0])));
    OCUUIdentity id;
    Hash(&id, deviceId, appId, sizeof(appId)/ sizeof(appId[0]));
    char piid[UUID_STRING_SIZE];
    OCConvertUuidToString(id.id, piid);

    EXPECT_EQ(OC_STACK_OK, SetDeviceProperties(NULL, NULL, &aboutData, NULL));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, (void**) &s));
    EXPECT_STREQ(piid, s);
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
    EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, "loc", &locArr, locDim));
    EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
    EXPECT_EQ(latitude, locArr[0]);
    EXPECT_EQ(longitude, locArr[1]);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "locn", &s));
    EXPECT_STREQ(locationName, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "c", &s));
    EXPECT_STREQ(currency, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "r", &s));
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

class VirtualServer : public AJOCSetUp
{
protected:
    ajn::BusAttachment *bus;
    virtual void SetUp()
    {
        AJOCSetUp::SetUp();
        bus = new ajn::BusAttachment("Producer");
        EXPECT_EQ(ER_OK, bus->Start());
        EXPECT_EQ(ER_OK, bus->Connect());
    }
    virtual void TearDown()
    {
        delete bus;
        AJOCSetUp::TearDown();
    }
};

struct ExpectedProperties
{
    /* oic.wk.d */
    const char *n;
    const char *piid;
    const char *dmv;
    LocalizedString *ld;
    size_t nld;
    const char *sv;
    LocalizedString *dmn;
    size_t ndmn;
    const char *dmno;
    /* oic.wk.con */
    double loc[2];
    const char *locn;
    const char *c;
    const char *r;
    LocalizedString *ln;
    size_t nln;
    const char *dl;
    /* oic.wk.p */
    const char *pi;
    const char *mnmn;
    const char *mnml;
    const char *mnmo;
    const char *mndt;
    const char *mnpv;
    const char *mnos;
    const char *mnhw;
    const char *mnfv;
    const char *mnsl;
    const char *st;
    const char *vid;
    /* oic.wk.con.p */
    LocalizedString *mnpn;
    size_t nmnpn;
    /* Vendor-specific */
    const char *vendorProperty;
    const char *vendorValue;
};

class ConfigBusObject : public ajn::BusObject
{
public:
    ConfigBusObject(ajn::BusAttachment *bus, ExpectedProperties *properties)
        : ajn::BusObject("/Config"), m_properties(properties)
    {
        EXPECT_EQ(ER_OK, bus->CreateInterfacesFromXml(ajn::org::alljoyn::Config::InterfaceXml));
        const ajn::InterfaceDescription *iface = bus->GetInterface("org.alljoyn.Config");
        AddInterface(*iface);
        const MethodEntry methodEntries[] =
        {
            { iface->GetMember("GetConfigurations"), static_cast<MessageReceiver::MethodHandler>(&ConfigBusObject::GetConfigurations) },
            { iface->GetMember("UpdateConfigurations"), static_cast<MessageReceiver::MethodHandler>(&ConfigBusObject::UpdateConfigurations) }
        };
        AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    }
    virtual ~ConfigBusObject() { }
    void GetConfigurations(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
    {
        (void) member;

        const char *lang;
        EXPECT_EQ(ER_OK, msg->GetArg(0)->Get("s", &lang));
        if (!strcmp(lang, ""))
        {
            lang = m_properties->dl;
        }

        ajn::MsgArg entries[7];
        ajn::MsgArg *entry = entries;
        if (m_properties->loc[0] || m_properties->loc[1])
        {
            entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
            entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.loc");
            entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("ad", 2, m_properties->loc));
            entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
            ++entry;
        }
        if (m_properties->locn)
        {
            entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
            entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.locn");
            entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", m_properties->locn));
            entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
            ++entry;
        }
        if (m_properties->c)
        {
            entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
            entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.c");
            entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", m_properties->c));
            entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
            ++entry;
        }
        if (m_properties->r)
        {
            entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
            entry->v_dictEntry.key = new ajn::MsgArg("s", "org.openconnectivity.r");
            entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", m_properties->r));
            entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
            ++entry;
        }
        entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
        entry->v_dictEntry.key = new ajn::MsgArg("s", "DefaultLanguage");
        entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", m_properties->dl));
        entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
        ++entry;
        for (size_t i = 0; i < m_properties->nmnpn; ++i)
        {
            if (!strcmp(m_properties->mnpn[i].language, lang))
            {
                entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
                entry->v_dictEntry.key = new ajn::MsgArg("s", "DeviceName");
                entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", m_properties->mnpn[i].value));
                entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
                ++entry;
                break;
            }
        }
        if (m_properties->vendorProperty)
        {
            entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
            entry->v_dictEntry.key = new ajn::MsgArg("s", &m_properties->vendorProperty[2]);
            entry->v_dictEntry.val = new ajn::MsgArg("v", new ajn::MsgArg("s", m_properties->vendorValue));
            entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
            ++entry;
        }
        ajn::MsgArg arg;
        arg.typeId = ajn::ALLJOYN_ARRAY;
        arg.v_array.SetElements("{sv}", entry - entries, entries);
        EXPECT_EQ(ER_OK, MethodReply(msg, &arg, 1));
    }
    void UpdateConfigurations(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
    {
        (void) member;
        fprintf(stderr, "%s\n", msg->ToString().c_str());
        EXPECT_EQ(ER_OK, MethodReply(msg, ER_OK));
    }

    ExpectedProperties *m_properties;
};

static OCStackApplicationResult DeviceResourceVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    ExpectedProperties *properties = (ExpectedProperties *) ctx;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) response->payload;
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
    EXPECT_STREQ(properties->n, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, &s));
    EXPECT_STREQ(properties->piid, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DATA_MODEL_VERSION, &s));
    EXPECT_STREQ(properties->dmv, s);
    size_t ldDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **ldArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_DESCRIPTION, &ldArr, ldDim));
    EXPECT_EQ(properties->nld, calcDimTotal(ldDim));
    for (size_t i = 0; i < properties->nld; ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(ldArr[i], "language", &s));
        EXPECT_STREQ(properties->ld[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(ldArr[i], "value", &s));
        EXPECT_STREQ(properties->ld[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_SOFTWARE_VERSION, &s));
    EXPECT_STREQ(properties->sv, s);
    size_t dmnDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **dmnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_MFG_NAME, &dmnArr, dmnDim));
    EXPECT_EQ(properties->ndmn, calcDimTotal(dmnDim));
    for (size_t i = 0; i < properties->ndmn; ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(dmnArr[i], "language", &s));
        EXPECT_STREQ(properties->dmn[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(dmnArr[i], "value", &s));
        EXPECT_STREQ(properties->dmn[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_MODEL_NUM, &s));
    EXPECT_STREQ(properties->dmno, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, properties->vendorProperty, &s));
    EXPECT_STREQ(properties->vendorValue, s);
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult DeviceConfigurationResourceVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    ExpectedProperties *properties = (ExpectedProperties *) ctx;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    if (response->payload)
    {
        EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
        OCRepPayload *payload = (OCRepPayload *) response->payload;
        char *s;
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
        EXPECT_STREQ(properties->n, s);
        size_t locDim[MAX_REP_ARRAY_DEPTH];
        double *locArr;
        EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, "loc", &locArr, locDim));
        EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
        EXPECT_EQ(properties->loc[0], locArr[0]);
        EXPECT_EQ(properties->loc[1], locArr[1]);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "locn", &s));
        EXPECT_STREQ(properties->locn, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "c", &s));
        EXPECT_STREQ(properties->c, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "r", &s));
        EXPECT_STREQ(properties->r, s);
        size_t lnDim[MAX_REP_ARRAY_DEPTH];
        OCRepPayload **lnArr;
        EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, &lnArr, lnDim));
        EXPECT_EQ(properties->nln, calcDimTotal(lnDim));
        for (size_t i = 0; i < properties->nln; ++i)
        {
            EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "language", &s));
            EXPECT_STREQ(properties->ln[i].language, s);
            EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "value", &s));
            EXPECT_STREQ(properties->ln[i].value, s);
        }
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, &s));
        EXPECT_STREQ(properties->dl, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, properties->vendorProperty, &s));
        EXPECT_STREQ(properties->vendorValue, s);
    }
    return OC_STACK_DELETE_TRANSACTION;
}

TEST_F(VirtualServer, DeviceResource)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    /* DefaultLanguage => dl */
    properties.dl = "fr";
    AboutData aboutData(properties.dl);
    /* AppName => n */
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    properties.ln = appNames;
    properties.nln = sizeof(appNames) / sizeof(appNames[0]);
    for (size_t i = 0; i < properties.nln; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(appNames[i].value, appNames[i].language));
    }
    properties.n = appNames[1].value;
    /* org.openconnectivity.piid => piid */
    properties.piid = "10f70cc4-2398-41f5-8062-4c1facbfc41b";
    EXPECT_EQ(ER_OK, aboutData.SetProtocolIndependentId(properties.piid));
    const char *deviceId = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(deviceId));
    const uint8_t appId[] = { 0x46, 0xe8, 0x0b, 0xf8, 0x9f, 0xf5, 0x47, 0x8a,
                              0xbe, 0x9f, 0x7f, 0xa3, 0x4a, 0xdc, 0x49, 0x7b };
    EXPECT_EQ(ER_OK, aboutData.SetAppId(appId, sizeof(appId) / sizeof(appId[0])));
    /* Version(s) => dmv */
    properties.dmv = "org.iotivity.A.1,org.iotivity.B.2,org.iotivity.C.3,org.iotivity.D.4";
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
    properties.ld = descriptions;
    properties.nld = sizeof(descriptions) / sizeof(descriptions[0]);
    for (size_t i = 0; i < properties.nld; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetDescription(properties.ld[i].value, properties.ld[i].language));
    }
    /* SoftwareVersion => sv */
    properties.sv = "software-version";
    EXPECT_EQ(ER_OK, aboutData.SetSoftwareVersion(properties.sv));
    /* Manufacturer => dmn */
    LocalizedString manufacturers[] = {
        { "en", "en-manufacturer" },
        { "fr", "fr-manufacturer" }
    };
    properties.dmn = manufacturers;
    properties.ndmn = sizeof(manufacturers) / sizeof(manufacturers[0]);
    for (size_t i = 0; i < properties.ndmn; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetManufacturer(properties.dmn[i].value, properties.dmn[i].language));
    }
    /* ModelNumber => dmno */
    properties.dmno = "model-number";
    EXPECT_EQ(ER_OK, aboutData.SetModelNumber(properties.dmno));
    /* vendor-defined => x. */
    properties.vendorProperty = "x.org.iotivity.Field";
    const char *vendorField = "org.iotivity.Field";
    properties.vendorValue = "value";
    ajn::MsgArg vendorArg("s", properties.vendorValue);
    EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg));

    /* AppName => n */
    /* org.openconnectivity.loc => loc */
    properties.loc[0] = -1.0;
    properties.loc[1] = 1.0;
    /* org.openconnectivity.locn => locn */
    properties.locn = "locn";
    /* org.openconnectivity.c => c */
    properties.c = "c";
    /* org.openconnectivity.r => r */
    properties.r = "r";
    /* AppName => ln */
    /* DefaultLanguage => dl */
    /* DeviceName => mnpn */
    LocalizedString deviceNames[] = {
        { "en", "en-device-name" },
        { "fr", "fr-device-name" }
    };
    properties.mnpn = deviceNames;
    properties.nmnpn = sizeof(deviceNames) / sizeof(deviceNames[0]);
    ConfigBusObject configObj(bus, &properties);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(configObj));

    VirtualDevice *device = new VirtualDevice(bus, bus->GetUniqueName().c_str(), 0);
    device->SetProperties(&objectDescription, &aboutData);
    CreateCallback createCB;
    VirtualConfigurationResource *resource = VirtualConfigurationResource::Create(bus,
            bus->GetUniqueName().c_str(), 0, "/Config", "v16.10.00", createCB, &createCB);
    resource->SetAboutData(&aboutData);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1));

    Callback getDeviceCB(&DeviceResourceVerify, &properties);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "127.0.0.1/oic/d", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, getDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getDeviceCB.Wait(1));

    Callback getDeviceConfigurationCB(&DeviceConfigurationResourceVerify, &properties);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "127.0.0.1/con", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, getDeviceConfigurationCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getDeviceConfigurationCB.Wait(1));

    delete resource;
    delete device;
}

static OCStackApplicationResult UpdateDeviceResourceVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    ExpectedProperties *properties = (ExpectedProperties *) ctx;
    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) response->payload;

    size_t locDim[MAX_REP_ARRAY_DEPTH];
    double *locArr;
    EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, "loc", &locArr, locDim));
    EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
    EXPECT_EQ(properties->loc[0], locArr[0]);
    EXPECT_EQ(properties->loc[1], locArr[1]);
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "locn", &s));
    EXPECT_STREQ(properties->locn, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "c", &s));
    EXPECT_STREQ(properties->c, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "r", &s));
    EXPECT_STREQ(properties->r, s);
    size_t lnDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **lnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, &lnArr, lnDim));
    EXPECT_EQ(properties->nln, calcDimTotal(lnDim));
    for (size_t i = 0; i < properties->nln; ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "language", &s));
        EXPECT_STREQ(properties->ln[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "value", &s));
        EXPECT_STREQ(properties->ln[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, &s));
    EXPECT_STREQ(properties->dl, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
    EXPECT_STREQ(properties->n, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, properties->vendorProperty, &s));
    EXPECT_STREQ(properties->vendorValue, s);
    return OC_STACK_DELETE_TRANSACTION;
}

TEST_F(VirtualServer, UpdateDeviceResource)
{
    ExpectedProperties before;
    memset(&before, 0, sizeof(before));
    before.loc[0] = -1.0;
    before.loc[1] = 1.0;
    before.locn = "locn";
    before.c = "c";
    before.r = "r";
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    before.ln = appNames;
    before.nln = sizeof(appNames) / sizeof(appNames[0]);
    before.dl = "fr";
    before.n = appNames[1].value;
    before.vendorProperty = "x.org.iotivity.Field";
    before.vendorValue = "value";

    AboutData aboutData(before.dl);
    for (size_t i = 0; i < before.nln; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(before.ln[i].value, before.ln[i].language));
    }
    ConfigBusObject configObj(bus, &before);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    VirtualConfigurationResource *resource = VirtualConfigurationResource::Create(bus,
            bus->GetUniqueName().c_str(), 0, "/Config", "v16.10.00", createCB, &createCB);
    resource->SetAboutData(&aboutData);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1));

    ExpectedProperties after;
    memset(&after, 0, sizeof(after));
    after.loc[0] = -2.0;
    after.loc[1] = 2.0;
    after.locn = "new-locn";
    after.c = "new-c";
    after.r = "new-r";
    LocalizedString newAppNames[] = {
        { "en", "new-en-app-name" },
        { "fr", "new-fr-app-name" }
    };
    after.ln = newAppNames;
    after.nln = sizeof(newAppNames) / sizeof(newAppNames[0]);
    after.dl = "en";
    after.n = newAppNames[0].value;
    after.vendorProperty = "x.org.iotivity.Field";
    after.vendorValue = "new-value";

    OCRepPayload *payload = OCRepPayloadCreate();
    size_t locDim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
    EXPECT_TRUE(OCRepPayloadSetDoubleArray(payload, "loc", after.loc, locDim));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "locn", after.locn));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "c", after.c));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "r", after.r));
    size_t lnDim[MAX_REP_ARRAY_DEPTH] = { after.nln, 0, 0 };
    size_t dimTotal = calcDimTotal(lnDim);
    OCRepPayload **ln = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        ln[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "language", after.ln[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "value", after.ln[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, ln, lnDim));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, after.dl));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEVICE_NAME, after.n));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, after.vendorProperty, after.vendorValue));
    Callback postDeviceCB(&UpdateDeviceResourceVerify, &after);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, "127.0.0.1/con", NULL,
            (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS, postDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postDeviceCB.Wait(1));

    delete resource;
}

static OCStackApplicationResult PlatformResourceVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    ExpectedProperties *properties = (ExpectedProperties *) ctx;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) response->payload;
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_PLATFORM_ID, &s));
    EXPECT_STREQ(properties->pi, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_NAME, &s));
    EXPECT_STREQ(properties->mnmn, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_URL, &s));
    EXPECT_STREQ(properties->mnml, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MODEL_NUM, &s));
    EXPECT_STREQ(properties->mnmo, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_DATE, &s));
    EXPECT_STREQ(properties->mndt, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_PLATFORM_VERSION, &s));
    EXPECT_STREQ(properties->mnpv, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_OS_VERSION, &s));
    EXPECT_STREQ(properties->mnos, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_HARDWARE_VERSION, &s));
    EXPECT_STREQ(properties->mnhw, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_FIRMWARE_VERSION, &s));
    EXPECT_STREQ(properties->mnfv, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_SUPPORT_URL, &s));
    EXPECT_STREQ(properties->mnsl, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_SYSTEM_TIME, &s));
    EXPECT_STREQ(properties->st, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_VID, &s));
    EXPECT_STREQ(properties->vid, s);
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult PlatformConfigurationResourceVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    ExpectedProperties *properties = (ExpectedProperties *) ctx;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) response->payload;
    size_t mnpnDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **mnpnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_PLATFORM_NAME, &mnpnArr, mnpnDim));
    EXPECT_EQ(properties->nmnpn, calcDimTotal(mnpnDim));
    for (size_t i = 0; i < properties->nmnpn; ++i)
    {
        char *s;
        EXPECT_TRUE(OCRepPayloadGetPropString(mnpnArr[i], "language", &s));
        EXPECT_STREQ(properties->mnpn[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(mnpnArr[i], "value", &s));
        EXPECT_STREQ(properties->mnpn[i].value, s);
    }
    return OC_STACK_DELETE_TRANSACTION;
}

TEST_F(VirtualServer, PlatformResource)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    /* DefaultLanguage => dl */
    properties.dl = "fr";
    AboutData aboutData(properties.dl);
    /* AppName => n */
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    properties.ln = appNames;
    properties.nln = sizeof(appNames) / sizeof(appNames[0]);
    for (size_t i = 0; i < properties.nln; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(appNames[i].value, appNames[i].language));
    }
    properties.n = appNames[1].value;
    /* DeviceId, if it is UUID => pi */
    properties.pi = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(properties.pi));
    /* Manufacturer in DefaultLanguage and truncated to 16 bytes => mnmn */
    properties.mnmn = "fr-manufacturer-";
    LocalizedString manufacturers[] = {
        { "en", "en-manufacturer-name" },
        { "fr", "fr-manufacturer-name" }
    };
    for (size_t i = 0; i < sizeof(manufacturers) / sizeof(manufacturers[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetManufacturer(manufacturers[i].value, manufacturers[i].language));
    }
    /* org.openconnectivity.mnml => mnml */
    properties.mnml = "manufacturer-url";
    EXPECT_EQ(ER_OK, aboutData.SetManufacturerUrl(properties.mnml));
    /* ModelNumber => mnmo */
    properties.mnmo = "model-number";
    EXPECT_EQ(ER_OK, aboutData.SetModelNumber(properties.mnmo));
    /* DateOfManufacture => mndt */
    properties.mndt = "date-of-manufacture";
    EXPECT_EQ(ER_OK, aboutData.SetDateOfManufacture(properties.mndt));
    /* org.openconnectivity.mnpv => mnpv */
    properties.mnpv = "platform-version";
    EXPECT_EQ(ER_OK, aboutData.SetPlatformVersion(properties.mnpv));
    /* org.openconnectivity.mnos => mnos */
    properties.mnos = "os-version";
    EXPECT_EQ(ER_OK, aboutData.SetOperatingSystemVersion(properties.mnos));
    /* HardwareVersion => mnhw */
    properties.mnhw = "hardware-version";
    EXPECT_EQ(ER_OK, aboutData.SetHardwareVersion(properties.mnhw));
    /* org.openconnectivity.mnfv => mnfv */
    properties.mnfv = "firmware-version";
    EXPECT_EQ(ER_OK, aboutData.SetFirmwareVersion(properties.mnfv));
    /* SupportUrl => mnsl */
    properties.mnsl = "support-url";
    EXPECT_EQ(ER_OK, aboutData.SetSupportUrl(properties.mnsl));
    /* org.openconnectivity.st => st */
    properties.st = "system-time";
    EXPECT_EQ(ER_OK, aboutData.SetSystemTime(properties.st));
    /* DeviceId => vid */
    properties.vid = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(properties.vid));

    /* DefaultLanguage => dl */
    /* DeviceName => mnpn */
    LocalizedString deviceNames[] = {
        { "en", "en-device-name" },
        { "fr", "fr-device-name" }
    };
    properties.mnpn = deviceNames;
    properties.nmnpn = sizeof(deviceNames) / sizeof(deviceNames[0]);

    VirtualDevice *device = new VirtualDevice(bus, "VirtualDevice", 0);
    device->SetProperties(NULL, &aboutData);
    ConfigBusObject configObj(bus, &properties);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    VirtualConfigurationResource *resource = VirtualConfigurationResource::Create(bus,
            bus->GetUniqueName().c_str(), 0, "/Config", "v16.10.00", createCB, &createCB);
    resource->SetAboutData(&aboutData);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1));

    Callback getPlatformCB(&PlatformResourceVerify, &properties);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "127.0.0.1/oic/p", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, getPlatformCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getPlatformCB.Wait(1));

    Callback getPlatformConfigurationCB(&PlatformConfigurationResourceVerify, &properties);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "127.0.0.1/con/p", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, getPlatformConfigurationCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getPlatformConfigurationCB.Wait(1));

    delete resource;
    delete device;
}
