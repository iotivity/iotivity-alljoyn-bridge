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

#include "VirtualConfigBusObject.h"

#include "Payload.h"
#include "Plugin.h"
#include "Signature.h"
#include <alljoyn/BusAttachment.h>
#include "ocpayload.h"
#include "oic_malloc.h"
#include <assert.h>

static const char *ifaceXml =
    "<interface name='org.alljoyn.Config'>"
    "  <method name='FactoryReset'>"
    "    <annotation name='org.freedesktop.DBus.Method.NoReply' value='true'/>"
    "  </method>"
    "  <method name='GetConfigurations'>"
    "    <arg name='languageTag' type='s' direction='in'/>"
    "    <arg name='languages' type='a{sv}' direction='out'/>"
    "  </method>"
    "  <method name='ResetConfigurations'>"
    "    <arg name='languageTag' type='s' direction='in'/>"
    "    <arg name='fieldList' type='as' direction='in'/>"
    "  </method>"
    "  <method name='Restart'>"
    "    <annotation name='org.freedesktop.DBus.Method.NoReply' value='true'/>"
    "  </method>"
    "  <method name='SetPasscode'>"
    "    <arg name='DaemonRealm' type='s' direction='in'/>"
    "    <arg name='newPasscode' type='ay' direction='in'/>"
    "  </method>"
    "  <method name='UpdateConfigurations'>"
    "    <arg name='languageTag' type='s' direction='in'/>"
    "    <arg name='configMap' type='a{sv}' direction='in'/>"
    "  </method>"
    "  <property name='Version' type='q' access='read'/>"
// TODO    "  <annotation name='org.alljoyn.Bus.Secure' value='true'/>"
    "</interface>";

VirtualConfigBusObject::VirtualConfigBusObject(ajn::BusAttachment *bus, const OCDevAddr *devAddr)
    : VirtualBusObject(bus, "/Config", devAddr)
{
    LOG(LOG_INFO, "[%p] bus=%p", this, bus);
    QStatus status;
    OC_UNUSED(status);
    status = bus->CreateInterfacesFromXml(ifaceXml);
    assert(status == ER_OK);
    m_iface = bus->GetInterface("org.alljoyn.Config");
    assert(m_iface);
    ajn::BusObject::AddInterface(*m_iface, ajn::BusObject::ANNOUNCED);
    const MethodEntry methodEntries[] =
    {
        { m_iface->GetMember("FactoryReset"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::FactoryReset) },
        { m_iface->GetMember("GetConfigurations"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::GetConfigurations) },
        { m_iface->GetMember("ResetConfigurations"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::ResetConfigurations) },
        { m_iface->GetMember("Restart"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::Restart) },
        { m_iface->GetMember("SetPasscode"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::SetPasscode) },
        { m_iface->GetMember("UpdateConfigurations"), static_cast<MessageReceiver::MethodHandler>(&VirtualConfigBusObject::UpdateConfigurations) },
    };
    AddMethodHandlers(methodEntries, sizeof(methodEntries) / sizeof(methodEntries[0]));
}

VirtualConfigBusObject::~VirtualConfigBusObject()
{
    LOG(LOG_INFO, "[%p]",
        this);
}

QStatus VirtualConfigBusObject::Get(const char *ifaceName, const char *propName, ajn::MsgArg &val)
{
    if (!strcmp(ifaceName, "org.alljoyn.Config"))
    {
        if (!strcmp(propName, "Version"))
        {
            return val.Set("q", 1);
        }
        else
        {
            return ER_BUS_NO_SUCH_PROPERTY;
        }
    }
    else
    {
        return ER_BUS_NO_SUCH_INTERFACE;
    }
}

void VirtualConfigBusObject::GetConfigurations(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p",
        this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    DoResource(OC_REST_GET, "/oic/con", NULL, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::GetConfigurationsCB));
}

static ajn::MsgArg ToAJDictEntry(const char *key, const char *value)
{
    ajn::MsgArg entry;
    entry.typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry.v_dictEntry.key = new ajn::MsgArg("s", key);
    ajn::MsgArg *valueArg = new ajn::MsgArg("s", value);
    valueArg->Stabilize();
    entry.v_dictEntry.val = new ajn::MsgArg("v", valueArg);
    entry.SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    return entry;
}

