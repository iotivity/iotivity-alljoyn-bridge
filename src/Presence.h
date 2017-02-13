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

#ifndef _PRESENCE_H
#define _PRESENCE_H

#include <inttypes.h>
#include <mutex>
#include <string>
#include <time.h>
#include <alljoyn/BusAttachment.h>
#include "octypes.h"

class Presence
{
    public:
        Presence(const std::string &id) : m_id(id) { }
        virtual ~Presence() { }
        virtual bool IsPresent() = 0;
        virtual void Seen() = 0;
        virtual std::string GetId() const { return m_id; }
    private:
        std::string m_id;
};

class AllJoynPresence : public Presence
    , private ajn::BusAttachment::PingAsyncCB
{
    public:
        AllJoynPresence(ajn::BusAttachment *m_bus, const std::string &name);
        virtual ~AllJoynPresence();

        virtual bool IsPresent();
        virtual void Seen();

    private:
        static const time_t PERIOD_SECS = 1;
        static const uint8_t RETRIES = 3;

        ajn::BusAttachment *m_bus;
        std::mutex m_mutex;
        time_t m_lastTick;
        uint8_t m_tries;
        enum { IDLE, PENDING, ABSENT } m_state;

        virtual void PingCB(QStatus status, void *context);
};

class OCPresence : public Presence
{
    public:
        OCPresence(const OCDevAddr *devAddr, const char *di, time_t periodSecs);
        virtual ~OCPresence();

        virtual bool IsPresent();
        virtual void Seen();

    private:
        static const uint8_t RETRIES = 3;

        OCDevAddr m_devAddr;
        const time_t m_periodSecs;
        std::mutex m_mutex;
        time_t m_lastTick;
};

#endif
