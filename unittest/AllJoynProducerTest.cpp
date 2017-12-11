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

/*
 * 7.1.1 Exposing AllJoyn producer devices to OCF Clients
 */

#include "UnitTest.h"

#include "AboutData.h"
#include "DeviceConfigurationResource.h"
#include "DeviceResource.h"
#include "Hash.h"
#include "Interfaces.h"
#include "Name.h"
#include "Plugin.h"
#include "VirtualConfigurationResource.h"
#include "VirtualDevice.h"
#include "VirtualResource.h"
#include "ocpayload.h"
#include "ocstack.h"
#include "oic_malloc.h"
#include <alljoyn/BusAttachment.h>
#include <alljoyn/SessionPortListener.h>

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

const char *TestInterfaceName = "org.iotivity.Interface";

class TestBusObject : public ajn::BusObject
{
public:
    TestBusObject(ajn::BusAttachment *bus, const char *path, const char *xml) : ajn::BusObject(path)
    {
        EXPECT_EQ(ER_OK, bus->CreateInterfacesFromXml(xml));
        const ajn::InterfaceDescription *iface = bus->GetInterface(TestInterfaceName);
        AddInterface(*iface);
        size_t numMembers = iface->GetMembers();
        const ajn::InterfaceDescription::Member *members[numMembers];
        iface->GetMembers(members, numMembers);
        for (size_t i = 0; i < numMembers; ++i)
        {
            if (members[i]->memberType == ajn::MESSAGE_METHOD_CALL)
            {
                EXPECT_EQ(ER_OK, AddMethodHandler(members[i], static_cast<MessageReceiver::MethodHandler>(&TestBusObject::Method)));
            }
        }
    }
    virtual ~TestBusObject() { }
    QStatus Get(const char* iface, const char* prop, ajn::MsgArg& val)
    {
        if (!strcmp(iface, TestInterfaceName))
        {
            if (!strcmp(prop, "Version"))
            {
                return val.Set("q", 1);
            }
            else if (!strcmp(prop, "Const") || !strcmp(prop, "False") || !strcmp(prop, "True") ||
                    !strcmp(prop, "Invalidates"))
            {
                return val.Set("q", 1);
            }
            else if (!strcmp(prop, "I64S") || !strcmp(prop, "I64L"))
            {
                return val.Set("x", 1);
            }
            else if (!strcmp(prop, "U64S") || !strcmp(prop, "U64L"))
            {
                return val.Set("t", 1);
            }
            else if (!strcmp(prop, "ObjPath"))
            {
                return val.Set("o", "/Test");
            }
            else if (!strcmp(prop, "Sig"))
            {
                return val.Set("g", "sig");
            }
            else if (!strcmp(prop, "StructName"))
            {
                return val.Set("(is)", 1, "string");
            }
            else if (!strcmp(prop, "ArrOfStructName"))
            {
                ajn::MsgArg args[2];
                args[0].Set("(is)", 0, "string0");
                args[1].Set("(is)", 1, "string1");
                QStatus status = val.Set("a(is)", 2, args);
                val.Stabilize();
                return status;
            }
            else if (!strcmp(prop, "ArrOfU8"))
            {
                uint8_t ay[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f };
                QStatus status = val.Set("ay", A_SIZEOF(ay), ay);
                val.Stabilize();
                return status;
            }
            else if (!strcmp(prop, "Variant"))
            {
                return val.Set("v", new ajn::MsgArg("i", 0));
            }
            else if (!strcmp(prop, "ArrOfI32"))
            {
                return val.Set("ai", 0, NULL);
            }
            else if (!strcmp(prop, "ArrOfI64"))
            {
                return val.Set("ax", 0, NULL);
            }
            else if (!strcmp(prop, "Struct"))
            {
                return val.Set("(ii)", 0, 1);
            }
            else if (!strcmp(prop, "oneTwo") ||
                    !strcmp(prop, "one_dtwo") ||
                    !strcmp(prop, "one_htwo"))
            {
                return val.Set("x", 12);
            }
            else if (!strcmp(prop, "dict"))
            {
                ajn::MsgArg entries[2];
                ajn::MsgArg *entry;
                entry = entries;
                (entry++)->Set("{sv}", "one.two", new ajn::MsgArg("x", 12));
                (entry++)->Set("{sv}", "one-two", new ajn::MsgArg("s", "one-two"));
                QStatus status = val.Set("a{sv}", entry - entries, entries);
                val.Stabilize();
                return status;
            }
            else if (!strcmp(prop, "struct"))
            {
                return val.Set("(xs)", 12, "one-two");
            }
            else
            {
                return ER_BUS_NO_SUCH_PROPERTY;
            }
        }
        else
        {
            return ER_BUS_NO_SUCH_INTERFACE;
        }
    }
    QStatus Set(const char* iface, const char* prop, ajn::MsgArg& val)
    {
        (void) iface;
        (void) prop;
        (void) val;
        return ER_OK;
    }
    void Method(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
    {
        if (member->name == "Error")
        {
            EXPECT_EQ(ER_OK, MethodReply(msg, ER_FAIL));
        }
        else if (member->name == "ErrorName")
        {
            EXPECT_EQ(ER_OK, MethodReply(msg, "org.freedesktop.DBus.Error.Failed", "Message"));
        }
        else if (member->name == "OCFErrorName")
        {
            EXPECT_EQ(ER_OK, MethodReply(msg, "org.openconnectivity.Error.Name", "Message"));
        }
        else if (member->name == "OCFErrorCode")
        {
            EXPECT_EQ(ER_OK, MethodReply(msg, "org.openconnectivity.Error.Code404", "Message"));
        }
        else if (member->name == "MethodWithIntrospection")
        {
            ajn::MsgArg args[13];
            args[0].Set("x", 1);
            args[1].Set("x", 1);
            args[2].Set("t", 1);
            args[3].Set("t", 1);
            args[4].Set("o", "/Test");
            args[5].Set("g", "sig");
            args[6].Set("(is)", 1, "string");
            ajn::MsgArg rs[2];
            rs[0].Set("(is)", 0, "string0");
            rs[1].Set("(is)", 1, "string1");
            args[7].Set("a(is)", 2, rs);
            uint8_t ay[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f };
            args[8].Set("ay", A_SIZEOF(ay), ay);
            args[8].Stabilize();
            args[9].Set("v", new ajn::MsgArg("i", 0));
            args[10].Set("ai", 0, NULL);
            args[11].Set("ax", 0, NULL);
            args[12].Set("(ii)", 0, 1);
            EXPECT_EQ(ER_OK, MethodReply(msg, args, 13));
        }
        else if (member->name == "PropertyNamesMethod")
        {
            size_t numArgs;
            const ajn::MsgArg *args;
            msg->GetArgs(numArgs, args);
            EXPECT_EQ(ER_OK, MethodReply(msg, args, numArgs));
        }
        else
        {
            EXPECT_EQ(ER_OK, MethodReply(msg, ER_OK));
        }
    }
    void PropertiesChanged()
    {
        const char *props[] = { "True", "Invalidates" };
        PropertiesChanged(props, A_SIZEOF(props));
    }
    void PropertiesChanged(const char **props, size_t numProps)
    {
        EXPECT_EQ(ER_OK, EmitPropChanged(TestInterfaceName, props, numProps,
                ajn::SESSION_ID_ALL_HOSTED));
    }
    void Signal()
    {
        const ajn::InterfaceDescription::Member *member =
                bus->GetInterface(TestInterfaceName)->GetSignal("Signal");
        EXPECT_TRUE(member != NULL);
        EXPECT_EQ(ER_OK, ajn::BusObject::Signal(NULL, ajn::SESSION_ID_ALL_HOSTED, *member));
    }
    void Signal(uint16_t arg0, uint16_t arg1)
    {
        const ajn::InterfaceDescription::Member *member =
                bus->GetInterface(TestInterfaceName)->GetSignal("Signal");
        EXPECT_TRUE(member != NULL);
        ajn::MsgArg args[2];
        args[0].Set("q", arg0);
        args[1].Set("q", arg1);
        EXPECT_EQ(ER_OK, ajn::BusObject::Signal(NULL, 0, *member, args, 2, 0,
                ajn::ALLJOYN_FLAG_SESSIONLESS));
    }
    void SignalWithIntrospection()
    {
        const ajn::InterfaceDescription::Member *member =
                bus->GetInterface(TestInterfaceName)->GetSignal("SignalWithIntrospection");
        EXPECT_TRUE(member != NULL);
        ajn::MsgArg args[13];
        args[0].Set("x", 1);
        args[1].Set("x", 1);
        args[2].Set("t", 1);
        args[3].Set("t", 1);
        args[4].Set("o", "/Test");
        args[5].Set("g", "sig");
        args[6].Set("(is)", 1, "string");
        ajn::MsgArg rs[2];
        rs[0].Set("(is)", 0, "string0");
        rs[1].Set("(is)", 1, "string1");
        args[7].Set("a(is)", 2, rs);
        uint8_t ay[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f };
        args[8].Set("ay", A_SIZEOF(ay), ay);
        args[8].Stabilize();
        args[9].Set("v", new ajn::MsgArg("i", 0));
        args[10].Set("ai", 0, NULL);
        args[11].Set("ax", 0, NULL);
        args[12].Set("(ii)", 0, 1);
        EXPECT_EQ(ER_OK, ajn::BusObject::Signal(NULL, ajn::SESSION_ID_ALL_HOSTED, *member, args, 13));
    }
    void PropertyNamesSignal()
    {
        const ajn::InterfaceDescription::Member *member =
                bus->GetInterface(TestInterfaceName)->GetSignal("PropertyNamesSignal");
        EXPECT_TRUE(member != NULL);
        ajn::MsgArg args[5];
        args[0].Set("x", 12);
        args[1].Set("x", 12);
        args[2].Set("x", 12);
        ajn::MsgArg entries[2];
        ajn::MsgArg *entry;
        entry = entries;
        (entry++)->Set("{sv}", "one.two", new ajn::MsgArg("x", 12));
        (entry++)->Set("{sv}", "one-two", new ajn::MsgArg("s", "one-two"));
        args[3].Set("a{sv}", entry - entries, entries);
        args[3].Stabilize();
        args[4].Set("(xs)", 12, "one-two");
        EXPECT_EQ(ER_OK, ajn::BusObject::Signal(NULL, ajn::SESSION_ID_ALL_HOSTED, *member, args, 5));
    }
};

class TestConfigBusObject : public ajn::BusObject
{
public:
    TestConfigBusObject(ajn::BusAttachment *bus, ExpectedProperties *properties)
        : ajn::BusObject("/Config"), m_properties(properties)
    {
        EXPECT_EQ(ER_OK, bus->CreateInterfacesFromXml(ajn::org::alljoyn::Config::InterfaceXml));
        const ajn::InterfaceDescription *iface = bus->GetInterface("org.alljoyn.Config");
        AddInterface(*iface);
        const MethodEntry methodEntries[] = {
            { iface->GetMember("GetConfigurations"), static_cast<MessageReceiver::MethodHandler>(&TestConfigBusObject::GetConfigurations) },
            { iface->GetMember("UpdateConfigurations"), static_cast<MessageReceiver::MethodHandler>(&TestConfigBusObject::UpdateConfigurations) },
            { iface->GetMember("FactoryReset"), static_cast<MessageReceiver::MethodHandler>(&TestConfigBusObject::FactoryReset) },
            { iface->GetMember("Restart"), static_cast<MessageReceiver::MethodHandler>(&TestConfigBusObject::Restart) }
        };
        AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
    }
    virtual ~TestConfigBusObject() { }
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
private:
    ExpectedProperties *m_properties;
};

template <class T>
class AllJoynProducerBase : public ajn::SessionPortListener, public T
{
protected:
    ajn::BusAttachment *m_bus;
    ajn::SessionPort m_port;
    ajn::SessionOpts m_opts;
    ajn::SessionId m_sid;
    TestBusObject *m_obj;
    VirtualResource *m_resource;
    virtual ~AllJoynProducerBase() { }
    virtual void SetUp()
    {
        AJOCSetUp::SetUpStack();
        m_bus = new ajn::BusAttachment("Producer");
        EXPECT_EQ(ER_OK, m_bus->Start());
        EXPECT_EQ(ER_OK, m_bus->Connect());
        m_port = ajn::SESSION_PORT_ANY;
        EXPECT_EQ(ER_OK, m_bus->BindSessionPort(m_port, m_opts, *this));
        ajn::SessionOpts opts;
        EXPECT_EQ(ER_OK, m_bus->JoinSession(m_bus->GetUniqueName().c_str(), m_port, NULL, m_sid,
                opts));
        m_obj = NULL;
        m_resource = NULL;
    }
    virtual void TearDown()
    {
        delete m_obj;
        delete m_resource;
        delete m_bus;
        AJOCSetUp::TearDownStack();
    }
    virtual bool AcceptSessionJoiner(ajn::SessionPort port, const char *name,
            const ajn::SessionOpts& opts)
    {
        (void) port;
        (void) name;
        (void) opts;
        return true;
    }
    DiscoverContext *CreateAndDiscoverVirtualResource(const char *xml)
    {
        return CreateAndDiscoverVirtualResource("/Test", xml);
    }
    DiscoverContext *CreateAndDiscoverVirtualResource(const char *path, const char *xml)
    {
        m_obj = new TestBusObject(m_bus, path, xml);
        EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(*m_obj));

