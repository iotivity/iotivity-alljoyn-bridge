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

static void DrainUnderscores(std::ostringstream& rt, size_t& n, const char* s)
{
    while (n) {
        rt << s;
        --n;
    }
}

std::string ToOCName(std::string ajName)
{
    size_t n = 0;
    std::ostringstream rt;
    rt << 'x';
    rt << '.';
    const char *in = ajName.c_str();
    while (*in)
    {
        if (*in == '_')
        {
            ++n;
        }
        else if (isupper(*in))
        {
            DrainUnderscores(rt, n, "--");
            rt << '-';
            rt << (char)(*in - 'A' + 'a');
        }
        else
        {
            DrainUnderscores(rt, n, isalpha(*in) ? "--" : "-");
            rt << *in;
        }
        ++in;
    }
    DrainUnderscores(rt, n, "-");
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

std::string ToOCPropName(std::string ajName)
{
    std::ostringstream ocName;
    const char *in = ajName.c_str();
    while (*in)
    {
        if (*in == '_' && *(in + 1) == 'd')
        {
            ++in;
            ocName << '.';
        }
        else if (*in == '_' && *(in + 1) == 'h')
        {
            ++in;
            ocName << '-';
        }
        else
        {
            ocName << *in;
        }
        ++in;
    }
    return ocName.str();
}

std::string ToAJPropName(std::string ocName)
{
    std::ostringstream ajName;
    const char *in = ocName.c_str();
    while (*in)
    {
        if (*in == '.')
        {
            ajName << "_d";
        }
        else if (*in == '-')
        {
            ajName << "_h";
        }
        else
        {
            ajName << *in;
        }
        ++in;
    }
    return ajName.str();
}

std::string ToUri(std::string objectPath)
{
    std::ostringstream uri;
    const char *in = objectPath.c_str();
    while (*in)
    {
        if (*in == '_' && *(in + 1) == 'u')
        {
            ++in;
            uri << '_';
        }
        else if (*in == '_' && *(in + 1) == 'h')
        {
            ++in;
            uri << '-';

        }
        else if (*in == '_' && *(in + 1) == 'd')
        {
            ++in;
            uri << '.';
        }
        else if (*in == '_' && *(in + 1) == 't')
        {
            ++in;
            uri << '~';
        }
        else if (*in == '_')
        {
            uri << "~u";
        }
        else
        {
            uri << *in;
        }
        ++in;
    }
    return uri.str();
}

std::string ToObjectPath(std::string uri)
{
    std::ostringstream objectPath;
    const char *in = uri.c_str();
    while (*in)
    {
        if (*in == '~' && *(in + 1) == 'u')
        {
            ++in;
            objectPath << "_";
        }
        else if (*in == '~')
        {
            objectPath << "_t";
        }
        else if (*in == '.')
        {
            objectPath << "_d";
        }
        else if (*in == '-')
        {
            objectPath << "_h";
        }
        else if (*in == '_')
        {
            objectPath << "_u";
        }
        else
        {
            objectPath << *in;
        }
        ++in;
    }
    return objectPath.str();
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

std::string GetPropName(const ajn::InterfaceDescription::Member *member, std::string argName, size_t i)
{
    std::stringstream name;
    name << GetResourceTypeName(member->iface, member->name) << "arg" << i << argName;
    return name.str();
}

std::string GetPropName(const ajn::InterfaceDescription::Member *member, std::string argName)
{
    return GetResourceTypeName(member->iface, member->name) + argName;
}

std::string GetPropName(const ajn::InterfaceDescription *iface, std::string memberName)
{
    return GetResourceTypeName(iface->GetName()) + "." + memberName;
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

std::string NextArgName(const char *&argNames)
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
        if (*argNames == ',')
        {
            ++argNames;
        }
    }
    return name.str();
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
