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

#ifndef _DEVICECONFIGURATIONRESOURCE_H
#define _DEVICECONFIGURATIONRESOURCE_H

#include "octypes.h"
#include <alljoyn/AboutData.h>

/** To represent device configuration resource type.*/
#define OC_RSRVD_RESOURCE_TYPE_DEVICE_CONFIGURATION "oic.wk.con"

OCStackResult SetDeviceConfigurationProperties(OCRepPayload *payload, ajn::AboutData *aboutData);
OCStackResult SetDeviceConfigurationProperties(ajn::AboutData *aboutData,
        const OCRepPayload *payload);

#endif