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

#include "ocpayload.h"
#include <alljoyn/MsgArg.h>
#include <assert.h>

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

void CreateSignature(char *sig, OCRepPayloadValue *value)
{
    switch (value->type)
    {
        case OCREP_PROP_NULL:
            assert(0); /* Explicitly not supported */
            break;
        case OCREP_PROP_INT:
            if (value->i < INT32_MIN || INT32_MAX < value->i)
            {
                strcat(sig, "x");
            }
            else
            {
                strcat(sig, "i");
            }
            break;
        case OCREP_PROP_DOUBLE:
            strcat(sig, "d");
            break;
        case OCREP_PROP_BOOL:
            strcat(sig, "b");
            break;
        case OCREP_PROP_STRING:
            strcat(sig, "s");
            break;
        case OCREP_PROP_BYTE_STRING:
            strcat(sig, "ay");
            break;
        case OCREP_PROP_OBJECT:
            {
                /* The object is a struct when names are consecutive non-negative integers. */
                if (!value->obj || !value->obj->values)
                {
                    strcat(sig, "a{sv}");
                    break;
                }
                size_t n = 0;
                OCRepPayloadValue *v;
                for (v = value->obj->values; v; v = v->next)
                {
                    char *endp;
                    long i = strtol(v->name, &endp, 0);
                    if (*endp != '\0' || i < 0 || n != (size_t)i)
                    {
                        break;
                    }
                    ++n;
                }
                if (v)
                {
                    strcat(sig, "a{sv}");
                }
                else
                {
                    strcat(sig, "(");
                    for (v = value->obj->values; v; v = v->next)
                    {
                        CreateSignature(sig, v);
                    }
                    strcat(sig, ")");
                }
                break;
            }
        case OCREP_PROP_ARRAY:
            CreateSignature(sig, &value->arr);
            break;
    }
}

void CreateSignature(char *sig, OCRepPayloadValueArray *arr, uint8_t di)
{
    for (size_t i = di; i < MAX_REP_ARRAY_DEPTH; ++i)
    {
        if (arr->dimensions[i])
        {
            strcat(sig, "a");
        }
    }
    switch (arr->type)
    {
        case OCREP_PROP_NULL:
        case OCREP_PROP_DOUBLE:
        case OCREP_PROP_BOOL:
        case OCREP_PROP_STRING:
        case OCREP_PROP_BYTE_STRING:
            {
                OCRepPayloadValue valueArr;
                valueArr.type = arr->type;
                CreateSignature(sig, &valueArr);
                break;
            }
        case OCREP_PROP_OBJECT:
            {
                /* The signatures must be the same for all array elements */
                char *sigp = sig + strlen(sig);
                size_t dimTotal = calcDimTotal(arr->dimensions);
                OCRepPayloadValue valueArr;
                valueArr.type = arr->type;
                valueArr.obj = dimTotal ? arr->objArray[0] : NULL;
                CreateSignature(sig, &valueArr);
                char s[256];
                for (size_t i = 1; i < dimTotal; ++i)
                {
                    s[0] = 0;
                    valueArr.type = arr->type;
                    valueArr.obj = arr->objArray[i];
                    CreateSignature(s, &valueArr);
                    if (strcmp(sigp, s))
                    {
                        *sigp = 0;
                        strcat(sig, "a{sv}");
                        break;
                    }
                }
                break;
            }
        case OCREP_PROP_INT:
            {
                const char *s = "i";
                size_t dimTotal = calcDimTotal(arr->dimensions);
                for (size_t i = 0; i < dimTotal; ++i)
                {
                    if (arr->iArray[i] < INT32_MIN || INT32_MAX < arr->iArray[i])
                    {
                        s = "x";
                        break;
                    }
                }
                strcat(sig, s);
                break;
            }
        case OCREP_PROP_ARRAY:
            assert(0); /* Not supported - dimensions provide for arrays of arrays */
            break;
    }
}