static ajn::MsgArg ToAJDictEntry(const char *key, size_t numValues, char **values)
{
    ajn::MsgArg entry;
    entry.typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry.v_dictEntry.key = new ajn::MsgArg("s", key);
    ajn::MsgArg *valueArg = new ajn::MsgArg("as", numValues, values);
    valueArg->Stabilize();
    entry.v_dictEntry.val = new ajn::MsgArg("v", valueArg);
    entry.SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    return entry;
}

static ajn::MsgArg ToAJDictEntry(const char *key, const ajn::MsgArg *valueArg)
{
    ajn::MsgArg entry;
    entry.typeId = ajn::ALLJOYN_DICT_ENTRY;
    entry.v_dictEntry.key = new ajn::MsgArg("s", key);
    entry.v_dictEntry.val = new ajn::MsgArg("v", valueArg);
    entry.SetOwnershipFlags(ajn::MsgArg::OwnsArgs, true);
    return entry;
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::GetConfigurationsCB(ajn::Message &msg, OCRepPayload *payload)
{
    LOG(LOG_INFO, "[%p]",
        this);

    std::vector<ajn::MsgArg> entries;
    char *dl = NULL;
    if (OCRepPayloadGetPropString(payload, "dl", &dl))
    {
        entries.push_back(ToAJDictEntry("DefaultLanguage", dl));
    }
    char *languageTag;
    msg->GetArgs("s", &languageTag);
    if (dl && !strcmp(languageTag, ""))
    {
        languageTag = dl;
    }
    char *n = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0 };
    OCRepPayload **objArray;
    if (OCRepPayloadGetPropObjectArray(payload, "ln", &objArray, dim))
    {
        size_t numLangs = calcDimTotal(dim);
        char **langs = new char *[numLangs];
        memset(langs, 0, numLangs * sizeof(char *));
        for (size_t i = 0; i < numLangs; ++i)
        {
            if (OCRepPayloadGetPropString(objArray[i], "language", &langs[i]))
            {
                if (!strcmp(langs[i], languageTag))
                {
                    OCRepPayloadGetPropString(objArray[i], "value", &n);
                }
            }
        }
        entries.push_back(ToAJDictEntry("SupportedLanguages", numLangs, langs));
        for (size_t i = 0; i < numLangs; ++i)
        {
            OICFree(langs[i]);
            OCRepPayloadDestroy(objArray[i]);
        }
        OICFree(objArray);
        delete[] langs;
    }
    if (!n && (!strcmp(languageTag, "") || (dl && !strcmp(languageTag, dl))))
    {
        OCRepPayloadGetPropString(payload, "n", &n);
    }
    if (n)
    {
        entries.push_back(ToAJDictEntry("AppName", n));
        char *value;
        if (OCRepPayloadGetPropString(payload, "loc", &value))
        {
            entries.push_back(ToAJDictEntry("org.openconnectivity.loc", value));
            OICFree(value);
        }
        if (OCRepPayloadGetPropString(payload, "locn", &value))
        {
            entries.push_back(ToAJDictEntry("org.openconnectivity.locn", value));
            OICFree(value);
        }
        if (OCRepPayloadGetPropString(payload, "c", &value))
        {
            entries.push_back(ToAJDictEntry("org.openconnectivity.c", value));
            OICFree(value);
        }
        if (OCRepPayloadGetPropString(payload, "r", &value))
        {
            entries.push_back(ToAJDictEntry("org.openconnectivity.r", value));
            OICFree(value);
        }
        /* Vendor-defined properties */
        for (OCRepPayloadValue *value = payload->values; value; value = value->next)
        {
            if (!strncmp(value->name, "x.", 2))
            {
                const char *fieldName = value->name + 2; /* Skip the leading x. */
                char fieldSig[] = "aaaa{sv}";
                CreateSignature(fieldSig, value);
                ajn::MsgArg *fieldValue = new ajn::MsgArg();
                ToAJMsgArg(fieldValue, fieldSig, value);
                entries.push_back(ToAJDictEntry(fieldName, fieldValue));
            }
        }
    }
    if (n)
    {
        ajn::MsgArg arg;
        arg.typeId = ajn::ALLJOYN_ARRAY;
        arg.v_array.SetElements("{sv}", entries.size(), &entries[0]);
        QStatus status = MethodReply(msg, &arg, 1);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
        }
    }
    else
    {
        QStatus status = MethodReply(msg, "org.alljoyn.Error.LanguageNotSupported");
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
        }
    }
    OICFree(n);
    OICFree(dl);
}

