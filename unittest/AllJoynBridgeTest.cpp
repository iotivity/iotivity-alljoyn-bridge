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
#include "VirtualConfigBusObject.h"
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
    EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, "loc", &locArr, locDim));
    EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
    EXPECT_EQ(loc[0], locArr[0]);
    EXPECT_EQ(loc[1], locArr[1]);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "locn", &s));
    EXPECT_STREQ(locn, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "c", &s));
    EXPECT_STREQ(c, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "r", &s));
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
            { iface->GetMember("UpdateConfigurations"), static_cast<MessageReceiver::MethodHandler>(&ConfigBusObject::UpdateConfigurations) },
            { iface->GetMember("FactoryReset"), static_cast<MessageReceiver::MethodHandler>(&ConfigBusObject::FactoryReset) },
            { iface->GetMember("Restart"), static_cast<MessageReceiver::MethodHandler>(&ConfigBusObject::Restart) }
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
        EXPECT_EQ(ER_OK, MethodReply(msg, ER_OK));
    }
    void FactoryReset(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
    {
        (void) member;
        EXPECT_EQ(ER_OK, MethodReply(msg, ER_OK));
    }
    void Restart(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
    {
        (void) member;
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

static OCStackApplicationResult MaintenanceResourceVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    (void) ctx;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) response->payload;
    bool b;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "fr", &b));
    EXPECT_FALSE(b);
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "rb", &b));
    EXPECT_FALSE(b);
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult FactoryResetVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    (void) ctx;
    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) response->payload;
    bool b;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "fr", &b));
    EXPECT_TRUE(b);
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult RebootVerify(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    (void) ctx;
    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) response->payload;
    bool b;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "rb", &b));
    EXPECT_TRUE(b);
    return OC_STACK_DELETE_TRANSACTION;
}

TEST_F(VirtualServer, MaintenanceResource)
{
    ConfigBusObject configObj(bus, NULL);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    VirtualConfigurationResource *resource = VirtualConfigurationResource::Create(bus,
            bus->GetUniqueName().c_str(), 0, "/Config", "v16.10.00", createCB, &createCB);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1));

    Callback getCB(&MaintenanceResourceVerify);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "127.0.0.1/oic/mnt", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1));

    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "fr", true));
    Callback setFactoryResetCB(&FactoryResetVerify);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, "127.0.0.1/oic/mnt", NULL,
            (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS, setFactoryResetCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, setFactoryResetCB.Wait(1));

    payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "rb", true));
    Callback setRebootCB(&RebootVerify);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, "127.0.0.1/oic/mnt", NULL,
            (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS, setRebootCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, setRebootCB.Wait(1));

    delete resource;
}

class VirtualProducer : public AJOCSetUp
{
protected:
    ajn::BusAttachment *bus;
    virtual void SetUp()
    {
        AJOCSetUp::SetUp();
        bus = new ajn::BusAttachment("Consumer");
        EXPECT_EQ(ER_OK, bus->Start());
        EXPECT_EQ(ER_OK, bus->Connect());
    }
    virtual void TearDown()
    {
        delete bus;
        AJOCSetUp::TearDown();
    }
};

class ConfigurationResource
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
        EXPECT_TRUE(OCRepPayloadSetDoubleArray(payload, "loc", loc, dim));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "locn", "locn"));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "c", "c"));
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "r", "r"));
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
        EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, "loc", &locArr, locDim));
        EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
        EXPECT_EQ(m_properties->loc[0], locArr[0]);
        EXPECT_EQ(m_properties->loc[1], locArr[1]);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "locn", &s));
        EXPECT_STREQ(m_properties->locn, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "c", &s));
        EXPECT_STREQ(m_properties->c, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "r", &s));
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

