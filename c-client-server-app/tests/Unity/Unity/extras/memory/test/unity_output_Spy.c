/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-25 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
========================================================================= */

#include "unity.h"
#include "unity_output_Spy.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static int size;
static int count;
static char* buffer;
static int spy_enable;

void UnityOutputCharSpy_Create(int s)
{
    size = (s > 0) ? s : 0;
    count = 0;
    spy_enable = 0;
    buffer = malloc((size_t)size);
    TEST_ASSERT_NOT_NULL_MESSAGE(buffer, "Internal malloc failed in Spy Create():" __FILE__);
    memset(buffer, 0, (size_t)size);
}

void UnityOutputCharSpy_Destroy(void)
{
    size = 0;
    free(buffer);
}

void UnityOutputCharSpy_OutputChar(int c)
{
    if (spy_enable)
    {
        if (count < (size-1))
            buffer[count++] = (char)c;
    }
    else
    {
        putchar(c);
    }
}

const char * UnityOutputCharSpy_Get(void)
{
    return buffer;
}

void UnityOutputCharSpy_Enable(int enable)
{
    spy_enable = enable;
}
