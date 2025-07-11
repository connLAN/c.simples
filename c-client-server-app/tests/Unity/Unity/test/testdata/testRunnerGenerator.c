/* =========================================================================
    Unity - A Test Framework for C
    ThrowTheSwitch.org
    Copyright (c) 2007-25 Mike Karlesky, Mark VanderVoord, & Greg Williams
    SPDX-License-Identifier: MIT
========================================================================= */

/* This Test File Is Used To Verify Many Combinations Of Using the Generate Test Runner Script */

#include <stdio.h>
#include "unity.h"
#include "Defs.h"

#ifdef USE_CEXCEPTION
#include "CException.h"
#endif

/* Notes about prefixes:
   test     - normal default prefix. these are "always run" tests for this procedure
   spec     - normal default prefix. required to run default setup/teardown calls.
   should   - normal default prefix.
   qwiktest - custom prefix for when tests skip all setup/teardown calls.
   custtest - custom prefix for when tests use custom setup/teardown calls.
   paratest - custom prefix for when we want to verify parameterized tests.
   extest   - custom prefix only used during cexception
   suitetest- custom prefix for when we want to use custom suite setup/teardown
*/

/* Include Passthroughs for Linking Tests */
void putcharSpy(int c) { (void)putchar(c);}
void flushSpy(void) {}

/* Global Variables Used During These Tests */
int CounterSetup = 0;
int CounterTeardown = 0;
int CounterSuiteSetup = 0;

void setUp(void)
{
    CounterSetup = 1;
}

void tearDown(void)
{
    CounterTeardown = 1;
}

void custom_setup(void)
{
    CounterSetup = 2;
}

void custom_teardown(void)
{
    CounterTeardown = 2;
}

/*
void test_OldSchoolCommentsShouldBeIgnored(void)
{
    TEST_ASSERT_FAIL("Old-School Comments Should Be Ignored");
}
*/

void test_ThisTestAlwaysPasses(void)
{
    TEST_PASS();
}

void test_ThisTestAlwaysFails(void)
{
    TEST_FAIL_MESSAGE("This Test Should Fail");
}

void test_ThisTestAlwaysIgnored(void)
{
    TEST_IGNORE_MESSAGE("This Test Should Be Ignored");
}

void qwiktest_ThisTestPassesWhenNoSetupRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(0, CounterSetup, "Setup Was Unexpectedly Run");
}

void qwiktest_ThisTestPassesWhenNoTeardownRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(0, CounterTeardown, "Teardown Was Unexpectedly Run");
}

void spec_ThisTestPassesWhenNormalSuiteSetupAndTeardownRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(0, CounterSuiteSetup, "Suite Setup Was Unexpectedly Run");
}

void spec_ThisTestPassesWhenNormalSetupRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(1, CounterSetup, "Normal Setup Wasn't Run");
}

void spec_ThisTestPassesWhenNormalTeardownRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(1, CounterTeardown, "Normal Teardown Wasn't Run");
}

void custtest_ThisTestPassesWhenCustomSetupRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(2, CounterSetup, "Custom Setup Wasn't Run");
}

void custtest_ThisTestPassesWhenCustomTeardownRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(2, CounterTeardown, "Custom Teardown Wasn't Run");
}

#ifndef UNITY_EXCLUDE_TESTING_NEW_COMMENTS
//void test_NewStyleCommentsShouldBeIgnored(void)
//{
//    TEST_ASSERT_FAIL("New Style Comments Should Be Ignored");
//}
#endif

void test_NotBeConfusedByLongComplicatedStrings(void)
{
    const char* crazyString = "GET / HTTP/1.1\r\nHost: 127.0.0.1:8081\r\nConnection: keep-alive\r\nCache-Control: no-cache\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2490.80 Safari/537.36\r\nPostman-Token: 768c7149-c3fb-f704-71a2-63918d9195b2\r\nAccept: */*\r\nAccept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-GB,en-US;q=0.8,en;q=0.6\r\n\r\n";

    TEST_ASSERT_EQUAL_STRING_MESSAGE(crazyString, crazyString, "These Strings Are The Same");
}

/* The next test should still appear even though we have this confusing nested comment thing going on http://looks_like_comments.com */
void test_NotDisappearJustBecauseTheTestBeforeAndAfterHaveCrazyStrings(void)
{
    TEST_ASSERT_TRUE_MESSAGE(1, "1 Should be True");
    /* still should not break anything */
}
/* nor should this */

void test_StillNotBeConfusedByLongComplicatedStrings(void)
{
    const char* crazyString = "GET / HTTP/1.1\r\nHost: 127.0.0.1:8081\r\nConnection: keep-alive\r\nCache-Control: no-cache\r\nUser-Agent: Mozilla/5.0 (Windows NT 6.3; WOW64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/46.0.2490.80 Safari/537.36\r\nPostman-Token: 768c7149-c3fb-f704-71a2-63918d9195b2\r\nAccept: */*\r\nAccept-Encoding: gzip, deflate, sdch\r\nAccept-Language: en-GB,en-US;q=0.8,en;q=0.6\r\n\r\n";

    TEST_ASSERT_EQUAL_STRING_MESSAGE(crazyString, crazyString, "These Strings Are Still The Same");
}

void should_RunTestsStartingWithShouldByDefault(void)
{
    TEST_ASSERT_TRUE_MESSAGE(1, "1 Should be True");
}

TEST_CASE(25)
TEST_CASE(125)
TEST_CASE(5)
void paratest_ShouldHandleParameterizedTests(int Num)
{
    TEST_ASSERT_EQUAL_MESSAGE(0, (Num % 5), "All The Values Are Divisible By 5");
}

TEST_CASE(7)
void paratest_ShouldHandleParameterizedTests2(int Num)
{
    TEST_ASSERT_EQUAL_MESSAGE(7, Num, "The Only Call To This Passes");
}

void paratest_ShouldHandleNonParameterizedTestsWhenParameterizationValid(void)
{
    TEST_PASS();
}

TEST_CASE(17)
void paratest_ShouldHandleParameterizedTestsThatFail(int Num)
{
    TEST_ASSERT_EQUAL_MESSAGE(3, Num, "This call should fail");
}

int isArgumentOne(int i)
{
    return i == 1;
}

TEST_CASE(isArgumentOne)
void paratest_WorksWithFunctionPointers(int function(int))
{
    TEST_ASSERT_TRUE_MESSAGE(function(1), "Function should return True");
}

#ifdef USE_CEXCEPTION
void extest_ShouldHandleCExceptionInTest(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(1, CEXCEPTION_BEING_USED, "Should be pulling in CException");
}
#endif

#ifdef USE_ANOTHER_MAIN
int custom_main(void);

int main(void)
{
    return custom_main();
}
#endif

void suitetest_ThisTestPassesWhenCustomSuiteSetupAndTeardownRan(void)
{
    TEST_ASSERT_EQUAL_MESSAGE(1, CounterSuiteSetup, "Suite Setup Should Have Run");
}
