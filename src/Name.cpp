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

#include "Name.h"

#include <sstream>

std::string ToOCName(std::string ajName)
{
    std::ostringstream rt;
    if (ajName.find("oic.d") != 0)
    {
        rt << 'x';
        rt << '.';
    }
    const char *in = ajName.c_str();
    while (*in)
    {
        if (isupper(*in))
        {
            rt << '-';
            rt << (char)(*in - 'A' + 'a');
        }
        else if (*in == '_' && isalpha(*(in + 1)))
        {
            rt << '-';
            rt << '-';
        }
        else if (*in == '_')
        {
            rt << '-';
        }
        else
        {
            rt << *in;
        }
        ++in;
    }
    return rt.str();
}

std::string ToAJName(std::string ocName)
{
    std::ostringstream ajName;
    const char *in = ocName.c_str();
    if (*in == 'x' && *(in + 1) == '.')
    {
        ++in;
        ++in;
    }
    while (*in)
    {
        if (*in == '-' && islower(*(in + 1)))
        {
            ++in;
            ajName << (char)(*in - 'a' + 'A');
        }
        else if (*in == '-' && *(in + 1) == '-' &&
                 (islower(*(in + 2)) || '-' == *(in + 2)))
        {
            ++in;
            ajName << '_';
        }
        else if (*in == '-')
        {
            ajName << '_';
        }
        else
        {
            ajName << *in;
        }
        ++in;
    }
    return ajName.str();
}

std::string GetResourceTypeName(std::string ifaceName)
{
    return ToOCName(ifaceName);
}

std::string GetResourceTypeName(std::string ifaceName, std::string suffix)
{
    return ToOCName(ifaceName + "." + suffix);
}

std::string GetResourceTypeName(const ajn::InterfaceDescription *iface, std::string suffix)
{
    return GetResourceTypeName(iface->GetName(), suffix);
}

std::string GetPropName(const ajn::InterfaceDescription::Member *member, std::string argName)
{
    return GetResourceTypeName(member->iface, member->name) + argName;
}

std::string GetPropName(const ajn::InterfaceDescription *iface, std::string memberName)
{
    return GetResourceTypeName(iface, memberName);
}

std::string GetInterface(std::string rt)
{
    std::string aj = ToAJName(rt);
    return aj.substr(0, aj.rfind('.'));
}

std::string GetMember(std::string rt)
{
    std::string aj = ToAJName(rt);
    return aj.substr(aj.rfind('.') + 1);
}

std::string NextArgName(const char *&argNames, size_t i)
{
    std::stringstream name;
    const char *argName = argNames;
    if (*argNames)
    {
        while (*argNames && *argNames != ',')
        {
            ++argNames;
        }
        if (argNames > argName)
        {
            name << std::string(argName, argNames - argName);
        }
        else
        {
            name << "arg" << i;
        }
        if (*argNames == ',')
        {
            ++argNames;
        }
    }
    else
    {
        name << "arg" << i;
    }
    return name.str();
}

bool TranslateInterface(const char *ifaceName)
{
    return !(strstr(ifaceName, "org.freedesktop.DBus") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.About") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.Bus") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.Daemon") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.Debug") == ifaceName ||
             strstr(ifaceName, "org.alljoyn.Security") == ifaceName ||
             strstr(ifaceName, "org.allseen.Introspectable") == ifaceName);
}

bool IsValidErrorName(const char *np, const char **endp)
{
    size_t numElements = 0;
    bool isStartOfElement = true;
    for (*endp = np; *endp; ++*endp)
    {
        if (isStartOfElement)
        {
            if (!isalpha(**endp) && (**endp != '_'))
            {
                return false;
            }
            isStartOfElement = false;
        }
        else
        {
            if (**endp == '.')
            {
                ++numElements;
                isStartOfElement = true;
            }
            else if (!isalnum(**endp) && (**endp != '_'))
            {
                break;
            }
        }
    }
    return (numElements > 0);
}
