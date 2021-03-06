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

static const uint8_t introspection_definitions[] = {
    0x78, 0x18, 0x6f, 0x69, 0x63, 0x2e, 0x77, 0x6b, 0x2e, 0x69, 0x6e, 0x74, 0x72, 0x6f, 0x73, 0x70,
    0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66, 0x6f, 0xa3, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x66, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x6a, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x69,
    0x65, 0x73, 0xa5, 0x62, 0x72, 0x74, 0xa5, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79,
    0xf5, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x81, 0x74, 0x6f, 0x69, 0x63, 0x2e, 0x77,
    0x6b, 0x2e, 0x69, 0x6e, 0x74, 0x72, 0x6f, 0x73, 0x70, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x68,
    0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65, 0x6d, 0x73, 0x01, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61,
    0x72, 0x72, 0x61, 0x79, 0x65, 0x69, 0x74, 0x65, 0x6d, 0x73, 0xa2, 0x69, 0x6d, 0x61, 0x78, 0x4c,
    0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72,
    0x69, 0x6e, 0x67, 0x62, 0x69, 0x66, 0xa4, 0x68, 0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65, 0x6d, 0x73,
    0x01, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x65, 0x69, 0x74, 0x65,
    0x6d, 0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64,
    0x65, 0x6e, 0x75, 0x6d, 0x82, 0x6f, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x62, 0x61, 0x73,
    0x65, 0x6c, 0x69, 0x6e, 0x65, 0x68, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x72, 0x68, 0x72,
    0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x67, 0x75, 0x72, 0x6c, 0x49, 0x6e, 0x66, 0x6f,
    0xa4, 0x68, 0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65, 0x6d, 0x73, 0x01, 0x65, 0x69, 0x74, 0x65, 0x6d,
    0x73, 0xa3, 0x6a, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0xa4, 0x68, 0x70,
    0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, 0xa2, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x86, 0x64, 0x63,
    0x6f, 0x61, 0x70, 0x65, 0x63, 0x6f, 0x61, 0x70, 0x73, 0x64, 0x68, 0x74, 0x74, 0x70, 0x65, 0x68,
    0x74, 0x74, 0x70, 0x73, 0x68, 0x63, 0x6f, 0x61, 0x70, 0x2b, 0x74, 0x63, 0x70, 0x69, 0x63, 0x6f,
    0x61, 0x70, 0x73, 0x2b, 0x74, 0x63, 0x70, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72,
    0x69, 0x6e, 0x67, 0x67, 0x76, 0x65, 0x72, 0x73, 0x69, 0x6f, 0x6e, 0xa3, 0x64, 0x65, 0x6e, 0x75,
    0x6d, 0x81, 0x01, 0x64, 0x74, 0x79, 0x70, 0x65, 0x67, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x72,
    0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x01, 0x63, 0x75, 0x72, 0x6c, 0xa2, 0x64, 0x74,
    0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x66, 0x66, 0x6f, 0x72, 0x6d, 0x61,
    0x74, 0x63, 0x75, 0x72, 0x69, 0x6c, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e, 0x74, 0x2d, 0x74, 0x79,
    0x70, 0x65, 0xa4, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x70, 0x61, 0x70, 0x70, 0x6c,
    0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x63, 0x62, 0x6f, 0x72, 0x6b, 0x64, 0x65, 0x73,
    0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x78, 0x26, 0x63, 0x6f, 0x6e, 0x74, 0x65, 0x6e,
    0x74, 0x2d, 0x74, 0x79, 0x70, 0x65, 0x20, 0x6f, 0x66, 0x20, 0x74, 0x68, 0x65, 0x20, 0x69, 0x6e,
    0x74, 0x72, 0x6f, 0x73, 0x70, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x20, 0x64, 0x61, 0x74, 0x61,
    0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64, 0x65, 0x6e, 0x75,
    0x6d, 0x82, 0x70, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x6a,
    0x73, 0x6f, 0x6e, 0x70, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f,
    0x63, 0x62, 0x6f, 0x72, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74,
    0x68, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x82, 0x63, 0x75, 0x72, 0x6c, 0x68, 0x70,
    0x72, 0x6f, 0x74, 0x6f, 0x63, 0x6f, 0x6c, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72,
    0x61, 0x79, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x62, 0x69, 0x64, 0xa3,
    0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64, 0x74, 0x79, 0x70,
    0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c,
    0x79, 0xf5, 0x61, 0x6e, 0xa3, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x64,
    0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61, 0x78, 0x4c,
    0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x68, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64,
    0x81, 0x67, 0x75, 0x72, 0x6c, 0x49, 0x6e, 0x66, 0x6f,
};

