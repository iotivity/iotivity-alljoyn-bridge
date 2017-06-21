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

#include "Payload.h"

#include "Plugin.h"
#include "Signature.h"
#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include <assert.h>
#include <math.h>

std::map<std::string, std::vector<Types::Field>> Types::m_structs;
std::map<std::string, std::map<std::string, Types::Value>> Types::m_dicts;

std::string Types::GenerateAnonymousName()
{
    static uint64_t i = 0;
    return std::string("[Anon") + std::to_string(++i) + "]";
}

static bool GetVariantSignature(const ajn::MsgArg *arg, std::string &signature)
{
    if (arg->typeId == ajn::ALLJOYN_VARIANT)
    {
        return GetVariantSignature(arg->v_variant.val, signature);
    }
    else if (arg->typeId == ajn::ALLJOYN_ARRAY &&
            arg->v_array.GetElemSig()[0] == ajn::ALLJOYN_VARIANT)
    {
        /* This could be an array or struct depending on the types of the elements */
        if (!arg->v_array.GetNumElements())
        {
            /* No further type info */
            signature = "av";
            return true;
        }
        bool useStructSig = false;
        std::string structSig = "(";
        if (!GetVariantSignature(&arg->v_array.GetElements()[0], signature))
        {
            return false;
        }
        structSig += signature;
        for (size_t i = 1; i < arg->v_array.GetNumElements(); ++i)
        {
            std::string elemSig;
            if (!GetVariantSignature(&arg->v_array.GetElements()[i], elemSig))
            {
                return false;
            }
            if (elemSig != signature)
            {
                useStructSig = true;
                structSig += elemSig;
            }
        }
        structSig += ")";
        if (useStructSig)
        {
            signature = structSig;
        }
        else
        {
            signature = "a" + signature;
        }
        return true;
    }
    else
    {
        signature = arg->Signature();
        return true;
    }
}

static bool calcDim(OCRepPayloadValueArray *arr,
                    uint8_t di, const ajn::MsgArg *arg, const char *signature)
{
    bool success = true;
    switch (signature[0])
    {
        case ajn::ALLJOYN_BOOLEAN:
            arr->type = OCREP_PROP_BOOL;
            break;
        case ajn::ALLJOYN_BYTE:
            arr->type = OCREP_PROP_BYTE_STRING;
            break;
        case ajn::ALLJOYN_INT16:
        case ajn::ALLJOYN_UINT16:
        case ajn::ALLJOYN_INT32:
        case ajn::ALLJOYN_UINT32:
        case ajn::ALLJOYN_INT64:
        case ajn::ALLJOYN_UINT64:
            arr->type = OCREP_PROP_INT; // TODO loss of information for UINT64 if any values are out of range
            break;
        case ajn::ALLJOYN_DOUBLE:
            arr->type = OCREP_PROP_DOUBLE;
            break;

        case ajn::ALLJOYN_HANDLE:
            success = false; /* Explicitly not supported */
            break;

        case ajn::ALLJOYN_STRING:
        case ajn::ALLJOYN_OBJECT_PATH:
        case ajn::ALLJOYN_SIGNATURE:
            arr->type = OCREP_PROP_STRING;
            break;

        case ajn::ALLJOYN_ARRAY:
            if (di >= MAX_REP_ARRAY_DEPTH)
            {
                return false;
            }
            switch (signature[1])
            {
                case ajn::ALLJOYN_BOOLEAN:
                    arr->type = OCREP_PROP_BOOL;
                    if (arr->dimensions[di])
                    {
                        /* Only rectangular arrays are allowed */
                        success = (arr->dimensions[di] == arg->v_scalarArray.numElements);
                    }
                    else
                    {
                        arr->dimensions[di] = arg->v_scalarArray.numElements;
                    }
                    break;
                case ajn::ALLJOYN_BYTE:
                    arr->type = OCREP_PROP_BYTE_STRING;
                    break;
                case ajn::ALLJOYN_INT16:
                case ajn::ALLJOYN_UINT16:
                case ajn::ALLJOYN_INT32:
                case ajn::ALLJOYN_UINT32:
                case ajn::ALLJOYN_INT64:
                case ajn::ALLJOYN_UINT64:
                    arr->type = OCREP_PROP_INT; // TODO loss of information for UINT64 if any values are out of range
                    if (arr->dimensions[di])
                    {
                        /* Only rectangular arrays are allowed */
                        success = (arr->dimensions[di] == arg->v_scalarArray.numElements);
                    }
                    else
                    {
                        arr->dimensions[di] = arg->v_scalarArray.numElements;
                    }
                    break;
                case ajn::ALLJOYN_DOUBLE:
                    arr->type = OCREP_PROP_DOUBLE;
                    if (arr->dimensions[di])
                    {
                        /* Only rectangular arrays are allowed */
                        success = (arr->dimensions[di] == arg->v_scalarArray.numElements);
                    }
                    else
                    {
                        arr->dimensions[di] = arg->v_scalarArray.numElements;
                    }
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    arr->type = OCREP_PROP_OBJECT;
                    break;
                case ajn::ALLJOYN_VARIANT:
                    {
                        std::string variantSig;
                        success = GetVariantSignature(arg, variantSig);
                        if (variantSig[1] == ajn::ALLJOYN_BYTE)
                        {
                            /*
                             * Drop the last dimension since a byte array is translated as a byte
                             * string, not an array of bytes
                             */
                            arr->type = OCREP_PROP_BYTE_STRING;
                        }
                        else
                        {
                            arr->dimensions[di] = arg->v_array.GetNumElements();
                            for (size_t i = 0; success && i < arr->dimensions[di]; ++i)
                            {
                                success = calcDim(arr, di + 1,
                                        arg->v_array.GetElements()[i].v_variant.val, &variantSig[1]);
                            }
                        }
                        break;
                    }
                default:
                    arr->dimensions[di] = arg->v_array.GetNumElements();
                    for (size_t i = 0; success && i < arr->dimensions[di]; ++i)
                    {
                        success = calcDim(arr, di + 1, &arg->v_array.GetElements()[i],
                                &signature[1]);
                    }
                    break;
            }
            break;
        case ajn::ALLJOYN_STRUCT_OPEN:
        case '[':
            arr->type = OCREP_PROP_OBJECT;
            break;
        case ajn::ALLJOYN_VARIANT:
        case ajn::ALLJOYN_DICT_ENTRY_OPEN:
            assert(0); /* Handled in ajn::ALLJOYN_ARRAY case above */
            break;

        case ajn::ALLJOYN_INVALID:
        case ajn::ALLJOYN_STRUCT_CLOSE:
        case ajn::ALLJOYN_DICT_ENTRY_CLOSE:
            success = false;
            break;

        case ajn::ALLJOYN_STRUCT:
        case ajn::ALLJOYN_DICT_ENTRY:
        case ajn::ALLJOYN_WILDCARD:
            assert(0); /* None of these ever used in a signature */
            break;
    }
    return success;
}

static OCRepPayload *CloneObject(OCRepPayloadPropType type, const ajn::MsgArg *arg,
        const char *signature)
{
    bool success = false;
    OCRepPayload *obj = OCRepPayloadCreate();
    if (!obj)
    {
        return NULL;
    }
    if ((arg->typeId == ajn::ALLJOYN_STRUCT) &&
            (Types::m_structs.find(signature) != Types::m_structs.end()))
    {
        success = true;
        for (size_t i = 0; success && i < arg->v_struct.numMembers; ++i)
        {
            assert(Types::m_structs.find(signature) != Types::m_structs.end());
            assert(i < Types::m_structs[signature].size());
            Types::Field &field = Types::m_structs[signature][i];
            success = ToOCPayload(obj, field.m_name.c_str(), type, &arg->v_struct.members[i],
                    field.m_signature.c_str());
        }
    }
    else if ((arg->typeId == ajn::ALLJOYN_ARRAY) &&
            (Types::m_dicts.find(signature) != Types::m_dicts.end()))
    {
        success = true;
        for (size_t i = 0; success && i < arg->v_array.GetNumElements(); ++i)
        {
            const ajn::MsgArg *elem = &arg->v_array.GetElements()[i];
            if ((elem->typeId != ajn::ALLJOYN_DICT_ENTRY) ||
                    (elem->v_dictEntry.key->typeId != ajn::ALLJOYN_STRING) ||
                    (elem->v_dictEntry.val->typeId != ajn::ALLJOYN_VARIANT))
            {
                success = false;
                break;
            }
            const char *keyName = elem->v_dictEntry.key->v_string.str;
            std::string valSig = Types::m_dicts[signature][keyName].m_signature;
            OCRepPayloadPropType valType = Types::m_dicts[signature][keyName].m_type;
            success = ToOCPayload(obj, keyName, valType,
                    elem->v_dictEntry.val->v_variant.val, valSig.c_str());
        }
    }
    if (success)
    {
        return obj;
    }
    else
    {
        OCRepPayloadDestroy(obj);
        return NULL;
    }
}

