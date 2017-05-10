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

#include "AboutData.h"

#include "Payload.h"
#include "Plugin.h"
#include "Signature.h"
#include "oic_malloc.h"

static void ToAppId(const char *di, uint8_t *appId)
{
    memset(appId, 0, 16);
    for (size_t i = 0; (i < 16) && *di; ++i)
    {
        if (*di == '-') ++di;
        for (size_t j = 0; (j < 2) && *di; ++j, ++di)
        {
            if ('0' <= *di && *di <= '9')
            {
                appId[i] = (appId[i] << 4) | (*di - '0');
            }
            else if ('a' <= *di && *di <= 'f')
            {
                appId[i] = (appId[i] << 4) | (*di - 'a' + 10);
            }
            else if ('A' <= *di && *di <= 'F')
            {
                appId[i] = (appId[i] << 4) | (*di - 'A' + 10);
            }
        }
    }
}

void AboutData::SetVendorFields(OCRepPayload *payload)
{
    for (OCRepPayloadValue *value = payload->values; value; value = value->next)
    {
        if (!strncmp(value->name, "x.", 2))
        {
            const char *fieldName = value->name + 2; /* Skip the leading x. */
            char fieldSig[] = "aaaa{sv}";
            CreateSignature(fieldSig, value); // TODO get this from introspection data when available
            ajn::MsgArg fieldValue;
            ToAJMsgArg(&fieldValue, fieldSig, value);
            SetNewFieldDetails(fieldName, 0, fieldSig);
            SetField(fieldName, fieldValue);
        }
    }
}

AboutData::AboutData(const char *defaultLanguage)
    : m_n(NULL)
{
    SetNewFieldDetails("org.openconnectivity.piid", ANNOUNCED, "s");
    SetNewFieldDetails("org.openconnectivity.mnfv", 0, "s");
    SetNewFieldDetails("org.openconnectivity.mnml", 0, "s");
    SetNewFieldDetails("org.openconnectivity.mnos", 0, "s");
    SetNewFieldDetails("org.openconnectivity.mnpv", 0, "s");
    SetNewFieldDetails("org.openconnectivity.st", 0, "s");
    if (defaultLanguage)
    {
        SetDefaultLanguage(defaultLanguage);
    }
}

