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

#include "UnitTest.h"

#include "Payload.h"
#include "ocpayload.h"
#include "oic_malloc.h"

/*
 * 7.2.1 Translation without aid of introspection
 */

TEST(Payload, Booleans)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* From D-Bus to OCF */
    bool value = true;
    ajn::MsgArg arg("b", value);
    EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
    bool ocValue = !value;
    EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "name", &ocValue));
    EXPECT_TRUE(ocValue);
    /* From OCF to D-Bus */
    ajn::MsgArg ajValue;
    EXPECT_TRUE(ToAJMsgArg(&ajValue, "b", payload->values));
    EXPECT_TRUE(arg == ajValue);
    OCRepPayloadDestroy(payload);
}

TEST(Payload, NumericTypes)
{
    /* From D-Bus to OCF */
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        uint8_t value = 1;
        ajn::MsgArg arg("y", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        uint16_t value = 1;
        ajn::MsgArg arg("n", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        uint32_t value = 1;
        ajn::MsgArg arg("u", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        uint64_t value = 1;
        ajn::MsgArg arg("t", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &ocValue));
        EXPECT_EQ((int64_t) value, ocValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        int16_t value = 1;
        ajn::MsgArg arg("q", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        int32_t value = 1;
        ajn::MsgArg arg("i", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        int64_t value = 1;
        ajn::MsgArg arg("x", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        double value = 1;
        ajn::MsgArg arg("d", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        double ocValue = !value;
        EXPECT_TRUE(OCRepPayloadGetPropDouble(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        OCRepPayloadDestroy(payload);
        /* From OCF to D-Bus */
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "d", payload->values));
        EXPECT_TRUE(arg == ajValue);
    }
}

TEST(Payload, TextStrings)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* From D-Bus to OCF */
    const char *value = "string";
    ajn::MsgArg arg("s", value);
    EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
    char *ocValue = NULL;
    EXPECT_TRUE(OCRepPayloadGetPropString(payload, "name", &ocValue));
    EXPECT_STREQ(value, ocValue);
    /* From OCF to D-Bus */
    ajn::MsgArg ajValue;
    EXPECT_TRUE(ToAJMsgArg(&ajValue, "s", payload->values));
    EXPECT_TRUE(arg == ajValue);
    OCRepPayloadDestroy(payload);
}

TEST(Payload, ByteArrays)
{
    OCRepPayload *payload = OCRepPayloadCreate();
    /* From D-Bus to OCF */
    uint8_t value[] = { 0, 1, 2 };
    ajn::MsgArg arg("ay", A_SIZEOF(value), value);
    EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
    OCByteString ocValue;
    EXPECT_TRUE(OCRepPayloadGetPropByteString(payload, "name", &ocValue));
    EXPECT_EQ(A_SIZEOF(value), ocValue.len);
    EXPECT_TRUE(!memcmp(value, ocValue.bytes, ocValue.len));
    /* From OCF to D-Bus */
    ajn::MsgArg ajValue;
    EXPECT_TRUE(ToAJMsgArg(&ajValue, "ay", payload->values));
    EXPECT_TRUE(arg == ajValue);
    OCRepPayloadDestroy(payload);
}

TEST(Payload, Variants)
{
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        /* From D-Bus to OCF */
        bool value = true;
        ajn::MsgArg arg("v", new ajn::MsgArg("b", value));
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        bool ocValue;
        EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        /* From OCF to D-Bus */
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "v", payload->values));
        EXPECT_TRUE(arg == ajValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        /* From D-Bus to OCF */
        bool value = true;
        ajn::MsgArg valueArg("v", new ajn::MsgArg("b", value));
        ajn::MsgArg arg("v", &valueArg);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        bool ocValue;
        EXPECT_TRUE(OCRepPayloadGetPropBool(payload, "name", &ocValue));
        EXPECT_EQ(value, ocValue);
        /* From OCF to D-Bus */
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "v", payload->values));
        /* Only one wrapping into a variant is done in this direction */
        EXPECT_TRUE(valueArg == ajValue);
        OCRepPayloadDestroy(payload);
    }
}

TEST(Payload, ObjectPathsAndSignatures)
{
    /* From D-Bus to OCF */
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        const char *value = "/path";
        ajn::MsgArg arg("o", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        char *ocValue = NULL;
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "name", &ocValue));
        EXPECT_STREQ(value, ocValue);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        const char *value = "signatur";
        ajn::MsgArg arg("g", value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        char *ocValue = NULL;
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "name", &ocValue));
        EXPECT_STREQ(value, ocValue);
    }
}

TEST(Payload, Structures)
{
    /* From D-Bus to OCF */
    OCRepPayload *payload = OCRepPayloadCreate();
    bool b = true;
    int32_t i = 1;
    const char *s = "string";
    ajn::MsgArg arg("(bis)", b, i, s);
    EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
    OCRepPayload *ocValue = NULL;
    EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "name", &ocValue));
    bool ocB;
    EXPECT_TRUE(OCRepPayloadGetPropBool(ocValue, "0", &ocB));
    EXPECT_EQ(b, ocB);
    int64_t ocI;
    EXPECT_TRUE(OCRepPayloadGetPropInt(ocValue, "1", &ocI));
    EXPECT_EQ(i, ocI);
    char *ocS;
    EXPECT_TRUE(OCRepPayloadGetPropString(ocValue, "2", &ocS));
    EXPECT_STREQ(s, ocS);
}