static bool CloneArray(OCRepPayloadValue *value, size_t *ai, uint8_t di, OCRepPayloadPropType type,
        const ajn::MsgArg *arg, const char *signature)
{
    bool success = true;
    switch (signature[0])
    {
        case ajn::ALLJOYN_BOOLEAN:
            if (arg->typeId != ajn::ALLJOYN_BOOLEAN)
            {
                success = false;
                break;
            }
            value->arr.bArray[(*ai)++] = arg->v_bool;
            break;
        case ajn::ALLJOYN_BYTE:
            assert(0); /* Handled in ajn::ALLJOYN_ARRAY case below */
            break;
        case ajn::ALLJOYN_INT16:
            if (arg->typeId != ajn::ALLJOYN_INT16)
            {
                success = false;
                break;
            }
            value->arr.iArray[(*ai)++] = arg->v_int16;
            break;
        case ajn::ALLJOYN_UINT16:
            if (arg->typeId != ajn::ALLJOYN_UINT16)
            {
                success = false;
                break;
            }
            value->arr.iArray[(*ai)++] = arg->v_uint16;
            break;
        case ajn::ALLJOYN_INT32:
            if (arg->typeId != ajn::ALLJOYN_INT32)
            {
                success = false;
                break;
            }
            value->arr.iArray[(*ai)++] = arg->v_int32;
            break;
        case ajn::ALLJOYN_UINT32:
            if (arg->typeId != ajn::ALLJOYN_UINT32)
            {
                success = false;
                break;
            }
            value->arr.iArray[(*ai)++] = arg->v_uint32;
            break;
        case ajn::ALLJOYN_INT64:
            if (arg->typeId != ajn::ALLJOYN_INT64)
            {
                success = false;
                break;
            }
            value->arr.iArray[(*ai)++] = arg->v_int64;
            break;
        case ajn::ALLJOYN_UINT64:
            if (arg->typeId != ajn::ALLJOYN_UINT64)
            {
                success = false;
                break;
            }
            value->arr.iArray[(*ai)++] = arg->v_uint64; // TODO loss of information for UINT64 if any values are out of range
            break;
        case ajn::ALLJOYN_DOUBLE:
            if (arg->typeId != ajn::ALLJOYN_DOUBLE)
            {
                success = false;
                break;
            }
            value->arr.dArray[(*ai)++] = arg->v_double;
            break;
        case ajn::ALLJOYN_HANDLE:
            success = false; /* Explicitly not supported */
            break;

        case ajn::ALLJOYN_STRING:
            if (arg->typeId != ajn::ALLJOYN_STRING)
            {
                success = false;
                break;
            }
            value->arr.strArray[(*ai)++] = OICStrdup(arg->v_objPath.str);
            break;
        case ajn::ALLJOYN_OBJECT_PATH:
            if (arg->typeId != ajn::ALLJOYN_OBJECT_PATH)
            {
                success = false;
                break;
            }
            value->arr.strArray[(*ai)++] = OICStrdup(arg->v_string.str);
            break;
        case ajn::ALLJOYN_SIGNATURE:
            if (arg->typeId != ajn::ALLJOYN_SIGNATURE)
            {
                success = false;
                break;
            }
            value->arr.strArray[(*ai)++] = OICStrdup(arg->v_signature.sig);
            break;

        case ajn::ALLJOYN_ARRAY:
            switch (signature[1])
            {
                case ajn::ALLJOYN_BOOLEAN:
                    if (arg->typeId != ajn::ALLJOYN_BOOLEAN_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    memcpy(&value->arr.bArray[(*ai)], arg->v_scalarArray.v_bool,
                            value->arr.dimensions[di] * sizeof(bool));
                    (*ai) += value->arr.dimensions[di];
                    break;
                case ajn::ALLJOYN_BYTE:
                    {
                        if (arg->typeId != ajn::ALLJOYN_BYTE_ARRAY)
                        {
                            success = false;
                            break;
                        }
                        OCByteString byteString;
                        byteString.len = arg->v_scalarArray.numElements;
                        byteString.bytes = (uint8_t *) OICMalloc(byteString.len * sizeof(uint8_t));
                        if (!byteString.bytes)
                        {
                            success = false;
                            break;
                        }
                        memcpy(byteString.bytes, arg->v_scalarArray.v_byte, byteString.len);
                        value->arr.ocByteStrArray[(*ai)++] = byteString;
                        break;
                    }
                case ajn::ALLJOYN_INT16:
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    if (arg->typeId != ajn::ALLJOYN_INT16_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    for (size_t i = 0; i < value->arr.dimensions[di]; ++i)
                    {
                        value->arr.iArray[(*ai)++] = arg->v_scalarArray.v_int16[i];
                    }
                    break;
                case ajn::ALLJOYN_UINT16:
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    if (arg->typeId != ajn::ALLJOYN_UINT16_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    for (size_t i = 0; i < value->arr.dimensions[di]; ++i)
                    {
                        value->arr.iArray[(*ai)++] = arg->v_scalarArray.v_uint16[i];
                    }
                    break;
                case ajn::ALLJOYN_INT32:
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    if (arg->typeId != ajn::ALLJOYN_INT32_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    for (size_t i = 0; i < value->arr.dimensions[di]; ++i)
                    {
                        value->arr.iArray[(*ai)++] = arg->v_scalarArray.v_int32[i];
                    }
                    break;
                case ajn::ALLJOYN_UINT32:
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    if (arg->typeId != ajn::ALLJOYN_UINT32_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    for (size_t i = 0; i < value->arr.dimensions[di]; ++i)
                    {
                        value->arr.iArray[(*ai)++] = arg->v_scalarArray.v_uint32[i];
                    }
                    break;
                case ajn::ALLJOYN_INT64:
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    if (arg->typeId != ajn::ALLJOYN_INT64_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    memcpy(&value->arr.iArray[(*ai)], arg->v_scalarArray.v_int64,
                            value->arr.dimensions[di] * sizeof(int64_t));
                    (*ai) += value->arr.dimensions[di];
                    break;
                case ajn::ALLJOYN_UINT64:
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    if (arg->typeId != ajn::ALLJOYN_UINT64_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    for (size_t i = 0; i < value->arr.dimensions[di]; ++i)
                    {
                        value->arr.iArray[(*ai)++] = arg->v_scalarArray.v_uint64[i];
                    }
                    break;
                case ajn::ALLJOYN_DOUBLE:
                    assert(value->arr.dimensions[di] == arg->v_scalarArray.numElements);
                    if (arg->typeId != ajn::ALLJOYN_DOUBLE_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    memcpy(&value->arr.dArray[(*ai)], arg->v_scalarArray.v_double,
                            value->arr.dimensions[di] * sizeof(double));
                    (*ai) += value->arr.dimensions[di];
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    {
                        if (arg->typeId != ajn::ALLJOYN_ARRAY)
                        {
                            success = false;
                            break;
                        }
                        OCRepPayload *obj = OCRepPayloadCreate();
                        if (!obj)
                        {
                            success = false;
                            break;
                        }
                        for (size_t i = 0; success && i < arg->v_array.GetNumElements(); ++i)
                        {
                            success = ToOCPayload(obj, NULL, &arg->v_array.GetElements()[i],
                                    &signature[1]);
                        }
                        if (success)
                        {
                            value->arr.objArray[(*ai)++] = obj;
                        }
                        else
                        {
                            OCRepPayloadDestroy(obj);
                        }
                        break;
                    }
                case ajn::ALLJOYN_VARIANT:
                    {
                        std::string variantSig;
                        success = GetVariantSignature(arg, variantSig);
                        if (variantSig[1] == ajn::ALLJOYN_BYTE)
                        {
                            if (arg->typeId != ajn::ALLJOYN_ARRAY)
                            {
                                success = false;
                                break;
                            }
                            if (value->type == OCREP_PROP_ARRAY)
                            {
                                OCByteString byteString;
                                byteString.len = arg->v_array.GetNumElements();
                                byteString.bytes =
                                        (uint8_t *) OICMalloc(byteString.len * sizeof(uint8_t));
                                if (!byteString.bytes)
                                {
                                    success = false;
                                    break;
                                }
                                for (size_t i = 0; i < byteString.len; ++i)
                                {
                                    const ajn::MsgArg *elem = &arg->v_array.GetElements()[i];
                                    if ((elem->typeId != ajn::ALLJOYN_VARIANT) ||
                                            (elem->v_variant.val->typeId != ajn::ALLJOYN_BYTE))
                                    {
                                        success = false;
                                        break;
                                    }
                                    byteString.bytes[i] = elem->v_variant.val->v_byte;
                                }
                                if (success)
                                {
                                    value->arr.ocByteStrArray[(*ai)++] = byteString;
                                }
                                else
                                {
                                    OICFree(byteString.bytes);
                                }
                                break;
                            }
                            else
                            {
                                value->type = OCREP_PROP_BYTE_STRING;
                                value->ocByteStr.len = arg->v_array.GetNumElements();
                                value->ocByteStr.bytes = (uint8_t *) OICMalloc(value->ocByteStr.len
                                        * sizeof(uint8_t));
                                if (!value->ocByteStr.bytes)
                                {
                                    success = false;
                                    break;
                                }
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    const ajn::MsgArg *elem = &arg->v_array.GetElements()[i];
                                    if (elem->typeId != ajn::ALLJOYN_VARIANT)
                                    {
                                        success = false;
                                        break;
                                    }
                                    ajn::MsgArg *val = elem->v_variant.val;
                                    while (val->typeId == ajn::ALLJOYN_VARIANT)
                                    {
                                        val = val->v_variant.val;
                                    }
                                    if (val->typeId != ajn::ALLJOYN_BYTE)
                                    {
                                        success = false;
                                        break;
                                    }
                                    value->ocByteStr.bytes[i] = val->v_byte;
                                }
                                if (!success)
                                {
                                    OICFree(value->ocByteStr.bytes);
                                    value->ocByteStr.bytes = NULL;
                                }
                            }
                        }
                        else
                        {
                            for (size_t i = 0; success && i < value->arr.dimensions[di]; ++i)
                            {
                                if (arg->typeId != ajn::ALLJOYN_ARRAY)
                                {
                                    success = false;
                                    break;
                                }
                                const ajn::MsgArg *elem = &arg->v_array.GetElements()[i];
                                if (elem->typeId != ajn::ALLJOYN_VARIANT)
                                {
                                    success = false;
                                    break;
                                }
                                ajn::MsgArg *val = elem->v_variant.val;
                                while (val->typeId == ajn::ALLJOYN_VARIANT)
                                {
                                    val = val->v_variant.val;
                                }
                                assert(val->Signature() == &variantSig[1]);
                                success = CloneArray(value, ai, di + 1, type, val, &variantSig[1]);
                            }
                        }
                    }
                    break;
                default:
                    if (arg->typeId != ajn::ALLJOYN_ARRAY)
                    {
                        success = false;
                        break;
                    }
                    value->type = OCREP_PROP_ARRAY;
                    for (size_t i = 0; success && i < value->arr.dimensions[di]; ++i)
                    {
                        success = CloneArray(value, ai, di + 1, type,
                                &arg->v_array.GetElements()[i], &signature[1]);
                    }
                    break;
            }
            break;
        case ajn::ALLJOYN_STRUCT_OPEN:
            {
                if (arg->typeId != ajn::ALLJOYN_STRUCT)
                {
                    success = false;
                    break;
                }
                OCRepPayload *obj = OCRepPayloadCreate();
                if (!obj)
                {
                    success = false;
                    break;
                }
                ++signature;
                const char *fieldSignature = signature;
                for (size_t i = 0; success && i < arg->v_struct.numMembers; ++i)
                {
                    ParseCompleteType(signature);
                    std::string sig(fieldSignature, signature - fieldSignature);
                    fieldSignature = signature;
                    char name[16];
                    snprintf(name, 16, "%zu", i);
                    success = ToOCPayload(obj, name, &arg->v_struct.members[i], sig.c_str());
                }
                if (success)
                {
                    value->arr.objArray[(*ai)++] = obj;
                }
                else
                {
                    OCRepPayloadDestroy(obj);
                }
                break;
            }
        case ajn::ALLJOYN_VARIANT:
        case ajn::ALLJOYN_DICT_ENTRY_OPEN:
            assert(0); /* Handled in ajn::ALLJOYN_ARRAY case above */
            break;

        case ajn::ALLJOYN_STRUCT:
        case ajn::ALLJOYN_DICT_ENTRY:
        case ajn::ALLJOYN_WILDCARD:
            assert(0); /* None of these ever used in a signature */
            break;

        case ajn::ALLJOYN_INVALID:
        case ajn::ALLJOYN_STRUCT_CLOSE:
        case ajn::ALLJOYN_DICT_ENTRY_CLOSE:
            success = false;
            break;

        case '[':
            {
                OCRepPayload *obj = CloneObject(type, arg, signature);
                if (obj)
                {
                    value->arr.objArray[(*ai)++] = obj;
                }
                else
                {
                    success = false;
                }
                break;
            }
    }
    return success;
}

static void DestroyArray(OCRepPayloadValue *value)
{
    if (value->type == OCREP_PROP_BYTE_STRING)
    {
        OICFree(value->ocByteStr.bytes);
    }
    else
    {
        size_t dimTotal = calcDimTotal(value->arr.dimensions);
        switch (value->arr.type)
        {
            case OCREP_PROP_INT:
                OICFree(value->arr.iArray);
                break;
            case OCREP_PROP_DOUBLE:
                OICFree(value->arr.dArray);
                break;
            case OCREP_PROP_BOOL:
                OICFree(value->arr.bArray);
                break;
            case OCREP_PROP_STRING:
                for (size_t i = 0; i < dimTotal; ++i)
                {
                    OICFree(value->arr.strArray[i]);
                }
                OICFree(value->arr.strArray);
                break;
            case OCREP_PROP_BYTE_STRING:
                for (size_t i = 0; i < dimTotal; ++i)
                {
                    OICFree(value->arr.ocByteStrArray[i].bytes);
                }
                OICFree(value->arr.ocByteStrArray);
                break;
            case OCREP_PROP_OBJECT:
                for (size_t i = 0; i < dimTotal; ++i)
                {
                    OICFree(value->arr.objArray[i]);
                }
                OICFree(value->arr.objArray);
                break;
            default:
                break;
        }
    }
}

static bool SetPropArray(OCRepPayload *payload, const char *name, OCRepPayloadPropType type,
        const ajn::MsgArg *arg, const char *signature)
{
    OCRepPayloadValue value;
    memset(&value, 0, sizeof(value));
    if (!calcDim(&value.arr, 0, arg, signature))
    {
        return false;
    }
    size_t dimTotal = calcDimTotal(value.arr.dimensions);
    switch (value.arr.type)
    {
        case OCREP_PROP_INT:
            value.arr.iArray = (int64_t *) OICCalloc(dimTotal, sizeof(int64_t));
            break;
        case OCREP_PROP_DOUBLE:
            value.arr.dArray = (double *) OICCalloc(dimTotal, sizeof(double));
            break;
        case OCREP_PROP_BOOL:
            value.arr.bArray = (bool *) OICCalloc(dimTotal, sizeof(bool));
            break;
        case OCREP_PROP_STRING:
            value.arr.strArray = (char **) OICCalloc(dimTotal, sizeof(char *));
            break;
        case OCREP_PROP_BYTE_STRING:
            value.arr.ocByteStrArray = (OCByteString *) OICCalloc(dimTotal, sizeof(OCByteString));
            break;
        case OCREP_PROP_OBJECT:
            value.arr.objArray = (OCRepPayload **) OICCalloc(dimTotal, sizeof(OCRepPayload *));
            break;
        case OCREP_PROP_NULL:
            /* Explicitly not supported, except that this is an empty array (see IOT-2457) */
            break;
        case OCREP_PROP_ARRAY:
            /* Not used as an array value type */
            assert(0);
            break;
    }
    /* Array pointers are in a union so it's sufficient to only check one */
    if (dimTotal && !value.arr.iArray)
    {
        return false;
    }
    size_t i = 0;
    bool success = CloneArray(&value, &i, 0, type, arg, signature);
    if (success)
    {
        if (value.type == OCREP_PROP_BYTE_STRING)
        {
            success = OCRepPayloadSetPropByteStringAsOwner(payload, name, &value.ocByteStr);
        }
        else
        {
            switch (value.arr.type)
            {
                case OCREP_PROP_INT:
                    success = OCRepPayloadSetIntArrayAsOwner(payload, name, value.arr.iArray,
                            value.arr.dimensions);
                    break;
                case OCREP_PROP_DOUBLE:
                    success = OCRepPayloadSetDoubleArrayAsOwner(payload, name, value.arr.dArray,
                            value.arr.dimensions);
                    break;
                case OCREP_PROP_BOOL:
                    success = OCRepPayloadSetBoolArrayAsOwner(payload, name, value.arr.bArray,
                            value.arr.dimensions);
                    break;
                case OCREP_PROP_STRING:
                    success = OCRepPayloadSetStringArrayAsOwner(payload, name, value.arr.strArray,
                            value.arr.dimensions);
                    break;
                case OCREP_PROP_BYTE_STRING:
                    success = OCRepPayloadSetByteStringArrayAsOwner(payload, name,
                            value.arr.ocByteStrArray, value.arr.dimensions);
                    break;
                case OCREP_PROP_OBJECT:
                    success = OCRepPayloadSetPropObjectArrayAsOwner(payload, name,
                            value.arr.objArray, value.arr.dimensions);
                    break;
                case OCREP_PROP_NULL:
                    /* Explicitly not supported, except that this is an empty array (see IOT-2457) */
                    success = true;
                    break;
                case OCREP_PROP_ARRAY:
                    /* Not used as an array value type */
                    assert(0);
                    break;
            }
        }
    }
    if (!success)
    {
        DestroyArray(&value);
    }
    return success;
}

bool ToOCPayload(OCRepPayload *payload, const char *name, const ajn::MsgArg *arg,
        const char *signature)
{
    return ToOCPayload(payload, name, OCREP_PROP_NULL, arg, signature);
}

bool ToOCPayload(OCRepPayload *payload, const char *name, OCRepPayloadPropType type,
        const ajn::MsgArg *arg, const char *signature)
{
    bool success = false;
    switch (signature[0])
    {
        case ajn::ALLJOYN_BOOLEAN:
            success = (arg->typeId == ajn::ALLJOYN_BOOLEAN) &&
                    OCRepPayloadSetPropBool(payload, name, arg->v_bool);
            break;

        case ajn::ALLJOYN_BYTE:
            success = (arg->typeId == ajn::ALLJOYN_BYTE) &&
                    OCRepPayloadSetPropInt(payload, name, arg->v_byte);
            break;
        case ajn::ALLJOYN_INT16:
            success = (arg->typeId == ajn::ALLJOYN_INT16) &&
                    OCRepPayloadSetPropInt(payload, name, arg->v_int16);
            break;
        case ajn::ALLJOYN_UINT16:
            success = (arg->typeId == ajn::ALLJOYN_UINT16) &&
                    OCRepPayloadSetPropInt(payload, name, arg->v_uint16);
            break;
        case ajn::ALLJOYN_INT32:
            success = (arg->typeId == ajn::ALLJOYN_INT32) &&
                    OCRepPayloadSetPropInt(payload, name, arg->v_int32);
            break;
        case ajn::ALLJOYN_UINT32:
            success = (arg->typeId == ajn::ALLJOYN_UINT32) &&
                    OCRepPayloadSetPropInt(payload, name, arg->v_uint32);
            break;
        case ajn::ALLJOYN_INT64:
            if (arg->typeId != ajn::ALLJOYN_INT64)
            {
                break;
            }
            if (type == OCREP_PROP_STRING)
            {
                char buf[20];
                success = snprintf(buf, 20, "%" PRId64, arg->v_int64) <= 20;
                if (success)
                {
                    success = OCRepPayloadSetPropString(payload, name, buf);
                }
            }
            else
            {
                success = OCRepPayloadSetPropInt(payload, name, arg->v_int64);
            }
            break;
        case ajn::ALLJOYN_UINT64:
            if (arg->typeId != ajn::ALLJOYN_UINT64)
            {
                break;
            }
            if (type == OCREP_PROP_STRING)
            {
                char buf[20];
                success = snprintf(buf, 20, "%" PRIu64, arg->v_uint64) <= 20;
                if (success)
                {
                    success = OCRepPayloadSetPropString(payload, name, buf);
                }
            }
            else if (type == OCREP_PROP_INT || arg->v_uint64 <= INT64_MAX)
            {
                success = OCRepPayloadSetPropInt(payload, name, arg->v_uint64);
            }
            break;
        case ajn::ALLJOYN_DOUBLE:
            success = (arg->typeId == ajn::ALLJOYN_DOUBLE) &&
                    OCRepPayloadSetPropDouble(payload, name, arg->v_double);
            break;
        case ajn::ALLJOYN_HANDLE:
            success = false; /* Explicitly not supported */
            break;

        case ajn::ALLJOYN_STRING:
            success = (arg->typeId == ajn::ALLJOYN_STRING) &&
                    OCRepPayloadSetPropString(payload, name, arg->v_string.str);
            break;
        case ajn::ALLJOYN_OBJECT_PATH:
            success = (arg->typeId == ajn::ALLJOYN_OBJECT_PATH) &&
                    OCRepPayloadSetPropString(payload, name, arg->v_objPath.str);
            break;
        case ajn::ALLJOYN_SIGNATURE:
            success = (arg->typeId == ajn::ALLJOYN_SIGNATURE) &&
                    OCRepPayloadSetPropString(payload, name, arg->v_signature.sig);
            break;

        case ajn::ALLJOYN_ARRAY:
            switch (signature[1])
            {
                case ajn::ALLJOYN_BYTE:
                    {
                        if (arg->typeId != ajn::ALLJOYN_BYTE_ARRAY)
                        {
                            break;
                        }
                        OCByteString value;
                        value.len = arg->v_scalarArray.numElements;
                        if (value.len)
                        {
                            value.bytes = (uint8_t *) OICMalloc(value.len * sizeof(uint8_t));
                            if (!value.bytes)
                            {
                                success = false;
                                break;
                            }
                            memcpy(value.bytes, arg->v_scalarArray.v_byte, value.len);
                        }
                        else
                        {
                            value.bytes = NULL;
                        }
                        success = OCRepPayloadSetPropByteStringAsOwner(payload, name, &value);
                        break;
                    }
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    {
                        if (arg->typeId != ajn::ALLJOYN_ARRAY)
                        {
                            break;
                        }
                        OCRepPayload *value = OCRepPayloadCreate();
                        if (!value)
                        {
                            break;
                        }
                        success = true;
                        for (size_t i = 0; success && i < arg->v_array.GetNumElements(); ++i)
                        {
                            success = ToOCPayload(value, NULL, type, &arg->v_array.GetElements()[i],
                                    &signature[1]);
                        }
                        if (success)
                        {
                            success = OCRepPayloadSetPropObjectAsOwner(payload, name, value);
                        }
                        else
                        {
                            OCRepPayloadDestroy(value);
                        }
                        break;
                    }
                case ajn::ALLJOYN_VARIANT:
                    {
                        std::string variantSig;
                        success = GetVariantSignature(arg, variantSig);
                        if (!success)
                        {
                            break;
                        }
                        if (variantSig[0] == ajn::ALLJOYN_STRUCT_OPEN)
                        {
                            if (arg->typeId != ajn::ALLJOYN_ARRAY ||
                                    arg->v_array.GetElemSig()[0] != ajn::ALLJOYN_VARIANT)
                            {
                                break;
                            }
                            OCRepPayload *value = OCRepPayloadCreate();
                            if (!value)
                            {
                                break;
                            }
                            success = true;
                            const char *vsig = variantSig.c_str();
                            ++vsig;
                            const char *fieldSignature = vsig;
                            for (size_t i = 0; success && i < arg->v_array.GetNumElements(); ++i)
                            {
                                ParseCompleteType(vsig);
                                std::string sig(fieldSignature, vsig - fieldSignature);
                                fieldSignature = vsig;
                                char name[16];
                                snprintf(name, 16, "%zu", i);
                                success = ToOCPayload(value, name, type,
                                        arg->v_array.GetElements()[i].v_variant.val, sig.c_str());
                            }
                            if (success)
                            {
                                success = OCRepPayloadSetPropObjectAsOwner(payload, name, value);
                            }
                            else
                            {
                                OCRepPayloadDestroy(value);
                            }
                            break;
                        }
                    }
                    /* FALLTHROUGH */
                default:
                    success = SetPropArray(payload, name, type, arg, signature);
                    break;
            }
            break;
        case ajn::ALLJOYN_STRUCT_OPEN:
            {
                if (arg->typeId != ajn::ALLJOYN_STRUCT)
                {
                    break;
                }
                OCRepPayload *value = OCRepPayloadCreate();
                if (!value)
                {
                    break;
                }
                success = true;
                ++signature;
                const char *fieldSignature = signature;
                for (size_t i = 0; success && i < arg->v_struct.numMembers; ++i)
                {
                    ParseCompleteType(signature);
                    std::string sig(fieldSignature, signature - fieldSignature);
                    fieldSignature = signature;
                    char name[16];
                    snprintf(name, 16, "%zu", i);
                    success = ToOCPayload(value, name, type, &arg->v_struct.members[i], sig.c_str());
                }
                if (success)
                {
                    success = OCRepPayloadSetPropObjectAsOwner(payload, name, value);
                }
                else
                {
                    OCRepPayloadDestroy(value);
                }
                break;
            }
        case ajn::ALLJOYN_VARIANT:
            success = (arg->typeId == ajn::ALLJOYN_VARIANT) &&
                    ToOCPayload(payload, name, type, arg->v_variant.val,
                            arg->v_variant.val->Signature().c_str());
            break;
        case ajn::ALLJOYN_DICT_ENTRY_OPEN:
            {
                if (arg->typeId != ajn::ALLJOYN_DICT_ENTRY)
                {
                    break;
                }
                const char *entrySig = &signature[1];
                const char *keySignature = entrySig;
                ParseCompleteType(entrySig);
                std::string keySig(keySignature, entrySig - keySignature);
                const char *valSignature = entrySig;
                ParseCompleteType(entrySig);
                std::string valSig(valSignature, entrySig - valSignature);
                ajn::MsgArg *key = arg->v_dictEntry.key;
                if (key->typeId != keySig[0])
                {
                    break;
                }
                static const int keyNameBufSize = 384;
                char keyNameBuf[keyNameBufSize];
                const char *keyName = keyNameBuf;
                switch (keySig[0])
                {
                    case ajn::ALLJOYN_BOOLEAN:
                        keyName = key->v_bool ? "true" : "false";
                        success = true;
                        break;
                    case ajn::ALLJOYN_BYTE:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%u",
                                key->v_byte) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_INT16:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%d",
                                key->v_int16) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_UINT16:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%u",
                                key->v_uint16) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_INT32:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%d",
                                key->v_int32) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_UINT32:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%u",
                                key->v_uint32) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_INT64:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%" PRIi64,
                                key->v_int64) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_UINT64:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%" PRIu64,
                                key->v_uint64) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_DOUBLE:
                        success = snprintf(keyNameBuf, keyNameBufSize, "%f",
                                key->v_double) <= keyNameBufSize;
                        break;
                    case ajn::ALLJOYN_STRING:
                    case ajn::ALLJOYN_OBJECT_PATH:
                        keyName = key->v_string.str;
                        success = true;
                        break;
                    case ajn::ALLJOYN_SIGNATURE:
                        keyName = key->v_signature.sig;
                        success = true;
                        break;
                    case ajn::ALLJOYN_HANDLE:
                        success = false; /* Explicitly not supported */
                        break;
                    default:
                        success = false; /* Only basic types are allowed as keys */
                        break;
                }
                if (success)
                {
                    success = ToOCPayload(payload, keyName, type, arg->v_dictEntry.val,
                            valSig.c_str());
                }
                break;
            }

        case ajn::ALLJOYN_STRUCT:
        case ajn::ALLJOYN_DICT_ENTRY:
        case ajn::ALLJOYN_WILDCARD:
            assert(0); /* None of these ever used in a signature */
            break;

        case ajn::ALLJOYN_INVALID:
        case ajn::ALLJOYN_STRUCT_CLOSE:
        case ajn::ALLJOYN_DICT_ENTRY_CLOSE:
            success = false;
            break;

        case '[':
            {
                OCRepPayload *obj = CloneObject(type, arg, signature);
                if (obj)
                {
                    success = OCRepPayloadSetPropObjectAsOwner(payload, name, obj);
                }
            }
            break;
    }
    return success;
}