        CreateCallback createCB;
        m_resource = VirtualResource::Create(m_bus, m_bus->GetUniqueName().c_str(), m_sid,
                m_obj->GetPath(), "v16.10.00", createCB, &createCB);
        EXPECT_EQ(OC_STACK_OK, createCB.Wait(1000));

        return DiscoverVirtualResource(ToUri(m_obj->GetPath()).c_str());
    }
    DiscoverContext *DiscoverVirtualResource(const char *path)
    {
        DiscoverContext *context = new DiscoverContext(path);
        Callback discoverCB(Discover, context);
        EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_DISCOVER, "/oic/res", NULL, 0,
                CT_DEFAULT, OC_HIGH_QOS, discoverCB, NULL, 0));
        EXPECT_EQ(OC_STACK_OK, discoverCB.Wait(1000));
        return context;
    }
};

class AllJoynProducer : public AllJoynProducerBase<::testing::Test>
{
public:
    virtual ~AllJoynProducer() { }
};

TEST_F(AllJoynProducer, WhenAllJoynInterfacesCanBeTranslatedToResourceTypesOnTheSameResourceThereShouldBeASingleVirtualOCFResource)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_EQ(m_obj->GetPath(), context->m_resource->m_uri);
    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.const"));
    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.false"));
    EXPECT_EQ(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "oic.wk.col"));
    EXPECT_EQ(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "oic.r.alljoynobject"));

    delete context;
}

static void VerifyAllJoynObjectLinks(DiscoverContext *context, OCDevAddr addr,
        OCRepPayload *payload)
{
    OCRepPayload **arr;
    size_t dim[MAX_REP_ARRAY_DEPTH];
    if (!OCRepPayloadGetPropObjectArray(payload, "links", &arr, dim))
    {
        memset(dim, 0, sizeof(size_t) * MAX_REP_ARRAY_DEPTH);
        OCRepPayload *p;
        for (p = payload; p; p = p->next)
        {
            ++dim[0];
        }
        arr = (OCRepPayload **) OICCalloc(dim[0], sizeof(OCRepPayload*));
        p = payload;
        for (size_t i = 0; i < dim[0]; ++i)
        {
            arr[i] = OCRepPayloadClone(p);
            p = p->next;
        }
    }
    size_t dimTotal = calcDimTotal(dim);
    EXPECT_EQ(2u, dimTotal);
    for (size_t i = 0; i < dimTotal; ++i)
    {
        OCResourcePayload *rp = ParseLink(arr[i]);
        Resource resource(addr, context->m_device->m_di.c_str(), rp);
        if (resource.m_isObservable)
        {
            EXPECT_NE(resource.m_rts.end(), std::find(resource.m_rts.begin(), resource.m_rts.end(),
                    "x.org.iotivity.-interface.true"));
            EXPECT_NE(resource.m_rts.end(), std::find(resource.m_rts.begin(), resource.m_rts.end(),
                    "x.org.iotivity.-interface.invalidates"));
        }
        else
        {
            EXPECT_NE(resource.m_rts.end(), std::find(resource.m_rts.begin(), resource.m_rts.end(),
                    "x.org.iotivity.-interface.const"));
            EXPECT_NE(resource.m_rts.end(), std::find(resource.m_rts.begin(), resource.m_rts.end(),
                    "x.org.iotivity.-interface.false"));
        }
    }
}

TEST_F(AllJoynProducer, WhenAllJoynInterfacesCannotBeTranslatedToResourceTypesOnTheSameResourceThereShouldBeACollectionOfVirtualOCFResources)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "  <property name='True' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_EQ(m_obj->GetPath(), context->m_resource->m_uri);
    EXPECT_EQ(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.const"));
    EXPECT_EQ(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.false"));
    EXPECT_EQ(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.true"));
    EXPECT_EQ(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.invalidates"));
    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "oic.wk.col"));
    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "oic.r.alljoynobject"));

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    VerifyAllJoynObjectLinks(context, getCB.m_response->devAddr, payload);

    delete context;
}

TEST_F(AllJoynProducer, ThePiidPropertyValueShallBeTheOCFDefinedOrgOpenconnectivityPiidFieldInTheAllJoynAboutAnnounceSignal)
{
    AboutData aboutData("");
    /* org.openconnectivity.piid => piid */
    const char *piid = "10f70cc4-2398-41f5-8062-4c1facbfc41b";
    const char *peerGuid = "10f70cc4239841f580624c1facbfc41b";
    EXPECT_EQ(ER_OK, aboutData.SetProtocolIndependentId(piid));

    EXPECT_EQ(OC_STACK_OK, SetDeviceProperties(NULL, NULL, &aboutData, peerGuid));

    char *s;
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID,
            (void**) &s));
    EXPECT_STREQ(piid, s);
}

TEST_F(AllJoynProducer, ThePiidPropertyValueShallBeThePeerGUID)
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
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID,
            (void**) &s));
    EXPECT_STREQ(piid, s);
}

TEST_F(AllJoynProducer, ThePiidPropertyValueShallBeDerivedFromTheDeviceIdAndAppId)
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
    EXPECT_EQ(OC_STACK_OK, OCGetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID,
            (void**) &s));
    EXPECT_STREQ(piid, s);
}

TEST_F(AllJoynProducer, TheTranslatorShallEitherNotTranslateTheAllJoynInterfaceOrAlgorithmicallyMapTheAllJoynInterface)
{
    /* Interface is in a well-defined set */
    EXPECT_TRUE(TranslateInterface("org.alljoyn.Config"));
    EXPECT_FALSE(TranslateInterface("Operation.OffControl"));

    /* Interface is not in a well-defined set */
    EXPECT_TRUE(TranslateInterface("org.iotivity.Interface"));
}

class ShallBeConvertedToATypeAsFollows : public ::testing::TestWithParam<const char *> { };

TEST_P(ShallBeConvertedToATypeAsFollows, RoundTrip)
{
    const char *ajName = GetParam();
    EXPECT_STREQ(ajName, ToAJName(ToOCName(ajName)).c_str());
}

INSTANTIATE_TEST_CASE_P(AllJoynInterfaceName, ShallBeConvertedToATypeAsFollows, ::testing::Values(
            /* From spec */
            "example.Widget", "example.my_widget", "example.My_Widget", "xn_p1ai.example",
            "xn__90ae.example", "example.myName_1",
            "com.example.M__w.m_W.x__9_1",
            /* Extras */
            "oneTwoThree", "One_Two_Three", "", "x", "example.foo_", "example.foo__"));

TEST(AllJoynInterfaceNameShallBeConvertedToATypeAsFollows, BoundsCheck)
{
    EXPECT_STREQ("", ToAJName("").c_str());
    EXPECT_STREQ("x", ToAJName("x").c_str());
    EXPECT_STREQ("example.foo_", ToAJName("x.example.foo-").c_str());
    EXPECT_STREQ("example.foo__", ToAJName("x.example.foo--").c_str());
    EXPECT_STREQ("example.foo__", ToAJName("x.example.foo---").c_str());
}

TEST_F(AllJoynProducer, TheNameOfEachPropertyShallBePrefixedWithTheInterfaceName)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "  <property name='True' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "  <method name='Method'>"
            "    <arg name='InArg' type='q' direction='in'/>"
            "    <arg type='q' direction='out'/>"
            "  </method>"
            "  <signal name='Signal'>"
            "    <arg name='ArgA' type='q'/>"
            "    <arg name='ArgB' type='q'/>"
            "  </signal>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "/Test/1?if=oic.if.baseline",
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    int64_t i;
    bool b;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Const", &i));
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.False", &i));
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "x.org.iotivity.-interface.-methodvalidity", &b));
    // TODO value is OCREP_PROP_NULL for below - need dummy value instead so that introspection data is correct
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-methodInArg"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-methodarg1"));

    getCB.Reset();
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, "/Test/3?if=oic.if.baseline",
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    payload = (OCRepPayload *) getCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.True", &i));
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Invalidates", &i));
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "x.org.iotivity.-interface.-signalvalidity", &b));
    // TODO value is OCREP_PROP_NULL for below - need dummy value instead so that introspection data is correct
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signalArgA"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signalArgB"));
}

