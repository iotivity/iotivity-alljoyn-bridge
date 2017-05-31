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

#ifndef _PLUGIN_H
#define _PLUGIN_H

#include <inttypes.h>
#include "cacommon.h"
#include "octypes.h"
#include <alljoyn/AboutData.h>
#include <alljoyn/AboutObjectDescription.h>
#include <stdint.h>
#include <vector>

/* Default language */
#define OC_RSRVD_DEFAULT_LANGUAGE "dl"

/* Device name, localized */
#define OC_RSRVD_DEVICE_NAME_LOCALIZED "ln"

/* Platform name, localized */
#define OC_RSRVD_PLATFORM_NAME "mnpn"

/** To represent secure mode resource type.*/
#define OC_RSRVD_RESOURCE_TYPE_SECURE_MODE "oic.r.securemode"

extern std::string gRD;

OCStackResult RDPublish();

#endif // _PLUGIN_H
