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

#ifndef _RESOURCE_H
#define _RESOURCE_H

#include "octypes.h"
#include <map>
#include <string>

OCStackResult CreateResource(OCResourceHandle *handle, const char *uri, const char *typeName,
        const char *interfaceName, OCEntityHandler entityHandler, void *callbackParam,
        uint8_t properties);

bool IsValidRequest(OCEntityHandlerRequest *request);
std::map<std::string, std::string> ParseQuery(const char *query);

OCRepPayload *CreatePayload(OCResourceHandle resource, const char *query);
bool SetResourceTypes(OCRepPayload *payload, OCResourceHandle resource);
bool SetInterfaces(OCRepPayload *payload, OCResourceHandle resource);
bool SetLinks(OCRepPayload *payload, OCResourceHandle *resources, uint8_t numResources);

#endif
