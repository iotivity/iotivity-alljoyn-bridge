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

#include "Interfaces.h"

#include <string.h>

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

bool IsInterfaceInWellDefinedSet(const char *name)
{
    const char *wellDefined[] = {
        /* From spec */
        "org.alljoyn.About", "org.alljoyn.Config",
        /* OCF ASA Mapping */
        "Environment.CurrentAirQuality", "Environment.CurrentAirQualityLevel",
        "Environment.CurrentHumidity", "Environment.CurrentTemperature",
        "Environment.TargetHumidity", "Environment.TargetTemperature",
        "Environment.TargetTemperatureLevel", "Environment.WaterLevel", "Environment.WindDirection",
        "Operation.AirRecirculationMode", "Operation.Alerts", "Operation.AudioVideoInput",
        "Operation.AudioVolume", "Operation.BatteryStatus", "Operation.Channel",
        "Operation.ClimateControlMode", "Operation.ClosedStatus", "Operation.CurrentPower",
        "Operation.CycleControl", "Operation.DishWashingCyclePhase", "Operation.EnergyUsage",
        "Operation.FanSpeedLevel", "Operation.FilterStatus", "Operation.HeatingZone",
        "Operation.HvacFanMode", "Operation.LaundryCyclePhase", "Operation.MoistureOutputLevel",
        "Operation.OffControl", "Operation.OnControl", "Operation.OnOffStatus",
        "Operation.OvenCyclePhase", "Operation.PlugInUnits", "Operation.RapidMode",
        "Operation.RemoteControllability", "Operation.RepeatMode", "Operation.ResourceSaving",
        "Operation.RobotCleaningCyclePhase", "Operation.SoilLevel", "Operation.SpinSpeedLevel",
        "Operation.Timer"
    };
    for (size_t i = 0; i < sizeof(wellDefined) / sizeof(wellDefined[0]); ++i)
    {
        if (!strcmp(wellDefined[i], name))
        {
            return true;
        }
    }
    return false;
}

bool TranslateInterface(const char *name)
{
    const char *doNotTranslatePrefix[] = {
        "org.alljoyn.Bus", "org.freedesktop.DBus"
    };
    for (size_t i = 0; i < sizeof(doNotTranslatePrefix) / sizeof(doNotTranslatePrefix[0]); ++i)
    {
        if (!strncmp(doNotTranslatePrefix[i], name, strlen(doNotTranslatePrefix[i])))
        {
            return false;
        }
    }
    const char *doNotTranslate[] = {
        "org.alljoyn.About", "org.alljoyn.Daemon", "org.alljoyn.Debug", "org.alljoyn.Icon",
        "org.allseen.Introspectable"
    };
    for (size_t i = 0; i < sizeof(doNotTranslate) / sizeof(doNotTranslate[0]); ++i)
    {
        if (!strcmp(doNotTranslate[i], name))
        {
            return false;
        }
    }
    return !IsInterfaceInWellDefinedSet(name);
}
