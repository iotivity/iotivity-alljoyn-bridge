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
    rt << 'x';
    rt << '.';
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