TEST(AllJoynPropertyName, ShallBeConvertedToAPropertyAsFollows)
{
    EXPECT_STREQ("one_dtwo", ToAJPropName("one.two").c_str());
    EXPECT_STREQ("one_htwo", ToAJPropName("one-two").c_str());

    EXPECT_STREQ("one.two", ToOCPropName("one_dtwo").c_str());
    EXPECT_STREQ("one-two", ToOCPropName("one_htwo").c_str());
}

TEST_F(AllJoynProducer, AllJoynPropertiesWithTheSameEmitsChangedSignalValueAreMappedToTheSameResourceType)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "  <property name='True' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    struct {
        const char *path;
        const char *rt;
        const char *value;
    } emitsChangedSignal[] = {
        { "/Test/1", "const", "Const" },
        { "/Test/1", "false", "False" },
        { "/Test/3", "true", "True" },
        { "/Test/3", "invalidates", "Invalidates" }
    };
    for (size_t i = 0; i < sizeof(emitsChangedSignal) / sizeof(emitsChangedSignal[0]); ++i)
    {
        std::string rt = std::string("x.org.iotivity.-interface.") + emitsChangedSignal[i].rt;
        std::string uri = std::string(emitsChangedSignal[i].path) + "?rt=" + rt;
        ResourceCallback getCB;
        EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
                &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
        EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

        EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
        EXPECT_TRUE(getCB.m_response->payload != NULL);
        EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
        OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
        for (OCRepPayloadValue *value = payload->values; value; value = value->next)
        {
            EXPECT_TRUE(strstr(value->name, emitsChangedSignal[i].value));
        }
    }

    delete context;
}

TEST_F(AllJoynProducer, AllJoynPropertiesWithEmitsChangedSignalValuesOfConstOrFalseAreMappedToResourcesThatAreNotObservable)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_FALSE(context->m_resource->m_isObservable);

    delete context;
}

TEST_F(AllJoynProducer, AllJoynPropertiesWithEmitsChangedSignalValuesOfTrueOrInvalidatesAreMappedToResourcesThatAreObservable)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='True' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_TRUE(context->m_resource->m_isObservable);

    delete context;
}

TEST_F(AllJoynProducer, VersionPropertyIsAlwaysConsideredConst)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Version' type='q' access='read'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string rt = "x.org.iotivity.-interface.const";
    std::string uri = std::string(m_obj->GetPath()) + "?rt=" + rt;
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    int64_t i;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Version", &i));

    delete context;
}

TEST_F(AllJoynProducer, ResourceTypesMappingAllJoynPropertiesWithAccessReadWriteShallSupportTheRWInterface)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='False' type='q' access='readwrite'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_EQ(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.r"));
    EXPECT_NE(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.rw"));

    delete context;
}

TEST_F(AllJoynProducer, ResourceTypesMappingAllJoynPropertiesWithAccessReadShallSupportTheRInterface)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='True' type='q' access='read'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_NE(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.r"));
    EXPECT_EQ(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.rw"));

    delete context;
}

TEST_F(AllJoynProducer, ResourceTypesSupportingBothTheRWAndRInterfacesShallChooseRAsTheDefaultInterface)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='False' type='q' access='readwrite'/>"
            "  <property name='True' type='q' access='read'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_NE(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.r"));
    EXPECT_NE(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.rw"));
    // TODO IoTivity always inserts oic.if.baseline as the first interface, so check below will fail:
    EXPECT_EQ("oic.if.rw", context->m_resource->m_ifs[0]);

    delete context;
}

TEST_F(AllJoynProducer, EachAllJoynMethodIsMappedToASeparateResourceType)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='MethodA'/>"
            "  <method name='MethodB'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.-method-a"));
    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.-method-b"));

    delete context;
}

TEST_F(AllJoynProducer, EachAllJoynMethodResourceTypeShallSupportTheRWInterface)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='Method'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_EQ(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.r"));
    EXPECT_NE(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.rw"));

    delete context;
}

TEST_F(AllJoynProducer, EachArgumentOfTheAllJoynMethodIsMappedToASeparatePropertyOnTheResourceType)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='Method'>"
            "    <arg name='InArg' type='q' direction='in'/>"
            "    <arg name='OutArg' type='q' direction='out'/>"
            "  </method>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    // TODO value is OCREP_PROP_NULL for below - need dummy value instead so that introspection data is correct
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-methodInArg"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-methodOutArg"));

    delete context;
}

TEST_F(AllJoynProducer, WhenTheAllJoynMethodArgumentNameIsNotSpecified)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='Method'>"
            "    <arg type='q' direction='in'/>"
            "    <arg type='q' direction='out'/>"
            "  </method>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    // TODO value is OCREP_PROP_NULL for below - need dummy value instead so that introspection data is correct
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-methodarg0"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-methodarg1"));

    delete context;
}

TEST_F(AllJoynProducer, TheAllJoynMethodResourceTypeHasAnExtraValidityProperty)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='Method'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    bool b;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "x.org.iotivity.-interface.-methodvalidity", &b));
    EXPECT_FALSE(b);

    delete context;
}

TEST_F(AllJoynProducer, InAnUpdateRequestAValidityValueOfFalseShallResultInAnErrorResponse)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='Method'>"
            "    <arg type='q' direction='in'/>"
            "    <arg type='q' direction='out'/>"
            "  </method>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    OCRepPayload *request = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropBool(request, "x.org.iotivity.-interface.-methodvalidity",
            false));
    ResourceCallback postCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) request, CT_DEFAULT, OC_HIGH_QOS,
            postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    EXPECT_GT(postCB.m_response->result, OC_STACK_RESOURCE_CHANGED);

    delete context;
}

TEST_F(AllJoynProducer, EachAllJoynSignalIsMappedToASeparateResourceTypeOnAnObservableResource)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <signal name='Sessionless' sessionless='true'/>"
            "  <signal name='Sessioncast' sessioncast='true'/>"
            "  <signal name='Unicast' unicast='true'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.-sessionless"));
    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.-sessioncast"));
    EXPECT_NE(context->m_resource->m_rts.end(),
            std::find(context->m_resource->m_rts.begin(), context->m_resource->m_rts.end(),
                    "x.org.iotivity.-interface.-unicast"));
    EXPECT_TRUE(context->m_resource->m_isObservable);

    delete context;
}

TEST_F(AllJoynProducer, EachAllJoynSignalResourceTypeShallSupportTheRInterface)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <signal name='Signal'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    EXPECT_NE(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.r"));
    EXPECT_EQ(context->m_resource->m_ifs.end(),
            std::find(context->m_resource->m_ifs.begin(), context->m_resource->m_ifs.end(),
                    "oic.if.rw"));

    delete context;
}

TEST_F(AllJoynProducer, EachArgumentOfTheAllJoynSignalIsMappedToASeparatePropertyOnTheResourceType)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <signal name='Signal'>"
            "    <arg name='ArgA' type='q'/>"
            "    <arg name='ArgB' type='q'/>"
            "  </signal>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    // TODO value is OCREP_PROP_NULL for below - need dummy value instead so that introspection data is correct
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signalArgA"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signalArgB"));

    delete context;
}

TEST_F(AllJoynProducer, WhenTheAllJoynSignalArgumentNameIsNotSpecified)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <signal name='Signal'>"
            "    <arg type='q'/>"
            "    <arg type='q'/>"
            "  </signal>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    // TODO value is OCREP_PROP_NULL for below - need dummy value instead so that introspection data is correct
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signalarg0"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signalarg1"));

    delete context;
}

TEST_F(AllJoynProducer, TheAllJoynSignalResourceTypeHasAnExtraValidityProperty)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <signal name='Signal'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    bool b;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "x.org.iotivity.-interface.-signalvalidity", &b));
    EXPECT_FALSE(b);

    delete context;
}

TEST_F(AllJoynProducer, WhenTheValuesAreSentAsPartOfANotifyResponseTheValidityPropertyIsTrue)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <signal name='Signal'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ObserveCallback observeCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_OBSERVE, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, observeCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    observeCB.Reset();
    m_obj->Signal();
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) observeCB.m_response->payload;
    bool b = false;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "x.org.iotivity.-interface.-signalvalidity", &b));
    EXPECT_TRUE(b);

    delete context;
}

TEST_F(AllJoynProducer, WhenAnAllJoynOperationFailsTheTranslatorShallSendAnOCFErrorResponse)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='Error'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback postCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], NULL, CT_DEFAULT, OC_HIGH_QOS, postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    EXPECT_GT(postCB.m_response->result, OC_STACK_RESOURCE_CHANGED);

    delete context;
}

TEST_F(AllJoynProducer, WhenTheAllJoynErrorNameIsAvailableAndDoesNotContainTheOCFPrefixTheTranslatorShallConstructAnOCFErrorMessage)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='ErrorName'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath());
    ResourceCallback postCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], NULL, CT_DEFAULT, OC_HIGH_QOS, postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    EXPECT_GT(postCB.m_response->result, OC_STACK_RESOURCE_CHANGED);
    EXPECT_TRUE(postCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_DIAGNOSTIC, postCB.m_response->payload->type);
    OCDiagnosticPayload *payload = (OCDiagnosticPayload *) postCB.m_response->payload;
    EXPECT_STREQ("org.freedesktop.DBus.Error.Failed: Message", payload->message);

    delete context;
}

TEST_F(AllJoynProducer, WhenTheAllJoynErrorNameIsAvailableAndDoesContainTheOCFPrefixTheTranslatorShallConstructAnOCFErrorMessage)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='OCFErrorName'/>"
            "  <method name='OCFErrorCode'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath()) + "?rt=x.org.iotivity.-interface.-o-c-f-error-name";
    ResourceCallback postCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], NULL, CT_DEFAULT, OC_HIGH_QOS, postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    EXPECT_GT(postCB.m_response->result, OC_STACK_RESOURCE_CHANGED);
    EXPECT_TRUE(postCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_DIAGNOSTIC, postCB.m_response->payload->type);
    OCDiagnosticPayload *payload = (OCDiagnosticPayload *) postCB.m_response->payload;
    EXPECT_STREQ("Message", payload->message);

    uri = std::string(m_obj->GetPath()) + "?rt=x.org.iotivity.-interface.-o-c-f-error-code";
    postCB.Reset();
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], NULL, CT_DEFAULT, OC_HIGH_QOS, postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    EXPECT_EQ(OC_STACK_NO_RESOURCE, postCB.m_response->result);
    EXPECT_TRUE(postCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_DIAGNOSTIC, postCB.m_response->payload->type);
    payload = (OCDiagnosticPayload *) postCB.m_response->payload;
    EXPECT_STREQ("Message", payload->message);

    delete context;
}

