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

#ifndef _INTERFACES_H
#define _INTERFACES_H

namespace ajn {
    namespace org {
        namespace alljoyn {
            namespace Config {
                const char *InterfaceXml =
                        "<interface name='org.alljoyn.Config'>"
                        "  <method name='FactoryReset'>"
                        "    <annotation name='org.freedesktop.DBus.Method.NoReply' value='true'/>"
                        "  </method>"
                        "  <method name='GetConfigurations'>"
                        "    <arg name='languageTag' type='s' direction='in'/>"
                        "    <arg name='languages' type='a{sv}' direction='out'/>"
                        "  </method>"
                        "  <method name='ResetConfigurations'>"
                        "    <arg name='languageTag' type='s' direction='in'/>"
                        "    <arg name='fieldList' type='as' direction='in'/>"
                        "  </method>"
                        "  <method name='Restart'>"
                        "    <annotation name='org.freedesktop.DBus.Method.NoReply' value='true'/>"
                        "  </method>"
                        "  <method name='SetPasscode'>"
                        "    <arg name='DaemonRealm' type='s' direction='in'/>"
                        "    <arg name='newPasscode' type='ay' direction='in'/>"
                        "  </method>"
                        "  <method name='UpdateConfigurations'>"
                        "    <arg name='languageTag' type='s' direction='in'/>"
                        "    <arg name='configMap' type='a{sv}' direction='in'/>"
                        "  </method>"
                        "  <property name='Version' type='q' access='read'/>"
                        // TODO "  <annotation name='org.alljoyn.Bus.Secure' value='true'/>"
                        "</interface>";
            }
        }
    }
}

#endif /* _INTERFACES_H */