QStatus AboutData::Set(const char *rt, OCRepPayload *payload)
{
    char *value = NULL;
    OCRepPayload **arr = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
    if (!strcmp(rt, OC_RSRVD_RESOURCE_TYPE_DEVICE))
    {
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_NAME, &value))
        {
            /* Prefer OC_RSRVD_DEVICE_NAME_LOCALIZED for AppName */
            m_n = value;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_ID, &value))
        {
            uint8_t appId[16];
            ToAppId(value, appId);
            SetAppId(appId, 16);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_MFG_NAME, &arr, dim))
        {
            size_t dimTotal = calcDimTotal(dim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                char *language = NULL;
                if (OCRepPayloadGetPropString(arr[i], "language", &language) &&
                        OCRepPayloadGetPropString(arr[i], "value", &value))
                {
                    SetManufacturer(value, language);
                }
                OICFree(language);
                OICFree(value);
                value = NULL;
            }
            for (size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(arr[i]);
            }
            OICFree(arr);
            arr = NULL;
        }
        if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_MFG_NAME, &arr, dim))
        {
            size_t dimTotal = calcDimTotal(dim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                char *language = NULL;
                if (OCRepPayloadGetPropString(arr[i], "language", &language) &&
                        OCRepPayloadGetPropString(arr[i], "value", &value))
                {
                    SetManufacturer(value, language);
                }
                OICFree(language);
                OICFree(value);
                value = NULL;
            }
            for (size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(arr[i]);
            }
            OICFree(arr);
            arr = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_DEVICE_MODEL_NUM, &value))
        {
            SetModelNumber(value);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_DESCRIPTION, &arr, dim))
        {
            size_t dimTotal = calcDimTotal(dim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                char *language = NULL;
                if (OCRepPayloadGetPropString(arr[i], "language", &language) &&
                        OCRepPayloadGetPropString(arr[i], "value", &value))
                {
                    SetDescription(value, language);
                }
                OICFree(language);
                OICFree(value);
                value = NULL;
            }
            for (size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(arr[i]);
            }
            OICFree(arr);
            arr = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, &value))
        {
            ajn::MsgArg valueArg("s", value);
            SetField("org.openconnectivity.piid", valueArg);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_SOFTWARE_VERSION, &value))
        {
            SetSoftwareVersion(value);
            OICFree(value);
            value = NULL;
        }
        SetVendorFields(payload);
    }
    else if (!strcmp(rt, OC_RSRVD_RESOURCE_TYPE_PLATFORM))
    {
        char *value = NULL;
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_DATE, &value))
        {
            SetDateOfManufacture(value);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_FIRMWARE_VERSION, &value))
        {
            ajn::MsgArg valueArg("s", value);
            SetField("org.openconnectivity.mnfv", valueArg);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_HARDWARE_VERSION, &value))
        {
            SetHardwareVersion(value);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_MFG_URL, &value))
        {
            ajn::MsgArg valueArg("s", value);
            SetField("org.openconnectivity.mnml", valueArg);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_OS_VERSION, &value))
        {
            ajn::MsgArg valueArg("s", value);
            SetField("org.openconnectivity.mnos", valueArg);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_PLATFORM_VERSION, &value))
        {
            ajn::MsgArg valueArg("s", value);
            SetField("org.openconnectivity.mnpv", valueArg);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_SUPPORT_URL, &value))
        {
            SetSupportUrl(value);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_PLATFORM_ID, &value))
        {
            SetDeviceId(value);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_SYSTEM_TIME, &value))
        {
            ajn::MsgArg valueArg("s", value);
            SetField("org.openconnectivity.st", valueArg);
            OICFree(value);
            value = NULL;
        }
        SetVendorFields(payload);
    }
    else if (!strcmp(rt, OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION))
    {
        char *value = NULL;
        if (OCRepPayloadGetPropString(payload, OC_RSRVD_DEFAULT_LANGUAGE, &value))
        {
            SetDefaultLanguage(value);
            OICFree(value);
            value = NULL;
        }
        if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_DEVICE_NAME_LOCALIZED, &arr, dim))
        {
            size_t dimTotal = calcDimTotal(dim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                char *language = NULL;
                if (OCRepPayloadGetPropString(arr[i], "language", &language) &&
                        OCRepPayloadGetPropString(arr[i], "value", &value))
                {
                    SetAppName(value, language);
                }
                OICFree(language);
                OICFree(value);
                value = NULL;
            }
            for (size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(arr[i]);
            }
            OICFree(arr);
            arr = NULL;
        }
    }
    else if (!strcmp(rt, OC_RSRVD_RESOURCE_TYPE_PLATFORM_CONFIGURATION))
    {
        char *value = NULL;
        if (OCRepPayloadGetPropObjectArray(payload, OC_RSRVD_PLATFORM_NAME, &arr, dim))
        {
            size_t dimTotal = calcDimTotal(dim);
            for (size_t i = 0; i < dimTotal; ++i)
            {
                char *language = NULL;
                if (OCRepPayloadGetPropString(arr[i], "language", &language) &&
                        OCRepPayloadGetPropString(arr[i], "value", &value))
                {
                    SetDeviceName(value, language);
                }
                OICFree(language);
                OICFree(value);
                value = NULL;
            }
            for (size_t i = 0; i < dimTotal; ++i)
            {
                OCRepPayloadDestroy(arr[i]);
            }
            OICFree(arr);
            arr = NULL;
        }
    }
    return ER_OK;
}

static bool HasField(const char **fields, size_t numFields, const char *field)
{
    for (size_t i = 0; i < numFields; ++i)
    {
        if (!strcmp(field, fields[i]))
        {
            return true;
        }
    }
    return false;
}

bool AboutData::IsValid()
{
    /*
     * ajn::AboutData does not provide a way to get a field without creating a default entry and we
     * only want to add a default entry if one is not already present.
     */
    size_t numFields = GetFields();
    const char *fields[numFields];
    GetFields(fields, numFields);
    /*
     * Set default mandatory values necessary for Announce to succeed when /oic/d or /oic/p not
     * present.
     */
    if (!HasField(fields, numFields, "DefaultLanguage"))
    {
        /* When DefaultLanguage has not been set use the first supported language if available. */
        const char *lang;
        if (GetSupportedLanguages(&lang, 1))
        {
            SetDefaultLanguage(lang);
        }
        else
        {
            SetDefaultLanguage("");
        }
    }
    if (!HasField(fields, numFields, "AppName"))
    {
        SetAppName(m_n ? m_n : "");
    }
    if (!HasField(fields, numFields, "Description"))
    {
        SetDescription("");
    }
    if (!HasField(fields, numFields, "SoftwareVersion"))
    {
        SetSoftwareVersion("");
    }
    if (!HasField(fields, numFields, "DeviceId"))
    {
        SetDeviceId("");
    }
    if (!HasField(fields, numFields, "Manufacturer"))
    {
        SetManufacturer("");
    }
    if (!HasField(fields, numFields, "ModelNumber"))
    {
        SetModelNumber("");
    }
    if (!HasField(fields, numFields, "DateOfManufacture"))
    {
        SetDateOfManufacture("");
    }
    if (!HasField(fields, numFields, "HardwareVersion"))
    {
        SetHardwareVersion("");
    }
    if (!HasField(fields, numFields, "SupportUrl"))
    {
        SetSupportUrl("");
    }
    if (!HasField(fields, numFields, "AppId"))
    {
        uint8_t appId[16] = { 0 };
        SetAppId(appId, 16);
    }
    return ajn::AboutData::IsValid();
}