/*
 * 7.1.1.1 Exposing an AllJoyn producer application as a Virtual OCF Server
 */

TEST_F(AllJoynProducer, OCFDeviceProperties)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    properties.dl = "fr";
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    properties.ln = appNames;
    properties.nln = sizeof(appNames) / sizeof(appNames[0]);
    properties.n = appNames[1].value;
    properties.piid = "10f70cc4-2398-41f5-8062-4c1facbfc41b";
    properties.dmv = "ocf.res.1.1.0,org.iotivity.A.1,org.iotivity.B.2,org.iotivity.C.3,org.iotivity.D.4";
    LocalizedString descriptions[] = {
        { "en", "en-description" },
        { "fr", "fr-description" }
    };
    properties.ld = descriptions;
    properties.nld = sizeof(descriptions) / sizeof(descriptions[0]);
    properties.sv = "software-version";
    LocalizedString manufacturers[] = {
        { "en", "en-manufacturer" },
        { "fr", "fr-manufacturer" }
    };
    properties.dmn = manufacturers;
    properties.ndmn = sizeof(manufacturers) / sizeof(manufacturers[0]);
    properties.dmno = "model-number";
    properties.vendorProperty = "x.org.iotivity.Field";
    properties.vendorValue = "value";

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
        EXPECT_EQ(ER_OK, m_bus->CreateInterfacesFromXml(ifs[i]));
    }
    const char *ifsOne[] = { "org.iotivity.A", "org.iotivity.B" };
    const char *ifsTwo[] = { "org.iotivity.B", "org.iotivity.C", "org.iotivity.D" };
    ajn::MsgArg os[2];
    os[0].Set("(oas)", "/one", sizeof(ifsOne) / sizeof(ifsOne[0]), ifsOne);
    os[1].Set("(oas)", "/two", sizeof(ifsTwo) / sizeof(ifsTwo[0]), ifsTwo);
    ajn::MsgArg odArg;
    EXPECT_EQ(ER_OK, odArg.Set("a(oas)", sizeof(os) / sizeof(os[0]), os));
    ajn::AboutObjectDescription objectDescription(odArg);

    AboutData aboutData(properties.dl);
    for (size_t i = 0; i < properties.nln; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(appNames[i].value, appNames[i].language));
    }
    EXPECT_EQ(ER_OK, aboutData.SetProtocolIndependentId(properties.piid));
    const char *deviceId = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(deviceId));
    const uint8_t appId[] = { 0x46, 0xe8, 0x0b, 0xf8, 0x9f, 0xf5, 0x47, 0x8a,
                              0xbe, 0x9f, 0x7f, 0xa3, 0x4a, 0xdc, 0x49, 0x7b };
    EXPECT_EQ(ER_OK, aboutData.SetAppId(appId, sizeof(appId) / sizeof(appId[0])));
    for (size_t i = 0; i < properties.nld; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetDescription(properties.ld[i].value,
                properties.ld[i].language));
    }
    EXPECT_EQ(ER_OK, aboutData.SetSoftwareVersion(properties.sv));
    for (size_t i = 0; i < properties.ndmn; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetManufacturer(properties.dmn[i].value,
                properties.dmn[i].language));
    }
    EXPECT_EQ(ER_OK, aboutData.SetModelNumber(properties.dmno));
    const char *vendorField = "org.iotivity.Field";
    ajn::MsgArg vendorArg("s", properties.vendorValue);
    EXPECT_EQ(ER_OK, aboutData.SetField(vendorField, vendorArg));

    VirtualDevice *device = new VirtualDevice(m_bus, m_bus->GetUniqueName().c_str(), 0);
    device->SetProperties(&objectDescription, &aboutData, false);

    /* Get /oic/d */
    DiscoverContext *context = DiscoverVirtualResource("/oic/d");

    ResourceCallback getDeviceCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getDeviceCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getDeviceCB.m_response->result);
    EXPECT_TRUE(getDeviceCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getDeviceCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getDeviceCB.m_response->payload;
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
    EXPECT_STREQ(properties.n, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, &s));
    EXPECT_STREQ(properties.piid, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DATA_MODEL_VERSION, &s));
    EXPECT_STREQ(properties.dmv, s);
    size_t ldDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **ldArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_DESCRIPTION, &ldArr, ldDim));
    EXPECT_EQ(properties.nld, calcDimTotal(ldDim));
    for (size_t i = 0; i < properties.nld; ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(ldArr[i], "language", &s));
        EXPECT_STREQ(properties.ld[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(ldArr[i], "value", &s));
        EXPECT_STREQ(properties.ld[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_SOFTWARE_VERSION, &s));
    EXPECT_STREQ(properties.sv, s);
    size_t dmnDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **dmnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_MFG_NAME, &dmnArr, dmnDim));
    EXPECT_EQ(properties.ndmn, calcDimTotal(dmnDim));
    for (size_t i = 0; i < properties.ndmn; ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(dmnArr[i], "language", &s));
        EXPECT_STREQ(properties.dmn[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(dmnArr[i], "value", &s));
        EXPECT_STREQ(properties.dmn[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_MODEL_NUM, &s));
    EXPECT_STREQ(properties.dmno, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, properties.vendorProperty, &s));
    EXPECT_STREQ(properties.vendorValue, s);

    delete context;
}

TEST_F(AllJoynProducer, OCFDeviceConfigurationProperties)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    properties.dl = "fr";
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    properties.ln = appNames;
    properties.nln = sizeof(appNames) / sizeof(appNames[0]);
    properties.n = appNames[1].value;
    properties.loc[0] = -1.0;
    properties.loc[1] = 1.0;
    properties.locn = "locn";
    properties.c = "c";
    properties.r = "r";
    properties.vendorProperty = "x.org.iotivity.Field";
    properties.vendorValue = "value";

    AboutData aboutData(properties.dl);
    for (size_t i = 0; i < properties.nln; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(appNames[i].value, appNames[i].language));
    }

    TestConfigBusObject configObj(m_bus, &properties);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    m_resource = VirtualConfigurationResource::Create(m_bus, m_bus->GetUniqueName().c_str(), 0,
            "v16.10.00", createCB, &createCB);
    ((VirtualConfigurationResource *) m_resource)->SetAboutData(&aboutData);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1000));

    /* Get /con */
    DiscoverContext *context = DiscoverVirtualResource("/con");

    ResourceCallback getDeviceConfigurationCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getDeviceConfigurationCB,
            NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getDeviceConfigurationCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getDeviceConfigurationCB.m_response->result);
    EXPECT_TRUE(getDeviceConfigurationCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getDeviceConfigurationCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getDeviceConfigurationCB.m_response->payload;
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
    EXPECT_STREQ(properties.n, s);
    size_t locDim[MAX_REP_ARRAY_DEPTH];
    double *locArr;
    EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, OC_RSRVD_LOCATION, &locArr, locDim));
    EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
    EXPECT_EQ(properties.loc[0], locArr[0]);
    EXPECT_EQ(properties.loc[1], locArr[1]);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_LOCATION_NAME, &s));
    EXPECT_STREQ(properties.locn, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_CURRENCY, &s));
    EXPECT_STREQ(properties.c, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_REGION, &s));
    EXPECT_STREQ(properties.r, s);
    size_t lnDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **lnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, &lnArr,
            lnDim));
    EXPECT_EQ(properties.nln, calcDimTotal(lnDim));
    for (size_t i = 0; i < properties.nln; ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "language", &s));
        EXPECT_STREQ(properties.ln[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "value", &s));
        EXPECT_STREQ(properties.ln[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, &s));
    EXPECT_STREQ(properties.dl, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, properties.vendorProperty, &s));
    EXPECT_STREQ(properties.vendorValue, s);

    delete context;
}

TEST_F(AllJoynProducer, UpdateOCFDeviceConfigurationProperties)
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

    TestConfigBusObject configObj(m_bus, &before);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    m_resource = VirtualConfigurationResource::Create(m_bus, m_bus->GetUniqueName().c_str(), 0,
            "v16.10.00", createCB, &createCB);
    ((VirtualConfigurationResource *) m_resource)->SetAboutData(&aboutData);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1000));

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

    /* Post /con */
    DiscoverContext *context = DiscoverVirtualResource("/con");

    OCRepPayload *payload = OCRepPayloadCreate();
    size_t locDim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
    EXPECT_TRUE(OCRepPayloadSetDoubleArray(payload, OC_RSRVD_LOCATION, after.loc, locDim));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_LOCATION_NAME, after.locn));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_CURRENCY, after.c));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_REGION, after.r));
    size_t lnDim[MAX_REP_ARRAY_DEPTH] = { after.nln, 0, 0 };
    size_t dimTotal = calcDimTotal(lnDim);
    OCRepPayload **ln = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        ln[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "language", after.ln[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(ln[i], "value", after.ln[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, ln,
            lnDim));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, after.dl));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, OC_RSRVD_DEVICE_NAME, after.n));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, after.vendorProperty, after.vendorValue));

    ResourceCallback postDeviceCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS,
            postDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postDeviceCB.Wait(1000));

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postDeviceCB.m_response->result);
    EXPECT_TRUE(postDeviceCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, postDeviceCB.m_response->payload->type);
    payload = (OCRepPayload *) postDeviceCB.m_response->payload;
    double *locArr;
    EXPECT_TRUE(OCRepPayloadGetDoubleArray(payload, OC_RSRVD_LOCATION, &locArr, locDim));
    EXPECT_EQ((size_t) 2, calcDimTotal(locDim));
    EXPECT_EQ(after.loc[0], locArr[0]);
    EXPECT_EQ(after.loc[1], locArr[1]);
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_LOCATION_NAME, &s));
    EXPECT_STREQ(after.locn, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_CURRENCY, &s));
    EXPECT_STREQ(after.c, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_REGION, &s));
    EXPECT_STREQ(after.r, s);
    OCRepPayload **lnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, &lnArr,
            lnDim));
    EXPECT_EQ(after.nln, calcDimTotal(lnDim));
    for (size_t i = 0; i < after.nln; ++i)
    {
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "language", &s));
        EXPECT_STREQ(after.ln[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(lnArr[i], "value", &s));
        EXPECT_STREQ(after.ln[i].value, s);
    }
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, &s));
    EXPECT_STREQ(after.dl, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &s));
    EXPECT_STREQ(after.n, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, after.vendorProperty, &s));
    EXPECT_STREQ(after.vendorValue, s);

    delete context;
}