static const uint8_t introspection_parameters[] = {
    0x76, 0x69, 0x6e, 0x74, 0x72, 0x6f, 0x73, 0x70, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x69, 0x6e,
    0x74, 0x65, 0x72, 0x66, 0x61, 0x63, 0x65, 0xa4, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74,
    0x72, 0x69, 0x6e, 0x67, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x82, 0x68, 0x6f, 0x69, 0x63, 0x2e, 0x69,
    0x66, 0x2e, 0x72, 0x6f, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x62, 0x61, 0x73, 0x65, 0x6c,
    0x69, 0x6e, 0x65, 0x62, 0x69, 0x6e, 0x65, 0x71, 0x75, 0x65, 0x72, 0x79, 0x64, 0x6e, 0x61, 0x6d,
    0x65, 0x62, 0x69, 0x66,
};

static const uint8_t introspection_paths[] = {
    0x6e, 0x2f, 0x69, 0x6e, 0x74, 0x72, 0x6f, 0x73, 0x70, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0xa1,
    0x63, 0x67, 0x65, 0x74, 0xa2, 0x6a, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73,
    0x81, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x23, 0x23, 0x2f, 0x70, 0x61, 0x72, 0x61, 0x6d,
    0x65, 0x74, 0x65, 0x72, 0x73, 0x2f, 0x69, 0x6e, 0x74, 0x72, 0x6f, 0x73, 0x70, 0x65, 0x63, 0x74,
    0x69, 0x6f, 0x6e, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x66, 0x61, 0x63, 0x65, 0x69, 0x72, 0x65, 0x73,
    0x70, 0x6f, 0x6e, 0x73, 0x65, 0x73, 0xa1, 0x63, 0x32, 0x30, 0x30, 0xa2, 0x66, 0x73, 0x63, 0x68,
    0x65, 0x6d, 0x61, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x26, 0x23, 0x2f, 0x64, 0x65, 0x66,
    0x69, 0x6e, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x2f, 0x6f, 0x69, 0x63, 0x2e, 0x77, 0x6b, 0x2e,
    0x69, 0x6e, 0x74, 0x72, 0x6f, 0x73, 0x70, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x49, 0x6e, 0x66,
    0x6f, 0x6b, 0x64, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x60,
};