static OCStackApplicationResult DiscoverConfigurationResource(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Resource **r = (Resource **) ctx;
    (void) handle;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_DISCOVERY, response->payload->type);
    for (OCDiscoveryPayload *payload = (OCDiscoveryPayload *) response->payload; payload;
         payload = payload->next)
    {
        for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
             resource = resource->next)
        {
            if (!strcmp(resource->uri, "/con"))
            {
                *r = new Resource(response->devAddr, payload->sid, resource);
            }
        }
    }
    return OC_STACK_DELETE_TRANSACTION;
}

static OCStackApplicationResult DiscoverPlatformConfigurationResource(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    Resource **r = (Resource **) ctx;
    (void) handle;
    EXPECT_EQ(OC_STACK_OK, response->result);
    EXPECT_TRUE(response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_DISCOVERY, response->payload->type);
    for (OCDiscoveryPayload *payload = (OCDiscoveryPayload *) response->payload; payload;
         payload = payload->next)
    {
        for (OCResourcePayload *resource = (OCResourcePayload *) payload->resources; resource;
             resource = resource->next)
        {
            if (!strcmp(resource->uri, "/con/p"))
            {
                *r = new Resource(response->devAddr, payload->sid, resource);
            }
        }
    }
    return OC_STACK_DELETE_TRANSACTION;
}

