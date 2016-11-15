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

#include "Signature.h"

#include <alljoyn/MsgArg.h>

/*
 * This behaves identically to the AllJoyn implementation and handles
 * extended introspection XML types such as [StructureName].
 * Additionally, we can take some shortcuts here by knowing that the
 * signature passed in is a valid signature already.
 */
QStatus ParseCompleteType(const char *&sigPtr)
{
    QStatus status;
    switch (*sigPtr++)
    {
        case ajn::ALLJOYN_BYTE:
        case ajn::ALLJOYN_BOOLEAN:
        case ajn::ALLJOYN_INT16:
        case ajn::ALLJOYN_UINT16:
        case ajn::ALLJOYN_INT32:
        case ajn::ALLJOYN_UINT32:
        case ajn::ALLJOYN_INT64:
        case ajn::ALLJOYN_UINT64:
        case ajn::ALLJOYN_DOUBLE:
        case ajn::ALLJOYN_STRING:
        case ajn::ALLJOYN_OBJECT_PATH:
        case ajn::ALLJOYN_SIGNATURE:
        case ajn::ALLJOYN_HANDLE:
            status = ER_OK;
            break;

        case ajn::ALLJOYN_ARRAY:
            status = ParseCompleteType(sigPtr);
            break;
        case ajn::ALLJOYN_STRUCT_OPEN:
            status = ER_OK;
            while (status == ER_OK && *sigPtr != ajn::ALLJOYN_STRUCT_CLOSE)
            {
                status = ParseCompleteType(sigPtr);
            }
            if (status == ER_OK)
            {
                status = (*sigPtr++ == ajn::ALLJOYN_STRUCT_CLOSE) ? ER_OK : ER_BUS_BAD_SIGNATURE;
            }
            break;
        case ajn::ALLJOYN_VARIANT:
            status = ER_OK;
            break;
        case ajn::ALLJOYN_DICT_ENTRY_OPEN:
            status = ParseCompleteType(sigPtr);
            if (status == ER_OK)
            {
                status = ParseCompleteType(sigPtr);
            }
            if (status == ER_OK)
            {
                status = (*sigPtr++ == ajn::ALLJOYN_DICT_ENTRY_CLOSE) ? ER_OK : ER_BUS_BAD_SIGNATURE;
            }
            break;

        case '[':
            while (*sigPtr != ']')
            {
                ++sigPtr;
            }
            status = (*sigPtr++ == ']') ? ER_OK : ER_BUS_BAD_SIGNATURE;
            break;

        default:
            status = ER_BUS_BAD_SIGNATURE;
            break;
    }
    return status;
}

uint8_t CountCompleteTypes(const char *signature)
{
    uint8_t count = 0;
    while (ParseCompleteType(signature) == ER_OK)
    {
        ++count;
    }
    return count;
}