static const uint8_t oic_rd_definitions[] = {
    0x69, 0x72, 0x64, 0x50, 0x75, 0x62, 0x6c, 0x69, 0x73, 0x68, 0xa1, 0x6a, 0x70, 0x72, 0x6f, 0x70,
    0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0xa7, 0x63, 0x74, 0x74, 0x6c, 0xa1, 0x64, 0x74, 0x79, 0x70,
    0x65, 0x67, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x72, 0x62, 0x69, 0x66, 0xa4, 0x68, 0x72, 0x65,
    0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x68, 0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65, 0x6d, 0x73,
    0x01, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x65, 0x69, 0x74, 0x65,
    0x6d, 0x73, 0xa2, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x81, 0x6f, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66,
    0x2e, 0x62, 0x61, 0x73, 0x65, 0x6c, 0x69, 0x6e, 0x65, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73,
    0x74, 0x72, 0x69, 0x6e, 0x67, 0x62, 0x72, 0x74, 0xa5, 0x68, 0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65,
    0x6d, 0x73, 0x01, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x65, 0x69,
    0x74, 0x65, 0x6d, 0x73, 0xa2, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18,
    0x40, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x68, 0x72, 0x65,
    0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x81,
    0x69, 0x6f, 0x69, 0x63, 0x2e, 0x77, 0x6b, 0x2e, 0x72, 0x64, 0x62, 0x64, 0x69, 0xa2, 0x67, 0x70,
    0x61, 0x74, 0x74, 0x65, 0x72, 0x6e, 0x78, 0x4d, 0x5e, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46,
    0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x38, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30,
    0x2d, 0x39, 0x5d, 0x7b, 0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d,
    0x39, 0x5d, 0x7b, 0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39,
    0x5d, 0x7b, 0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d,
    0x7b, 0x31, 0x32, 0x7d, 0x24, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e,
    0x67, 0x62, 0x69, 0x64, 0xa3, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x69,
    0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x65, 0x6c, 0x69, 0x6e, 0x6b, 0x73, 0xa2, 0x65, 0x69,
    0x74, 0x65, 0x6d, 0x73, 0xa3, 0x68, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x83, 0x64,
    0x68, 0x72, 0x65, 0x66, 0x62, 0x72, 0x74, 0x62, 0x69, 0x66, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66,
    0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x6a, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x69, 0x65,
    0x73, 0xab, 0x66, 0x61, 0x6e, 0x63, 0x68, 0x6f, 0x72, 0xa3, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66,
    0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68,
    0x19, 0x01, 0x00, 0x66, 0x66, 0x6f, 0x72, 0x6d, 0x61, 0x74, 0x63, 0x75, 0x72, 0x69, 0x62, 0x64,
    0x69, 0xa2, 0x67, 0x70, 0x61, 0x74, 0x74, 0x65, 0x72, 0x6e, 0x78, 0x4d, 0x5e, 0x5b, 0x61, 0x2d,
    0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x38, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66,
    0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41,
    0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d,
    0x46, 0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46,
    0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x31, 0x32, 0x7d, 0x24, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73,
    0x74, 0x72, 0x69, 0x6e, 0x67, 0x61, 0x70, 0xa3, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x6f, 0x62,
    0x6a, 0x65, 0x63, 0x74, 0x6a, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0xa1,
    0x62, 0x62, 0x6d, 0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x67, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65,
    0x72, 0x68, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x81, 0x62, 0x62, 0x6d, 0x64, 0x68,
    0x72, 0x65, 0x66, 0xa3, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67,
    0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x19, 0x01, 0x00, 0x66, 0x66, 0x6f,
    0x72, 0x6d, 0x61, 0x74, 0x63, 0x75, 0x72, 0x69, 0x62, 0x69, 0x66, 0xa3, 0x68, 0x6d, 0x69, 0x6e,
    0x49, 0x74, 0x65, 0x6d, 0x73, 0x01, 0x65, 0x69, 0x74, 0x65, 0x6d, 0x73, 0xa2, 0x64, 0x65, 0x6e,
    0x75, 0x6d, 0x87, 0x6f, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x62, 0x61, 0x73, 0x65, 0x6c,
    0x69, 0x6e, 0x65, 0x69, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x6c, 0x6c, 0x68, 0x6f, 0x69,
    0x63, 0x2e, 0x69, 0x66, 0x2e, 0x62, 0x69, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x72, 0x77,
    0x68, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x72, 0x68, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66,
    0x2e, 0x61, 0x68, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x73, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72,
    0x61, 0x79, 0x64, 0x74, 0x79, 0x70, 0x65, 0xa4, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74,
    0x70, 0x61, 0x70, 0x70, 0x6c, 0x69, 0x63, 0x61, 0x74, 0x69, 0x6f, 0x6e, 0x2f, 0x63, 0x62, 0x6f,
    0x72, 0x65, 0x69, 0x74, 0x65, 0x6d, 0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74,
    0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40,
    0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x68, 0x6d, 0x69, 0x6e, 0x49,
    0x74, 0x65, 0x6d, 0x73, 0x01, 0x63, 0x72, 0x65, 0x6c, 0xa1, 0x65, 0x6f, 0x6e, 0x65, 0x4f, 0x66,
    0x82, 0xa4, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x81, 0x65, 0x68, 0x6f, 0x73, 0x74,
    0x73, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x65, 0x69, 0x74, 0x65,
    0x6d, 0x73, 0xa2, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64,
    0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x68, 0x6d, 0x69, 0x6e, 0x49,
    0x74, 0x65, 0x6d, 0x73, 0x01, 0xa3, 0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x65, 0x68,
    0x6f, 0x73, 0x74, 0x73, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40,
    0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x65, 0x74, 0x69, 0x74,
    0x6c, 0x65, 0xa2, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64,
    0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x63, 0x69, 0x6e, 0x73, 0xa1,
    0x65, 0x6f, 0x6e, 0x65, 0x4f, 0x66, 0x83, 0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x67, 0x69, 0x6e,
    0x74, 0x65, 0x67, 0x65, 0x72, 0xa3, 0x66, 0x66, 0x6f, 0x72, 0x6d, 0x61, 0x74, 0x63, 0x75, 0x72,
    0x69, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61,
    0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x19, 0x01, 0x00, 0xa2, 0x67, 0x70, 0x61, 0x74, 0x74,
    0x65, 0x72, 0x6e, 0x78, 0x4d, 0x5e, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39,
    0x5d, 0x7b, 0x38, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d,
    0x7b, 0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d, 0x7b,
    0x34, 0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x34,
    0x7d, 0x2d, 0x5b, 0x61, 0x2d, 0x66, 0x41, 0x2d, 0x46, 0x30, 0x2d, 0x39, 0x5d, 0x7b, 0x31, 0x32,
    0x7d, 0x24, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x63, 0x65,
    0x70, 0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x65, 0x69,
    0x74, 0x65, 0x6d, 0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x6f, 0x62, 0x6a, 0x65, 0x63,
    0x74, 0x6a, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0xa2, 0x63, 0x70, 0x72,
    0x69, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x67, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65, 0x72, 0x67,
    0x6d, 0x69, 0x6e, 0x69, 0x6d, 0x75, 0x6d, 0x01, 0x62, 0x65, 0x70, 0xa2, 0x64, 0x74, 0x79, 0x70,
    0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x66, 0x66, 0x6f, 0x72, 0x6d, 0x61, 0x74, 0x63,
    0x75, 0x72, 0x69, 0x62, 0x72, 0x74, 0xa3, 0x68, 0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65, 0x6d, 0x73,
    0x01, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x65, 0x69, 0x74, 0x65,
    0x6d, 0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69,
    0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x61, 0x6e, 0xa3, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e,
    0x6c, 0x79, 0xf5, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69,
    0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x6b, 0x72, 0x64, 0x53, 0x65,
    0x6c, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x6f, 0x62,
    0x6a, 0x65, 0x63, 0x74, 0x65, 0x6f, 0x6e, 0x65, 0x4f, 0x66, 0x82, 0xa2, 0x68, 0x72, 0x65, 0x71,
    0x75, 0x69, 0x72, 0x65, 0x64, 0x81, 0x63, 0x73, 0x65, 0x6c, 0x6a, 0x70, 0x72, 0x6f, 0x70, 0x65,
    0x72, 0x74, 0x69, 0x65, 0x73, 0xa1, 0x63, 0x73, 0x65, 0x6c, 0xa3, 0x67, 0x6d, 0x69, 0x6e, 0x69,
    0x6d, 0x75, 0x6d, 0x00, 0x64, 0x74, 0x79, 0x70, 0x65, 0x67, 0x69, 0x6e, 0x74, 0x65, 0x67, 0x65,
    0x72, 0x67, 0x6d, 0x61, 0x78, 0x69, 0x6d, 0x75, 0x6d, 0x18, 0x64, 0xa2, 0x6a, 0x70, 0x72, 0x6f,
    0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0xa5, 0x63, 0x73, 0x65, 0x6c, 0xa2, 0x6a, 0x70, 0x72,
    0x6f, 0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0xa5, 0x64, 0x6c, 0x6f, 0x61, 0x64, 0xa4, 0x68,
    0x6d, 0x61, 0x78, 0x49, 0x74, 0x65, 0x6d, 0x73, 0x03, 0x68, 0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65,
    0x6d, 0x73, 0x03, 0x65, 0x69, 0x74, 0x65, 0x6d, 0x73, 0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66,
    0x6e, 0x75, 0x6d, 0x62, 0x65, 0x72, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61,
    0x79, 0x62, 0x6d, 0x66, 0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x67, 0x69, 0x6e, 0x74, 0x65, 0x67,
    0x65, 0x72, 0x64, 0x63, 0x6f, 0x6e, 0x6e, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74,
    0x72, 0x69, 0x6e, 0x67, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x82, 0x63, 0x77, 0x72, 0x64, 0x64, 0x77,
    0x72, 0x6c, 0x73, 0x62, 0x62, 0x77, 0xa2, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x83, 0x64, 0x68, 0x69,
    0x67, 0x68, 0x63, 0x6c, 0x6f, 0x77, 0x65, 0x6c, 0x6f, 0x73, 0x73, 0x79, 0x64, 0x74, 0x79, 0x70,
    0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x63, 0x70, 0x77, 0x72, 0xa2, 0x64, 0x65, 0x6e,
    0x75, 0x6d, 0x83, 0x62, 0x61, 0x63, 0x64, 0x62, 0x61, 0x74, 0x74, 0x64, 0x73, 0x61, 0x66, 0x65,
    0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64, 0x74, 0x79, 0x70,
    0x65, 0x66, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74, 0x62, 0x72, 0x74, 0xa4, 0x68, 0x6d, 0x69, 0x6e,
    0x49, 0x74, 0x65, 0x6d, 0x73, 0x01, 0x65, 0x69, 0x74, 0x65, 0x6d, 0x73, 0xa2, 0x64, 0x74, 0x79,
    0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e,
    0x67, 0x74, 0x68, 0x18, 0x40, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79,
    0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x62, 0x69, 0x66, 0xa5, 0x67, 0x64,
    0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x81, 0x69, 0x6f, 0x69, 0x63, 0x2e, 0x77, 0x6b, 0x2e, 0x72,
    0x64, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x65, 0x69, 0x74, 0x65, 0x6d,
    0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64, 0x65,
    0x6e, 0x75, 0x6d, 0x81, 0x6f, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x62, 0x61, 0x73, 0x65,
    0x6c, 0x69, 0x6e, 0x65, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x68,
    0x6d, 0x69, 0x6e, 0x49, 0x74, 0x65, 0x6d, 0x73, 0x01, 0x61, 0x6e, 0xa3, 0x69, 0x6d, 0x61, 0x78,
    0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74,
    0x72, 0x69, 0x6e, 0x67, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x62, 0x69,
    0x64, 0xa3, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x64, 0x74,
    0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f,
    0x6e, 0x6c, 0x79, 0xf5, 0x68, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0x81, 0x63, 0x73,
    0x65, 0x6c,
};

