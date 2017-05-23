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

#include "DeviceResource.h"

#include "AboutData.h"
#include "ocrandom.h"
#include "ocstack.h"

OCStackResult SetDeviceProperties(ajn::BusAttachment *bus,
        ajn::AboutObjectDescription *objectDescription, ajn::AboutData *aboutData,
        const char *peerGuid)
{
    assert(aboutData);

    size_t numLangs = aboutData->GetSupportedLanguages();
    const char **langs = new const char *[numLangs];
    aboutData->GetSupportedLanguages(langs, numLangs);

    char *s = NULL;
    aboutData->GetAppName(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, s);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SPEC_VERSION, OC_SPEC_VERSION);
    /* OC_RSRVD_DEVICE_ID is set internally by stack */
    char piid[UUID_STRING_SIZE] = { 0 };
    GetProtocolIndependentId(piid, aboutData, peerGuid);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, piid);
    if (bus && objectDescription)
    {
        std::set<std::string> dataModelVersions;
        size_t np = objectDescription->GetPaths(NULL, 0);
        const char **ps = new const char *[np];
        objectDescription->GetPaths(ps, np);
        for (size_t i = 0; i < np; ++i)
        {
            size_t nitfs = objectDescription->GetInterfaces(ps[i], NULL, 0);
            const char **itfs = new const char *[nitfs];
            objectDescription->GetInterfaces(ps[i], itfs, nitfs);
            for (size_t j = 0; j < nitfs; ++j)
            {
                qcc::String version = "1";
                const ajn::InterfaceDescription *itf = bus->GetInterface(itfs[j]);
                if (itf) // TODO introspection needs to happen first for this to succeed
                {
                    itf->GetAnnotation("org.gtk.GDBus.Since", version);
                }
                dataModelVersions.insert(std::string(itfs[j]) + "." + version);
            }
            delete[] itfs;
        }
        delete[] ps;
        std::string dmv;
        for (auto it = dataModelVersions.begin(); it != dataModelVersions.end(); ++it)
        {
            if (it != dataModelVersions.begin())
            {
                dmv += ",";
            }
            dmv += *it;
        }
        if (!dmv.empty())
        {
            OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DATA_MODEL_VERSION, dmv.c_str());
        }
    }
    OCStringLL *ll = NULL;
    for (size_t i = 0; i < numLangs; ++i)
    {
        s = NULL;
        aboutData->GetDescription(&s, langs[i]);
        if (s)
        {
            OCResourcePayloadAddStringLL(&ll, langs[i]);
            OCResourcePayloadAddStringLL(&ll, s);
        }
    }
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_DESCRIPTION, ll);
    OCFreeOCStringLL(ll);
    ll = NULL;
    s = NULL;
    aboutData->GetSoftwareVersion(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SOFTWARE_VERSION, s);
    for (size_t i = 0; i < numLangs; ++i)
    {
        s = NULL;
        aboutData->GetManufacturer(&s, langs[i]);
        if (s)
        {
            OCResourcePayloadAddStringLL(&ll, langs[i]);
            OCResourcePayloadAddStringLL(&ll, s);
        }
    }
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MFG_NAME, ll);
    OCFreeOCStringLL(ll);
    ll = NULL;
    s = NULL;
    aboutData->GetModelNumber(&s);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MODEL_NUM, s);
    size_t nf = aboutData->GetFields();
    const char *fs[nf] = { 0 };
    aboutData->GetFields(fs, nf);
    for (size_t i = 0; i < nf; ++i)
    {
        if (!AboutData::IsVendorField(fs[i]))
        {
            continue;
        }
        ajn::MsgArg *arg = NULL;
        aboutData->GetField(fs[i], arg);
        if (arg && (arg->Get("s", &s) == ER_OK))
        {
            std::string name = std::string("x.") + fs[i];
            OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, name.c_str(), s);
        }
    }

    delete[] langs;
    return OC_STACK_OK;
}