TEST_F(AllJoynProducer, OCFPlatformProperties)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    properties.dl = "fr";
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    properties.ln = appNames;
    properties.nln = sizeof(appNames) / sizeof(appNames[0]);
    properties.n = appNames[1].value;
    properties.pi = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";
    properties.mnmn = "fr-manufacturer-";
    properties.mnml = "manufacturer-url";
    properties.mnmo = "model-number";
    properties.mndt = "date-of-manufacture";
    properties.mnpv = "platform-version";
    properties.mnos = "os-version";
    properties.mnhw = "hardware-version";
    properties.mnfv = "firmware-version";
    properties.mnsl = "support-url";
    properties.st = "system-time";
    properties.vid = "0ce43c8b-b997-4a05-b77d-1c92e01fe7ae";

    AboutData aboutData(properties.dl);
    for (size_t i = 0; i < properties.nln; ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(appNames[i].value, appNames[i].language));
    }
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(properties.pi));
    LocalizedString manufacturers[] = {
        { "en", "en-manufacturer-name" },
        { "fr", "fr-manufacturer-name" }
    };
    for (size_t i = 0; i < sizeof(manufacturers) / sizeof(manufacturers[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetManufacturer(manufacturers[i].value,
                manufacturers[i].language));
    }
    EXPECT_EQ(ER_OK, aboutData.SetManufacturerUrl(properties.mnml));
    EXPECT_EQ(ER_OK, aboutData.SetModelNumber(properties.mnmo));
    EXPECT_EQ(ER_OK, aboutData.SetDateOfManufacture(properties.mndt));
    EXPECT_EQ(ER_OK, aboutData.SetPlatformVersion(properties.mnpv));
    EXPECT_EQ(ER_OK, aboutData.SetOperatingSystemVersion(properties.mnos));
    EXPECT_EQ(ER_OK, aboutData.SetHardwareVersion(properties.mnhw));
    EXPECT_EQ(ER_OK, aboutData.SetFirmwareVersion(properties.mnfv));
    EXPECT_EQ(ER_OK, aboutData.SetSupportUrl(properties.mnsl));
    EXPECT_EQ(ER_OK, aboutData.SetSystemTime(properties.st));
    EXPECT_EQ(ER_OK, aboutData.SetDeviceId(properties.vid));

    VirtualDevice *device = new VirtualDevice(m_bus, m_bus->GetUniqueName().c_str(), 0);
    device->SetProperties(NULL, &aboutData, false);

    /* Get /oic/p */
    DiscoverContext *context = DiscoverVirtualResource("/oic/p");

    ResourceCallback getPlatformCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getPlatformCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getPlatformCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getPlatformCB.m_response->result);
    EXPECT_TRUE(getPlatformCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getPlatformCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getPlatformCB.m_response->payload;
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_PLATFORM_ID, &s));
    EXPECT_STREQ(properties.pi, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_NAME, &s));
    EXPECT_STREQ(properties.mnmn, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_URL, &s));
    EXPECT_STREQ(properties.mnml, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MODEL_NUM, &s));
    EXPECT_STREQ(properties.mnmo, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_DATE, &s));
    EXPECT_STREQ(properties.mndt, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_PLATFORM_VERSION, &s));
    EXPECT_STREQ(properties.mnpv, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_OS_VERSION, &s));
    EXPECT_STREQ(properties.mnos, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_HARDWARE_VERSION, &s));
    EXPECT_STREQ(properties.mnhw, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_FIRMWARE_VERSION, &s));
    EXPECT_STREQ(properties.mnfv, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_SUPPORT_URL, &s));
    EXPECT_STREQ(properties.mnsl, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_SYSTEM_TIME, &s));
    EXPECT_STREQ(properties.st, s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, OC_RSRVD_VID, &s));
    EXPECT_STREQ(properties.vid, s);

    delete context;
    delete device;
}

TEST_F(AllJoynProducer, OCFPlatformConfigurationProperties)
{
    ExpectedProperties properties;
    memset(&properties, 0, sizeof(properties));
    LocalizedString deviceNames[] = {
        { "en", "en-device-name" },
        { "fr", "fr-device-name" }
    };
    properties.mnpn = deviceNames;
    properties.nmnpn = sizeof(deviceNames) / sizeof(deviceNames[0]);

    AboutData aboutData("fr");
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    for (size_t i = 0; i < sizeof(appNames) / sizeof(appNames[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(appNames[i].value, appNames[i].language));
    }

    TestConfigBusObject configObj(m_bus, &properties);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    m_resource = VirtualConfigurationResource::Create(m_bus, m_bus->GetUniqueName().c_str(), 0,
            "v16.10.00", createCB, &createCB);
    ((VirtualConfigurationResource *) m_resource)->SetAboutData(&aboutData);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1000));

    /* Get /con/p */
    DiscoverContext *context = DiscoverVirtualResource("/con/p");

    ResourceCallback getPlatformConfigurationCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getPlatformConfigurationCB,
            NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getPlatformConfigurationCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getPlatformConfigurationCB.m_response->result);
    EXPECT_TRUE(getPlatformConfigurationCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getPlatformConfigurationCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getPlatformConfigurationCB.m_response->payload;
    size_t mnpnDim[MAX_REP_ARRAY_DEPTH];
    OCRepPayload **mnpnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_PLATFORM_NAME, &mnpnArr, mnpnDim));
    EXPECT_EQ(properties.nmnpn, calcDimTotal(mnpnDim));
    for (size_t i = 0; i < properties.nmnpn; ++i)
    {
        char *s;
        EXPECT_TRUE(OCRepPayloadGetPropString(mnpnArr[i], "language", &s));
        EXPECT_STREQ(properties.mnpn[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(mnpnArr[i], "value", &s));
        EXPECT_STREQ(properties.mnpn[i].value, s);
    }

    delete context;
}

TEST_F(AllJoynProducer, UpdateOCFPlatformConfigurationProperties)
{
    ExpectedProperties before;
    memset(&before, 0, sizeof(before));
    LocalizedString deviceNames[] = {
        { "en", "en-device-name" },
        { "fr", "fr-device-name" }
    };
    before.mnpn = deviceNames;
    before.nmnpn = sizeof(deviceNames) / sizeof(deviceNames[0]);

    AboutData aboutData("fr");
    LocalizedString appNames[] = {
        { "en", "en-app-name" },
        { "fr", "fr-app-name" }
    };
    for (size_t i = 0; i < sizeof(appNames) / sizeof(appNames[0]); ++i)
    {
        EXPECT_EQ(ER_OK, aboutData.SetAppName(appNames[i].value, appNames[i].language));
    }

    TestConfigBusObject configObj(m_bus, &before);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    m_resource = VirtualConfigurationResource::Create(m_bus, m_bus->GetUniqueName().c_str(), 0,
            "v16.10.00", createCB, &createCB);
    ((VirtualConfigurationResource *) m_resource)->SetAboutData(&aboutData);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1000));

    ExpectedProperties after;
    memset(&after, 0, sizeof(after));
    LocalizedString newDeviceNames[] = {
        { "en", "new-en-device-name" },
        { "fr", "new-fr-device-name" }
    };
    after.mnpn = newDeviceNames;
    after.nmnpn = sizeof(newDeviceNames) / sizeof(newDeviceNames[0]);

    /* Post /con/p */
    DiscoverContext *context = DiscoverVirtualResource("/con/p");

    OCRepPayload *payload = OCRepPayloadCreate();
    size_t mnpnDim[MAX_REP_ARRAY_DEPTH] = { after.nmnpn, 0, 0 };
    size_t dimTotal = calcDimTotal(mnpnDim);
    OCRepPayload **mnpn = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        mnpn[i] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[i], "language", after.mnpn[i].language));
        EXPECT_TRUE(OCRepPayloadSetPropString(mnpn[i], "value", after.mnpn[i].value));
    }
    EXPECT_TRUE(OCRepPayloadSetPropObjectArrayAsOwner(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, mnpn,
            mnpnDim));

    ResourceCallback postDeviceCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS,
            postDeviceCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postDeviceCB.Wait(1000));

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postDeviceCB.m_response->result);
    EXPECT_TRUE(postDeviceCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, postDeviceCB.m_response->payload->type);
    payload = (OCRepPayload *) postDeviceCB.m_response->payload;
    OCRepPayload **mnpnArr;
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, &mnpnArr,
            mnpnDim));
    EXPECT_EQ(after.nmnpn, calcDimTotal(mnpnDim));
    for (size_t i = 0; i < after.nmnpn; ++i)
    {
        char *s;
        EXPECT_TRUE(OCRepPayloadGetPropString(mnpnArr[i], "language", &s));
        EXPECT_STREQ(after.mnpn[i].language, s);
        EXPECT_TRUE(OCRepPayloadGetPropString(mnpnArr[i], "value", &s));
        EXPECT_STREQ(after.mnpn[i].value, s);
    }

    delete context;
}

TEST_F(AllJoynProducer, MaintenanceResource)
{
    TestConfigBusObject configObj(m_bus, NULL);
    EXPECT_EQ(ER_OK, m_bus->RegisterBusObject(configObj));
    CreateCallback createCB;
    m_resource = VirtualConfigurationResource::Create(m_bus, m_bus->GetUniqueName().c_str(), 0,
            "v16.10.00", createCB, &createCB);
    EXPECT_EQ(OC_STACK_OK, createCB.Wait(1000));

    DiscoverContext *context = DiscoverVirtualResource("/oic/mnt");

    /* Get */
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    bool b;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "fr", &b));
    EXPECT_FALSE(b);
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "rb", &b));
    EXPECT_FALSE(b);

    /* Factory Reset */
    payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "fr", true));
    ResourceCallback setFactoryResetCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS,
            setFactoryResetCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, setFactoryResetCB.Wait(1000));

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, setFactoryResetCB.m_response->result);
    EXPECT_TRUE(setFactoryResetCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, setFactoryResetCB.m_response->payload->type);
    payload = (OCRepPayload *) setFactoryResetCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "fr", &b));
    EXPECT_TRUE(b);

    /* Reboot */
    payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropBool(payload, "rb", true));
    ResourceCallback setRebootCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, context->m_resource->m_uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS,
            setRebootCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, setRebootCB.Wait(1000));

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, setRebootCB.m_response->result);
    EXPECT_TRUE(setRebootCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, setRebootCB.m_response->payload->type);
    payload = (OCRepPayload *) setRebootCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "rb", &b));
    EXPECT_TRUE(b);

    delete context;
}