TEST(Payload, Arrays)
{
    /* 'ay' is covered by ByteArrays test above */
    /* 'ae' is covered by DictionariesObjects test below */

    /* From D-Bus 'a' of anything else to OCF array */
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        int32_t value[] = { -1, 0, 1 };
        ajn::MsgArg arg("ai", A_SIZEOF(value), value);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        int64_t *ocValue;
        size_t ocValueDim[MAX_REP_ARRAY_DEPTH];
        EXPECT_TRUE(OCRepPayloadGetIntArray(payload, "name", &ocValue, ocValueDim));
        EXPECT_EQ(A_SIZEOF(value), calcDimTotal(ocValueDim));
        for (size_t i = 0; i < A_SIZEOF(value); ++i)
        {
            EXPECT_EQ(value[i], ocValue[i]);
        }
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        ajn::MsgArg vs[2];
        vs[0].Set("v", new ajn::MsgArg("i", 1));
        vs[1].Set("v", new ajn::MsgArg("s", "string"));
        ajn::MsgArg av("av", A_SIZEOF(vs), vs);
        EXPECT_TRUE(ToOCPayload(payload, "name", &av, av.Signature().c_str()));
        OCRepPayload *ocValue;
        EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "name", &ocValue));
        int64_t ocI;
        EXPECT_TRUE(OCRepPayloadGetPropInt(ocValue, "0", &ocI));
        EXPECT_EQ(1, ocI);
        char *ocS;
        EXPECT_TRUE(OCRepPayloadGetPropString(ocValue, "1", &ocS));
        EXPECT_STREQ("string", ocS);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        ajn::MsgArg av("av", 0, NULL);
        EXPECT_TRUE(ToOCPayload(payload, "name", &av, av.Signature().c_str()));
        /* Empty arrays are decoded as OCREP_PROP_NULL (see IOT-2457) */
        EXPECT_TRUE(OCRepPayloadIsNull(payload, "name"));
        OCRepPayloadDestroy(payload);
    }

    {
        /* From OCF array, length = 0 to D-Bus 'av' */
        OCRepPayload *payload = OCRepPayloadCreate();
        int64_t *ocValue = NULL;
        size_t ocValueDim[MAX_REP_ARRAY_DEPTH] = { 0 };
        EXPECT_TRUE(OCRepPayloadSetIntArrayAsOwner(payload, "name", ocValue, ocValueDim));
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "av", payload->values));
        size_t numElements;
        ajn::MsgArg *elements;
        EXPECT_EQ(ER_OK, ajValue.Get("av", &numElements, &elements));
        EXPECT_EQ(0u, numElements);
        EXPECT_TRUE(NULL == elements);
        EXPECT_STREQ("v", ajValue.v_array.GetElemSig());
        OCRepPayloadDestroy(payload);
    }
    {
        /* From OCF array, length > 0, all elements of same type to D-Bus 'a' */
        OCRepPayload *payload = OCRepPayloadCreate();
        int64_t *ocValue = (int64_t *) OICMalloc(3 * sizeof(int64_t));
        ocValue[0] = -1;
        ocValue[1] = 0;
        ocValue[2] = 1;
        size_t ocValueDim[MAX_REP_ARRAY_DEPTH] = { 3, 0, 0 };
        EXPECT_TRUE(OCRepPayloadSetIntArrayAsOwner(payload, "name", ocValue, ocValueDim));
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "ai", payload->values));
        size_t numElements;
        int32_t *elements;
        EXPECT_EQ(ER_OK, ajValue.Get("ai", &numElements, &elements));
        EXPECT_EQ(3u, numElements);
        for (size_t i = 0; i < 3; ++i)
        {
            EXPECT_EQ(ocValue[i], elements[i]);
        }
        OCRepPayloadDestroy(payload);
    }
    {
        /* OCF array, length > 0, elements of different types to D-Bus 'r' */
        OCRepPayload *payload = OCRepPayloadCreate();
        OCRepPayload *ocValue = OCRepPayloadCreate();
        bool ocB = true;
        EXPECT_TRUE(OCRepPayloadSetPropBool(ocValue, "0", ocB));
        int64_t ocI = 1;
        EXPECT_TRUE(OCRepPayloadSetPropInt(ocValue, "1", ocI));
        const char *ocS = "string";
        EXPECT_TRUE(OCRepPayloadSetPropString(ocValue, "2", ocS));
        EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "name", ocValue));
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "(bis)", payload->values));
        bool b;
        int32_t i;
        const char *s;
        EXPECT_EQ(ER_OK, ajValue.Get("(bis)", &b, &i, &s));
        EXPECT_EQ(b, ocB);
        EXPECT_EQ(i, ocI);
        EXPECT_STREQ(s, ocS);
    }
    {
        /* OCF array, length > 0, elements of different types to D-Bus 'av' */
        OCRepPayload *payload = OCRepPayloadCreate();
        OCRepPayload *ocValue = OCRepPayloadCreate();
        bool ocB = true;
        EXPECT_TRUE(OCRepPayloadSetPropBool(ocValue, "0", ocB));
        int64_t ocI = 1;
        EXPECT_TRUE(OCRepPayloadSetPropInt(ocValue, "1", ocI));
        const char *ocS = "string";
        EXPECT_TRUE(OCRepPayloadSetPropString(ocValue, "2", ocS));
        EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "name", ocValue));
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "av", payload->values));
        ajn::MsgArg *entries;
        size_t numEntries;
        EXPECT_EQ(ER_OK, ajValue.Get("av", &numEntries, &entries));
        EXPECT_EQ(3u, numEntries);
        bool b;
        int32_t i;
        const char *s;
        ajn::MsgArg *entry;
        EXPECT_EQ(ER_OK, entries[0].Get("v", &entry));
        EXPECT_EQ(ER_OK, entry->Get("b", &b));
        EXPECT_EQ(ER_OK, entries[1].Get("v", &entry));
        EXPECT_EQ(ER_OK, entry->Get("i", &i));
        EXPECT_EQ(ER_OK, entries[2].Get("v", &entry));
        EXPECT_EQ(ER_OK, entry->Get("s", &s));
        EXPECT_EQ(b, ocB);
        EXPECT_EQ(i, ocI);
        EXPECT_STREQ(s, ocS);
    }

    /* Array conversions of more types are tested seperately below */
}