TEST_F(VirtualProducer, DeviceConfigurationProperties)
{
    DeviceConfigurationResource deviceConfiguration;

    Resource *resource = NULL;
    Callback discoverDeviceCB(&DiscoverConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
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
    MethodCall get(bus, proxyObj);
    EXPECT_EQ(ER_OK, get.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
    EXPECT_EQ(OC_STACK_OK, get.Wait(1));
    size_t numArgs;
    const ajn::MsgArg *args;
    get.Reply()->GetArgs(numArgs, args);
    EXPECT_EQ(1u, numArgs);
    EXPECT_EQ(0u, args[0].v_array.GetNumElements());

    /* All properties */
    deviceConfiguration.SetAllProperties();
    MethodCall getAll(bus, proxyObj);
    EXPECT_EQ(ER_OK, getAll.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
    EXPECT_EQ(OC_STACK_OK, getAll.Wait(1));
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

    Resource *resource = NULL;
    Callback discoverPlatformCB(&DiscoverPlatformConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverPlatformCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverPlatformCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.GetConfigurations */

    /* All properties (there are no mandatory properties) */
    platformConfiguration.SetAllProperties();
    ajn::MsgArg lang("s", "");
    MethodCall getAll(bus, proxyObj);
    EXPECT_EQ(ER_OK, getAll.Call("org.alljoyn.Config", "GetConfigurations", &lang, 1));
    EXPECT_EQ(OC_STACK_OK, getAll.Wait(1));
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
    EXPECT_EQ(OC_STACK_OK, get.Wait(1));
    size_t numArgs;
    const ajn::MsgArg *args;
    get.Reply()->GetArgs(numArgs, args);
    EXPECT_EQ(1u, numArgs);
    EXPECT_EQ(0u, args[0].v_array.GetNumElements());

    /* All properties */
    configuration->SetAllProperties();
    MethodCall getAll(bus, proxyObj);
    EXPECT_EQ(ER_OK, getAll.Call("org.alljoyn.Config", "GetConfigurations", &arg, 1));
    EXPECT_EQ(OC_STACK_OK, getAll.Wait(1));
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

    Resource *resource = NULL;
    Callback discoverDeviceCB(&DiscoverConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);

    resource->m_uri = "/oic/mnt";
    resource->m_ifs.clear();
    resource->m_ifs.push_back("oic.if.rw");
    resource->m_ifs.push_back("oic.if.r");
    resource->m_ifs.push_back("oic.if.baseline");
    resource->m_rts.clear();
    resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*resource);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    ConfigurationPropertiesVerify(&configuration, bus, proxyObj);

    delete proxyObj;
    delete obj;
}

TEST_F(VirtualProducer, ConfigurationPropertiesDifferentResources)
{
    ConfigurationResource configuration(true, true);

    Resource *resource = NULL;
    Callback discoverDeviceCB(&DiscoverConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);

    resource->m_uri = "/con/p";
    resource->m_ifs.clear();
    resource->m_ifs.push_back("oic.if.rw");
    resource->m_ifs.push_back("oic.if.baseline");
    resource->m_rts.clear();
    resource->m_rts.push_back("oic.wk.con.p");
    obj->AddResource(*resource);

    resource->m_uri = "/oic/mnt";
    resource->m_ifs.clear();
    resource->m_ifs.push_back("oic.if.rw");
    resource->m_ifs.push_back("oic.if.r");
    resource->m_ifs.push_back("oic.if.baseline");
    resource->m_rts.clear();
    resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*resource);

    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    ConfigurationPropertiesVerify(&configuration, bus, proxyObj);

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

    Resource *resource = NULL;
    Callback discoverDeviceCB(&DiscoverConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(bus, proxyObj);
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
    EXPECT_EQ(OC_STACK_OK, update.Wait(1));

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

    Resource *resource = NULL;
    Callback discoverPlatformCB(&DiscoverPlatformConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverPlatformCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverPlatformCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(bus, proxyObj);
    ajn::MsgArg elems[1];
    ajn::MsgArg *elem = elems;
    elem->Set("{sv}", "DeviceName", new ajn::MsgArg("s", deviceNames[0].value));
    ++elem;
    ajn::MsgArg args[2];
    args[0].Set("s", deviceNames[0].language);
    args[1].Set("a{sv}", elem - elems, elems);
    EXPECT_EQ(ER_OK, update.Call("org.alljoyn.Config", "UpdateConfigurations", args, 2));
    EXPECT_EQ(OC_STACK_OK, update.Wait(1));

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

    Resource *resource = NULL;
    Callback discoverDeviceCB(&DiscoverConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);

    resource->m_uri = "/oic/mnt";
    resource->m_ifs.clear();
    resource->m_ifs.push_back("oic.if.rw");
    resource->m_ifs.push_back("oic.if.r");
    resource->m_ifs.push_back("oic.if.baseline");
    resource->m_rts.clear();
    resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*resource);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(bus, proxyObj);
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
    EXPECT_EQ(OC_STACK_OK, update.Wait(1));

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

    Resource *resource = NULL;
    Callback discoverDeviceCB(&DiscoverConfigurationResource, &resource);
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "127.0.0.1/oic/res", NULL, 0,
            CT_DEFAULT, OC_HIGH_QOS, discoverDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, discoverDeviceCB.Wait(1));
    VirtualConfigBusObject *obj = new VirtualConfigBusObject(bus, *resource);

    resource->m_uri = "/con/p";
    resource->m_ifs.clear();
    resource->m_ifs.push_back("oic.if.rw");
    resource->m_ifs.push_back("oic.if.baseline");
    resource->m_rts.clear();
    resource->m_rts.push_back("oic.wk.con.p");
    obj->AddResource(*resource);

    resource->m_uri = "/oic/mnt";
    resource->m_ifs.clear();
    resource->m_ifs.push_back("oic.if.rw");
    resource->m_ifs.push_back("oic.if.r");
    resource->m_ifs.push_back("oic.if.baseline");
    resource->m_rts.clear();
    resource->m_rts.push_back("oic.wk.mnt");
    obj->AddResource(*resource);
    EXPECT_EQ(ER_OK, bus->RegisterBusObject(*obj));

    ajn::ProxyBusObject *proxyObj = new ajn::ProxyBusObject(*bus, bus->GetUniqueName().c_str(), "/Config", 0);
    EXPECT_EQ(ER_OK, proxyObj->IntrospectRemoteObject());

    /* org.alljoyn.Config.UpdateConfigurations */
    MethodCall update(bus, proxyObj);
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
    EXPECT_EQ(OC_STACK_OK, update.Wait(1));

    delete proxyObj;
    delete obj;
}
