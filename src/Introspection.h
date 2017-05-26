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

class VirtualBusObject;

typedef enum
{
    NONE = 0,
    READ = (1 << 0),
    READWRITE = (1 << 1),
} AccessFlags;

typedef std::pair<std::string, std::string> Annotation;
typedef std::vector<Annotation> Annotations;

CborError Introspect(ajn::BusAttachment *bus, const char *ajSoftwareVersion, const char *title,
        const char *version, uint8_t *out, size_t *outSize);

/*
 * @param[in] payload a response payload to create the schema from
 * @param[in] resourceType the resource type of payload
 * @param[in] interfaces the interfaces implemented by the resource
 *
 * @return a schema definition object
 */
OCRepPayload *IntrospectDefinition(OCRepPayload *payload, std::string resourceType,
        std::vector<std::string> &interfaces);

/*
 * @param[in] resourceTypes the resource types implemented by the resource
 * @param[in] interfaces the interfaces implemented by the resource
 *
 * @return a path object
 */
OCRepPayload *IntrospectPath(std::vector<std::string> &resourceTypes,
        std::vector<std::string> &interfaces);

/*
 * @param[in] definitions definitions of OC introspection data.
 * @param[out] annotations map from definition name to AJ annotations
 */
void ParseAnnotations(const OCRepPayload *definitions,
        std::map<std::string, Annotations> &annotations);

/*
 * @param[in] definitions the definitions of the OC introspection data.
 * @param[in] annotations a map from definition name to AJ annotations.
 * @param[in] isObservable a map from rt name to observable flag.
 * @param[in,out] bus the bus to create AJ interfaces on.
 * @param[out] ajNames a map from definition name to interface name.
 */
void ParseInterfaces(const OCRepPayload *definitions,
        std::map<std::string, Annotations> &annotations, std::map<std::string, bool> &isObservable,
        ajn::BusAttachment *bus, std::map<std::string, std::string> &ajNames);

/*
 * @param[in] path a path of the OC introspection data.
 * @param[in] ajNames a map from definition name to interface name.
 * @param[in,out] obj the AJ bus object to add interfaces to.
 */
void ParsePath(OCRepPayload *path, std::map<std::string, std::string> &ajNames,
        VirtualBusObject *obj);

#endif