TEST(Payload, DictionariesObjects)
{
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        /* From D-Bus a{sv} to OCF object */
        bool b = true;
        int32_t i = 1;
        const char *s = "string";
        ajn::MsgArg dict[3];
        EXPECT_EQ(ER_OK, dict[0].Set("{sv}", "b", new ajn::MsgArg("b", b)));
        EXPECT_EQ(ER_OK, dict[1].Set("{sv}", "i", new ajn::MsgArg("i", i)));
        EXPECT_EQ(ER_OK, dict[2].Set("{sv}", "s", new ajn::MsgArg("s", s)));
        ajn::MsgArg arg("a{sv}", 3, dict);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        OCRepPayload *ocValue = NULL;
        EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "name", &ocValue));
        bool ocB;
        EXPECT_TRUE(OCRepPayloadGetPropBool(ocValue, "b", &ocB));
        EXPECT_EQ(b, ocB);
        int64_t ocI;
        EXPECT_TRUE(OCRepPayloadGetPropInt(ocValue, "i", &ocI));
        EXPECT_EQ(i, ocI);
        char *ocS;
        EXPECT_TRUE(OCRepPayloadGetPropString(ocValue, "s", &ocS));
        EXPECT_STREQ(s, ocS);
        /* From OCF object to D-Bus a{sv} */
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "a{sv}", payload->values));
        EXPECT_TRUE(arg == ajValue);
        OCRepPayloadDestroy(payload);
    }
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        /* From D-Bus not a{sv} to OCF object */
        bool b = true;
        int32_t i = 1;
        const char *s = "string";
        ajn::MsgArg dict[3];
        EXPECT_EQ(ER_OK, dict[0].Set("{iv}", 10, new ajn::MsgArg("b", b)));
        EXPECT_EQ(ER_OK, dict[1].Set("{iv}", 20, new ajn::MsgArg("i", i)));
        EXPECT_EQ(ER_OK, dict[2].Set("{iv}", 30, new ajn::MsgArg("s", s)));
        ajn::MsgArg arg("a{iv}", 3, dict);
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        OCRepPayload *ocValue = NULL;
        EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "name", &ocValue));
        bool ocB;
        EXPECT_TRUE(OCRepPayloadGetPropBool(ocValue, "10", &ocB));
        EXPECT_EQ(b, ocB);
        int64_t ocI;
        EXPECT_TRUE(OCRepPayloadGetPropInt(ocValue, "20", &ocI));
        EXPECT_EQ(i, ocI);
        char *ocS;
        EXPECT_TRUE(OCRepPayloadGetPropString(ocValue, "30", &ocS));
        EXPECT_STREQ(s, ocS);
        /* From OCF object to D-Bus a{sv} */
        ajn::MsgArg ajValue;
        EXPECT_TRUE(ToAJMsgArg(&ajValue, "a{sv}", payload->values));
        ajn::MsgArg ajDict[3];
        EXPECT_EQ(ER_OK, ajDict[0].Set("{sv}", "10", new ajn::MsgArg("b", b)));
        EXPECT_EQ(ER_OK, ajDict[1].Set("{sv}", "20", new ajn::MsgArg("i", i)));
        EXPECT_EQ(ER_OK, ajDict[2].Set("{sv}", "30", new ajn::MsgArg("s", s)));
        ajn::MsgArg ajArg("a{sv}", 3, ajDict);
        EXPECT_TRUE(ajArg == ajValue);
        OCRepPayloadDestroy(payload);
    }
}

TEST(Payload, NonTranslatableTypes)
{
    /* From D-Bus to OCF */
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        qcc::SocketFd value = 1;
        ajn::MsgArg arg("h", value);
        EXPECT_FALSE(ToOCPayload(payload, "name", &arg, arg.Signature().c_str()));
        OCRepPayloadDestroy(payload);
    }
    /* From OCF to D-Bus */
    {
        OCRepPayload *payload = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetNull(payload, "name"));
        ajn::MsgArg ajValue;
        EXPECT_FALSE(ToAJMsgArg(&ajValue, "b", payload->values));
        OCRepPayloadDestroy(payload);
    }
    /* 'undefined' cannot be represented in OCRepPayload */
}

bool operator==(const OCRepPayloadValue &a, const OCRepPayloadValue &b);

bool operator==(const OCRepPayload &a, const OCRepPayload &b)
{
    bool isEqual = true;
    OCRepPayloadValue *pa = NULL;
    OCRepPayloadValue *pb = NULL;
    for (pa = a.values, pb = b.values; pa && pb; pa = pa->next, pb = pb->next)
    {
        isEqual = isEqual && (*pa == *pb);
    }
    isEqual = isEqual && !pa && !pb;
    return isEqual;
}