static const uint8_t oic_rd_parameters[] = {
    0x6e, 0x72, 0x64, 0x67, 0x65, 0x74, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x66, 0x61, 0x63, 0x65, 0xa4,
    0x62, 0x69, 0x6e, 0x65, 0x71, 0x75, 0x65, 0x72, 0x79, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x81, 0x6f,
    0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x62, 0x61, 0x73, 0x65, 0x6c, 0x69, 0x6e, 0x65, 0x64,
    0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64, 0x6e, 0x61, 0x6d, 0x65,
    0x62, 0x69, 0x66, 0x6b, 0x72, 0x64, 0x64, 0x65, 0x6c, 0x65, 0x74, 0x65, 0x2d, 0x64, 0x69, 0xa3,
    0x64, 0x6e, 0x61, 0x6d, 0x65, 0x62, 0x64, 0x69, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74,
    0x72, 0x69, 0x6e, 0x67, 0x62, 0x69, 0x6e, 0x65, 0x71, 0x75, 0x65, 0x72, 0x79, 0x6f, 0x72, 0x64,
    0x70, 0x6f, 0x73, 0x74, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x66, 0x61, 0x63, 0x65, 0xa4, 0x64, 0x6e,
    0x61, 0x6d, 0x65, 0x62, 0x72, 0x74, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69,
    0x6e, 0x67, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x81, 0x6c, 0x6f, 0x69, 0x63, 0x2e, 0x77, 0x6b, 0x2e,
    0x72, 0x64, 0x70, 0x75, 0x62, 0x62, 0x69, 0x6e, 0x65, 0x71, 0x75, 0x65, 0x72, 0x79, 0x6c, 0x72,
    0x64, 0x64, 0x65, 0x6c, 0x65, 0x74, 0x65, 0x2d, 0x69, 0x6e, 0x73, 0xa3, 0x64, 0x6e, 0x61, 0x6d,
    0x65, 0x63, 0x69, 0x6e, 0x73, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e,
    0x67, 0x62, 0x69, 0x6e, 0x65, 0x71, 0x75, 0x65, 0x72, 0x79,
};