void VirtualConfigBusObject::UpdateConfigurations(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] member=%p",
        this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    OCRepPayload *payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    char *languageTag;
    char *defaultLanguage;
    ajn::MsgArg *dict;
    msg->GetArgs("s*", &languageTag, &dict);
    char *value;
    if (dict->GetElement("{ss}", "org.openconnectivity.loc", &value) == ER_OK)
    {
        OCRepPayloadSetPropString(payload, "loc", value);
    }
    if (dict->GetElement("{ss}", "org.openconnectivity.locn", &value) == ER_OK)
    {
        OCRepPayloadSetPropString(payload, "locn", value);
    }
    if (dict->GetElement("{ss}", "org.openconnectivity.c", &value) == ER_OK)
    {
        OCRepPayloadSetPropString(payload, "c", value);
    }
    if (dict->GetElement("{ss}", "org.openconnectivity.r", &value) == ER_OK)
    {
        OCRepPayloadSetPropString(payload, "r", value);
    }
    defaultLanguage = NULL;
    if (dict->GetElement("{ss}", "DefaultLanguage", &defaultLanguage) == ER_OK)
    {
        OCRepPayloadSetPropString(payload, "dl", defaultLanguage);
    }
    if (dict->GetElement("{ss}", "AppName", &value) == ER_OK)
    {
        if (!strcmp(languageTag, "") && !defaultLanguage)
        {
            OCRepPayloadSetPropString(payload, "n", value);
        }
        else
        {
            if (!strcmp(languageTag, "") && defaultLanguage)
            {
                languageTag = defaultLanguage;
            }
            size_t dim[MAX_REP_ARRAY_DEPTH] = { 1, 0, 0 };
            OCRepPayload **objArray = (OCRepPayload **) OICCalloc(calcDimTotal(dim), sizeof(OCRepPayload *));
            if (!objArray)
            {
                goto error;
            }
            OCRepPayload *obj = OCRepPayloadCreate();
            if (!obj)
            {
                OICFree(objArray);
                goto error;
            }
            OCRepPayloadSetPropString(obj, "language", languageTag);
            OCRepPayloadSetPropString(obj, "value", value);
            objArray[0] = obj;
            OCRepPayloadSetPropObjectArrayAsOwner(payload, "ln", objArray, dim);
        }
    }
    DoResource(OC_REST_POST, "/oic/con", payload, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::UpdateConfigurationsCB));
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::UpdateConfigurationsCB(ajn::Message &msg, OCRepPayload *payload)
{
    (void) payload;
    LOG(LOG_INFO, "[%p]",
        this);

    QStatus status = MethodReply(msg);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

void VirtualConfigBusObject::ResetConfigurations(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) msg;
    LOG(LOG_INFO, "[%p] member=%p",
        this, member);
    MethodReply(msg, ER_NOT_IMPLEMENTED);
}

void VirtualConfigBusObject::SetPasscode(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) msg;
    LOG(LOG_INFO, "[%p] member=%p",
        this, member);
    MethodReply(msg, ER_NOT_IMPLEMENTED);
}

void VirtualConfigBusObject::FactoryReset(const ajn::InterfaceDescription::Member *member,
        ajn::Message &msg)
{
    (void) msg;
    LOG(LOG_INFO, "[%p] member=%p",
        this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    OCRepPayload *payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    OCRepPayloadSetPropBool(payload, "fr", true);
    DoResource(OC_REST_POST, "/oic/mnt", payload, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::FactoryResetCB));
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::FactoryResetCB(ajn::Message &msg, OCRepPayload *payload)
{
    (void) payload;
    LOG(LOG_INFO, "[%p]",
        this);

    QStatus status = MethodReply(msg);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

void VirtualConfigBusObject::Restart(const ajn::InterfaceDescription::Member *member,
                                     ajn::Message &msg)
{
    (void) msg;
    LOG(LOG_INFO, "[%p] member=%p",
        this, member);

    std::lock_guard<std::mutex> lock(m_mutex);
    OCRepPayload *payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    OCRepPayloadSetPropBool(payload, "rb", true);
    DoResource(OC_REST_POST, "/oic/mnt", payload, msg,
            static_cast<VirtualBusObject::DoResourceHandler>(&VirtualConfigBusObject::RestartCB));
    return;

error:
    QStatus status = MethodReply(msg, ER_FAIL);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}

/* Called with m_mutex held. */
void VirtualConfigBusObject::RestartCB(ajn::Message &msg, OCRepPayload *payload)
{
    (void) payload;
    LOG(LOG_INFO, "[%p]",
        this);

    QStatus status = MethodReply(msg);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "MethodReply - %s", QCC_StatusText(status));
    }
}