static bool ToAJMsgArg(ajn::MsgArg *arg, const char *signature, OCRepPayloadValueArray *arr,
        const char *arrSignature, size_t *ai, uint8_t di);

static bool ToAJMsgArg(ajn::MsgArg *arg, const char *signature, OCRepPayloadValueArray *arr,
        size_t *ai, uint8_t di)
{
    return ToAJMsgArg(arg, signature, arr, NULL, ai, di);
}

static bool ToAJMsgArg(ajn::MsgArg *arg, const char *signature, OCRepPayloadValueArray *arr,
        const char *arrSignature, size_t *ai, uint8_t di)
{
    (void) arrSignature;
    bool success = true;
    switch (signature[0])
    {
        case ajn::ALLJOYN_BOOLEAN:
        case ajn::ALLJOYN_BYTE:
        case ajn::ALLJOYN_INT16:
        case ajn::ALLJOYN_UINT16:
        case ajn::ALLJOYN_INT32:
        case ajn::ALLJOYN_UINT32:
        case ajn::ALLJOYN_INT64:
        case ajn::ALLJOYN_UINT64:
        case ajn::ALLJOYN_DOUBLE:
            success = false; /* Loss of information */
            break;
        case ajn::ALLJOYN_HANDLE:
            success = false; /* Explicitly not supported */
            break;
        case ajn::ALLJOYN_STRING:
        case ajn::ALLJOYN_OBJECT_PATH:
        case ajn::ALLJOYN_SIGNATURE:
            success = false; /* Loss of information */
            break;
        case ajn::ALLJOYN_ARRAY:
            if ((di >= MAX_REP_ARRAY_DEPTH) ||
                    (((di + 1) < MAX_REP_ARRAY_DEPTH) && (arr->dimensions[di + 1]) &&
                            (signature[1] != ajn::ALLJOYN_ARRAY) &&
                            (signature[1] != ajn::ALLJOYN_VARIANT)))
            {
                success = false;
                break;
            }
            switch (signature[1])
            {
                case ajn::ALLJOYN_BOOLEAN:
                    {
                        bool *v_bool = new bool[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                case OCREP_PROP_DOUBLE:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_bool[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (!strcmp(arr->strArray[(*ai)], "true"))
                                    {
                                        v_bool[i] = true;
                                    }
                                    else if (!strcmp(arr->strArray[(*ai)], "false"))
                                    {
                                        v_bool[i] = false;
                                    }
                                    else
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_BOOLEAN_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_bool = v_bool;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_bool;
                        }
                        break;
                    }
                case ajn::ALLJOYN_BYTE:
                    {
                        uint8_t *v_byte = new uint8_t[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    success = (0 <= arr->iArray[(*ai)] &&
                                            arr->iArray[(*ai)] <= UINT8_MAX);
                                    if (success)
                                    {
                                        v_byte[i] = arr->iArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    success = (floor(arr->dArray[(*ai)]) == arr->dArray[(*ai)]);
                                    if (success)
                                    {
                                        success = (0 <= arr->dArray[(*ai)] &&
                                                arr->dArray[(*ai)] <= UINT8_MAX);
                                    }
                                    if (success)
                                    {
                                        v_byte[i] = arr->dArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_byte[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%" SCNu8, &v_byte[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_BYTE_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_byte = v_byte;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_byte;
                        }
                        break;
                    }
                case ajn::ALLJOYN_INT16:
                    {
                        int16_t *v_int16 = new int16_t[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    success = (INT16_MIN <= arr->iArray[(*ai)] &&
                                            arr->iArray[(*ai)] <= INT16_MAX);
                                    if (success)
                                    {
                                        v_int16[i] = arr->iArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    success = (floor(arr->dArray[(*ai)]) == arr->dArray[(*ai)]);
                                    if (success)
                                    {
                                        success = (INT16_MIN <= arr->dArray[(*ai)] &&
                                                arr->dArray[(*ai)] <= INT16_MAX);
                                    }
                                    if (success)
                                    {
                                        v_int16[i] = arr->dArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_int16[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%" SCNd16, &v_int16[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_INT16_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_int16 = v_int16;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_int16;
                        }
                        break;
                    }
                case ajn::ALLJOYN_UINT16:
                    {
                        uint16_t *v_uint16 = new uint16_t[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    success = (0 <= arr->iArray[(*ai)] &&
                                            arr->iArray[(*ai)] <= UINT16_MAX);
                                    if (success)
                                    {
                                        v_uint16[i] = arr->iArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    success = (floor(arr->dArray[(*ai)]) == arr->dArray[(*ai)]);
                                    if (success)
                                    {
                                        success = (0 <= arr->dArray[(*ai)] &&
                                                arr->dArray[(*ai)] <= UINT16_MAX);
                                    }
                                    if (success)
                                    {
                                        v_uint16[i] = arr->dArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_uint16[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%" SCNu16, &v_uint16[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_UINT16_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_uint16 = v_uint16;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_uint16;
                        }
                        break;
                    }
                case ajn::ALLJOYN_INT32:
                    {
                        int32_t *v_int32 = new int32_t[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    success = (INT32_MIN <= arr->iArray[(*ai)] &&
                                            arr->iArray[(*ai)] <= INT32_MAX);
                                    if (success)
                                    {
                                        v_int32[i] = arr->iArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    success = (floor(arr->dArray[(*ai)]) == arr->dArray[(*ai)]);
                                    if (success)
                                    {
                                        success = (INT32_MIN <= arr->dArray[(*ai)] &&
                                                arr->dArray[(*ai)] <= INT32_MAX);
                                    }
                                    if (success)
                                    {
                                        v_int32[i] = arr->dArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_int32[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%" SCNd32, &v_int32[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_INT32_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_int32 = v_int32;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_int32;
                        }
                        break;
                    }
                case ajn::ALLJOYN_UINT32:
                    {
                        uint32_t *v_uint32 = new uint32_t[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    success = (0 <= arr->iArray[(*ai)] &&
                                            arr->iArray[(*ai)] <= UINT32_MAX);
                                    if (success)
                                    {
                                        v_uint32[i] = arr->iArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    success = (floor(arr->dArray[(*ai)]) == arr->dArray[(*ai)]);
                                    if (success)
                                    {
                                        success = (0 <= arr->dArray[(*ai)] && arr->dArray[(*ai)]
                                                <= UINT32_MAX);
                                    }
                                    if (success)
                                    {
                                        v_uint32[i] = arr->dArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_uint32[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%" SCNu32, &v_uint32[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_UINT32_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_uint32 = v_uint32;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_uint32;
                        }
                        break;
                    }
                case ajn::ALLJOYN_INT64:
                    {
                        int64_t *v_int64 = new int64_t[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    v_int64[i] = arr->iArray[(*ai)];
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    success = (floor(arr->dArray[(*ai)]) == arr->dArray[(*ai)]);
                                    if (success)
                                    {
                                        success = (MIN_SAFE_INTEGER <= arr->dArray[(*ai)] &&
                                                arr->dArray[(*ai)] <= MAX_SAFE_INTEGER);
                                    }
                                    if (success)
                                    {
                                        v_int64[i] = arr->dArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_int64[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%" SCNd64, &v_int64[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_INT64_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_int64 = v_int64;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_int64;
                        }
                        break;
                    }
                case ajn::ALLJOYN_UINT64:
                    {
                        uint64_t *v_uint64 = new uint64_t[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    success = (0 <= arr->iArray[(*ai)]);
                                    if (success)
                                    {
                                        v_uint64[i] = arr->iArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    success = (floor(arr->dArray[(*ai)]) == arr->dArray[(*ai)]);
                                    if (success)
                                    {
                                        success = (0 <= arr->dArray[(*ai)] &&
                                                arr->dArray[(*ai)] <= MAX_SAFE_INTEGER);
                                    }
                                    if (success)
                                    {
                                        v_uint64[i] = arr->dArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_uint64[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%" SCNu64, &v_uint64[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_UINT64_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_uint64 = v_uint64;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_uint64;
                        }
                        break;
                    }
                case ajn::ALLJOYN_DOUBLE:
                    {
                        double *v_double = new double[arr->dimensions[di]];
                        for (size_t i = 0; success && i < arr->dimensions[di]; ++i, ++(*ai))
                        {
                            switch (arr->type)
                            {
                                case OCREP_PROP_INT:
                                    success = (MIN_SAFE_INTEGER <= arr->iArray[(*ai)] &&
                                            arr->iArray[(*ai)] <= MAX_SAFE_INTEGER);
                                    if (success)
                                    {
                                        arg->typeId = ajn::ALLJOYN_DOUBLE;
                                        arg->v_double = arr->iArray[(*ai)];
                                    }
                                    break;
                                case OCREP_PROP_DOUBLE:
                                    v_double[i] = arr->dArray[(*ai)];
                                    break;
                                case OCREP_PROP_BOOL:
                                    v_double[i] = arr->bArray[(*ai)];
                                    break;
                                case OCREP_PROP_STRING:
                                    if (sscanf(arr->strArray[(*ai)], "%lf", &v_double[i]) != 1)
                                    {
                                        success = false;
                                    }
                                    break;
                                case OCREP_PROP_BYTE_STRING:
                                case OCREP_PROP_OBJECT:
                                    success = false; /* Loss of information */
                                    break;
                                case OCREP_PROP_NULL:
                                    break; /* Explicitly not supported */
                                case OCREP_PROP_ARRAY:
                                    assert(0); /* Not used as an array value type */
                                    break;
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_DOUBLE_ARRAY;
                            arg->v_scalarArray.numElements = arr->dimensions[di];
                            arg->v_scalarArray.v_double = v_double;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                        }
                        else
                        {
                            delete[] v_double;
                        }
                        break;
                    }
                case ajn::ALLJOYN_HANDLE:
                    success = false; /* Explicitly not supported */
                    break;
                case ajn::ALLJOYN_STRING:
                case ajn::ALLJOYN_OBJECT_PATH:
                case ajn::ALLJOYN_SIGNATURE:
                case ajn::ALLJOYN_VARIANT:
                    {
                        size_t numElems = arr->dimensions[di];
                        ajn::MsgArg *elems = numElems ? new ajn::MsgArg[numElems] : NULL;
                        for (size_t i = 0; success && i < numElems; ++i, ++(*ai))
                        {
                            if ((di + 1) == MAX_REP_ARRAY_DEPTH || !arr->dimensions[di + 1])
                            {
                                OCRepPayloadValue arrValue;
                                arrValue.type = arr->type;
                                switch (arr->type)
                                {
                                    case OCREP_PROP_INT:
                                        arrValue.i = arr->iArray[(*ai)];
                                        break;
                                    case OCREP_PROP_DOUBLE:
                                        arrValue.d = arr->dArray[(*ai)];
                                        break;
                                    case OCREP_PROP_BOOL:
                                        arrValue.b = arr->bArray[(*ai)];
                                        break;
                                    case OCREP_PROP_STRING:
                                        arrValue.str = arr->strArray[(*ai)];
                                        break;
                                    case OCREP_PROP_BYTE_STRING:
                                        arrValue.ocByteStr = arr->ocByteStrArray[(*ai)];
                                        break;
                                    case OCREP_PROP_OBJECT:
                                        arrValue.obj = arr->objArray[(*ai)];
                                        break;
                                    case OCREP_PROP_NULL:
                                        /* Explicitly not supported */
                                        break;
                                    case OCREP_PROP_ARRAY:
                                        /* Not used as an array value type */
                                        assert(0);
                                        break;
                                }
                                success = ToAJMsgArg(&elems[i], &signature[1], &arrValue);
                            }
                            else
                            {
                                success = ToAJMsgArg(&elems[i], &signature[1], arr, ai, di + 1);
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_ARRAY;
                            success = (arg->v_array.SetElements(&signature[1], numElems, elems) ==
                                    ER_OK);
                        }
                        if (success)
                        {
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                        }
                        else
                        {
                            delete[] elems;
                        }
                        break;
                    }
                case ajn::ALLJOYN_ARRAY:
                    {
                        size_t numElems = arr->dimensions[di];
                        ajn::MsgArg *elems = new ajn::MsgArg[numElems];
                        for (size_t i = 0; success && i < numElems; ++i)
                        {
                            if (signature[2] == ajn::ALLJOYN_DICT_ENTRY_OPEN)
                            {
                                if (arr->type == OCREP_PROP_OBJECT)
                                {
                                    OCRepPayloadValue arrValue;
                                    arrValue.type = arr->type;
                                    arrValue.obj = arr->objArray[(*ai)++];
                                    success = ToAJMsgArg(&elems[i], &signature[1], &arrValue);
                                }
                                else
                                {
                                    success = false;
                                }
                            }
                            else if (arr->type == OCREP_PROP_BYTE_STRING &&
                                    ((di + 1) == MAX_REP_ARRAY_DEPTH || !arr->dimensions[di + 1]))
                            {
                                OCRepPayloadValue arrValue;
                                arrValue.type = arr->type;
                                arrValue.ocByteStr = arr->ocByteStrArray[(*ai)++];
                                success = ToAJMsgArg(&elems[i], &signature[1], &arrValue);
                            }
                            else
                            {
                                success = ToAJMsgArg(&elems[i], &signature[1], arr, ai, di + 1);
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_ARRAY;
                            success = (arg->v_array.SetElements(&signature[1], numElems, elems) ==
                                    ER_OK);
                        }
                        if (success)
                        {
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                        }
                        else
                        {
                            delete[] elems;
                        }
                        break;
                    }
                case ajn::ALLJOYN_STRUCT_OPEN:
                    switch (arr->type)
                    {
                        case OCREP_PROP_INT:
                        case OCREP_PROP_DOUBLE:
                        case OCREP_PROP_BOOL:
                        case OCREP_PROP_STRING:
                        case OCREP_PROP_BYTE_STRING:
                            success = false; /* Loss of information */ // TODO unless struct members are all the same type
                            break;
                        case OCREP_PROP_OBJECT:
                            {
                                size_t numElems = arr->dimensions[di];
                                ajn::MsgArg *elems = new ajn::MsgArg[numElems];
                                for (size_t i = 0; success && i < numElems; ++i, ++(*ai))
                                {
                                    OCRepPayloadValue arrValue;
                                    arrValue.type = arr->type;
                                    arrValue.obj = arr->objArray[(*ai)];
                                    success = ToAJMsgArg(&elems[i], &signature[1], &arrValue);
                                }
                                if (success)
                                {
                                    arg->typeId = ajn::ALLJOYN_ARRAY;
                                    success = (arg->v_array.SetElements(&signature[1], numElems,
                                            elems) == ER_OK);
                                }
                                if (success)
                                {
                                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                }
                                else
                                {
                                    delete[] elems;
                                }
                                break;
                            }
                        case OCREP_PROP_NULL:
                            success = false; /* Explicitly not supported */
                            break;
                        case OCREP_PROP_ARRAY:
                            assert(0); /* Not used as an array value type */
                            break;
                    }
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    switch (arr->type)
                    {
                        case OCREP_PROP_INT:
                        case OCREP_PROP_DOUBLE:
                        case OCREP_PROP_BOOL:
                        case OCREP_PROP_STRING:
                        case OCREP_PROP_BYTE_STRING:
                        case OCREP_PROP_OBJECT:
                            success = false; /* Loss of information */
                            break;
                        case OCREP_PROP_NULL:
                            success = false; /* Explicitly not supported */
                            break;
                        case OCREP_PROP_ARRAY:
                            assert(0); /* Not used as an array value type */
                            break;
                    }
                    break;
                case '[':
                    switch (arr->type)
                    {
                        case OCREP_PROP_INT:
                        case OCREP_PROP_DOUBLE:
                        case OCREP_PROP_BOOL:
                        case OCREP_PROP_STRING:
                        case OCREP_PROP_BYTE_STRING:
                            success = false; /* Loss of information */ // TODO unless struct members are all the same type
                            break;
                        case OCREP_PROP_OBJECT:
                            {
                                size_t numElems = arr->dimensions[di];
                                ajn::MsgArg *elems = new ajn::MsgArg[numElems];
                                for (size_t i = 0; success && i < numElems; ++i, ++(*ai))
                                {
                                    OCRepPayloadValue arrValue;
                                    arrValue.type = arr->type;
                                    arrValue.obj = arr->objArray[(*ai)];
                                    success = ToAJMsgArg(&elems[i], &signature[1], &arrValue);
                                }
                                if (success)
                                {
                                    arg->typeId = ajn::ALLJOYN_ARRAY;
                                    std::string elemSig = "(";
                                    for (std::vector<Types::Field>::iterator field =
                                                 Types::m_structs[&signature[1]].begin();
                                         field != Types::m_structs[&signature[1]].end(); ++field)
                                    {
                                        elemSig += field->m_signature;
                                    }
                                    elemSig += ")";
                                    success = (arg->v_array.SetElements(elemSig.c_str(), numElems,
                                            elems) == ER_OK);
                                }
                                if (success)
                                {
                                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                }
                                else
                                {
                                    delete[] elems;
                                }
                                break;
                            }
                        case OCREP_PROP_NULL:
                            success = false; /* Explicitly not supported */
                            break;
                        case OCREP_PROP_ARRAY:
                            assert(0); /* Not used as an array value type */
                            break;
                    }
                    break;
                default:
                    success = false;
                    break;
            }
            break;
        case ajn::ALLJOYN_STRUCT_OPEN:
        case '[':
            // TODO maybe, if struct members are all the same type
            success = false;
            break;
        case ajn::ALLJOYN_VARIANT:
            {
                arg->typeId = ajn::ALLJOYN_VARIANT;
                arg->v_variant.val = new ajn::MsgArg();
                arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                char variantSig[256] = { 0 };
                if (!arrSignature)
                {
                    CreateSignature(variantSig, arr, di);
                    arrSignature = variantSig;
                }
                success = ToAJMsgArg(arg->v_variant.val, arrSignature, arr, ai, di);
            }
            break;
        case ajn::ALLJOYN_DICT_ENTRY_OPEN:
            success = false; /* Loss of information */
            break;
        default:
            success = false;
            break;
    }
    return success;
}

bool ToAJMsgArg(ajn::MsgArg *arg, const char *signature, OCRepPayloadValue *value,
        const char *valueSignature)
{
    const char *argSignature = signature;
    ParseCompleteType(signature);
    std::string sig(argSignature, signature - argSignature);
    bool success = true;
    switch (value->type)
    {
        case OCREP_PROP_NULL:
            /* Explicitly not supported, except that it may be an empty array (see IOT-2457) */
            if (sig[0] == ajn::ALLJOYN_ARRAY)
            {
                size_t i = 0;
                success = ToAJMsgArg(arg, sig.c_str(), &value->arr, valueSignature, &i, 0);
            }
            else
            {
                success = false;
            }
            break;
        case OCREP_PROP_INT:
            switch (sig[0])
            {
                case ajn::ALLJOYN_BOOLEAN:
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_BYTE:
                    success = (0 <= value->i && value->i <= UINT8_MAX);
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_BYTE;
                        arg->v_byte = value->i;
                    }
                    break;
                case ajn::ALLJOYN_INT16:
                    success = (INT16_MIN <= value->i && value->i <= INT16_MAX);
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_INT16;
                        arg->v_int16 = value->i;
                    }
                    break;
                case ajn::ALLJOYN_UINT16:
                    success = (0 <= value->i && value->i <= UINT16_MAX);
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT16;
                        arg->v_uint16 = value->i;
                    }
                    break;
                case ajn::ALLJOYN_INT32:
                    success = (INT32_MIN <= value->i && value->i <= INT32_MAX);
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_INT32;
                        arg->v_int32 = value->i;
                    }
                    break;
                case ajn::ALLJOYN_UINT32:
                    success = (0 <= value->i && value->i <= UINT32_MAX);
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT32;
                        arg->v_uint32 = value->i;
                    }
                    break;
                case ajn::ALLJOYN_INT64:
                    arg->typeId = ajn::ALLJOYN_INT64;
                    arg->v_int64 = value->i;
                    break;
                case ajn::ALLJOYN_UINT64:
                    success = (0 <= value->i);
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT64;
                        arg->v_uint64 = value->i;
                    }
                    break;
                case ajn::ALLJOYN_DOUBLE:
                    success = (MIN_SAFE_INTEGER <= value->i && value->i <= MAX_SAFE_INTEGER);
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_DOUBLE;
                        arg->v_double = value->i;
                    }
                    break;
                case ajn::ALLJOYN_HANDLE:
                    success = false; /* Explicitly not supported */
                    break;
                case ajn::ALLJOYN_STRING:
                case ajn::ALLJOYN_OBJECT_PATH:
                case ajn::ALLJOYN_SIGNATURE:
                case ajn::ALLJOYN_ARRAY:
                case ajn::ALLJOYN_STRUCT_OPEN:
                case '[':
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_VARIANT:
                    arg->typeId = ajn::ALLJOYN_VARIANT;
                    arg->v_variant.val = new ajn::MsgArg();
                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                    if (valueSignature)
                    {
                        success = ToAJMsgArg(arg->v_variant.val, valueSignature, value);
                    }
                    else
                    {
                        if (value->i < INT32_MIN || INT32_MAX < value->i)
                        {
                            success = ToAJMsgArg(arg->v_variant.val, "x", value);
                        }
                        else
                        {
                            success = ToAJMsgArg(arg->v_variant.val, "i", value);
                        }
                    }
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    success = false; /* Loss of information */
                    break;
                default:
                    success = false;
                    break;
            }
            break;
        case OCREP_PROP_DOUBLE:
            switch (sig[0])
            {
                case ajn::ALLJOYN_BOOLEAN:
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_BYTE:
                    success = (floor(value->d) == value->d);
                    if (success)
                    {
                        success = (0 <= value->d && value->d <= UINT8_MAX);
                    }
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_BYTE;
                        arg->v_byte = value->d;
                    }
                    break;
                case ajn::ALLJOYN_INT16:
                    success = (floor(value->d) == value->d);
                    if (success)
                    {
                        success = (INT16_MIN <= value->d && value->d <= INT16_MAX);
                    }
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_INT16;
                        arg->v_int16 = value->d;
                    }
                    break;
                case ajn::ALLJOYN_UINT16:
                    success = (floor(value->d) == value->d);
                    if (success)
                    {
                        success = (0 <= value->d && value->d <= UINT16_MAX);
                    }
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT16;
                        arg->v_uint16 = value->d;
                    }
                    break;
                case ajn::ALLJOYN_INT32:
                    success = (floor(value->d) == value->d);
                    if (success)
                    {
                        success = (INT32_MIN <= value->d && value->d <= INT32_MAX);
                    }
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_INT32;
                        arg->v_int32 = value->d;
                    }
                    break;
                case ajn::ALLJOYN_UINT32:
                    success = (floor(value->d) == value->d);
                    if (success)
                    {
                        success = (0 <= value->d && value->d <= UINT32_MAX);
                    }
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT32;
                        arg->v_uint32 = value->d;
                    }
                    break;
                case ajn::ALLJOYN_INT64:
                    success = (floor(value->d) == value->d);
                    if (success)
                    {
                        success = (MIN_SAFE_INTEGER <= value->d && value->d <= MAX_SAFE_INTEGER);
                    }
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_INT64;
                        arg->v_int64 = value->d;
                    }
                    break;
                case ajn::ALLJOYN_UINT64:
                    success = (floor(value->d) == value->d);
                    if (success)
                    {
                        success = (0 <= value->d && value->d <= MAX_SAFE_INTEGER);
                    }
                    if (success)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT64;
                        arg->v_uint64 = value->d;
                    }
                    break;
                case ajn::ALLJOYN_DOUBLE:
                    arg->typeId = ajn::ALLJOYN_DOUBLE;
                    arg->v_double = value->d;
                    break;
                case ajn::ALLJOYN_HANDLE:
                    success = false; /* Explicitly not supported */
                    break;
                case ajn::ALLJOYN_STRING:
                case ajn::ALLJOYN_OBJECT_PATH:
                case ajn::ALLJOYN_SIGNATURE:
                case ajn::ALLJOYN_ARRAY:
                case ajn::ALLJOYN_STRUCT_OPEN:
                case '[':
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_VARIANT:
                    arg->typeId = ajn::ALLJOYN_VARIANT;
                    arg->v_variant.val = new ajn::MsgArg();
                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                    success = ToAJMsgArg(arg->v_variant.val, "d", value);
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    success = false; /* Loss of information */
                    break;
                default:
                    success = false;
                    break;
            }
            break;
        case OCREP_PROP_BOOL:
            switch (sig[0])
            {
                case ajn::ALLJOYN_BOOLEAN:
                    arg->typeId = ajn::ALLJOYN_BOOLEAN;
                    arg->v_bool = value->b;
                    break;
                case ajn::ALLJOYN_BYTE:
                    arg->typeId = ajn::ALLJOYN_BYTE;
                    arg->v_byte = value->b;
                    break;
                case ajn::ALLJOYN_INT16:
                    arg->typeId = ajn::ALLJOYN_INT16;
                    arg->v_int16 = value->b;
                    break;
                case ajn::ALLJOYN_UINT16:
                    arg->typeId = ajn::ALLJOYN_UINT16;
                    arg->v_uint16 = value->b;
                    break;
                case ajn::ALLJOYN_INT32:
                    arg->typeId = ajn::ALLJOYN_INT32;
                    arg->v_int32 = value->b;
                    break;
                case ajn::ALLJOYN_UINT32:
                    arg->typeId = ajn::ALLJOYN_UINT32;
                    arg->v_uint32 = value->b;
                    break;
                case ajn::ALLJOYN_INT64:
                    arg->typeId = ajn::ALLJOYN_INT64;
                    arg->v_int64 = value->b;
                    break;
                case ajn::ALLJOYN_UINT64:
                    arg->typeId = ajn::ALLJOYN_UINT64;
                    arg->v_uint64 = value->b;
                    break;
                case ajn::ALLJOYN_DOUBLE:
                    arg->typeId = ajn::ALLJOYN_DOUBLE;
                    arg->v_double = value->b;
                    break;
                case ajn::ALLJOYN_HANDLE:
                    success = false; /* Explicitly not supported */
                    break;
                case ajn::ALLJOYN_STRING:
                    arg->typeId = ajn::ALLJOYN_STRING;
                    if (value->b)
                    {
                        arg->v_string.len = 4;
                        arg->v_string.str = "true";
                    }
                    else
                    {
                        arg->v_string.len = 5;
                        arg->v_string.str = "false";
                    }
                    break;
                case ajn::ALLJOYN_OBJECT_PATH:
                case ajn::ALLJOYN_SIGNATURE:
                case ajn::ALLJOYN_ARRAY:
                case ajn::ALLJOYN_STRUCT_OPEN:
                case '[':
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_VARIANT:
                    arg->typeId = ajn::ALLJOYN_VARIANT;
                    arg->v_variant.val = new ajn::MsgArg();
                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                    success = ToAJMsgArg(arg->v_variant.val, "b", value);
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    success = false; /* Loss of information */
                    break;
                default:
                    success = false;
                    break;
            }
            break;
        case OCREP_PROP_STRING:
            switch (sig[0])
            {
                case ajn::ALLJOYN_BOOLEAN:
                    if (!strcmp(value->str, "true"))
                    {
                        arg->typeId = ajn::ALLJOYN_BOOLEAN;
                        arg->v_bool = true;
                    }
                    else if (!strcmp(value->str, "false"))
                    {
                        arg->typeId = ajn::ALLJOYN_BOOLEAN;
                        arg->v_bool = false;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_BYTE:
                    if (sscanf(value->str, "%" SCNu8, &arg->v_byte) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_BYTE;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_INT16:
                    if (sscanf(value->str, "%" SCNd16, &arg->v_int16) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_INT16;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_UINT16:
                    if (sscanf(value->str, "%" SCNu16, &arg->v_uint16) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT16;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_INT32:
                    if (sscanf(value->str, "%" SCNd32, &arg->v_int32) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_INT32;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_UINT32:
                    if (sscanf(value->str, "%" SCNu32, &arg->v_uint32) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT32;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_INT64:
                    if (sscanf(value->str, "%" SCNd64, &arg->v_int64) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_INT64;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_UINT64:
                    if (sscanf(value->str, "%" SCNu64, &arg->v_uint64) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_UINT64;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_DOUBLE:
                    if (sscanf(value->str, "%lf", &arg->v_double) == 1)
                    {
                        arg->typeId = ajn::ALLJOYN_DOUBLE;
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                case ajn::ALLJOYN_HANDLE:
                    success = false; /* Explicitly not supported */
                    break;
                case ajn::ALLJOYN_STRING:
                    arg->typeId = ajn::ALLJOYN_STRING;
                    arg->v_string.len = strlen(value->str);
                    arg->v_string.str = value->str;
                    break;
                case ajn::ALLJOYN_OBJECT_PATH:
                    arg->typeId = ajn::ALLJOYN_OBJECT_PATH;
                    arg->v_objPath.len = strlen(value->str);
                    arg->v_objPath.str = value->str;
                    break;
                case ajn::ALLJOYN_SIGNATURE:
                    arg->typeId = ajn::ALLJOYN_SIGNATURE;
                    arg->v_signature.len = strlen(value->str);
                    arg->v_signature.sig = value->str;
                    break;
                case ajn::ALLJOYN_ARRAY:
                case ajn::ALLJOYN_STRUCT_OPEN:
                case '[':
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_VARIANT:
                    arg->typeId = ajn::ALLJOYN_VARIANT;
                    arg->v_variant.val = new ajn::MsgArg();
                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                    if (valueSignature)
                    {
                        success = ToAJMsgArg(arg->v_variant.val, valueSignature, value);
                    }
                    else
                    {
                        success = ToAJMsgArg(arg->v_variant.val, "s", value);
                    }
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    success = false; /* Loss of information */
                    break;
                default:
                    success = false;
                    break;
            }
            break;
        case OCREP_PROP_BYTE_STRING:
            switch (sig[0])
            {
                case ajn::ALLJOYN_BOOLEAN:
                case ajn::ALLJOYN_BYTE:
                case ajn::ALLJOYN_INT16:
                case ajn::ALLJOYN_UINT16:
                case ajn::ALLJOYN_INT32:
                case ajn::ALLJOYN_UINT32:
                case ajn::ALLJOYN_INT64:
                case ajn::ALLJOYN_UINT64:
                case ajn::ALLJOYN_DOUBLE:
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_HANDLE:
                    success = false; /* Explicitly not supported */
                    break;
                case ajn::ALLJOYN_STRING:
                case ajn::ALLJOYN_OBJECT_PATH:
                case ajn::ALLJOYN_SIGNATURE:
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_ARRAY:
                    switch (sig[1])
                    {
                        case ajn::ALLJOYN_BOOLEAN:
                            success = false; /* Loss of information */
                            break;
                        case ajn::ALLJOYN_BYTE:
                            arg->typeId = ajn::ALLJOYN_BYTE_ARRAY;
                            arg->v_scalarArray.numElements = value->ocByteStr.len;
                            arg->v_scalarArray.v_byte = value->ocByteStr.bytes;
                            break;
                        case ajn::ALLJOYN_INT16:
                            {
                                int16_t *v_int16 = new int16_t[value->ocByteStr.len];
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    v_int16[i] = value->ocByteStr.bytes[i];
                                }
                                arg->typeId = ajn::ALLJOYN_INT16_ARRAY;
                                arg->v_scalarArray.numElements = value->ocByteStr.len;
                                arg->v_scalarArray.v_int16 = v_int16;
                                arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                                break;
                            }
                        case ajn::ALLJOYN_UINT16:
                            {
                                uint16_t *v_uint16 = new uint16_t[value->ocByteStr.len];
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    v_uint16[i] = value->ocByteStr.bytes[i];
                                }
                                arg->typeId = ajn::ALLJOYN_UINT16_ARRAY;
                                arg->v_scalarArray.numElements = value->ocByteStr.len;
                                arg->v_scalarArray.v_uint16 = v_uint16;
                                arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                                break;
                            }
                        case ajn::ALLJOYN_INT32:
                            {
                                int32_t *v_int32 = new int32_t[value->ocByteStr.len];
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    v_int32[i] = value->ocByteStr.bytes[i];
                                }
                                arg->typeId = ajn::ALLJOYN_INT32_ARRAY;
                                arg->v_scalarArray.numElements = value->ocByteStr.len;
                                arg->v_scalarArray.v_int32 = v_int32;
                                arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                                break;
                            }
                        case ajn::ALLJOYN_UINT32:
                            {
                                uint32_t *v_uint32 = new uint32_t[value->ocByteStr.len];
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    v_uint32[i] = value->ocByteStr.bytes[i];
                                }
                                arg->typeId = ajn::ALLJOYN_UINT32_ARRAY;
                                arg->v_scalarArray.numElements = value->ocByteStr.len;
                                arg->v_scalarArray.v_uint32 = v_uint32;
                                arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                                break;
                            }
                        case ajn::ALLJOYN_INT64:
                            {
                                int64_t *v_int64 = new int64_t[value->ocByteStr.len];
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    v_int64[i] = value->ocByteStr.bytes[i];
                                }
                                arg->typeId = ajn::ALLJOYN_INT64_ARRAY;
                                arg->v_scalarArray.numElements = value->ocByteStr.len;
                                arg->v_scalarArray.v_int64 = v_int64;
                                arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                                break;
                            }
                        case ajn::ALLJOYN_UINT64:
                            {
                                uint64_t *v_uint64 = new uint64_t[value->ocByteStr.len];
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    v_uint64[i] = value->ocByteStr.bytes[i];
                                }
                                arg->typeId = ajn::ALLJOYN_UINT64_ARRAY;
                                arg->v_scalarArray.numElements = value->ocByteStr.len;
                                arg->v_scalarArray.v_uint64 = v_uint64;
                                arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                                break;
                            }
                        case ajn::ALLJOYN_DOUBLE:
                            {
                                double *v_double = new double[value->ocByteStr.len];
                                for (size_t i = 0; i < value->ocByteStr.len; ++i)
                                {
                                    v_double[i] = value->ocByteStr.bytes[i];
                                }
                                arg->typeId = ajn::ALLJOYN_DOUBLE_ARRAY;
                                arg->v_scalarArray.numElements = value->ocByteStr.len;
                                arg->v_scalarArray.v_double = v_double;
                                arg->SetOwnershipFlags(ajn::MsgArg::OwnsData, false);
                                break;
                            }
                        case ajn::ALLJOYN_HANDLE:
                            success = false; /* Explicitly not supported */
                            break;
                        case ajn::ALLJOYN_STRING:
                        case ajn::ALLJOYN_OBJECT_PATH:
                        case ajn::ALLJOYN_SIGNATURE:
                            success = false; /* Loss of information */
                            break;
                        case ajn::ALLJOYN_ARRAY:
                        case ajn::ALLJOYN_STRUCT_OPEN:
                        case '[':
                            success = false; /* Loss of information */
                            break;
                        case ajn::ALLJOYN_VARIANT:
                            {
                                ajn::MsgArg *elems = new ajn::MsgArg[value->ocByteStr.len];
                                for (size_t i = 0; success && i < value->ocByteStr.len; ++i)
                                {
                                    elems[i].typeId = ajn::ALLJOYN_VARIANT;
                                    elems[i].v_variant.val = new ajn::MsgArg();
                                    elems[i].SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                    elems[i].v_variant.val->typeId = ajn::ALLJOYN_BYTE;
                                    elems[i].v_variant.val->v_byte = value->ocByteStr.bytes[i];
                                }
                                if (success)
                                {
                                    arg->typeId = ajn::ALLJOYN_ARRAY;
                                    success = (arg->v_array.SetElements("v", value->ocByteStr.len,
                                            elems) == ER_OK);
                                }
                                if (success)
                                {
                                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                }
                                else
                                {
                                    delete[] elems;
                                }
                                break;
                            }
                        case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                            success = false; /* Loss of information */
                            break;
                        default:
                            success = false;
                            break;
                    }
                    break;
                case ajn::ALLJOYN_STRUCT_OPEN:
                case '[':
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_VARIANT:
                    arg->typeId = ajn::ALLJOYN_VARIANT;
                    arg->v_variant.val = new ajn::MsgArg();
                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                    success = ToAJMsgArg(arg->v_variant.val, "ay", value);
                    break;
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    success = false; /* Loss of information */
                    break;
                default:
                    success = false;
                    break;
            }
            break;
        case OCREP_PROP_OBJECT:
            switch (sig[0])
            {
                case ajn::ALLJOYN_BOOLEAN:
                case ajn::ALLJOYN_BYTE:
                case ajn::ALLJOYN_INT16:
                case ajn::ALLJOYN_UINT16:
                case ajn::ALLJOYN_INT32:
                case ajn::ALLJOYN_UINT32:
                case ajn::ALLJOYN_INT64:
                case ajn::ALLJOYN_UINT64:
                case ajn::ALLJOYN_DOUBLE:
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_HANDLE:
                    success = false; /* Explicitly not supported */
                    break;
                case ajn::ALLJOYN_STRING:
                case ajn::ALLJOYN_OBJECT_PATH:
                case ajn::ALLJOYN_SIGNATURE:
                    success = false; /* Loss of information */
                    break;
                case ajn::ALLJOYN_ARRAY:
                    switch (sig[1])
                    {
                        case ajn::ALLJOYN_BOOLEAN:
                        case ajn::ALLJOYN_BYTE:
                        case ajn::ALLJOYN_INT16:
                        case ajn::ALLJOYN_UINT16:
                        case ajn::ALLJOYN_INT32:
                        case ajn::ALLJOYN_UINT32:
                        case ajn::ALLJOYN_INT64:
                        case ajn::ALLJOYN_UINT64:
                        case ajn::ALLJOYN_DOUBLE:
                            success = false; /* Loss of information */
                            break;
                        case ajn::ALLJOYN_HANDLE:
                            success = false; /* Explicitly not supported */
                            break;
                        case ajn::ALLJOYN_STRING:
                        case ajn::ALLJOYN_OBJECT_PATH:
                        case ajn::ALLJOYN_SIGNATURE:
                            success = false; /* Loss of information */
                            break;
                        case ajn::ALLJOYN_ARRAY:
                        case ajn::ALLJOYN_STRUCT_OPEN:
                        case '[':
                            success = false; /* Loss of information */
                            break;
                        case ajn::ALLJOYN_VARIANT:
                            {
                                char s[256] = { 0 };
                                CreateSignature(s, value);
                                size_t numElems = 0;
                                for (OCRepPayloadValue *v = value->obj->values; v; v = v->next)
                                {
                                    ++numElems;
                                }
                                ajn::MsgArg *elems = new ajn::MsgArg[numElems];
                                ajn::MsgArg *elem = elems;
                                bool success = true;
                                for (OCRepPayloadValue *v = value->obj->values; success && v;
                                     v = v->next)
                                {
                                    if (s[0] == ajn::ALLJOYN_STRUCT_OPEN)
                                    {
                                        success = ToAJMsgArg(elem, "v", v);
                                    }
                                    else
                                    {
                                        elem->typeId = ajn::ALLJOYN_VARIANT;
                                        elem->v_variant.val = new ajn::MsgArg();
                                        elem->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                        OCRepPayloadValue k;
                                        memset(&k, 0, sizeof(k));
                                        k.type = OCREP_PROP_STRING;
                                        k.str = v->name;
                                        elem->v_variant.val->typeId = ajn::ALLJOYN_DICT_ENTRY;
                                        elem->v_variant.val->v_dictEntry.key = new ajn::MsgArg();
                                        elem->v_variant.val->v_dictEntry.val = new ajn::MsgArg();
                                        elem->v_variant.val->SetOwnershipFlags(
                                            ajn::MsgArg::OwnsArgs, false);
                                        success = ToAJMsgArg(elem->v_variant.val->v_dictEntry.key,
                                                "s", &k);
                                        if (success)
                                        {
                                            success = ToAJMsgArg(
                                                elem->v_variant.val->v_dictEntry.val, "v", v);
                                        }
                                    }
                                    ++elem;
                                }
                                if (success)
                                {
                                    arg->typeId = ajn::ALLJOYN_ARRAY;
                                    success = (arg->v_array.SetElements("v", numElems, elems) ==
                                            ER_OK);
                                }
                                if (success)
                                {
                                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                }
                                else
                                {
                                    delete[] elems;
                                }
                                break;
                            }
                        case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                            {
                                const char *entrySig = &sig[2];
                                const char *keySignature = entrySig;
                                ParseCompleteType(entrySig);
                                std::string keySig(keySignature, entrySig - keySignature);
                                const char *valSignature = entrySig;
                                ParseCompleteType(entrySig);
                                std::string valSig(valSignature, entrySig - valSignature);
                                size_t numEntries = 0;
                                ajn::MsgArg *entries = NULL;
                                if (value->obj)
                                {
                                    for (OCRepPayloadValue *v = value->obj->values; v; v = v->next)
                                    {
                                        ++numEntries;
                                    }
                                    entries = new ajn::MsgArg[numEntries];
                                    ajn::MsgArg *entry = entries;
                                    success = true;
                                    for (OCRepPayloadValue *v = value->obj->values; success && v;
                                         v = v->next)
                                    {
                                        OCRepPayloadValue k;
                                        memset(&k, 0, sizeof(k));
                                        k.type = OCREP_PROP_STRING;
                                        k.str = v->name;
                                        std::string vSig;
                                        if (valueSignature &&
                                                (Types::m_dicts.find(valueSignature) != Types::m_dicts.end()) &&
                                                (Types::m_dicts[valueSignature].find(k.str) != Types::m_dicts[valueSignature].end()))
                                        {
                                            vSig = Types::m_dicts[valueSignature][k.str].m_signature;
                                        }
                                        entry->typeId = ajn::ALLJOYN_DICT_ENTRY;
                                        entry->v_dictEntry.key = new ajn::MsgArg();
                                        entry->v_dictEntry.val = new ajn::MsgArg();
                                        entry->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                        success = ToAJMsgArg(entry->v_dictEntry.key, keySig.c_str(),
                                                &k);
                                        if (success)
                                        {
                                            success = ToAJMsgArg(entry->v_dictEntry.val,
                                                    valSig.c_str(), v,
                                                    !vSig.empty() ? vSig.c_str() : NULL);
                                        }
                                        ++entry;
                                    }
                                }
                                if (success)
                                {
                                    arg->typeId = ajn::ALLJOYN_ARRAY;
                                    std::string elemSig(&sig[1], entrySig + 1 - &sig[1]);
                                    success = (arg->v_array.SetElements(elemSig.c_str(), numEntries,
                                            entries) == ER_OK);
                                }
                                if (success)
                                {
                                    arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                                }
                                else
                                {
                                    delete[] entries;
                                }
                                break;
                            }
                        default:
                            success = false;
                            break;
                    }
                    break;
                case ajn::ALLJOYN_STRUCT_OPEN:
                    {
                        const char *structSig = &sig[1];
                        const char *memberSignature = structSig;
                        size_t numMembers = 0;
                        for (OCRepPayloadValue *v = value->obj->values; v; v = v->next)
                        {
                            ++numMembers;
                        }
                        ajn::MsgArg *members = new ajn::MsgArg[numMembers];
                        ajn::MsgArg *member = members;
                        for (size_t i = 0; success && i < numMembers; ++i)
                        {
                            ParseCompleteType(structSig);
                            std::string memberSig(memberSignature, structSig - memberSignature);
                            memberSignature = structSig;
                            char name[16];
                            snprintf(name, 16, "%zu", i);
                            success = false;
                            for (OCRepPayloadValue *v = value->obj->values; v; v = v->next)
                            {
                                if (!strcmp(v->name, name))
                                {
                                    success = ToAJMsgArg(member, memberSig.c_str(), v);
                                    ++member;
                                    break;
                                }
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_STRUCT;
                            arg->v_struct.numMembers = numMembers;
                            arg->v_struct.members = members;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                        }
                        else
                        {
                            delete[] members;
                        }
                        break;
                    }
                case ajn::ALLJOYN_VARIANT:
                    {
                        arg->typeId = ajn::ALLJOYN_VARIANT;
                        arg->v_variant.val = new ajn::MsgArg();
                        arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                        if (valueSignature)
                        {
                            success = ToAJMsgArg(arg->v_variant.val, valueSignature, value);
                        }
                        else
                        {
                            char variantSig[256] = { 0 };
                            CreateSignature(variantSig, value);
                            success = ToAJMsgArg(arg->v_variant.val, variantSig, value);
                        }
                        break;
                    }
                case ajn::ALLJOYN_DICT_ENTRY_OPEN:
                    success = false; /* Loss of information */
                    break;
                case '[':
                    if (Types::m_structs.find(sig) != Types::m_structs.end())
                    {
                        size_t numMembers = Types::m_structs[sig].size();
                        ajn::MsgArg *members = new ajn::MsgArg[numMembers];
                        ajn::MsgArg *member = members;
                        std::vector<Types::Field>::iterator field;
                        for (field = Types::m_structs[sig].begin();
                             success && field != Types::m_structs[sig].end(); ++field)
                        {
                            success = false;
                            for (OCRepPayloadValue *v = value->obj->values; v; v = v->next)
                            {
                                if (!strcmp(v->name, field->m_name.c_str()))
                                {
                                    success = ToAJMsgArg(member, field->m_signature.c_str(), v);
                                    ++member;
                                    break;
                                }
                            }
                        }
                        if (success)
                        {
                            arg->typeId = ajn::ALLJOYN_STRUCT;
                            arg->v_struct.numMembers = numMembers;
                            arg->v_struct.members = members;
                            arg->SetOwnershipFlags(ajn::MsgArg::OwnsArgs, false);
                        }
                        else
                        {
                            delete[] members;
                        }
                    }
                    else if (Types::m_dicts.find(sig) != Types::m_dicts.end())
                    {
                        success = ToAJMsgArg(arg, "a{sv}", value, sig.c_str());
                    }
                    else
                    {
                        success = false;
                    }
                    break;
                default:
                    success = false;
                    break;
            }
            break;
        case OCREP_PROP_ARRAY:
            {
                size_t i = 0;
                success = ToAJMsgArg(arg, sig.c_str(), &value->arr, valueSignature, &i, 0);
                break;
            }
    }
    return success;
}

static OCRepPayloadPropType GetPropType(const ajn::MsgArg *arg, const qcc::String &minValue,
        const qcc::String &maxValue)
{
    OCRepPayloadPropType type = OCREP_PROP_NULL;
    if (arg->typeId == ajn::ALLJOYN_INT64)
    {
        int64_t min = INT64_MIN;
        int64_t max = INT64_MAX;
        if (!minValue.empty() && sscanf(minValue.c_str(), "%" SCNd64, &min) != 1)
        {
            goto exit;
        }
        if (!maxValue.empty() && sscanf(maxValue.c_str(), "%" SCNd64, &max) != 1)
        {
            goto exit;
        }
        if (min < MIN_SAFE_INTEGER || MAX_SAFE_INTEGER < max)
        {
            type = OCREP_PROP_STRING;
        }
    }
    else if (arg->typeId == ajn::ALLJOYN_UINT64)
    {
        uint64_t max = UINT64_MAX;
        if (!maxValue.empty() && sscanf(maxValue.c_str(), "%" SCNu64, &max) != 1)
        {
            goto exit;
        }
        if (MAX_SAFE_INTEGER < max)
        {
            type = OCREP_PROP_STRING;
        }
    }
exit:
    return type;
}

OCRepPayloadPropType GetPropType(const ajn::InterfaceDescription::Property *prop,
        const ajn::MsgArg *arg)
{
    qcc::String minValue, maxValue;
    prop->GetAnnotation("org.alljoyn.Bus.Type.Min", minValue);
    prop->GetAnnotation("org.alljoyn.Bus.Type.Max", maxValue);
    return GetPropType(arg, minValue, maxValue);
}

OCRepPayloadPropType GetPropType(const ajn::InterfaceDescription::Member *member,
        const char *argName, const ajn::MsgArg *arg)
{
    qcc::String minValue, maxValue;
    member->GetArgAnnotation(argName, "org.alljoyn.Bus.Type.Min", minValue);
    member->GetArgAnnotation(argName, "org.alljoyn.Bus.Type.Max", maxValue);
    return GetPropType(arg, minValue, maxValue);
}