static const uint8_t oic_rd_paths[] = {
    0x67, 0x2f, 0x6f, 0x69, 0x63, 0x2f, 0x72, 0x64, 0xa3, 0x63, 0x67, 0x65, 0x74, 0xa2, 0x6a, 0x70,
    0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x81, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66,
    0x78, 0x1b, 0x23, 0x2f, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x2f, 0x72,
    0x64, 0x67, 0x65, 0x74, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x66, 0x61, 0x63, 0x65, 0x69, 0x72, 0x65,
    0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x73, 0xa1, 0x63, 0x32, 0x30, 0x30, 0xa2, 0x6b, 0x64, 0x65,
    0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x78, 0x54, 0x52, 0x65, 0x73, 0x70, 0x6f,
    0x6e, 0x64, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x6c, 0x65,
    0x63, 0x74, 0x6f, 0x72, 0x20, 0x63, 0x72, 0x69, 0x74, 0x65, 0x72, 0x69, 0x61, 0x20, 0x2d, 0x20,
    0x65, 0x69, 0x74, 0x68, 0x65, 0x72, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x65, 0x74, 0x20, 0x6f,
    0x66, 0x20, 0x61, 0x74, 0x74, 0x72, 0x69, 0x62, 0x75, 0x74, 0x65, 0x73, 0x20, 0x6f, 0x72, 0x20,
    0x74, 0x68, 0x65, 0x20, 0x62, 0x69, 0x61, 0x73, 0x20, 0x66, 0x61, 0x63, 0x74, 0x6f, 0x72, 0x66,
    0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x19, 0x23, 0x2f,
    0x64, 0x65, 0x66, 0x69, 0x6e, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x2f, 0x72, 0x64, 0x53, 0x65,
    0x6c, 0x65, 0x63, 0x74, 0x69, 0x6f, 0x6e, 0x66, 0x64, 0x65, 0x6c, 0x65, 0x74, 0x65, 0xa2, 0x6a,
    0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x82, 0xa1, 0x64, 0x24, 0x72, 0x65,
    0x66, 0x78, 0x18, 0x23, 0x2f, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x2f,
    0x72, 0x64, 0x64, 0x65, 0x6c, 0x65, 0x74, 0x65, 0x2d, 0x64, 0x69, 0xa1, 0x64, 0x24, 0x72, 0x65,
    0x66, 0x78, 0x19, 0x23, 0x2f, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x2f,
    0x72, 0x64, 0x64, 0x65, 0x6c, 0x65, 0x74, 0x65, 0x2d, 0x69, 0x6e, 0x73, 0x69, 0x72, 0x65, 0x73,
    0x70, 0x6f, 0x6e, 0x73, 0x65, 0x73, 0xa1, 0x63, 0x32, 0x30, 0x30, 0xa1, 0x6b, 0x64, 0x65, 0x73,
    0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x74, 0x54, 0x68, 0x65, 0x20, 0x64, 0x65, 0x6c,
    0x65, 0x74, 0x65, 0x20, 0x73, 0x75, 0x63, 0x63, 0x65, 0x65, 0x64, 0x65, 0x64, 0x64, 0x70, 0x6f,
    0x73, 0x74, 0xa2, 0x6a, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x82, 0xa1,
    0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x1c, 0x23, 0x2f, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74,
    0x65, 0x72, 0x73, 0x2f, 0x72, 0x64, 0x70, 0x6f, 0x73, 0x74, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x66,
    0x61, 0x63, 0x65, 0xa4, 0x68, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0xf5, 0x62, 0x69,
    0x6e, 0x64, 0x62, 0x6f, 0x64, 0x79, 0x64, 0x6e, 0x61, 0x6d, 0x65, 0x64, 0x62, 0x6f, 0x64, 0x79,
    0x66, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66, 0x77, 0x23, 0x2f,
    0x64, 0x65, 0x66, 0x69, 0x6e, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x2f, 0x72, 0x64, 0x50, 0x75,
    0x62, 0x6c, 0x69, 0x73, 0x68, 0x69, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x73, 0xa1,
    0x63, 0x32, 0x30, 0x30, 0xa2, 0x66, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0xa1, 0x64, 0x24, 0x72,
    0x65, 0x66, 0x77, 0x23, 0x2f, 0x64, 0x65, 0x66, 0x69, 0x6e, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x73,
    0x2f, 0x72, 0x64, 0x50, 0x75, 0x62, 0x6c, 0x69, 0x73, 0x68, 0x6b, 0x64, 0x65, 0x73, 0x63, 0x72,
    0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x78, 0xc2, 0x52, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x64, 0x20,
    0x77, 0x69, 0x74, 0x68, 0x20, 0x74, 0x68, 0x65, 0x20, 0x73, 0x61, 0x6d, 0x65, 0x20, 0x73, 0x63,
    0x68, 0x65, 0x6d, 0x61, 0x20, 0x61, 0x73, 0x20, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x73, 0x68, 0x20,
    0x62, 0x75, 0x74, 0x2c, 0x20, 0x77, 0x68, 0x65, 0x6e, 0x20, 0x61, 0x20, 0x4c, 0x69, 0x6e, 0x6b,
    0x20, 0x69, 0x73, 0x20, 0x66, 0x69, 0x72, 0x73, 0x74, 0x20, 0x70, 0x75, 0x62, 0x6c, 0x69, 0x73,
    0x68, 0x65, 0x64, 0x2c, 0x20, 0x77, 0x69, 0x74, 0x68, 0x20, 0x74, 0x68, 0x65, 0x20, 0x61, 0x64,
    0x64, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x61, 0x6c, 0x20, 0x27, 0x69, 0x6e, 0x73, 0x27, 0x20, 0x50,
    0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x20, 0x69, 0x6e, 0x20, 0x74, 0x68, 0x65, 0x20,
    0x4c, 0x69, 0x6e, 0x6b, 0x2e, 0x20, 0x54, 0x68, 0x69, 0x73, 0x20, 0x76, 0x61, 0x6c, 0x75, 0x65,
    0x20, 0x69, 0x73, 0x20, 0x75, 0x73, 0x65, 0x64, 0x20, 0x62, 0x79, 0x20, 0x74, 0x68, 0x65, 0x20,
    0x72, 0x65, 0x63, 0x65, 0x69, 0x76, 0x65, 0x72, 0x20, 0x74, 0x6f, 0x20, 0x6d, 0x61, 0x6e, 0x61,
    0x67, 0x65, 0x20, 0x74, 0x68, 0x61, 0x74, 0x20, 0x4f, 0x43, 0x46, 0x20, 0x4c, 0x69, 0x6e, 0x6b,
    0x20, 0x69, 0x6e, 0x73, 0x74, 0x61, 0x6e, 0x63, 0x65, 0x2e,
};

