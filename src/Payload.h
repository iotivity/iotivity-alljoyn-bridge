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

#include "cacommon.h"
#include "octypes.h"
#include <inttypes.h>
#include <alljoyn/InterfaceDescription.h>
#include <alljoyn/MsgArg.h>
#include <map>
#include <vector>

const int64_t MAX_SAFE_INTEGER = 9007199254740992;
const int64_t MIN_SAFE_INTEGER = -9007199254740992;

struct Types
{
    struct Field
    {
        Field(std::string name, std::string signature) : m_name(name), m_signature(signature) { }
        std::string m_name;
        std::string m_signature;
    };
    struct Value
    {
        Value() : m_type(OCREP_PROP_NULL) { }
        Value(std::string signature, OCRepPayloadPropType type = OCREP_PROP_NULL)
            : m_signature(signature), m_type(type) { }
        std::string m_signature;
        OCRepPayloadPropType m_type;
    };

    static std::map<std::string, std::vector<Field>> m_structs;
    static std::map<std::string, std::map<std::string, Value>> m_dicts;

    static std::string GenerateAnonymousName();
};

bool ToOCPayload(OCRepPayload *payload, const char *name, const ajn::MsgArg *arg,
        const char *signature);
bool ToOCPayload(OCRepPayload *payload, const char *name, OCRepPayloadPropType type,
        const ajn::MsgArg *arg, const char *signature);
bool ToAJMsgArg(ajn::MsgArg *arg, const char *argSignature, OCRepPayloadValue *value,
        const char *valueSignature = NULL);

OCRepPayloadPropType GetPropType(const ajn::InterfaceDescription::Property *prop,
        const ajn::MsgArg *arg);
OCRepPayloadPropType GetPropType(const ajn::InterfaceDescription::Member *member,
        const char *argName, const ajn::MsgArg *arg);

#endif
