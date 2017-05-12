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

#include "Security.h"

#include "Log.h"
#include "Plugin.h"
#include <alljoyn/BusAttachment.h>
#include "pinoxmcommon.h"

AllJoynSecurity::AllJoynSecurity(ajn::BusAttachment *bus, Role role)
    : m_bus(bus), m_role(role)
{
    LOG(LOG_INFO, "[%p] bus=%p", this, bus);
}

QStatus AllJoynSecurity::SetClaimable()
{
    LOG(LOG_INFO, "[%p]", this);

    QStatus status = m_bus->EnablePeerSecurity("ALLJOYN_ECDHE_ECDSA ALLJOYN_ECDHE_NULL "
            "ALLJOYN_ECDHE_PSK ALLJOYN_ECDHE_SPEKE", this, NULL, true, this);
    if (status != ER_OK)
    {
        LOG(LOG_ERR, "EnablePeerSecurity - %s", QCC_StatusText(status));
        return status;
    }

    ajn::PermissionConfigurator::ApplicationState appState;
    status = m_bus->GetPermissionConfigurator().GetApplicationState(appState);
    if (status != ER_OK)
    {
        return status;
    }
    LOG(LOG_INFO, "[%p] appState=%d", this, appState);
    if (appState == ajn::PermissionConfigurator::NOT_CLAIMABLE)
    {
        status = m_bus->GetPermissionConfigurator().SetClaimCapabilities(
            ajn::PermissionConfigurator::CAPABLE_ECDHE_NULL);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "SetClaimCapabilities() - %s", QCC_StatusText(status));
            return status;
        }
        ajn::PermissionPolicy::Rule rule;
        rule.SetObjPath("*");
        rule.SetInterfaceName("*");
        if (m_role == CONSUMER)
        {
            ajn::PermissionPolicy::Rule::Member member;
            member.SetMemberName("*");
            member.SetMemberType(ajn::PermissionPolicy::Rule::Member::NOT_SPECIFIED);
            member.SetActionMask(ajn::PermissionPolicy::Rule::Member::ACTION_OBSERVE |
                    ajn::PermissionPolicy::Rule::Member::ACTION_MODIFY);
            rule.SetMembers(1, &member);
        }
        else if (m_role == PRODUCER)
        {
            ajn::PermissionPolicy::Rule::Member member;
            member.SetMemberName("*");
            member.SetMemberType(ajn::PermissionPolicy::Rule::Member::NOT_SPECIFIED);
            member.SetActionMask(ajn::PermissionPolicy::Rule::Member::ACTION_PROVIDE);
            rule.SetMembers(1, &member);
        }
        status = m_bus->GetPermissionConfigurator().SetPermissionManifestTemplate(&rule, 1);
        if (status != ER_OK)
        {
            LOG(LOG_ERR, "SetPermissionManifestTemplate() - %s", QCC_StatusText(status));
            return status;
        }
    }
    return ER_OK;
}

bool AllJoynSecurity::IsClaimed()
{
#if __WITH_DTLS__
    ajn::PermissionConfigurator::ApplicationState appState =
            ajn::PermissionConfigurator::NOT_CLAIMABLE;
    m_bus->GetPermissionConfigurator().GetApplicationState(appState);
    return (appState == ajn::PermissionConfigurator::ApplicationState::CLAIMED);
#else
    return true;
#endif
}

void AllJoynSecurity::SecurityViolation(QStatus status, const ajn::Message &msg)
{
    LOG(LOG_INFO, "[%p] status=%s,msg=%s",
            this, QCC_StatusText(status), msg->ToString().c_str());
}

void AllJoynSecurity::AuthenticationComplete(const char *authMechanism, const char *peerName,
        bool success)
{
    LOG(LOG_INFO, "[%p] authMechanism=%s,peerName=%s,success=%d",
            this, authMechanism, peerName, success);
}

QStatus AllJoynSecurity::FactoryReset()
{
    LOG(LOG_INFO, "[%p]", this);
    return ER_NOT_IMPLEMENTED;
}

void AllJoynSecurity::PolicyChanged()
{
    LOG(LOG_INFO, "[%p]", this);
}

void AllJoynSecurity::StartManagement()
{
    LOG(LOG_INFO, "[%p]", this);
}

void AllJoynSecurity::EndManagement()
{
    LOG(LOG_INFO, "[%p]", this);
}

bool OCSecurity::Init()
{
#if __WITH_DTLS__
    OCStackResult result = SetDisplayPinWithContextCB(OCSecurity::DisplayPinCB, this);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "SetDisplayPinWithContextCB() - %d", result);
        return false;
    }
    SetClosePinDisplayCB(OCSecurity::ClosePinDisplayCB);
    result = SetRandomPinPolicy(OXM_RANDOM_PIN_DEFAULT_SIZE,
            (OicSecPinType_t) OXM_RANDOM_PIN_DEFAULT_PIN_TYPE);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "SetRandomPinPolicy() - %d", result);
        return false;
    }
#endif
    return true;
}

void OCSecurity::DisplayPinCB(char *pin, size_t pinSize, void *context)
{
    LOG(LOG_INFO, "pin=%s,pinSize=%d,context=%p", pin, pinSize, context);
}

void OCSecurity::ClosePinDisplayCB()
{
    LOG(LOG_INFO, "");
}
