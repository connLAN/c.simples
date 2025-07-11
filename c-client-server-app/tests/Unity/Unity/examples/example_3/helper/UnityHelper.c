/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-25 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
========================================================================= */

#include "unity.h"
#include "UnityHelper.h"
#include <stdio.h>
#include <string.h>

void AssertEqualExampleStruct(const EXAMPLE_STRUCT_T expected, const EXAMPLE_STRUCT_T actual, const unsigned short line)
{
    UNITY_TEST_ASSERT_EQUAL_INT(expected.x, actual.x, line, "Example Struct Failed For Field x");
    UNITY_TEST_ASSERT_EQUAL_INT(expected.y, actual.y, line, "Example Struct Failed For Field y");
}
