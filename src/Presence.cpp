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

#include "Presence.h"

#include "Plugin.h"

AllJoynPresence::AllJoynPresence(ajn::BusAttachment *bus, const std::string &name)
    : Presence(name), m_bus(bus), m_lastTick(time(NULL)), m_tries(0), m_state(IDLE)
{
    LOG(LOG_INFO, "[%p]", this);
}

AllJoynPresence::~AllJoynPresence()
{
    LOG(LOG_INFO, "[%p]", this);
}

bool AllJoynPresence::IsPresent()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    switch (m_state)
    {
        case IDLE:
            if ((time(NULL) - m_lastTick) > PERIOD_SECS)
            {
                QStatus status = m_bus->PingAsync(GetId().c_str(), PERIOD_SECS * 1000, this, NULL);
                if (status == ER_OK)
                {
                    m_state = PENDING;
                }
                else
                {
                    LOG(LOG_ERR, "PingAsync - %s", QCC_StatusText(status));
                }
            }
            return true;
        case PENDING:
            return true;
        case ABSENT:
            return false;
    }
    return true;
}

void AllJoynPresence::Seen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastTick = time(NULL);
}

void AllJoynPresence::PingCB(QStatus status, void *context)
{
    (void) context;
    std::lock_guard<std::mutex> lock(m_mutex);
    m_lastTick = time(NULL);
    if (status == ER_OK)
    {
        m_tries = 0;
        m_state = IDLE;
    }
    else if (m_tries < RETRIES)
    {
        ++m_tries;
        m_state = IDLE;
    }
    else
    {
        m_state = ABSENT;
    }
}

OCPresence::OCPresence(const OCDevAddr *devAddr, const char *di)
    : Presence(di), m_devAddr(*devAddr), m_isPresent(true), m_handle(NULL),
      m_result(OC_STACK_KEEP_TRANSACTION)
{
    LOG(LOG_INFO, "[%p]", this);

    OCCallbackData cbData;
    cbData.cb = OCPresence::PresenceCB;
    cbData.context = this;
    cbData.cd = NULL;
    OCStackResult result = DoResource(&m_handle, OC_REST_PRESENCE, "/oic/ad", &m_devAddr, 0, &cbData);
    if (result != OC_STACK_OK)
    {
        LOG(LOG_ERR, "DoResource(OC_REST_PRESENCE) - %d", result);
    }
}

OCPresence::~OCPresence()
{
    LOG(LOG_INFO, "[%p]", this);

    OCDoHandle handle;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        handle = m_handle;
        m_result = OC_STACK_DELETE_TRANSACTION;
    }
    Cancel(handle, OC_LOW_QOS);
}

OCStackApplicationResult OCPresence::PresenceCB(void *ctx, OCDoHandle handle,
        OCClientResponse *response)
{
    (void) handle;
    OCPresence *thiz = reinterpret_cast<OCPresence *>(ctx);
    LOG(LOG_INFO, "[%p]", thiz);

    std::lock_guard<std::mutex> lock(thiz->m_mutex);
    if (!response)
    {
        goto exit;
    }
    switch (response->result)
    {
        case OC_STACK_OK:
            thiz->m_isPresent = true;
            break;
        case OC_STACK_PRESENCE_STOPPED:
        case OC_STACK_PRESENCE_TIMEOUT:
            thiz->m_isPresent = false;
            break;
        case OC_STACK_PRESENCE_DO_NOT_HANDLE:
        default:
            break;
    }
    LOG(LOG_INFO, "[%p] result=%d", thiz, response->result);
    if (response->payload)
    {
        static const char *triggerText[] = { "CREATE", "CHANGE", "DELETE" };
        OCPresencePayload *payload = (OCPresencePayload *) response->payload;
        LOG(LOG_INFO, "[%p] sequenceNumber=%u,maxAge=%u,trigger=%s,resourceType=%s", thiz,
            payload->sequenceNumber, payload->maxAge, triggerText[payload->trigger], payload->resourceType);
    }
exit:
    return thiz->m_result;
}
