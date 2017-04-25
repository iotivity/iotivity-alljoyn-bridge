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

#include <gtest/gtest.h>

#include "Name.h"

class NameTranslationTest : public ::testing::TestWithParam<const char *> { };

TEST_P(NameTranslationTest, RoundTrip)
{
    const char *ajName = GetParam();
    EXPECT_STREQ(ajName, ToAJName(ToOCName(ajName)).c_str());
}

INSTANTIATE_TEST_CASE_P(FromSpec,
                        NameTranslationTest,
                        ::testing::Values("example.Widget",
                                "example.my_widget",
                                "example.My_Widget",
                                "xn_p1ai.example",
                                "xn__90ae.example",
                                "example.myName_1"));

INSTANTIATE_TEST_CASE_P(Extras,
                        NameTranslationTest,
                        ::testing::Values("oneTwoThree",
                                "One_Two_Three",
                                "",
                                "x",
                                "example.foo_",
                                "example.foo__"));

TEST(NameTranslationTest, BoundsCheck)
{
    EXPECT_STREQ("", ToAJName("").c_str());
    EXPECT_STREQ("x", ToAJName("x").c_str());
    EXPECT_STREQ("example.foo_", ToAJName("x.example.foo-").c_str());
    EXPECT_STREQ("example.foo__", ToAJName("x.example.foo--").c_str());
    EXPECT_STREQ("example.foo__", ToAJName("x.example.foo---").c_str());
}

TEST(IsValidErrorNameTest, Check)
{
    const char *endp;

    EXPECT_TRUE(IsValidErrorName("a.b", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("A.b", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("_.b", &endp) && (*endp == '\0'));
    EXPECT_FALSE(IsValidErrorName("0.b", &endp));

    EXPECT_TRUE(IsValidErrorName("aa.bb", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("aA.bB", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("a_.b_", &endp) && (*endp == '\0'));
    EXPECT_TRUE(IsValidErrorName("a0.b0", &endp) && (*endp == '\0'));

    EXPECT_FALSE(IsValidErrorName("", &endp));
    EXPECT_FALSE(IsValidErrorName(".", &endp));
    EXPECT_FALSE(IsValidErrorName("a.", &endp));
    EXPECT_FALSE(IsValidErrorName("a..b", &endp));

    EXPECT_TRUE(IsValidErrorName("a.b ", &endp) && (*endp == ' '));
    EXPECT_TRUE(IsValidErrorName("a.b:", &endp) && (*endp == ':'));
}