bool operator==(const OCRepPayloadValue &a, const OCRepPayloadValue &b)
{
    bool isEqual = !strcmp(a.name, b.name) && (a.type == b.type);
    if (isEqual)
    {
        switch (a.type)
        {
            case OCREP_PROP_NULL:
                break;
            case OCREP_PROP_INT:
                isEqual = !memcmp(&a.i, &b.i, sizeof(a.i));
                break;
            case OCREP_PROP_DOUBLE:
                isEqual = !memcmp(&a.d, &b.d, sizeof(a.d));
                break;
            case OCREP_PROP_BOOL:
                isEqual = !memcmp(&a.b, &b.b, sizeof(a.b));
                break;
            case OCREP_PROP_STRING:
                isEqual = !strcmp(a.str, b.str);
                break;
            case OCREP_PROP_BYTE_STRING:
                isEqual = (a.ocByteStr.len == b.ocByteStr.len) &&
                        !memcmp(a.ocByteStr.bytes, b.ocByteStr.bytes, a.ocByteStr.len);
                break;
            case OCREP_PROP_OBJECT:
                isEqual = a.obj && b.obj && (*a.obj == *b.obj);
                break;
            case OCREP_PROP_ARRAY:
                {
                    isEqual = (a.arr.type == b.arr.type) &&
                            !memcmp(a.arr.dimensions, b.arr.dimensions, sizeof(a.arr.dimensions));
                    size_t dimTotal = calcDimTotal(a.arr.dimensions);
                    switch (a.arr.type)
                    {
                        case OCREP_PROP_INT:
                            isEqual = isEqual && !memcmp(a.arr.iArray, b.arr.iArray,
                                    sizeof(int64_t) * dimTotal);
                            break;
                        case OCREP_PROP_DOUBLE:
                            isEqual = isEqual && !memcmp(a.arr.dArray, b.arr.dArray,
                                    sizeof(double) * dimTotal);
                            break;
                        case OCREP_PROP_BOOL:
                            isEqual = isEqual && !memcmp(a.arr.bArray, b.arr.bArray,
                                    sizeof(bool) * dimTotal);
                            break;
                        case OCREP_PROP_STRING:
                            for (size_t i = 0; i < dimTotal; ++i)
                            {
                                isEqual = isEqual && !strcmp(a.arr.strArray[i], b.arr.strArray[i]);
                            }
                            break;
                        case OCREP_PROP_BYTE_STRING:
                            for (size_t i = 0; i < dimTotal; ++i)
                            {
                                isEqual = isEqual &&
                                        (a.arr.ocByteStrArray[i].len == b.arr.ocByteStrArray[i].len) &&
                                        !memcmp(a.arr.ocByteStrArray[i].bytes, b.arr.ocByteStrArray[i].bytes,
                                                a.arr.ocByteStrArray[i].len);
                            }
                            break;
                        case OCREP_PROP_OBJECT:
                            for (size_t i = 0; i < dimTotal; ++i)
                            {
                                isEqual = isEqual && a.arr.objArray[i] && b.arr.objArray[i] &&
                                        (*a.arr.objArray[i] == *b.arr.objArray[i]);
                            }
                            break;
                        default:
                            isEqual = false;
                            break;
                    }
                    break;
                }
        }
    }
    return isEqual;
}

static void PrintTo(const OCRepPayloadValue& a, ::std::ostream* os);

static void PrintTo(const OCByteString& ocByteStr, ::std::ostream* os)
{
    std::ios fmt(nullptr);
    fmt.copyfmt(*os);
    *os << std::hex << std::setw(2) << std::setfill('0');
    for (size_t i = 0; i < ocByteStr.len; ++i)
    {
        if (i != 0)
        {
            *os << " ";
        }
        *os << "0x" << (int) ocByteStr.bytes[i];
    }
    os->copyfmt(fmt);
}

static void PrintTo(const OCRepPayload *obj, ::std::ostream* os)
{
    if (obj)
    {
        *os << "{";
        for (OCRepPayloadValue *value = obj->values; value; value = value->next)
        {
            if (value != obj->values)
            {
                *os << ",";
            }
            PrintTo(*value, os);
        }
        *os << "}";
    }
    else
    {
        *os << "{}";
    }
}

static void PrintTo(const OCRepPayloadValueArray& a, size_t i, ::std::ostream* os)
{
    switch (a.type)
    {
        case OCREP_PROP_INT:
            *os << a.iArray[i];
            break;
        case OCREP_PROP_DOUBLE:
            *os << a.dArray[i];
            break;
        case OCREP_PROP_BOOL:
            *os << a.bArray[i];
            break;
        case OCREP_PROP_STRING:
            *os << "\"" << a.strArray[i] << "\"";
            break;
        case OCREP_PROP_BYTE_STRING:
            PrintTo(a.ocByteStrArray[i], os);
            break;
        case OCREP_PROP_OBJECT:
            PrintTo(a.objArray[i], os);
            break;
        default:
            break;
    }
    *os << " ";
}

static void PrintTo(std::string indent, const OCRepPayloadValue& a, ::std::ostream* os)
{
    static const char *typeStr[] = { "NULL", "INT", "DOUBLE", "BOOL", "STRING", "BYTE_STRING",
                                     "OBJECT", "ARRAY" };
    *os << indent << "\"" << a.name << "\": " << typeStr[a.type] << " ";
    switch (a.type)
    {
        case OCREP_PROP_NULL:
            *os << "null";
            break;
        case OCREP_PROP_INT:
            *os << a.i;
            break;
        case OCREP_PROP_DOUBLE:
            *os << a.d;
            break;
        case OCREP_PROP_BOOL:
            *os << (a.b ? "true" : "false");
            break;
        case OCREP_PROP_STRING:
            *os << a.str;
            break;
        case OCREP_PROP_BYTE_STRING:
            PrintTo(a.ocByteStr, os);
            break;
        case OCREP_PROP_OBJECT:
            PrintTo(a.obj, os);
            break;
        case OCREP_PROP_ARRAY:
            {
                size_t i = 0;
                *os << typeStr[a.arr.type] << "[ ";
                for (size_t x = 0; x < a.arr.dimensions[0]; ++x)
                {
                    if (a.arr.dimensions[1])
                    {
                        *os << "[ ";
                        for (size_t y = 0; y < a.arr.dimensions[1]; ++y)
                        {
                            if (a.arr.dimensions[2])
                            {
                                *os << "[ ";
                                for (size_t z = 0; z < a.arr.dimensions[2]; ++z)
                                {
                                    PrintTo(a.arr, i++, os);
                                }
                                *os << "] ";
                            }
                            else
                            {
                                PrintTo(a.arr, i++, os);
                            }
                        }
                        *os << "] ";
                    }
                    else
                    {
                        PrintTo(a.arr, i++, os);
                    }
                }
                *os << "]";
                break;
            }
    }
}

static void PrintTo(const OCRepPayloadValue& a, ::std::ostream* os)
{
    PrintTo("", a, os);
}

