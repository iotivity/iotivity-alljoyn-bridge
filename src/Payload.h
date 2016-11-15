//******************************************************************
//
// Copyright 2016 Intel Corporation All Rights Reserved.
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

#ifndef _PAYLOAD_H
#define _PAYLOAD_H

#include <alljoyn/MsgArg.h>
#include "octypes.h"
#include <map>
#include <vector>

struct Types
{
    struct Field
    {
        Field(std::string name, std::string signature) : m_name(name), m_signature(signature) { }
        std::string m_name;
        std::string m_signature;
    };

    static std::map<std::string, std::vector<Field>> m_structs;
};

bool ToOCPayload(OCRepPayload *payload, const char *name, const ajn::MsgArg *arg,
                 const char *signature);
bool ToAJMsgArg(ajn::MsgArg *arg, const char *signature, OCRepPayloadValue *value);

#endif