TEST_F(AllJoynProducer, TranslationWithAidOfIntrospection)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='I64S' type='x' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "  </property>"
            "  <property name='I64L' type='x' access='readwrite'/>"
            "  <property name='U64S' type='t' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "  </property>"
            "  <property name='U64L' type='t' access='readwrite'/>"
            "  <property name='ObjPath' type='o' access='readwrite'/>"
            "  <property name='Sig' type='g' access='readwrite'/>"
            "  <property name='StructName' type='(is)' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='[StructName]'/>"
            "  </property>"
            "  <property name='ArrOfStructName' type='a(is)' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='a[StructName]'/>"
            "  </property>"
            "  <property name='ArrOfU8' type='ay' access='readwrite'/>"
            "  <property name='Variant' type='v' access='readwrite'/>"
            "  <property name='ArrOfI32' type='ai' access='readwrite'/>"
            "  <property name='ArrOfI64' type='ax' access='readwrite'/>"
            "  <property name='Struct' type='(ii)' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='[Point]'/>"
            "  </property>"
            "  <method name='MethodWithIntrospection'>"
            "    <arg name='i64S' type='x' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='i64L' type='x' direction='out'/>"
            "    <arg name='u64S' type='t' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "    </arg>"
            "    <arg name='u64L' type='t' direction='out'/>"
            "    <arg name='objPath' type='o' direction='out'/>"
            "    <arg name='sig' type='g' direction='out'/>"
            "    <arg name='structName' type='(is)' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='[StructName]'/>"
            "    </arg>"
            "    <arg name='arrOfStructName' type='a(is)' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='a[StructName]'/>"
            "    </arg>"
            "    <arg name='arrOfU8' type='ay' direction='out'/>"
            "    <arg name='variant' type='v' direction='out'/>"
            "    <arg name='arrOfI32' type='ai' direction='out'/>"
            "    <arg name='arrOfI64' type='ax' direction='out'/>"
            "    <arg name='struct' type='(ii)' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='[Point]'/>"
            "    </arg>"
            "  </method>"
            "  <signal name='SignalWithIntrospection'>"
            "    <arg name='i64S' type='x' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='i64L' type='x' direction='out'/>"
            "    <arg name='u64S' type='t' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "    </arg>"
            "    <arg name='u64L' type='t' direction='out'/>"
            "    <arg name='objPath' type='o' direction='out'/>"
            "    <arg name='sig' type='g' direction='out'/>"
            "    <arg name='structName' type='(is)' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='[StructName]'/>"
            "    </arg>"
            "    <arg name='arrOfStructName' type='a(is)' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='a[StructName]'/>"
            "    </arg>"
            "    <arg name='arrOfU8' type='ay' direction='out'/>"
            "    <arg name='variant' type='v' direction='out'/>"
            "    <arg name='arrOfI32' type='ai' direction='out'/>"
            "    <arg name='arrOfI64' type='ax' direction='out'/>"
            "    <arg name='struct' type='(ii)' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='[Point]'/>"
            "    </arg>"
            "  </signal>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.int.Type' value='i'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.StructName.Field.string.Type' value='s'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.Point.Field.x.Type' value='i'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.Point.Field.y.Type' value='i'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri;
    OCRepPayload *payload;
    int64_t i;
    char *s;
    OCRepPayload *obj;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **objArr;
    OCByteString bs;
    const uint8_t ay[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f };

    /* Method */
    ResourceCallback postCB;
    uri = context->m_resource->m_uri + "/1?rt=x.org.iotivity.-interface.-method-with-introspection";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);
    EXPECT_TRUE(postCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, postCB.m_response->payload->type);
    payload = (OCRepPayload *) postCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-method-with-introspectionarg0i64S",
            &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-method-with-introspectionarg1i64L",
            &s));
    EXPECT_STREQ("1", s);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-method-with-introspectionarg2u64S",
            &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-method-with-introspectionarg3u64L",
            &s));
    EXPECT_STREQ("1", s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-method-with-introspectionarg4objPath",
            &s));
    EXPECT_STREQ("/Test", s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-method-with-introspectionarg5sig",
            &s));
    EXPECT_STREQ("sig", s);
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-method-with-introspectionarg6structName",
            &obj));
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "int", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(obj, "string", &s));
    EXPECT_STREQ("string", s);
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, "x.org.iotivity.-interface.-method-with-introspectionarg7arrOfStructName",
            &objArr, dim));
    EXPECT_EQ(2u, calcDimTotal(dim));
    EXPECT_TRUE(OCRepPayloadGetPropInt(objArr[0], "int", &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(objArr[0], "string", &s));
    EXPECT_STREQ("string0", s);
    EXPECT_TRUE(OCRepPayloadGetPropInt(objArr[1], "int", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(objArr[1], "string", &s));
    EXPECT_STREQ("string1", s);
    EXPECT_TRUE(OCRepPayloadGetPropByteString(payload, "x.org.iotivity.-interface.-method-with-introspectionarg8arrOfU8",
            &bs));
    EXPECT_EQ(5u, bs.len);
    EXPECT_EQ(0, memcmp(ay, bs.bytes, bs.len));
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-method-with-introspectionarg9variant",
            &i));
    EXPECT_EQ(0, i);
    /* Empty arrays are decoded as OCREP_PROP_NULL (see IOT-2457) */
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-method-with-introspectionarg10arrOfI32"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-method-with-introspectionarg11arrOfI64"));
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-method-with-introspectionarg12struct",
            &obj));
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "x", &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "y", &i));
    EXPECT_EQ(1, i);

    /* Signal */
    ObserveCallback observeCB;
    uri = context->m_resource->m_uri + "/3";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_OBSERVE, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, observeCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    observeCB.Reset();
    m_obj->SignalWithIntrospection();
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    payload = (OCRepPayload *) observeCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg0i64S",
            &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg1i64L",
            &s));
    EXPECT_STREQ("1", s);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg2u64S",
            &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg3u64L",
            &s));
    EXPECT_STREQ("1", s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg4objPath",
            &s));
    EXPECT_STREQ("/Test", s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg5sig",
            &s));
    EXPECT_STREQ("sig", s);
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg6structName",
            &obj));
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "int", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(obj, "string", &s));
    EXPECT_STREQ("string", s);
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg7arrOfStructName",
            &objArr, dim));
    EXPECT_EQ(2u, calcDimTotal(dim));
    EXPECT_TRUE(OCRepPayloadGetPropInt(objArr[0], "int", &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(objArr[0], "string", &s));
    EXPECT_STREQ("string0", s);
    EXPECT_TRUE(OCRepPayloadGetPropInt(objArr[1], "int", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(objArr[1], "string", &s));
    EXPECT_STREQ("string1", s);
    EXPECT_TRUE(OCRepPayloadGetPropByteString(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg8arrOfU8",
            &bs));
    EXPECT_EQ(5u, bs.len);
    EXPECT_EQ(0, memcmp(ay, bs.bytes, bs.len));
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg9variant",
            &i));
    EXPECT_EQ(0, i);
    /* Empty arrays are decoded as OCREP_PROP_NULL (see IOT-2457) */
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg10arrOfI32"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg11arrOfI64"));
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-signal-with-introspectionarg12struct",
            &obj));
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "x", &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "y", &i));
    EXPECT_EQ(1, i);

    /* Get */
    ResourceCallback getCB;
    uri = context->m_resource->m_uri + "/1?rt=x.org.iotivity.-interface.false";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    payload = (OCRepPayload *) getCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.I64S", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.I64L", &s));
    EXPECT_STREQ("1", s);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.U64S", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.U64L", &s));
    EXPECT_STREQ("1", s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.ObjPath",
            &s));
    EXPECT_STREQ("/Test", s);
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "x.org.iotivity.-interface.Sig",
            &s));
    EXPECT_STREQ("sig", s);
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.StructName",
            &obj));
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "int", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(obj, "string", &s));
    EXPECT_STREQ("string", s);
    EXPECT_TRUE(OCRepPayloadGetPropObjectArray(payload, "x.org.iotivity.-interface.ArrOfStructName",
            &objArr, dim));
    EXPECT_EQ(2u, calcDimTotal(dim));
    EXPECT_TRUE(OCRepPayloadGetPropInt(objArr[0], "int", &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(objArr[0], "string", &s));
    EXPECT_STREQ("string0", s);
    EXPECT_TRUE(OCRepPayloadGetPropInt(objArr[1], "int", &i));
    EXPECT_EQ(1, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(objArr[1], "string", &s));
    EXPECT_STREQ("string1", s);
    EXPECT_TRUE(OCRepPayloadGetPropByteString(payload, "x.org.iotivity.-interface.ArrOfU8",
            &bs));
    EXPECT_EQ(5u, bs.len);
    EXPECT_EQ(0, memcmp(ay, bs.bytes, bs.len));
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Variant",
            &i));
    EXPECT_EQ(0, i);
    /* Empty arrays are decoded as OCREP_PROP_NULL (see IOT-2457) */
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.ArrOfI32"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.ArrOfI64"));
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.Struct",
            &obj));
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "x", &i));
    EXPECT_EQ(0, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "y", &i));
    EXPECT_EQ(1, i);

    /* Set */
    payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.I64S", 1));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "x.org.iotivity.-interface.I64L", "1"));
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.U64S", 1));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "x.org.iotivity.-interface.U64L", "1"));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "x.org.iotivity.-interface.ObjPath",
            "/Test"));
    EXPECT_TRUE(OCRepPayloadSetPropString(payload, "x.org.iotivity.-interface.Sig",
            "sig"));
    obj = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(obj, "int", 1));
    EXPECT_TRUE(OCRepPayloadSetPropString(obj, "string", "string"));
    EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "x.org.iotivity.-interface.StructName", obj));
    dim[0] = 2;
    dim[1] = dim[2] = 0;
    objArr = (OCRepPayload **) OICCalloc(2, sizeof(OCRepPayload*));
    objArr[0] = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(objArr[0], "int", 0));
    EXPECT_TRUE(OCRepPayloadSetPropString(objArr[0], "string", "string0"));
    objArr[1] = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(objArr[1], "int", 1));
    EXPECT_TRUE(OCRepPayloadSetPropString(objArr[1], "string", "string1"));
    EXPECT_TRUE(OCRepPayloadSetPropObjectArray(payload, "x.org.iotivity.-interface.ArrOfStructName",
            (const OCRepPayload **) objArr, dim));
    EXPECT_TRUE(OCRepPayloadSetPropByteString(payload, "x.org.iotivity.-interface.ArrOfU8",
            bs));
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.Variant", 0));
    size_t emptyDim[MAX_REP_ARRAY_DEPTH] = { 0 };
    EXPECT_TRUE(OCRepPayloadSetIntArrayAsOwner(payload, "x.org.iotivity.-interface.ArrOfI32",
            NULL, emptyDim));
    EXPECT_TRUE(OCRepPayloadSetStringArrayAsOwner(payload, "x.org.iotivity.-interface.ArrOfI64",
            NULL, emptyDim));
    obj = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(obj, "x", 0));
    EXPECT_TRUE(OCRepPayloadSetPropInt(obj, "y", 1));
    EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "x.org.iotivity.-interface.Struct",
            obj));

    ResourceCallback setCB;
    uri = context->m_resource->m_uri + "/1?rt=x.org.iotivity.-interface.false";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS, setCB,
            NULL, 0));
    EXPECT_EQ(OC_STACK_OK, setCB.Wait(1000));

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, setCB.m_response->result);

    delete context;
}