static class ExampleData
{
public:
    static const uint8_t ay0[];
    static const uint8_t ay1[];
    static const double *av;
    static const double ad0[];
    static OCRepPayload *ddbs;
    static OCRepPayload *aesd;
    static OCRepPayload *aesv;
    static ajn::MsgArg *aesvArg;
    ExampleData();
} exampleData;

const uint8_t ExampleData::ay0[] = { };
const uint8_t ExampleData::ay1[] = { 0x48, 0x65, 0x6c, 0x6c, 0x6f };
const double *ExampleData::av = NULL;
const double ExampleData::ad0[] = { 1.0 };
OCRepPayload *ExampleData::ddbs = NULL;
OCRepPayload *ExampleData::aesd = NULL;
OCRepPayload *ExampleData::aesv = NULL;
ajn::MsgArg *ExampleData::aesvArg = NULL;

ExampleData::ExampleData()
{
    if (!ddbs)
    {
        ddbs = OCRepPayloadCreate();
        OCRepPayloadSetPropInt(ddbs, "0", 1);
        OCRepPayloadSetPropInt(ddbs, "1", 2147483648);
        OCRepPayloadSetPropBool(ddbs, "2", false);
        OCRepPayloadSetPropString(ddbs, "3", "Hello");
    }
    if (!aesd)
    {
        aesd = OCRepPayloadCreate();
        OCRepPayloadSetPropDouble(aesd, "1", 1);
    }
    if (!aesv)
    {
        aesv = OCRepPayloadCreate();
        OCRepPayload *rep = OCRepPayloadCreate();
        OCRepPayloadSetPropBool(rep, "state", false);
        OCRepPayloadSetPropDouble(rep, "power", 1.0);
        OCRepPayloadSetPropString(rep, "name", "My Light");
        OCRepPayloadSetPropObject(aesv, "rep", rep);

        ajn::MsgArg nestedDict[3];
        nestedDict[0].Set("{sv}", "state", new ajn::MsgArg("b", false));
        nestedDict[1].Set("{sv}", "power", new ajn::MsgArg("d", 1.0));
        nestedDict[2].Set("{sv}", "name", new ajn::MsgArg("s", "My Light"));
        ajn::MsgArg dict[1];
        dict[0].Set("{sv}", "rep", new ajn::MsgArg("a{sv}", 3, nestedDict));
        aesvArg = new ajn::MsgArg("a{sv}", 1, dict);
        aesvArg->Stabilize();
    }
}

Row::Row(ajn::MsgArg arg, int64_t i)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_INT;
    m_value.i = i;
}
Row::Row(ajn::MsgArg arg, double d)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_DOUBLE;
    m_value.d = d;
}
Row::Row(ajn::MsgArg arg, bool b)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_BOOL;
    m_value.b = b;
}
Row::Row(ajn::MsgArg arg, const char *str)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_STRING;
    m_value.str = (char *) str;
}
Row::Row(ajn::MsgArg arg, size_t len, const uint8_t *bytes)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_BYTE_STRING;
    m_value.ocByteStr.bytes = (uint8_t *) bytes;
    m_value.ocByteStr.len = len;
}
Row::Row(ajn::MsgArg arg, const OCRepPayload *payload)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_OBJECT;
    m_value.obj = OCRepPayloadClone(payload);
}
Row::Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const int64_t *iArray)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_ARRAY;
    m_value.arr.type = OCREP_PROP_INT;
    m_value.arr.dimensions[0] = d0;
    m_value.arr.dimensions[1] = d1;
    m_value.arr.dimensions[2] = d2;
    m_value.arr.iArray = (int64_t *) iArray;
}
Row::Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const double *dArray)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_ARRAY;
    m_value.arr.type = OCREP_PROP_DOUBLE;
    m_value.arr.dimensions[0] = d0;
    m_value.arr.dimensions[1] = d1;
    m_value.arr.dimensions[2] = d2;
    m_value.arr.dArray = (double *) dArray;
}
Row::Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const bool *bArray)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_ARRAY;
    m_value.arr.type = OCREP_PROP_BOOL;
    m_value.arr.dimensions[0] = d0;
    m_value.arr.dimensions[1] = d1;
    m_value.arr.dimensions[2] = d2;
    m_value.arr.bArray = (bool *) bArray;
}
Row::Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const char **strArray)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_ARRAY;
    m_value.arr.type = OCREP_PROP_STRING;
    m_value.arr.dimensions[0] = d0;
    m_value.arr.dimensions[1] = d1;
    m_value.arr.dimensions[2] = d2;
    m_value.arr.strArray = (char **) strArray;
}
Row::Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const uint8_t *bytes)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    size_t len;
    if (d2)
    {
        len = d2;
        d2 = 0;
    }
    else if (d1)
    {
        len = d1;
        d1 = 0;
    }
    else if (d0)
    {
        len = d0;
        d0 = 0;
    }
    m_value.arr.dimensions[0] = d0;
    m_value.arr.dimensions[1] = d1;
    m_value.arr.dimensions[2] = d2;
    size_t dimTotal = calcDimTotal(m_value.arr.dimensions);
    if (dimTotal)
    {
        m_value.type = OCREP_PROP_ARRAY;
        m_value.arr.type = OCREP_PROP_BYTE_STRING;
        m_value.arr.ocByteStrArray = (OCByteString*) OICCalloc(dimTotal, sizeof(OCByteString));
        for (size_t i = 0; i < dimTotal; ++i)
        {
            m_value.arr.ocByteStrArray[i].bytes = (uint8_t *) bytes;
            m_value.arr.ocByteStrArray[i].len = len;
        }
    }
    else
    {
        m_value.type = OCREP_PROP_BYTE_STRING;
        m_value.ocByteStr.bytes = (uint8_t *) bytes;
        m_value.ocByteStr.len = len;
    }
}
Row::Row(ajn::MsgArg arg, size_t d0, size_t d1, size_t d2, const OCRepPayload *payload)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_ARRAY;
    m_value.arr.type = OCREP_PROP_OBJECT;
    m_value.arr.dimensions[0] = d0;
    m_value.arr.dimensions[1] = d1;
    m_value.arr.dimensions[2] = d2;
    size_t dimTotal = calcDimTotal(m_value.arr.dimensions);
    m_value.arr.objArray = (OCRepPayload**) OICCalloc(dimTotal, sizeof(OCRepPayload*));
    for (size_t i = 0; i < dimTotal; ++i)
    {
        m_value.arr.objArray[i] = OCRepPayloadClone(payload);
    }
}
Row::Row(ajn::MsgArg arg, uint8_t y)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_INT;
    m_value.i = y;
}
Row::Row(ajn::MsgArg arg, uint64_t t)
{
    m_arg = arg;
    memset(&m_value, 0, sizeof(m_value));
    m_value.name = (char *) "name";
    m_value.type = OCREP_PROP_DOUBLE;
    m_value.d = t;
}

