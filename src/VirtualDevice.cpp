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

#include "VirtualDevice.h"

#include "Bridge.h"
#include "Plugin.h"
#include "ocpayload.h"
#include "ocrandom.h"
#include "ocstack.h"

VirtualDevice::VirtualDevice(ajn::BusAttachment *bus, const char *name, ajn::SessionId sessionId)
    : m_bus(bus), m_name(name), m_sessionId(sessionId)
{
    LOG(LOG_INFO, "[%p] name=%s,sessionId=%d",
        this, name, sessionId);
}

VirtualDevice::~VirtualDevice()
{
    LOG(LOG_INFO, "[%p]",
        this);

    OCStackResult result = StopPresence();
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "StopPresence - %d", result);
    }
}

void VirtualDevice::SetInfo(ajn::AboutObjectDescription &objectDescription,
                            ajn::AboutData &aboutData)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_aboutData = aboutData;
    SetPlatformAndDeviceInfo(objectDescription, aboutData);
}

void VirtualDevice::StartPresence()
{
    OCStackResult result = ::StartPresence();
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "StartPresence - %d", result);
    }
}

void VirtualDevice::SetPlatformAndDeviceInfo(ajn::AboutObjectDescription &objectDescription,
        ajn::AboutData &aboutData)
{
    char *value = NULL;
    aboutData.GetAppName(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_NAME, value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SPEC_VERSION, "0.3");
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_ID, OCGetServerInstanceIDString());
    qcc::String peerGuid;
    m_bus->GetPeerGUID(m_name.c_str(), peerGuid);
    OCUUIdentity piid;
    GetPiid(&piid, peerGuid.c_str(), &aboutData);
    char piidStr[UUID_STRING_SIZE];
    OCConvertUuidToString(piid.id, piidStr);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_PROTOCOL_INDEPENDENT_ID, piidStr);
    std::set<std::string> dataModelVersions;
    size_t numPaths = objectDescription.GetPaths(NULL, 0);
    const char **paths = new const char *[numPaths];
    objectDescription.GetPaths(paths, numPaths);
    for (size_t p = 0; p < numPaths; ++p)
    {
        size_t numIfaces = objectDescription.GetInterfaces(paths[p], NULL, 0);
        const char **ifaces = new const char *[numIfaces];
        objectDescription.GetInterfaces(paths[p], ifaces, numIfaces);
        for (size_t i = 0; i < numIfaces; ++i)
        {
            dataModelVersions.insert(std::string(ifaces[i]) + ".1");
        }
        delete[] ifaces;
    }
    delete[] paths;
    std::string dmv;
    for (std::set<std::string>::iterator it = dataModelVersions.begin(); it != dataModelVersions.end();
         ++it)
    {
        if (it != dataModelVersions.begin())
        {
            dmv += ",";
        }
        dmv += *it;
    }
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DATA_MODEL_VERSION, dmv.c_str());
    value = NULL;
    aboutData.GetSoftwareVersion(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_SOFTWARE_VERSION, value);
    value = NULL;
    aboutData.GetManufacturer(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MFG_NAME, value);
    value = NULL;
    aboutData.GetModelNumber(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_DEVICE, OC_RSRVD_DEVICE_MODEL_NUM, value);

    char *deviceId;
    aboutData.GetDeviceId(&deviceId);
    unsigned int pi[16];
    if (sscanf(deviceId, "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
               &pi[0], &pi[1], &pi[2], &pi[3], &pi[4], &pi[5], &pi[6], &pi[7],
               &pi[8], &pi[9], &pi[10], &pi[11], &pi[12], &pi[13], &pi[14], &pi[15]) == 16)
    {
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, deviceId);
    }
    else if (sscanf(deviceId, "%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x",
                    &pi[0], &pi[1], &pi[2], &pi[3], &pi[4], &pi[5], &pi[6], &pi[7],
                    &pi[8], &pi[9], &pi[10], &pi[11], &pi[12], &pi[13], &pi[14], &pi[15]) == 16)
    {
        char uuid[UUID_STRING_SIZE];
        snprintf(uuid, UUID_STRING_SIZE,
                "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x",
                pi[0], pi[1], pi[2], pi[3], pi[4], pi[5], pi[6], pi[7],
                pi[8], pi[9], pi[10], pi[11], pi[12], pi[13], pi[14], pi[15]);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, uuid);
    }
    else
    {
        OCUUIdentity id;
        DeriveUniqueId(&id, deviceId, NULL, 0);
        char uuid[UUID_STRING_SIZE];
        OCConvertUuidToString(id.id, uuid);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_ID, uuid);
    }
    value = NULL;
    aboutData.GetManufacturer(&value);
    if (value)
    {
        char mfgName[17];
        strncpy(mfgName, value, 17);
        mfgName[16] = '\0';
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_NAME, mfgName);
    }
    ajn::MsgArg *valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnml", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_URL, value);
    }
    value = NULL;
    aboutData.GetModelNumber(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MODEL_NUM, value);
    value = NULL;
    aboutData.GetDateOfManufacture(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_MFG_DATE, value);
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnpv", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_PLATFORM_VERSION, value);
    }
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnos", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_OS_VERSION, value);
    }
    value = NULL;
    aboutData.GetHardwareVersion(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_HARDWARE_VERSION, value);
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.mnfv", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_FIRMWARE_VERSION, value);
    }
    value = NULL;
    aboutData.GetSupportUrl(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SUPPORT_URL, value);
    valueArg = NULL;
    aboutData.GetField("org.openconnectivity.st", valueArg);
    if (valueArg)
    {
        value = NULL;
        valueArg->Get("s", &value);
        OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_SYSTEM_TIME, value);
    }
    value = NULL;
    aboutData.GetDeviceId(&value);
    OCSetPropertyValue(PAYLOAD_TYPE_PLATFORM, OC_RSRVD_VID, value);
}