/*
 * 1.2 AllJoyn Object
 */

TEST_F(AllJoynProducer, GetAllJoynObjectBaseline)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "  <property name='True' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath()) + "?if=oic.if.baseline";
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    EXPECT_STREQ("oic.wk.col", payload->types->value);
    EXPECT_STREQ("oic.r.alljoynobject", payload->types->next->value);
    EXPECT_TRUE(payload->types->next->next == NULL);
    EXPECT_STREQ("oic.if.baseline", payload->interfaces->value);
    EXPECT_STREQ("oic.if.ll", payload->interfaces->next->value);
    EXPECT_TRUE(payload->interfaces->next->next == NULL);
    char **arr;
    size_t dim[MAX_REP_ARRAY_DEPTH];
    EXPECT_TRUE(OCRepPayloadGetStringArray(payload, "rts", &arr, dim));
    size_t dimTotal = calcDimTotal(dim);
    EXPECT_EQ(2u, dimTotal);
    static const char *rts[] = { "oic.wk.col", "oic.r.alljoynobject" };
    bool foundRt[] = {
        false, false,
        false, false, false,
        false };
    for (size_t i = 0; i < dimTotal; ++i)
    {
        for (size_t j = 0; j < dimTotal; ++j)
        {
            if (!strcmp(rts[j], arr[i]))
            {
                foundRt[j] = true;
                break;
            }
        }
    }
    for (size_t j = 0; j < dimTotal; ++j)
    {
        EXPECT_TRUE(foundRt[j]);
    }
    VerifyAllJoynObjectLinks(context, getCB.m_response->devAddr, payload);

    delete context;
}

TEST_F(AllJoynProducer, GetAllJoynObjectLL)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "  <property name='True' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath()) + "?if=oic.if.ll";
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;
    VerifyAllJoynObjectLinks(context, getCB.m_response->devAddr, payload);

    delete context;
}

TEST_F(AllJoynProducer, PostAllJoynObjectBaseline)
{
    FAIL();
}

TEST_F(AllJoynProducer, ObserveAllJoynObject)
{
    FAIL();
}

/*
 * Multi-value "rt" resource
 */

class MultiValueRtValue
{
public:
    MultiValueRtValue(const char *uri, bool v, bool c, bool f, bool t, bool i)
        : m_uri(uri), m_version(v), m_const(c), m_false(f), m_true(t), m_invalidates(i) { }
    const char *m_uri;
    bool m_version;
    bool m_const;
    bool m_false;
    bool m_true;
    bool m_invalidates;
};

static void PrintTo(const MultiValueRtValue& value, ::std::ostream* os)
{
    *os << "{ ";
    if (value.m_version)
    {
        *os << "\"Version\": 1, ";
    }
    if (value.m_const)
    {
        *os << "\"Const\": 1, ";
    }
    if (value.m_false)
    {
        *os << "\"False\": 1, ";
    }
    if (value.m_true)
    {
        *os << "\"True\": 1, ";
    }
    if (value.m_invalidates)
    {
        *os << "\"Invalidates\": 1, ";
    }
    *os << "}";
}

class MultiValueRt : public AllJoynProducerBase<::testing::TestWithParam<MultiValueRtValue>>
{
public:
    virtual ~MultiValueRt() { }
};

TEST_P(MultiValueRt, Get)
{
    MultiValueRtValue value = GetParam();
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Version' type='q' access='read'/>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='readwrite'/>"
            "  <property name='True' type='q' access='readwrite'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, value.m_uri,
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    EXPECT_TRUE(getCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, getCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) getCB.m_response->payload;

    int64_t i;
    EXPECT_EQ(value.m_version, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Version", &i));
    EXPECT_EQ(value.m_const, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Const", &i));
    EXPECT_EQ(value.m_false, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.False", &i));
    EXPECT_EQ(value.m_true, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.True", &i));

    delete context;
}

TEST_P(MultiValueRt, Post)
{
    MultiValueRtValue value = GetParam();
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Version' type='q' access='read'/>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='readwrite'/>"
            "  <property name='True' type='q' access='readwrite'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    OCRepPayload *request = OCRepPayloadCreate();
    if (value.m_version)
    {
        EXPECT_TRUE(OCRepPayloadSetPropInt(request, "x.org.iotivity.-interface.Version", 1));
    }
    if (value.m_const)
    {
        EXPECT_TRUE(OCRepPayloadSetPropInt(request, "x.org.iotivity.-interface.Const", 1));
    }
    if (value.m_false)
    {
        EXPECT_TRUE(OCRepPayloadSetPropInt(request, "x.org.iotivity.-interface.False", 1));
    }
    if (value.m_true)
    {
        EXPECT_TRUE(OCRepPayloadSetPropInt(request, "x.org.iotivity.-interface.True", 1));
    }

    ResourceCallback postCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, value.m_uri,
            &context->m_resource->m_addrs[0], (OCPayload *) request, CT_DEFAULT, OC_HIGH_QOS,
            postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    bool ifDoesNotSupportUpdate = strstr(value.m_uri, "oic.if.r") && !strstr(value.m_uri, "oic.if.rw");
    bool rtDoesNotSupportUpdate = strstr(value.m_uri, "x.org.iotivity.-interface.const");
    bool updatedReadOnlyProperties = value.m_version || value.m_const;
    bool updatedNoProperties = !value.m_false && !value.m_true;
    EXPECT_TRUE(postCB.m_response != NULL);
    if (postCB.m_response) {
        if (ifDoesNotSupportUpdate || rtDoesNotSupportUpdate || updatedReadOnlyProperties ||
                updatedNoProperties)
        {
            EXPECT_NE(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);
        }
        else
        {
            EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);
        }
    }

    delete context;
}

INSTANTIATE_TEST_CASE_P(AllJoynProducer, MultiValueRt, ::testing::Values(\
            MultiValueRtValue("/Test", true, true, true, true, true),
            MultiValueRtValue("/Test?if=oic.if.baseline", true, true, true, true, true),
            MultiValueRtValue("/Test?if=oic.if.r", true, true, true, true, true),
            MultiValueRtValue("/Test?if=oic.if.rw", false, false, true, true, true),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.const", true, true, false, false, false),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.const&if=oic.if.baseline", true, true, false, false, false),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.const&if=oic.if.r", true, true, false, false, false),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.const&if=oic.if.rw", false, false, false, false, false),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.false", false, false, true, true, true),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.false&if=oic.if.baseline", false, false, true, true, true),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.false&if=oic.if.r", false, false, true, true, true),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.false&if=oic.if.rw", false, false, true, true, true)
        ));

class MultiValueRtObserve : public AllJoynProducerBase<::testing::TestWithParam<MultiValueRtValue>>
{
public:
    virtual ~MultiValueRtObserve() { }
};

TEST_P(MultiValueRtObserve, Observe)
{
    MultiValueRtValue value = GetParam();
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Version' type='q' access='read'/>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='false'/>"
            "  </property>"
            "  <property name='True' type='q' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    ObserveCallback observeCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_OBSERVE, value.m_uri,
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, observeCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) observeCB.m_response->payload;
    int64_t i;
    EXPECT_EQ(value.m_true, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.True", &i));
    EXPECT_EQ(value.m_invalidates, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Invalidates", &i));

    observeCB.Reset();
    m_obj->PropertiesChanged();
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));

    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    payload = (OCRepPayload *) observeCB.m_response->payload;
    EXPECT_EQ(value.m_true, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.True", &i));
    EXPECT_EQ(value.m_invalidates, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.Invalidates", &i));

    delete context;
}

INSTANTIATE_TEST_CASE_P(AllJoynProducer, MultiValueRtObserve, ::testing::Values(\
            MultiValueRtValue("/Test/3", false, false, false, true, true),
            MultiValueRtValue("/Test/3?if=oic.if.baseline", false, false, false, true, true),
            MultiValueRtValue("/Test/3?if=oic.if.rw", false, false, false, true, true),
            MultiValueRtValue("/Test/3?rt=x.org.iotivity.-interface.true", false, false, false, true, false),
            MultiValueRtValue("/Test/3?rt=x.org.iotivity.-interface.true&if=oic.if.baseline", false, false, false, true, false),
            MultiValueRtValue("/Test/3?rt=x.org.iotivity.-interface.true&if=oic.if.rw", false, false, false, true, false),
            MultiValueRtValue("/Test/3?rt=x.org.iotivity.-interface.invalidates", false, false, false, false, true),
            MultiValueRtValue("/Test/3?rt=x.org.iotivity.-interface.invalidates&if=oic.if.baseline", false, false, false, false, true),
            MultiValueRtValue("/Test/3?rt=x.org.iotivity.-interface.invalidates&if=oic.if.rw", false, false, false, false, true)
        ));

class MultiValueRtObserveSessionlessSignal : public AllJoynProducerBase<::testing::TestWithParam<MultiValueRtValue>>
{
public:
    virtual ~MultiValueRtObserveSessionlessSignal() { }
};

TEST_P(MultiValueRtObserveSessionlessSignal, Observe)
{
    MultiValueRtValue value = GetParam();
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='True' type='q' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "  <signal name='Signal' sessionless='true'>"
            "    <arg type='q'/>"
            "    <arg type='q'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    ObserveCallback observeCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_OBSERVE, value.m_uri,
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, observeCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);

    observeCB.Reset();
    m_obj->Signal(value.m_true, value.m_invalidates);
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    OCRepPayload *payload = (OCRepPayload *) observeCB.m_response->payload;
    int64_t i;
    EXPECT_EQ(value.m_true, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-signalarg0", &i));
    EXPECT_EQ(value.m_invalidates, OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-signalarg1", &i));

    delete context;
}

INSTANTIATE_TEST_CASE_P(AllJoynProducer, MultiValueRtObserveSessionlessSignal, ::testing::Values(\
            MultiValueRtValue("/Test", false, false, false, true, true),
            MultiValueRtValue("/Test?if=oic.if.baseline", false, false, false, true, true),
            MultiValueRtValue("/Test?if=oic.if.r", false, false, false, true, true),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.-signal", false, false, false, true, true),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.-signal&if=oic.if.baseline", false, false, false, true, true),
            MultiValueRtValue("/Test?rt=x.org.iotivity.-interface.-signal&if=oic.if.r", false, false, false, true, true)
        ));

TEST_F(AllJoynProducer, MultiValueRtGetUnsupportedRt)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='Version' type='q' access='read'/>"
            "  <property name='Const' type='q' access='read'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='const'/>"
            "  </property>"
            "  <property name='False' type='q' access='readwrite'/>"
            "  <property name='True' type='q' access='readwrite'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    std::string uri = std::string(m_obj->GetPath()) + "?rt=x.org.iotivity.-unsupported";
    ResourceCallback getCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));
    EXPECT_NE(OC_STACK_OK, getCB.m_response->result);

    delete context;
}