static void PrintTo(const Row& a, ::std::ostream* os)
{
    *os << "\n" << a.m_arg.ToString() << "\n";
    PrintTo(a.m_value, os);
}

TEST_P(FromDBus, ToOC)
{
    Row row = GetParam();
    OCRepPayload *payload = OCRepPayloadCreate();
    EXPECT_TRUE(ToOCPayload(payload, "name", &row.m_arg, row.m_arg.Signature().c_str()));
    ASSERT_TRUE(payload->values != NULL);
    EXPECT_EQ(row.m_value, *payload->values);
    OCRepPayloadDestroy(payload);
}

INSTANTIATE_TEST_CASE_P(PayloadExamples, FromDBus, ::testing::Values(\
            Row(ajn::MsgArg("b", false), false),
            Row(ajn::MsgArg("b", true), true),
            Row(ajn::MsgArg("v", new ajn::MsgArg("b", false)), false),
            Row(ajn::MsgArg("v", new ajn::MsgArg("b", true)), true),
            Row(ajn::MsgArg("y", 0), (int64_t) 0),
            Row(ajn::MsgArg("y", 255), (int64_t) 255),
            Row(ajn::MsgArg("n", 0), (int64_t) 0),
            Row(ajn::MsgArg("n", -1), (int64_t) -1),
            Row(ajn::MsgArg("n", -32768), (int64_t) -32768),
            Row(ajn::MsgArg("q", 0), (int64_t) 0),
            Row(ajn::MsgArg("q", 65535), (int64_t) 65535),
            Row(ajn::MsgArg("i", 0), (int64_t) 0),
            Row(ajn::MsgArg("i", -2147483648), (int64_t) -2147483648),
            Row(ajn::MsgArg("i", 2147483647), (int64_t) 2147483647),
            Row(ajn::MsgArg("u", 0), (int64_t) 0),
            Row(ajn::MsgArg("u", 4294967295), (int64_t) 4294967295),
            Row(ajn::MsgArg("x", 0), (int64_t) 0),
            Row(ajn::MsgArg("x", (int64_t) -1), (int64_t) -1),
            Row(ajn::MsgArg("t", 18446744073709551615u), 18446744073709551615u),
            Row(ajn::MsgArg("d", 0.0), 0.0),
            Row(ajn::MsgArg("d", 0.5), 0.5),
            Row(ajn::MsgArg("s", ""), ""),
            Row(ajn::MsgArg("s", "Hello"), "Hello"),
            Row(ajn::MsgArg("ay", A_SIZEOF(ExampleData::ay0), ExampleData::ay0),
                    A_SIZEOF(ExampleData::ay0), ExampleData::ay0),
            Row(ajn::MsgArg("ay", A_SIZEOF(ExampleData::ay1), ExampleData::ay1),
                    A_SIZEOF(ExampleData::ay1), ExampleData::ay1),
            Row(ajn::MsgArg("o", "/"), "/"),
            Row(ajn::MsgArg("g", ""), ""),
            Row(ajn::MsgArg("g", "s"), "s"),
            Row(ajn::MsgArg("v", new ajn::MsgArg("i", 0)), (int64_t) 0),
            Row(ajn::MsgArg("v", new ajn::MsgArg("v", new ajn::MsgArg("i", 0))), (int64_t) 0),
            Row(ajn::MsgArg("v", new ajn::MsgArg("s", "Hello")), "Hello")
        ));

TEST_P(FromOC, ToDBus)
{
    Row row = GetParam();
    ajn::MsgArg arg;
    EXPECT_TRUE(ToAJMsgArg(&arg, row.m_arg.Signature().c_str(), &row.m_value));
    EXPECT_TRUE(row.m_arg == arg);
}

INSTANTIATE_TEST_CASE_P(PayloadExamples, FromOC, ::testing::Values(\
            Row(ajn::MsgArg("b", false), false),
            Row(ajn::MsgArg("b", true), true),
            Row(ajn::MsgArg("d", 0.0), (int64_t) 0),
            Row(ajn::MsgArg("d", -1.0), (int64_t) -1),
            Row(ajn::MsgArg("d", -2147483648.0), (int64_t) -2147483648),
            Row(ajn::MsgArg("d", 2147483647.0), (int64_t) 2147483647),
            Row(ajn::MsgArg("d", 2147483648.0), (int64_t) 2147483648),
            Row(ajn::MsgArg("d", -2147483649.0), (int64_t) -2147483649),
            Row(ajn::MsgArg("d", 9223372036854775808.0), 9223372036854775808u),
            Row(ajn::MsgArg("d", 0.0), 0.0),
            Row(ajn::MsgArg("d", 0.5), 0.5),
            /* 0.0f, 0.5f values are no different from above two tests */
            Row(ajn::MsgArg("s", ""), ""),
            Row(ajn::MsgArg("s", "Hello"), "Hello"),
            Row(ajn::MsgArg("av", 0, ExampleData::av),
                    0, 0, 0, ExampleData::av),
            Row(ajn::MsgArg("ad", A_SIZEOF(ExampleData::ad0), ExampleData::ad0),
                    A_SIZEOF(ExampleData::ad0), 0, 0, ExampleData::ad0),
            Row(ajn::MsgArg("(ddbs)", 1.0, 2147483648.0, false, "Hello"), ExampleData::ddbs),
            Row(ajn::MsgArg("a{sv}", 0, NULL), (OCRepPayload *) NULL),
            /* {1: 1} cannot be represented in OCRepPayload (numeric property name) */
            Row(ajn::MsgArg("a{sv}", 1, new ajn::MsgArg("{sv}", "1", new ajn::MsgArg("d", 1.0))),
                    ExampleData::aesd),
            Row(*ExampleData::aesvArg, ExampleData::aesv)
        ));

