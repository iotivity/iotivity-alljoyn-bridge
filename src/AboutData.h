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

#ifndef _ABOUTDATA_H
#define _ABOUTDATA_H

#include "cacommon.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include <alljoyn/AboutData.h>

class AboutData : public ajn::AboutData
{
public:
    static bool IsVendorField(const char *name);
    AboutData();
    AboutData(const char *defaultLanguage);
    AboutData(const ajn::MsgArg *arg, const char *language);
    QStatus CreateFromMsgArg(const ajn::MsgArg *arg, const char* language);
    QStatus GetConfigData(ajn::MsgArg* arg, const char* language);
    /**
     * Returns the value of the DefaultLanguage field.
     */
    QStatus GetDefaultLanguage(char** defaultLanguage);
    /**
     * Returns the announced DefaultLanguage which may be one of:
     * - the DefaultLanguage field, or
     * - the first SupportedLanguages field when DefaultLanguage is not present, or
     * - the empty string when neither DefaultLanguage nor SupportedLanguage is present.
     */
    const char *GetDefaultLanguage();
    QStatus Set(OCRepPayload *payload);
    QStatus SetSupportedLanguage(const char* language);
    QStatus SetProtocolIndependentId(const char* piid);
    QStatus SetManufacturerUrl(const char *url);
    QStatus SetPlatformVersion(const char *version);
    QStatus SetOperatingSystemVersion(const char *version);
    QStatus SetFirmwareVersion(const char *version);
    QStatus SetSystemTime(const char *systemTime);
    QStatus SetLocation(double latitude, double longitude);
    QStatus SetLocationName(const char *name);
    QStatus SetCurrency(const char *currency);
    QStatus SetRegion(const char *region);
    bool IsValid();
private:
    char *m_n;
    bool m_setDefaultLanguage;
    bool m_setSupportedLanguage;
    void SetFieldDetails();
    void SetVendorFields(OCRepPayload *payload);
};

QStatus GetProtocolIndependentId(char piid[UUID_STRING_SIZE], ajn::AboutData *aboutData,
        const char *peerGuid);
QStatus GetPlatformId(char pi[UUID_STRING_SIZE], ajn::AboutData *aboutData);

#endif