TEST_F(AllJoynProducer, MultiValueRtPostSimultaneouslyToMultipleResourceTypes)
{
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='True' type='q' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='Invalidates' type='q' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='invalidates'/>"
            "  </property>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource(xml);

    OCRepPayload *request = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(request, "x.org.iotivity.-interface.True", 1));
    EXPECT_TRUE(OCRepPayloadSetPropInt(request, "x.org.iotivity.-interface.Invalidates", 1));

    ResourceCallback postCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, "/Test",
            &context->m_resource->m_addrs[0], (OCPayload *) request, CT_DEFAULT, OC_HIGH_QOS,
            postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));

    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);

    delete context;
}

static void VerifyPropertyNamesAreEscaped(OCRepPayload *payload)
{
    int64_t i;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.oneTwo", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.one.two", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.one-two", &i));
    EXPECT_EQ(12, i);
    OCRepPayload *dict;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.dict", &dict));
    EXPECT_TRUE(OCRepPayloadGetPropInt(dict, "one.two", &i));
    EXPECT_EQ(12, i);
    char *s;
    EXPECT_TRUE(OCRepPayloadGetPropString(dict, "one-two", &s));
    EXPECT_STREQ("one-two", s);
    OCRepPayload *_struct;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.struct", &_struct));
    EXPECT_TRUE(OCRepPayloadGetPropInt(_struct, "one.two", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(_struct, "one-two", &s));
    EXPECT_STREQ("one-two", s);
}

TEST_F(AllJoynProducer, ObjectPathsAndPropertyNamesAreEscaped)
{
    static const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <property name='oneTwo' type='x' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='one_dtwo' type='x' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='one_htwo' type='x' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "    <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='dict' type='a{sv}' access='readwrite'>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <property name='struct' type='(xs)' access='readwrite'>"
            "    <annotation name='org.alljoyn.Bus.Type.Name' value='[PropertiesStruct]'/>"
            "    <annotation name='org.freedesktop.DBus.Property.EmitsChangedSignal' value='true'/>"
            "  </property>"
            "  <method name='PropertyNamesMethod'>"
            "    <arg name='oneTwo' type='x' access='readwrite' direction='in'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='one_dtwo' type='x' access='readwrite' direction='in'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='one_htwo' type='x' access='readwrite' direction='in'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='dict' type='a{sv}' access='readwrite' direction='in'/>"
            "    <arg name='struct' type='(xs)' access='readwrite' direction='in'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='[PropertiesStruct]'/>"
            "    </arg>"
            "    <arg name='outoneTwo' type='x' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='outone_dtwo' type='x' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='outone_htwo' type='x' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='outdict' type='a{sv}' access='readwrite' direction='out'/>"
            "    <arg name='outstruct' type='(xs)' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='[PropertiesStruct]'/>"
            "    </arg>"
            "  </method>"
            "  <signal name='PropertyNamesSignal'>"
            "    <arg name='oneTwo' type='x' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='one_dtwo' type='x' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='one_htwo' type='x' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Max' value='9007199254740992'/>"
            "      <annotation name='org.alljoyn.Bus.Type.Min' value='-9007199254740992'/>"
            "    </arg>"
            "    <arg name='dict' type='a{sv}' access='readwrite' direction='out'/>"
            "    <arg name='struct' type='(xs)' access='readwrite' direction='out'>"
            "      <annotation name='org.alljoyn.Bus.Type.Name' value='[PropertiesStruct]'/>"
            "    </arg>"
            "  </signal>"
            "  <annotation name='org.alljoyn.Bus.Struct.PropertiesStruct.Field.one_dtwo.Type' value='x'/>"
            "  <annotation name='org.alljoyn.Bus.Struct.PropertiesStruct.Field.one_htwo.Type' value='s'/>"
            "</interface>";
    DiscoverContext *context = CreateAndDiscoverVirtualResource("/abc_ddef_hghi_tjkl_umno", xml);

    ResourceCallback getCB;
    ResourceCallback postCB;
    std::string uri;
    OCRepPayload *payload;
    int64_t i;
    OCRepPayload *dict;
    char *s;
    OCRepPayload *_struct;

    /* Get - verify that property names are escaped when rt is not specified */
    getCB.Reset();
    uri = "/abc.def-ghi~jkl_mno/3";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    payload = (OCRepPayload *) getCB.m_response->payload;
    VerifyPropertyNamesAreEscaped(payload);

    /* Get - Verify that property names are escaped when rt is specified */
    getCB.Reset();
    uri = "/abc.def-ghi~jkl_mno/3?rt=x.org.iotivity.-interface.true";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    payload = (OCRepPayload *) getCB.m_response->payload;
    VerifyPropertyNamesAreEscaped(payload);

    /* Get - Verify that arg names are unescaped */
    getCB.Reset();
    uri = "/abc.def-ghi~jkl_mno/1?rt=x.org.iotivity.-interface.-property-names-method";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_GET, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, getCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, getCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, getCB.m_response->result);
    payload = (OCRepPayload *) getCB.m_response->payload;
    // TODO value is OCREP_PROP_NULL for below - need dummy value instead so that introspection data is correct
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-property-names-methodarg0oneTwo"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-property-names-methodarg1one_dtwo"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-property-names-methodarg2one_htwo"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-property-names-methodarg3dict"));
    EXPECT_TRUE(OCRepPayloadIsNull(payload, "x.org.iotivity.-interface.-property-names-methodarg4struct"));

    /* Post - Verify that property names are escaped */
    dict = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(dict, "one.two", 21));
    EXPECT_TRUE(OCRepPayloadSetPropString(dict, "one-two", "two-one"));
    _struct = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(_struct, "one.two", 21));
    EXPECT_TRUE(OCRepPayloadSetPropString(_struct, "one-two", "two-one"));
    payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.oneTwo", 21));
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.one.two", 21));
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.one-two", 21));
    EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "x.org.iotivity.-interface.dict", dict));
    EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "x.org.iotivity.-interface.struct", _struct));
    uri = "/abc.def-ghi~jkl_mno/3?rt=x.org.iotivity.-interface.true";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS,
            postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));
    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);

    /* Post - Verify that arg names are unescaped */
    dict = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(dict, "one.two", 21));
    EXPECT_TRUE(OCRepPayloadSetPropString(dict, "one-two", "two-one"));
    _struct = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(_struct, "one.two", 21));
    EXPECT_TRUE(OCRepPayloadSetPropString(_struct, "one-two", "two-one"));
    payload = OCRepPayloadCreate();
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.-property-names-methodarg0oneTwo", 21));
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.-property-names-methodarg1one_dtwo", 21));
    EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "x.org.iotivity.-interface.-property-names-methodarg2one_htwo", 21));
    EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "x.org.iotivity.-interface.-property-names-methodarg3dict", dict));
    EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "x.org.iotivity.-interface.-property-names-methodarg4struct", _struct));
    postCB.Reset();
    uri = "/abc.def-ghi~jkl_mno/1?rt=x.org.iotivity.-interface.-property-names-method";
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_POST, uri.c_str(),
            &context->m_resource->m_addrs[0], (OCPayload *) payload, CT_DEFAULT, OC_HIGH_QOS,
            postCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, postCB.Wait(1000));
    EXPECT_EQ(OC_STACK_RESOURCE_CHANGED, postCB.m_response->result);
    payload = (OCRepPayload *) postCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-property-names-methodarg5outoneTwo", &i));
    EXPECT_EQ(21, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-property-names-methodarg6outone_dtwo", &i));
    EXPECT_EQ(21, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-property-names-methodarg7outone_htwo", &i));
    EXPECT_EQ(21, i);
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-property-names-methodarg8outdict", &dict));
    EXPECT_TRUE(OCRepPayloadGetPropInt(dict, "one.two", &i));
    EXPECT_EQ(21, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(dict, "one-two", &s));
    EXPECT_STREQ("two-one", s);
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-property-names-methodarg9outstruct", &_struct));
    EXPECT_TRUE(OCRepPayloadGetPropInt(_struct, "one.two", &i));
    EXPECT_EQ(21, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(_struct, "one-two", &s));
    EXPECT_STREQ("two-one", s);

    /* Observe - Verify that property names are escaped */
    uri = "/abc.def-ghi~jkl_mno/3?rt=x.org.iotivity.-interface.true";
    ObserveCallback observeCB;
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_OBSERVE, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, observeCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    payload = (OCRepPayload *) observeCB.m_response->payload;
    VerifyPropertyNamesAreEscaped(payload);

    observeCB.Reset();
    const char *props[] = { "oneTwo", "one_dtwo", "one_htwo", "dict", "struct" };
    m_obj->PropertiesChanged(props, A_SIZEOF(props));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    payload = (OCRepPayload *) observeCB.m_response->payload;
    VerifyPropertyNamesAreEscaped(payload);

    /* Observe - Verify that arg names are unescaped */
    uri = "/abc.def-ghi~jkl_mno/3?rt=x.org.iotivity.-interface.-property-names-signal";
    observeCB.Reset();
    EXPECT_EQ(OC_STACK_OK, OCDoResource(NULL, OC_REST_OBSERVE, uri.c_str(),
            &context->m_resource->m_addrs[0], 0, CT_DEFAULT, OC_HIGH_QOS, observeCB, NULL, 0));
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);

    observeCB.Reset();
    m_obj->PropertyNamesSignal();
    EXPECT_EQ(OC_STACK_OK, observeCB.Wait(1000));
    EXPECT_EQ(OC_STACK_OK, observeCB.m_response->result);
    EXPECT_TRUE(observeCB.m_response->payload != NULL);
    EXPECT_EQ(PAYLOAD_TYPE_REPRESENTATION, observeCB.m_response->payload->type);
    payload = (OCRepPayload *) observeCB.m_response->payload;
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-property-names-signalarg0oneTwo", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-property-names-signalarg1one_dtwo", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "x.org.iotivity.-interface.-property-names-signalarg2one_htwo", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-property-names-signalarg3dict", &dict));
    EXPECT_TRUE(OCRepPayloadGetPropInt(dict, "one.two", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(dict, "one-two", &s));
    EXPECT_STREQ("one-two", s);
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "x.org.iotivity.-interface.-property-names-signalarg4struct", &_struct));
    EXPECT_TRUE(OCRepPayloadGetPropInt(_struct, "one.two", &i));
    EXPECT_EQ(12, i);
    EXPECT_TRUE(OCRepPayloadGetPropString(_struct, "one-two", &s));
    EXPECT_STREQ("one-two", s);

    delete context;
}