/*
 * 7.2.2 Translation with aid of introspection
 */

TEST(Payload, NumericTypesWithIntrospection)
{
    /* From OCF to D-Bus */
    {
        /* Basic types */
        ajn::MsgArg arg;
        OCRepPayload *payload = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", -1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "y", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", UINT8_MAX + 1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "y", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", 1));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "y"));
        EXPECT_STREQ("y", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", -1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "q", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", UINT16_MAX + 1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "q", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", 1));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "q"));
        EXPECT_STREQ("q", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", INT16_MIN - 1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "n", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", INT16_MAX + 1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "n", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", -1));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "n"));
        EXPECT_STREQ("n", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", -1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "u", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", (int64_t)UINT32_MAX + 1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "u", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", 1));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "u"));
        EXPECT_STREQ("u", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", (int64_t)INT32_MIN - 1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "i", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", (int64_t)INT32_MAX + 1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "i", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", -1));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "i"));
        EXPECT_STREQ("i", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", -1));
        EXPECT_FALSE(ToAJMsgArg(&arg, "t", payload->values));
        EXPECT_TRUE(OCRepPayloadSetPropInt(payload, "name", 1));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "t"));
        EXPECT_STREQ("t", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "name", "1"));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "t"));
        EXPECT_STREQ("t", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "name", "-1"));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "x"));
        EXPECT_STREQ("x", arg.v_variant.val->Signature().c_str());

        /* Arrays */
        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            int64_t ltArray[] = { -1 };
            int64_t gtArray[] = { UINT8_MAX + 1 };
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", ltArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "ay"));
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", gtArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "ay"));
        }
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
        int64_t iArray[] = { -1, 1 };
        int64_t uArray[] = { 0, 1 };
        EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", uArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "ay"));
        EXPECT_STREQ("ay", arg.v_variant.val->Signature().c_str());

        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            int64_t ltArray[] = { -1 };
            int64_t gtArray[] = { UINT16_MAX + 1 };
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", ltArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "aq"));
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", gtArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "aq"));
        }
        EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", uArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "aq"));
        EXPECT_STREQ("aq", arg.v_variant.val->Signature().c_str());

        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            int64_t ltArray[] = { INT16_MIN - 1 };
            int64_t gtArray[] = { INT16_MAX + 1 };
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", ltArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "an"));
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", gtArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "an"));
        }
        EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", iArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "an"));
        EXPECT_STREQ("an", arg.v_variant.val->Signature().c_str());

        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            int64_t ltArray[] = { -1 };
            int64_t gtArray[] = { (int64_t)UINT32_MAX + 1 };
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", ltArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "au"));
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", gtArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "au"));
        }
        EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", uArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "au"));
        EXPECT_STREQ("au", arg.v_variant.val->Signature().c_str());

        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            int64_t ltArray[] = { (int64_t)INT32_MIN - 1 };
            int64_t gtArray[] = { (int64_t)INT32_MAX + 1 };
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", ltArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "ai"));
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", gtArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "ai"));
        }
        EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", iArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "ai"));
        EXPECT_STREQ("ai", arg.v_variant.val->Signature().c_str());

        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            int64_t ltArray[] = { -1 };
            EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", ltArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "at"));
        }
        EXPECT_TRUE(OCRepPayloadSetIntArray(payload, "name", uArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "at"));
        EXPECT_STREQ("at", arg.v_variant.val->Signature().c_str());

        {
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            const char *ltArray[] = { "-1" };
            EXPECT_TRUE(OCRepPayloadSetStringArray(payload, "name", ltArray, dim));
            EXPECT_FALSE(ToAJMsgArg(&arg, "v", payload->values, "at"));
        }
        const char *uArrayStr[] = { "0", "1" };
        EXPECT_TRUE(OCRepPayloadSetStringArray(payload, "name", uArrayStr, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "at"));
        EXPECT_STREQ("at", arg.v_variant.val->Signature().c_str());

        const char *iArrayStr[] = { "-1", "1" };
        EXPECT_TRUE(OCRepPayloadSetStringArray(payload, "name", iArrayStr, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "ax"));
        EXPECT_STREQ("ax", arg.v_variant.val->Signature().c_str());

        OCRepPayloadDestroy(payload);
    }
    /* From D-Bus to OCF */
    {
        /* ynquid translations are tested in Payload.NumericTypes */

        OCRepPayload *payload = OCRepPayloadCreate();

        int64_t i;
        char *s;
        ajn::MsgArg arg("t", 1);
        EXPECT_TRUE(ToOCPayload(payload, "name", OCREP_PROP_INT, &arg, arg.Signature().c_str()));
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &i));
        EXPECT_EQ(1, i);

        EXPECT_TRUE(ToOCPayload(payload, "name", OCREP_PROP_STRING, &arg, arg.Signature().c_str()));
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "name", &s));
        EXPECT_STREQ("1", s);

        arg = ajn::MsgArg("x", 1);
        EXPECT_TRUE(ToOCPayload(payload, "name", OCREP_PROP_INT, &arg, arg.Signature().c_str()));
        EXPECT_TRUE(OCRepPayloadGetPropInt(payload, "name", &i));
        EXPECT_EQ(1, i);

        EXPECT_TRUE(ToOCPayload(payload, "name", OCREP_PROP_STRING, &arg, arg.Signature().c_str()));
        EXPECT_TRUE(OCRepPayloadGetPropString(payload, "name", &s));
        EXPECT_STREQ("1", s);

        OCRepPayloadDestroy(payload);
    }
}

