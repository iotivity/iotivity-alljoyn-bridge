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

#include "Resource.h"

#include "oic_malloc.h"
#include "oic_string.h"
#include "ocpayload.h"
#include "ocstack.h"

#define INTERFACE_DEFAULT_QUERY "if=" OC_RSRVD_INTERFACE_DEFAULT

OCRepPayload *CreatePayload(OCResourceHandle handle, const char *query)
{
    OCStackResult result;
    OCRepPayload *payload = NULL;
    uint8_t n;
    char** array = NULL;
    size_t dim[MAX_REP_ARRAY_DEPTH] = { 0, 0, 0 };

    payload = OCRepPayloadCreate();
    if (!payload)
    {
        goto error;
    }
    if (!OCRepPayloadSetUri(payload, OCGetResourceUri(handle)))
    {
        goto error;
    }
    if (query && strstr(query, INTERFACE_DEFAULT_QUERY))
    {
        result = OCGetNumberOfResourceTypes(handle, &n);
        if (result != OC_STACK_OK)
        {
            goto error;
        }
        array = (char**)OICCalloc(n, sizeof(char*));
        if (!array)
        {
            goto error;
        }
        for (uint8_t i = 0; i < n; ++i)
        {
            array[i] = OICStrdup(OCGetResourceTypeName(handle, i));
            if (!array[i])
            {
                goto error;
            }
        }
        dim[0] = n;
        if (!OCRepPayloadSetStringArrayAsOwner(payload, OC_RSRVD_RESOURCE_TYPE, array, dim))
        {
            goto error;
        }
        array = NULL;
        result = OCGetNumberOfResourceInterfaces(handle, &n);
        if (result != OC_STACK_OK)
        {
            goto error;
        }
        array = (char**)OICCalloc(n, sizeof(char*));
        if (!array)
        {
            goto error;
        }
        for (uint8_t i = 0; i < n; ++i)
        {
            array[i] = OICStrdup(OCGetResourceInterfaceName(handle, i));
            if (!array[i])
            {
                goto error;
            }
        }
        dim[0] = n;
        if (!OCRepPayloadSetStringArrayAsOwner(payload, OC_RSRVD_INTERFACE, array, dim))
        {
            goto error;
        }
        array = NULL;
    }
    return payload;

error:
    if (array)
    {
        for(uint8_t i = 0; i < n; ++i)
        {
            OICFree(array[i]);
        }
        OICFree(array);
    }
    OCRepPayloadDestroy(payload);
    return NULL;
}
