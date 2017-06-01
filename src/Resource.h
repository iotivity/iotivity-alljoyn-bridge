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

#include "cacommon.h"
#include "octypes.h"
#include <algorithm>
#include <map>
#include <string>
#include <vector>

class Resource
{
public:
    std::string m_uri;
    std::vector<std::string> m_ifs;
    std::vector<std::string> m_rts;
    bool m_isObservable;
    std::vector<OCDevAddr> m_addrs;
    std::vector<Resource> m_resources;
    Resource(OCDevAddr origin, const char *di, OCResourcePayload *resource);
    Resource(OCRepPayload *payload);
    bool IsSecure();
};

template <typename T>
bool HasResourceType(std::vector<std::string> &rts, T rt)
{
    return std::find(rts.begin(), rts.end(), rt) != rts.end();
}

std::vector<Resource>::iterator FindResourceFromUri(std::vector<Resource> &resources,
        std::string uri);

std::vector<Resource>::iterator FindResourceFromType(std::vector<Resource> &resources,
        std::string rt);

OCStackResult CreateResource(OCResourceHandle *handle, const char *uri, const char *typeName,
        const char *interfaceName, OCEntityHandler entityHandler, void *callbackParam,
        uint8_t properties);
OCStackResult DoResource(OCDoHandle *handle, OCMethod method, const char *uri,
        const OCDevAddr *destination, OCPayload *payload, OCCallbackData *cbData,
        OCHeaderOption *options, uint8_t numOptions);
OCStackResult DoResource(OCDoHandle *handle, OCMethod method, const char *uri,
        const std::vector<OCDevAddr> &destinations, OCPayload *payload, OCCallbackData *cbData,
        OCHeaderOption *options, uint8_t numOptions);

bool IsValidRequest(OCEntityHandlerRequest *request);
std::map<std::string, std::string> ParseQuery(const char *query);

OCRepPayload *CreatePayload(OCResourceHandle resource, const char *query);
bool SetResourceTypes(OCRepPayload *payload, OCResourceHandle resource);
bool SetInterfaces(OCRepPayload *payload, OCResourceHandle resource);

#endif