TEST(Payload, TextStringsAndByteArraysWithIntrospection)
{
    /* From OCF to D-Bus */
    {
        /* Basic types */
        ajn::MsgArg arg;
        OCRepPayload *payload = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "name", "string"));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "s"));
        EXPECT_STREQ("s", arg.v_variant.val->Signature().c_str());

        uint8_t bytes[] = { 1, 2, 3 };
        OCByteString byteString = { bytes, A_SIZEOF(bytes) };
        EXPECT_TRUE(OCRepPayloadSetPropByteString(payload, "name", byteString));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "ay"));
        EXPECT_STREQ("ay", arg.v_variant.val->Signature().c_str());

        /* Arrays */
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
        const char *strArray[] = { "zero", "one" };
        EXPECT_TRUE(OCRepPayloadSetStringArray(payload, "name", strArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "as"));
        EXPECT_STREQ("as", arg.v_variant.val->Signature().c_str());

        OCByteString ocByteStrArray[] = { byteString, byteString };
        EXPECT_TRUE(OCRepPayloadSetByteStringArray(payload, "name", ocByteStrArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "aay"));
        EXPECT_STREQ("aay", arg.v_variant.val->Signature().c_str());
    }
    /* From D-Bus to OCF */
    {
        /* s,ay tested in Payload.TextStrings,Payload.ByteArrays */
    }
}

/* VariantWithIntrospection tested in OCFResource.VariantTypes */

TEST(Payload, ObjectPathsAndSignaturesWithIntrospection)
{
    /* From OCF to D-Bus */
    {
        /* Basic types */
        ajn::MsgArg arg;
        OCRepPayload *payload = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "name", "/path"));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "o"));
        EXPECT_STREQ("o", arg.v_variant.val->Signature().c_str());

        EXPECT_TRUE(OCRepPayloadSetPropString(payload, "name", "sig"));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "g"));
        EXPECT_STREQ("g", arg.v_variant.val->Signature().c_str());

        /* Arrays */
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
        const char *oArray[] = { "/zero", "/one" };
        EXPECT_TRUE(OCRepPayloadSetStringArray(payload, "name", oArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "ao"));
        EXPECT_STREQ("ao", arg.v_variant.val->Signature().c_str());

        const char *gArray[] = { "sig", "nat" };
        EXPECT_TRUE(OCRepPayloadSetStringArray(payload, "name", gArray, dim));
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "ag"));
        EXPECT_STREQ("ag", arg.v_variant.val->Signature().c_str());
    }
    /* From D-Bus to OCF */
    {
        /* o,g tested in Payload.ObjectPathsAndSignatures */
    }
}

TEST(Payload, StructuresWithIntrospection)
{
    Types::m_structs["[Struct]"].push_back(Types::Field("int", "i"));
    Types::m_structs["[Struct]"].push_back(Types::Field("string", "s"));

    /* From OCF to D-Bus */
    {
        /* Basic type */
        OCRepPayload *payload = OCRepPayloadCreate();
        OCRepPayload *obj = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(obj, "string", "string"));
        EXPECT_TRUE(OCRepPayloadSetPropInt(obj, "int", 1));
        EXPECT_TRUE(OCRepPayloadSetPropObject(payload, "name", obj));

        ajn::MsgArg arg;
        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "[Struct]"));
        EXPECT_STREQ("(is)", arg.v_variant.val->Signature().c_str());

        OCRepPayloadDestroy(payload);

        /* Array */
        payload = OCRepPayloadCreate();
        size_t dim[MAX_REP_ARRAY_DEPTH] = { 2, 0, 0 };
        OCRepPayload *objArray[2];
        objArray[0] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(objArray[0], "string", "string0"));
        EXPECT_TRUE(OCRepPayloadSetPropInt(objArray[0], "int", 0));
        objArray[1] = OCRepPayloadCreate();
        EXPECT_TRUE(OCRepPayloadSetPropString(objArray[1], "string", "string1"));
        EXPECT_TRUE(OCRepPayloadSetPropInt(objArray[1], "int", 1));
        EXPECT_TRUE(OCRepPayloadSetPropObjectArray(payload, "name",
                (const OCRepPayload **) objArray, dim));

        EXPECT_TRUE(ToAJMsgArg(&arg, "v", payload->values, "a[Struct]"));
        EXPECT_STREQ("a(is)", arg.v_variant.val->Signature().c_str());

        OCRepPayloadDestroy(payload);
    }
    /* From D-Bus to OCF */
    {
        ajn::MsgArg arg("(is)", 1, "string");

        OCRepPayload *payload = OCRepPayloadCreate();
        EXPECT_TRUE(ToOCPayload(payload, "name", &arg, "[Struct]"));
        int64_t i;
        char *s;
        OCRepPayload *obj;
        EXPECT_TRUE(OCRepPayloadGetPropObject(payload, "name", &obj));
        EXPECT_TRUE(OCRepPayloadGetPropInt(obj, "int", &i));
        EXPECT_EQ(1, i);
        EXPECT_TRUE(OCRepPayloadGetPropString(obj, "string", &s));
        EXPECT_STREQ("string", s);

        OCRepPayloadDestroy(payload);
    }
}

/* ArraysAndDictionariesWithIntrospection tested in OCFResource.VariantTypes */
