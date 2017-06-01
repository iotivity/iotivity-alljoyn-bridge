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

#include "ocstack.h"
#include "VirtualResource.h"
#include <alljoyn/BusAttachment.h>
#include <alljoyn/Init.h>
#include <stdlib.h>
#include <thread>

class SessionPortListener : public ajn::SessionPortListener
{
public:
    virtual ~SessionPortListener() { }
    virtual bool AcceptSessionJoiner(ajn::SessionPort port, const char *name,
            const ajn::SessionOpts& opts)
    {
        (void) port;
        (void) name;
        (void) opts;
        return true;
    }
};

static const char *TestInterfaceName = "org.iotivity.Interface";

class TestBusObject : public ajn::BusObject
{
public:
    TestBusObject(ajn::BusAttachment *bus, const char *xml) : ajn::BusObject("/Test")
    {
        if (ER_OK != bus->CreateInterfacesFromXml(xml))
        {
            exit(EXIT_FAILURE);
        }
        const ajn::InterfaceDescription *iface = bus->GetInterface(TestInterfaceName);
        AddInterface(*iface);
        size_t numMembers = iface->GetMembers();
        const ajn::InterfaceDescription::Member *members[numMembers];
        iface->GetMembers(members, numMembers);
        for (size_t i = 0; i < numMembers; ++i)
        {
            if (members[i]->memberType == ajn::MESSAGE_METHOD_CALL)
            {
                if (ER_OK != AddMethodHandler(members[i], static_cast<MessageReceiver::MethodHandler>(&TestBusObject::Method)))
                {
                    exit(EXIT_FAILURE);
                }
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
    void Method(const ajn::InterfaceDescription::Member *member, ajn::Message &msg)
    {
        QStatus status = ER_OK;
        if (member->name == "Error")
        {
            status = MethodReply(msg, ER_FAIL);
        }
        else if (member->name == "ErrorName")
        {
            status = MethodReply(msg, "org.openconnectivity.Error.Name", "Message");
        }
        else if (member->name == "ErrorCode")
        {
            status = MethodReply(msg, "org.openconnectivity.Error.404", "Message");
        }
        else
        {
            status = MethodReply(msg, ER_OK);
        }
        if (ER_OK != status)
        {
            exit(EXIT_FAILURE);
        }
    }
    void Signal()
    {
        const ajn::InterfaceDescription::Member *member =
                bus->GetInterface(TestInterfaceName)->GetSignal("Signal");
        assert(member != NULL);
        if (ER_OK != ajn::BusObject::Signal(NULL, ajn::SESSION_ID_ALL_HOSTED, *member))
        {
            exit(EXIT_FAILURE);
        }
    }
};

static void CreateCB(void *ctx)
{
    (void) ctx;
}

int main(int, char **)
{
    if (OC_STACK_OK != OCInit2(OC_SERVER, OC_DEFAULT_FLAGS, OC_DEFAULT_FLAGS, OC_ADAPTER_IP))
    {
        exit(EXIT_FAILURE);
    }
    if (ER_OK != AllJoynInit())
    {
        exit(EXIT_FAILURE);
    }
    if (ER_OK != AllJoynRouterInit())
    {
        exit(EXIT_FAILURE);
    }

    ajn::BusAttachment *m_bus = new ajn::BusAttachment("Producer");
    if (ER_OK != m_bus->Start())
    {
        exit(EXIT_FAILURE);
    }
    if (ER_OK != m_bus->Connect())
    {
        exit(EXIT_FAILURE);
    }
    ajn::SessionPort m_port = ajn::SESSION_PORT_ANY;
    ajn::SessionOpts m_opts;
    SessionPortListener m_listener;
    if (ER_OK != m_bus->BindSessionPort(m_port, m_opts, m_listener))
    {
        exit(EXIT_FAILURE);
    }
    ajn::SessionId m_sid;
    ajn::SessionOpts opts;
    if (ER_OK != m_bus->JoinSession(m_bus->GetUniqueName().c_str(), m_port, NULL, m_sid, opts))
    {
        exit(EXIT_FAILURE);
    }

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
    TestBusObject *m_obj = new TestBusObject(m_bus, xml);
    if (ER_OK != m_bus->RegisterBusObject(*m_obj))
    {
        exit(EXIT_FAILURE);
    }
    VirtualResource *m_resource = VirtualResource::Create(m_bus, m_bus->GetUniqueName().c_str(),
            m_sid, m_obj->GetPath(), "v16.10.00", CreateCB, NULL);

    for (;;)
    {
        OCProcess();
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    delete m_resource;
    delete m_obj;
    delete m_bus;
    if (ER_OK != AllJoynShutdown())
    {
        exit(EXIT_FAILURE);
    }
    if(OC_STACK_OK != OCStop())
    {
        exit(EXIT_FAILURE);
    }
    exit(EXIT_SUCCESS);
}
