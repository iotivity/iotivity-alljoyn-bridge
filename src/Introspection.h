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

#ifndef _INTROSPECTION_H
#define _INTROSPECTION_H

#include "cacommon.h"
#include "cbor.h"
#include "ocpayload.h"
#include "octypes.h"
#include <alljoyn/BusAttachment.h>
#include <iostream>

class Device;
class VirtualBusAttachment;
class VirtualBusObject;

typedef enum
{
    NONE = 0,
    READ = (1 << 0),
    READWRITE = (1 << 1),
} AccessFlags;

typedef std::pair<std::string, std::string> Annotation;
typedef std::vector<Annotation> Annotations;

/*
 * Creates CBOR-encoded introspection data from the supplied BusAttachment.
 */
CborError Introspect(ajn::BusAttachment *bus, const char *ajSoftwareVersion, const char *title,
        const char *version, uint8_t *out, size_t *outSize);

/*
 * Creates an introspection definition object from a GET request payload.
 *
 * @param[in] payload a response payload to create the schema from
 * @param[in] resourceType the resource type of payload
 * @param[in] interfaces the interfaces implemented by the resource
 *
 * @return a schema definition object
 */
OCRepPayload *IntrospectDefinition(OCRepPayload *payload, std::string resourceType,
        std::vector<std::string> &interfaces);

/*
 * Creates an introspection path object from the supplied resource types and interfaces.
 *
 * @param[in] resourceTypes the resource types implemented by the resource
 * @param[in] interfaces the interfaces implemented by the resource
 *
 * @return a path object
 */
OCRepPayload *IntrospectPath(std::vector<std::string> &resourceTypes,
        std::vector<std::string> &interfaces);

/*
 * @param[in] device the device the OC introspection data is from.
 * @param[in,out] bus the bus to create AJ interfaces and objects on.
 * @param[in] payload the OC introspection data.
 */
bool ParseIntrospectionPayload(Device *device, VirtualBusAttachment *bus,
        const OCRepPayload *payload);

#endif