static const uint8_t securemode_definitions[] = {
    0x6a, 0x53, 0x65, 0x63, 0x75, 0x72, 0x65, 0x4d, 0x6f, 0x64, 0x65, 0xa3, 0x68, 0x72, 0x65, 0x71,
    0x75, 0x69, 0x72, 0x65, 0x64, 0x81, 0x6a, 0x73, 0x65, 0x63, 0x75, 0x72, 0x65, 0x4d, 0x6f, 0x64,
    0x65, 0x6a, 0x70, 0x72, 0x6f, 0x70, 0x65, 0x72, 0x74, 0x69, 0x65, 0x73, 0xa5, 0x6a, 0x73, 0x65,
    0x63, 0x75, 0x72, 0x65, 0x4d, 0x6f, 0x64, 0x65, 0xa1, 0x64, 0x74, 0x79, 0x70, 0x65, 0x67, 0x62,
    0x6f, 0x6f, 0x6c, 0x65, 0x61, 0x6e, 0x62, 0x69, 0x66, 0xa4, 0x68, 0x6d, 0x69, 0x6e, 0x49, 0x74,
    0x65, 0x6d, 0x73, 0x01, 0x65, 0x69, 0x74, 0x65, 0x6d, 0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x82, 0x6f, 0x6f, 0x69,
    0x63, 0x2e, 0x69, 0x66, 0x2e, 0x62, 0x61, 0x73, 0x65, 0x6c, 0x69, 0x6e, 0x65, 0x69, 0x6f, 0x69,
    0x63, 0x2e, 0x69, 0x66, 0x2e, 0x72, 0x77, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72,
    0x61, 0x79, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x62, 0x72, 0x74, 0xa5,
    0x67, 0x64, 0x65, 0x66, 0x61, 0x75, 0x6c, 0x74, 0x81, 0x70, 0x6f, 0x69, 0x63, 0x2e, 0x72, 0x2e,
    0x73, 0x65, 0x63, 0x75, 0x72, 0x65, 0x6d, 0x6f, 0x64, 0x65, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f,
    0x6e, 0x6c, 0x79, 0xf5, 0x65, 0x69, 0x74, 0x65, 0x6d, 0x73, 0xa2, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74,
    0x68, 0x18, 0x40, 0x64, 0x74, 0x79, 0x70, 0x65, 0x65, 0x61, 0x72, 0x72, 0x61, 0x79, 0x68, 0x6d,
    0x69, 0x6e, 0x49, 0x74, 0x65, 0x6d, 0x73, 0x01, 0x61, 0x6e, 0xa3, 0x64, 0x74, 0x79, 0x70, 0x65,
    0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61, 0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74,
    0x68, 0x18, 0x40, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e, 0x6c, 0x79, 0xf5, 0x62, 0x69, 0x64,
    0xa3, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x69, 0x6d, 0x61,
    0x78, 0x4c, 0x65, 0x6e, 0x67, 0x74, 0x68, 0x18, 0x40, 0x68, 0x72, 0x65, 0x61, 0x64, 0x4f, 0x6e,
    0x6c, 0x79, 0xf5, 0x64, 0x74, 0x79, 0x70, 0x65, 0x66, 0x6f, 0x62, 0x6a, 0x65, 0x63, 0x74,
};

