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

#include "Name.h"

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

TEST(UriToObjectPath, Translation)
{
    std::string uri = "/abc.def-ghi~jkl_mno";
    std::string path = "/abc_ddef_hghi_tjkl_umno";
    EXPECT_STREQ(path.c_str(), ToObjectPath(uri).c_str());
    EXPECT_STREQ(uri.c_str(), ToUri(path).c_str());
    EXPECT_STREQ(path.c_str(), ToObjectPath(ToUri(path)).c_str());
    EXPECT_STREQ(uri.c_str(), ToUri(ToObjectPath(uri)).c_str());
}

TEST(PropName, IncludesArgNumberPrefix)
{
    AJOCSetUp::SetUpStack();
    ajn::BusAttachment *bus = new ajn::BusAttachment("PropName");
    const char *xml =
            "<interface name='org.iotivity.Interface'>"
            "  <method name='Method0'>"
            "    <arg name='a' type='q'/>"
            "    <arg name='b' type='q'/>"
            "    <arg name='c' type='q'/>"
            "  </method>"
            "  <method name='Method1'>"
            "    <arg type='q'/>"
            "    <arg name='b' type='q'/>"
            "    <arg name='c' type='q'/>"
            "  </method>"
            "  <method name='Method2'>"
            "    <arg name='a' type='q'/>"
            "    <arg type='q'/>"
            "    <arg name='c' type='q'/>"
            "  </method>"
            "  <method name='Method3'>"
            "    <arg name='a' type='q'/>"
            "    <arg name='b' type='q'/>"
            "    <arg type='q'/>"
            "  </method>"
            "</interface>";
    EXPECT_EQ(ER_OK, bus->CreateInterfacesFromXml(xml));
    const ajn::InterfaceDescription *iface = bus->GetInterface("org.iotivity.Interface");
    const ajn::InterfaceDescription::Member *member;
    const char *argNames;

    member = iface->GetMember("Method0");
    argNames = member->argNames.c_str();
    EXPECT_STREQ("x.org.iotivity.-interface.-method0arg0a", GetPropName(member, NextArgName(argNames), 0).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method0arg1b", GetPropName(member, NextArgName(argNames), 1).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method0arg2c", GetPropName(member, NextArgName(argNames), 2).c_str());

    member = iface->GetMember("Method1");
    argNames = member->argNames.c_str();
    EXPECT_STREQ("x.org.iotivity.-interface.-method1arg0", GetPropName(member, NextArgName(argNames), 0).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method1arg1b", GetPropName(member, NextArgName(argNames), 1).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method1arg2c", GetPropName(member, NextArgName(argNames), 2).c_str());

    member = iface->GetMember("Method2");
    argNames = member->argNames.c_str();
    EXPECT_STREQ("x.org.iotivity.-interface.-method2arg0a", GetPropName(member, NextArgName(argNames), 0).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method2arg1", GetPropName(member, NextArgName(argNames), 1).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method2arg2c", GetPropName(member, NextArgName(argNames), 2).c_str());

    member = iface->GetMember("Method3");
    argNames = member->argNames.c_str();
    EXPECT_STREQ("x.org.iotivity.-interface.-method3arg0a", GetPropName(member, NextArgName(argNames), 0).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method3arg1b", GetPropName(member, NextArgName(argNames), 1).c_str());
    EXPECT_STREQ("x.org.iotivity.-interface.-method3arg2", GetPropName(member, NextArgName(argNames), 2).c_str());

    delete bus;
    AJOCSetUp::TearDownStack();
}
