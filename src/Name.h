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

#ifndef _NAME_H
#define _NAME_H

#include <alljoyn/InterfaceDescription.h>
#include <string>

std::string ToOCName(std::string ajName);
std::string ToAJName(std::string ocName);
std::string ToOCPropName(std::string ajName);
std::string ToAJPropName(std::string ocName);
std::string ToUri(std::string objectPath);
std::string ToObjectPath(std::string uri);

std::string GetResourceTypeName(std::string ifaceName);
std::string GetResourceTypeName(std::string ifaceName, std::string suffix);
std::string GetResourceTypeName(const ajn::InterfaceDescription *iface, std::string suffix);
std::string GetPropName(const ajn::InterfaceDescription::Member *member, std::string argName, size_t i);
std::string GetPropName(const ajn::InterfaceDescription::Member *member, std::string argName);
std::string GetPropName(const ajn::InterfaceDescription *iface, std::string memberName);

std::string GetInterface(std::string rt);
std::string GetMember(std::string rt);

std::string NextArgName(const char *&argNames);

bool IsValidErrorName(const char *np, const char **endp);

#endif
