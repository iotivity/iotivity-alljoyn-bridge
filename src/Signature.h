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

#ifndef _SIGNATURE_H
#define _SIGNATURE_H

#include "octypes.h"
#include <alljoyn/Status.h>
#include <stdint.h>

QStatus ParseCompleteType(const char *&sigPtr);
uint8_t CountCompleteTypes(const char *signature);

void CreateSignature(char *sig, OCRepPayloadValue *value);
void CreateSignature(char *sig, OCRepPayloadValueArray *arr, uint8_t di = 0);

#endif