static const uint8_t securemode_parameters[] = {
    0x73, 0x73, 0x65, 0x63, 0x75, 0x72, 0x65, 0x6d, 0x6f, 0x64, 0x65, 0x69, 0x6e, 0x74, 0x65, 0x72,
    0x66, 0x61, 0x63, 0x65, 0xa4, 0x62, 0x69, 0x6e, 0x65, 0x71, 0x75, 0x65, 0x72, 0x79, 0x64, 0x74,
    0x79, 0x70, 0x65, 0x66, 0x73, 0x74, 0x72, 0x69, 0x6e, 0x67, 0x64, 0x65, 0x6e, 0x75, 0x6d, 0x82,
    0x69, 0x6f, 0x69, 0x63, 0x2e, 0x69, 0x66, 0x2e, 0x72, 0x77, 0x6f, 0x6f, 0x69, 0x63, 0x2e, 0x69,
    0x66, 0x2e, 0x62, 0x61, 0x73, 0x65, 0x6c, 0x69, 0x6e, 0x65, 0x64, 0x6e, 0x61, 0x6d, 0x65, 0x62,
    0x69, 0x66,
};

static const uint8_t securemode_paths[] = {
    0x6b, 0x2f, 0x73, 0x65, 0x63, 0x75, 0x72, 0x65, 0x6d, 0x6f, 0x64, 0x65, 0xa2, 0x64, 0x70, 0x6f,
    0x73, 0x74, 0xa2, 0x6a, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x82, 0xa1,
    0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x20, 0x23, 0x2f, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74,
    0x65, 0x72, 0x73, 0x2f, 0x73, 0x65, 0x63, 0x75, 0x72, 0x65, 0x6d, 0x6f, 0x64, 0x65, 0x69, 0x6e,
    0x74, 0x65, 0x72, 0x66, 0x61, 0x63, 0x65, 0xa4, 0x62, 0x69, 0x6e, 0x64, 0x62, 0x6f, 0x64, 0x79,
    0x68, 0x72, 0x65, 0x71, 0x75, 0x69, 0x72, 0x65, 0x64, 0xf5, 0x66, 0x73, 0x63, 0x68, 0x65, 0x6d,
    0x61, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x18, 0x23, 0x2f, 0x64, 0x65, 0x66, 0x69, 0x6e,
    0x69, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x2f, 0x53, 0x65, 0x63, 0x75, 0x72, 0x65, 0x4d, 0x6f, 0x64,
    0x65, 0x64, 0x6e, 0x61, 0x6d, 0x65, 0x64, 0x62, 0x6f, 0x64, 0x79, 0x69, 0x72, 0x65, 0x73, 0x70,
    0x6f, 0x6e, 0x73, 0x65, 0x73, 0xa1, 0x63, 0x32, 0x30, 0x30, 0xa2, 0x66, 0x73, 0x63, 0x68, 0x65,
    0x6d, 0x61, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x18, 0x23, 0x2f, 0x64, 0x65, 0x66, 0x69,
    0x6e, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x73, 0x2f, 0x53, 0x65, 0x63, 0x75, 0x72, 0x65, 0x4d, 0x6f,
    0x64, 0x65, 0x6b, 0x64, 0x65, 0x73, 0x63, 0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x60, 0x63,
    0x67, 0x65, 0x74, 0xa2, 0x69, 0x72, 0x65, 0x73, 0x70, 0x6f, 0x6e, 0x73, 0x65, 0x73, 0xa1, 0x63,
    0x32, 0x30, 0x30, 0xa2, 0x66, 0x73, 0x63, 0x68, 0x65, 0x6d, 0x61, 0xa1, 0x64, 0x24, 0x72, 0x65,
    0x66, 0x78, 0x18, 0x23, 0x2f, 0x64, 0x65, 0x66, 0x69, 0x6e, 0x69, 0x74, 0x69, 0x6f, 0x6e, 0x73,
    0x2f, 0x53, 0x65, 0x63, 0x75, 0x72, 0x65, 0x4d, 0x6f, 0x64, 0x65, 0x6b, 0x64, 0x65, 0x73, 0x63,
    0x72, 0x69, 0x70, 0x74, 0x69, 0x6f, 0x6e, 0x60, 0x6a, 0x70, 0x61, 0x72, 0x61, 0x6d, 0x65, 0x74,
    0x65, 0x72, 0x73, 0x81, 0xa1, 0x64, 0x24, 0x72, 0x65, 0x66, 0x78, 0x20, 0x23, 0x2f, 0x70, 0x61,
    0x72, 0x61, 0x6d, 0x65, 0x74, 0x65, 0x72, 0x73, 0x2f, 0x73, 0x65, 0x63, 0x75, 0x72, 0x65, 0x6d,
    0x6f, 0x64, 0x65, 0x69, 0x6e, 0x74, 0x65, 0x72, 0x66, 0x61, 0x63, 0x65,
};

