# Unity Assertions Reference

## Background and Overview

### Super Condensed Version

- An assertion establishes truth (i.e. boolean True) for a single condition.
Upon boolean False, an assertion stops execution and reports the failure.
- Unity is mainly a rich collection of assertions and the support to gather up
and easily execute those assertions.
- The structure of Unity allows you to easily separate test assertions from
source code in, well, test code.
- Unity’s assertions:
  - Come in many, many flavors to handle different C types and assertion cases.
  - Use context to provide detailed and helpful failure messages.
  - Document types, expected values, and basic behavior in your source code for
free.

### Unity Is Several Things But Mainly It’s Assertions

One way to think of Unity is simply as a rich collection of assertions you can
use to establish whether your source code behaves the way you think it does.
Unity provides a framework to easily organize and execute those assertions in
test code separate from your source code.

### What’s an Assertion?

At their core, assertions are an establishment of truth - boolean truth. Was this
thing equal to that thing? Does that code doohickey have such-and-such property
or not? You get the idea. Assertions are executable code. Static analysis is a 
valuable approach to improving code quality, but it is not executing your code
in the way an assertion can. A failing assertion stops execution and reports an 
error through some appropriate I/O channel (e.g. stdout, GUI, output file, 
blinky light).

Fundamentally, for dynamic verification all you need is a single assertion
mechanism. In fact, that’s what the [assert() macro][] in C’s standard library
is for. So why not just use it? Well, we can do far better in the reporting
department. C’s `assert()` is pretty dumb as-is and is particularly poor for
handling common data types like arrays, structs, etc. And, without some other
support, it’s far too tempting to litter source code with C’s `assert()`’s. It’s
generally much cleaner, manageable, and more useful to separate test and source
code in the way Unity facilitates.

### Unity’s Assertions: Helpful Messages _and_ Free Source Code Documentation

Asserting a simple truth condition is valuable, but using the context of the
assertion is even more valuable. For instance, if you know you’re comparing bit
flags and not just integers, then why not use that context to give explicit,
readable, bit-level feedback when an assertion fails?

That’s what Unity’s collection of assertions do - capture context to give you
helpful, meaningful assertion failure messages. In fact, the assertions
themselves also serve as executable documentation about types and values in your
source code. So long as your tests remain current with your source and all those
tests pass, you have a detailed, up-to-date view of the intent and mechanisms in
your source code. And due to a wondrous mystery, well-tested code usually tends
to be well designed code.

## Assertion Conventions and Configurations

### Naming and Parameter Conventions

The convention of assertion parameters generally follows this order:

```c
TEST_ASSERT_X( {modifiers}, {expected}, actual, {size/count} )
```

The very simplest assertion possible uses only a single `actual` parameter (e.g.
a simple null check).

- `Actual` is the value being tested and unlike the other parameters in an
  assertion construction is the only parameter present in all assertion variants.
- `Modifiers` are masks, ranges, bit flag specifiers, floating point deltas.
- `Expected` is your expected value (duh) to compare to an `actual` value; it’s
  marked as an optional parameter because some assertions only need a single
  `actual` parameter (e.g. null check).
- `Size/count` refers to string lengths, number of array elements, etc.

Many of Unity’s assertions are clear duplications in that the same data type
is handled by several assertions. The differences among these are in how failure
messages are presented. For instance, a `_HEX` variant of an assertion prints
the expected and actual values of that assertion formatted as hexadecimal.

#### TEST_ASSERT_X_MESSAGE Variants

_All_ assertions are complemented with a variant that includes a simple string
message as a final parameter. The string you specify is appended to an assertion
failure message in Unity output.

For brevity, the assertion variants with a message parameter are not listed
below. Just tack on `_MESSAGE` as the final component to any assertion name in
the reference list below and add a string as the final parameter.

_Example:_

```c
TEST_ASSERT_X( {modifiers}, {expected}, actual, {size/count} )
```

becomes messageified like thus…

```c
TEST_ASSERT_X_MESSAGE( {modifiers}, {expected}, actual, {size/count}, message )
```

Notes:

- The `_MESSAGE` variants intentionally do not support `printf` style formatting
  since many embedded projects don’t support or avoid `printf` for various reasons.
  It is possible to use `sprintf` before the assertion to assemble a complex fail
  message, if necessary.
- If you want to output a counter value within an assertion fail message (e.g. from
  a loop) , building up an array of results and then using one of the `_ARRAY`
  assertions (see below) might be a handy alternative to `sprintf`.

#### TEST_ASSERT_X_ARRAY Variants

Unity provides a collection of assertions for arrays containing a variety of
types. These are documented in the Array section below. These are almost on par
with the `_MESSAGE`variants of Unity’s Asserts in that for pretty much any Unity
type assertion you can tack on `_ARRAY` and run assertions on an entire block of
memory.

```c
    TEST_ASSERT_EQUAL_TYPEX_ARRAY( expected, actual, {size/count} )
```

- `Expected` is an array itself.
- `Size/count` is one or two parameters necessary to establish the number of array
  elements and perhaps the length of elements within the array.

Notes:

- The `_MESSAGE` variant convention still applies here to array assertions. The
  `_MESSAGE` variants of the `_ARRAY` assertions have names ending with
  `_ARRAY_MESSAGE`.
- Assertions for handling arrays of floating point values are grouped with float
  and double assertions (see immediately following section).

### TEST_ASSERT_EACH_EQUAL_X Variants

Unity provides a collection of assertions for arrays containing a variety of
types which can be compared to a single value as well. These are documented in
the Each Equal section below. these are almost on par with the `_MESSAGE`
variants of Unity’s Asserts in that for pretty much any Unity type assertion you
can inject `_EACH_EQUAL` and run assertions on an entire block of memory.

```c
TEST_ASSERT_EACH_EQUAL_TYPEX( expected, actual, {size/count} )
```

- `Expected` is a single value to compare to.
- `Actual` is an array where each element will be compared to the expected value.
- `Size/count` is one of two parameters necessary to establish the number of array
  elements and perhaps the length of elements within the array.

Notes:

- The `_MESSAGE` variant convention still applies here to Each Equal assertions.
- Assertions for handling Each Equal of floating point values are grouped with
  float and double assertions (see immediately following section).

### Configuration

#### Floating Point Support Is Optional

Support for floating point types is configurable. That is, by defining the
appropriate preprocessor symbols, floats and doubles can be individually enabled
or disabled in Unity code. This is useful for embedded targets with no floating
point math support (i.e. Unity compiles free of errors for fixed point only
platforms). See Unity documentation for specifics.

#### Maximum Data Type Width Is Configurable

Not all targets support 64 bit wide types or even 32 bit wide types. Define the
appropriate preprocessor symbols and Unity will omit all operations from
compilation that exceed the maximum width of your target. See Unity
documentation for specifics.

## The Assertions in All Their Blessed Glory

### Basic Fail, Pass and Ignore

#### `TEST_FAIL()`

#### `TEST_FAIL_MESSAGE("message")`

This fella is most often used in special conditions where your test code is
performing logic beyond a simple assertion. That is, in practice, `TEST_FAIL()`
will always be found inside a conditional code block.

_Examples:_

- Executing a state machine multiple times that increments a counter your test
code then verifies as a final step.
- Triggering an exception and verifying it (as in Try / Catch / Throw - see the
[CException](https://github.com/ThrowTheSwitch/CException) project).

#### `TEST_PASS()`

#### `TEST_PASS_MESSAGE("message")`

This will abort the remainder of the test, but count the test as a pass. Under
normal circumstances, it is not necessary to include this macro in your tests…
a lack of failure will automatically be counted as a `PASS`. It is occasionally
useful for tests with `#ifdef`s and such.

#### `TEST_IGNORE()`

#### `TEST_IGNORE_MESSAGE("message")`

Marks a test case (i.e. function meant to contain test assertions) as ignored.
Usually this is employed as a breadcrumb to come back and implement a test case.
An ignored test case has effects if other assertions are in the enclosing test
case (see Unity documentation for more).

#### `TEST_MESSAGE(message)`

This can be useful for outputting `INFO` messages into the Unity output stream
without actually ending the test. Like pass and fail messages, it will be output
with the filename and line number.

### Boolean

#### `TEST_ASSERT (condition)`

#### `TEST_ASSERT_TRUE (condition)`

#### `TEST_ASSERT_FALSE (condition)`

#### `TEST_ASSERT_UNLESS (condition)`

A simple wording variation on `TEST_ASSERT_FALSE`.The semantics of
`TEST_ASSERT_UNLESS` aid readability in certain test constructions or
conditional statements.

#### `TEST_ASSERT_NULL (pointer)`

#### `TEST_ASSERT_NOT_NULL (pointer)`

Verify if a pointer is or is not NULL.

#### `TEST_ASSERT_EMPTY (pointer)`

#### `TEST_ASSERT_NOT_EMPTY (pointer)`

Verify if the first element dereferenced from a pointer is or is not zero. This
is particularly useful for checking for empty (or non-empty) null-terminated
C strings, but can be just as easily used for other null-terminated arrays.

### Signed and Unsigned Integers (of all sizes)

Large integer sizes can be disabled for build targets that do not support them.
For example, if your target only supports up to 16 bit types, by defining the
appropriate symbols Unity can be configured to omit 32 and 64 bit operations
that would break compilation (see Unity documentation for more). Refer to
Advanced Asserting later in this document for advice on dealing with other word
sizes.

#### `TEST_ASSERT_EQUAL_INT (expected, actual)`

#### `TEST_ASSERT_EQUAL_INT8 (expected, actual)`

#### `TEST_ASSERT_EQUAL_INT16 (expected, actual)`

#### `TEST_ASSERT_EQUAL_INT32 (expected, actual)`

#### `TEST_ASSERT_EQUAL_INT64 (expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT (expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT8 (expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT16 (expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT32 (expected, actual)`

#### `TEST_ASSERT_EQUAL_UINT64 (expected, actual)`

### Unsigned Integers (of all sizes) in Hexadecimal

All `_HEX` assertions are identical in function to unsigned integer assertions
but produce failure messages with the `expected` and `actual` values formatted
in hexadecimal. Unity output is big endian.

#### `TEST_ASSERT_EQUAL_HEX (expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX8 (expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX16 (expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX32 (expected, actual)`

#### `TEST_ASSERT_EQUAL_HEX64 (expected, actual)`

### Characters

While you can use the 8-bit integer assertions to compare `char`, another option is
to use this specialized assertion which will show printable characters as printables,
otherwise showing the HEX escape code for the characters.

#### `TEST_ASSERT_EQUAL_CHAR (expected, actual)`

### Masked and Bit-level Assertions

Masked and bit-level assertions produce output formatted in hexadecimal. Unity
output is big endian.

#### `TEST_ASSERT_BITS (mask, expected, actual)`

Only compares the masked (i.e. high) bits of `expected` and `actual` parameters.

#### `TEST_ASSERT_BITS_HIGH (mask, actual)`

Asserts the masked bits of the `actual` parameter are high.

#### `TEST_ASSERT_BITS_LOW (mask, actual)`

Asserts the masked bits of the `actual` parameter are low.

#### `TEST_ASSERT_BIT_HIGH (bit, actual)`

Asserts the specified bit of the `actual` parameter is high.

#### `TEST_ASSERT_BIT_LOW (bit, actual)`

Asserts the specified bit of the `actual` parameter is low.

### Integer Less Than / Greater Than

These assertions verify that the `actual` parameter is less than or greater
than `threshold` (exclusive). For example, if the threshold value is 0 for the
greater than assertion will fail if it is 0 or less. There are assertions for
all the various sizes of ints, as for the equality assertions. Some examples:

#### `TEST_ASSERT_GREATER_THAN_INT8 (threshold, actual)`

#### `TEST_ASSERT_GREATER_OR_EQUAL_INT16 (threshold, actual)`

#### `TEST_ASSERT_LESS_THAN_INT32 (threshold, actual)`

#### `TEST_ASSERT_LESS_OR_EQUAL_UINT (threshold, actual)`

#### `TEST_ASSERT_NOT_EQUAL_UINT8 (threshold, actual)`

### Integer Ranges (of all sizes)

These assertions verify that the `expected` parameter is within +/- `delta`
(inclusive) of the `actual` parameter. For example, if the expected value is 10
and the delta is 3 then the assertion will fail for any value outside the range
of 7 - 13.

#### `TEST_ASSERT_INT_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_INT8_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_INT16_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_INT32_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_INT64_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_UINT_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_UINT8_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_UINT16_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_UINT32_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_UINT64_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_HEX_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_HEX8_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_HEX16_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_HEX32_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_HEX64_WITHIN (delta, expected, actual)`

#### `TEST_ASSERT_CHAR_WITHIN (delta, expected, actual)`

### Structs and Strings

#### `TEST_ASSERT_EQUAL_PTR (expected, actual)`

Asserts that the pointers point to the same memory location.

#### `TEST_ASSERT_EQUAL_STRING (expected, actual)`

Asserts that the null terminated (`’\0’`)strings are identical. If strings are
of different lengths or any portion of the strings before their terminators
differ, the assertion fails. Two NULL strings (i.e. zero length) are considered
equivalent.

#### `TEST_ASSERT_EQUAL_MEMORY (expected, actual, len)`

Asserts that the contents of the memory specified by the `expected` and `actual`
pointers is identical. The size of the memory blocks in bytes is specified by
the `len` parameter.

### Arrays

`expected` and `actual` parameters are both arrays. `num_elements` specifies the
number of elements in the arrays to compare.

`_HEX` assertions produce failure messages with expected and actual array
contents formatted in hexadecimal.

For array of strings comparison behavior, see comments for
`TEST_ASSERT_EQUAL_STRING` in the preceding section.

Assertions fail upon the first element in the compared arrays found not to
match. Failure messages specify the array index of the failed comparison.

#### `TEST_ASSERT_EQUAL_INT_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT8_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT16_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT32_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_INT64_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT8_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT16_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT32_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_UINT64_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX8_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX16_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX32_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_HEX64_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_CHAR_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_PTR_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_STRING_ARRAY (expected, actual, num_elements)`

#### `TEST_ASSERT_EQUAL_MEMORY_ARRAY (expected, actual, len, num_elements)`

`len` is the memory in bytes to be compared at each array element.

### Integer Array Ranges (of all sizes)

These assertions verify that the `expected` array parameter is within +/- `delta`
(inclusive) of the `actual` array parameter. For example, if the expected value is
\[10, 12\] and the delta is 3 then the assertion will fail for any value
outside the range of \[7 - 13, 9 - 15\].

#### `TEST_ASSERT_INT_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT8_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT16_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT32_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_INT64_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT8_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT16_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT32_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_UINT64_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX8_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX16_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX32_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_HEX64_ARRAY_WITHIN (delta, expected, actual, num_elements)`

#### `TEST_ASSERT_CHAR_ARRAY_WITHIN (delta, expected, actual, num_elements)`

### Each Equal (Arrays to Single Value)

`expected` are single values and `actual` are arrays. `num_elements` specifies
the number of elements in the arrays to compare.

`_HEX` assertions produce failure messages with expected and actual array
contents formatted in hexadecimal.

Assertions fail upon the first element in the compared arrays found not to
match. Failure messages specify the array index of the failed comparison.

#### `TEST_ASSERT_EACH_EQUAL_INT (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT8 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT16 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT32 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_INT64 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT8 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT16 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT32 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_UINT64 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX8 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX16 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX32 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_HEX64 (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_CHAR (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_PTR (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_STRING (expected, actual, num_elements)`

#### `TEST_ASSERT_EACH_EQUAL_MEMORY (expected, actual, len, num_elements)`

`len` is the memory in bytes to be compared at each array element.

### Floating Point (If enabled)

#### `TEST_ASSERT_FLOAT_WITHIN (delta, expected, actual)`

Asserts that the `actual` value is within +/- `delta` of the `expected` value.
The nature of floating point representation is such that exact evaluations of
equality are not guaranteed.

#### `TEST_ASSERT_FLOAT_NOT_WITHIN (delta, expected, actual)`

Asserts that the `actual` value is NOT within +/- `delta` of the `expected` value.

#### `TEST_ASSERT_EQUAL_FLOAT (expected, actual)`

Asserts that the `actual` value is “close enough to be considered equal” to the
`expected` value. If you are curious about the details, refer to the Advanced
Asserting section for more details on this. Omitting a user-specified delta in a
floating point assertion is both a shorthand convenience and a requirement of
code generation conventions for CMock.

#### `TEST_ASSERT_NOT_EQUAL_FLOAT (expected, actual)`

Asserts that the `actual` value is NOT “close enough to be considered equal” to the
`expected` value.

#### `TEST_ASSERT_FLOAT_ARRAY_WITHIN (delta, expected, actual, num_elements)`

See Array assertion section for details. Note that individual array element
uses user-provided delta plus default comparison delta for checking
and is based on `TEST_ASSERT_FLOAT_WITHIN` comparison.

#### `TEST_ASSERT_EQUAL_FLOAT_ARRAY (expected, actual, num_elements)`

See Array assertion section for details. Note that individual array element
float comparisons are executed using `TEST_ASSERT_EQUAL_FLOAT`. That is, user
specified delta comparison values requires a custom-implemented floating point
array assertion.

#### `TEST_ASSERT_LESS_THAN_FLOAT (threshold, actual)`

Asserts that the `actual` parameter is less than `threshold` (exclusive).
For example, if the threshold value is 1.0f, the assertion will fail if it is
greater than 1.0f.

#### `TEST_ASSERT_GREATER_THAN_FLOAT (threshold, actual)`

Asserts that the `actual` parameter is greater than `threshold` (exclusive).
For example, if the threshold value is 1.0f, the assertion will fail if it is
less than 1.0f.

#### `TEST_ASSERT_LESS_OR_EQUAL_FLOAT (threshold, actual)`

Asserts that the `actual` parameter is less than or equal to `threshold`.
The rules for equality are the same as for `TEST_ASSERT_EQUAL_FLOAT`.

#### `TEST_ASSERT_GREATER_OR_EQUAL_FLOAT (threshold, actual)`

Asserts that the `actual` parameter is greater than `threshold`.
The rules for equality are the same as for `TEST_ASSERT_EQUAL_FLOAT`.

#### `TEST_ASSERT_FLOAT_IS_INF (actual)`

Asserts that `actual` parameter is equivalent to positive infinity floating
point representation.

#### `TEST_ASSERT_FLOAT_IS_NEG_INF (actual)`

Asserts that `actual` parameter is equivalent to negative infinity floating
point representation.

#### `TEST_ASSERT_FLOAT_IS_NAN (actual)`

Asserts that `actual` parameter is a Not A Number floating point representation.

#### `TEST_ASSERT_FLOAT_IS_DETERMINATE (actual)`

Asserts that `actual` parameter is a floating point representation usable for
mathematical operations. That is, the `actual` parameter is neither positive
infinity nor negative infinity nor Not A Number floating point representations.

#### `TEST_ASSERT_FLOAT_IS_NOT_INF (actual)`

Asserts that `actual` parameter is a value other than positive infinity floating
point representation.

#### `TEST_ASSERT_FLOAT_IS_NOT_NEG_INF (actual)`

Asserts that `actual` parameter is a value other than negative infinity floating
point representation.

#### `TEST_ASSERT_FLOAT_IS_NOT_NAN (actual)`

Asserts that `actual` parameter is a value other than Not A Number floating
point representation.

#### `TEST_ASSERT_FLOAT_IS_NOT_DETERMINATE (actual)`

Asserts that `actual` parameter is not usable for mathematical operations. That
is, the `actual` parameter is either positive infinity or negative infinity or
Not A Number floating point representations.

### Double (If enabled)

#### `TEST_ASSERT_DOUBLE_WITHIN (delta, expected, actual)`

Asserts that the `actual` value is within +/- `delta` of the `expected` value.
The nature of floating point representation is such that exact evaluations of
equality are not guaranteed.

#### `TEST_ASSERT_DOUBLE_NOT_WITHIN (delta, expected, actual)`

Asserts that the `actual` value is NOT within +/- `delta` of the `expected` value.

#### `TEST_ASSERT_EQUAL_DOUBLE (expected, actual)`

Asserts that the `actual` value is “close enough to be considered equal” to the
`expected` value. If you are curious about the details, refer to the Advanced
Asserting section for more details. Omitting a user-specified delta in a
floating point assertion is both a shorthand convenience and a requirement of
code generation conventions for CMock.

#### `TEST_ASSERT_NOT_EQUAL_DOUBLE (expected, actual)`

Asserts that the `actual` value is NOT “close enough to be considered equal” to the
`expected` value.

#### `TEST_ASSERT_DOUBLE_ARRAY_WITHIN (delta, expected, actual, num_elements)`

See Array assertion section for details. Note that individual array element
uses user-provided delta plus default comparison delta  for checking
and is based on `TEST_ASSERT_DOUBLE_WITHIN` comparison.

#### `TEST_ASSERT_EQUAL_DOUBLE_ARRAY (expected, actual, num_elements)`

See Array assertion section for details. Note that individual array element
double comparisons are executed using `TEST_ASSERT_EQUAL_DOUBLE`. That is, user
specified delta comparison values requires a custom implemented double array
assertion.

#### `TEST_ASSERT_LESS_THAN_DOUBLE (threshold, actual)`

Asserts that the `actual` parameter is less than `threshold` (exclusive).
For example, if the threshold value is 1.0, the assertion will fail if it is
greater than 1.0.

#### `TEST_ASSERT_LESS_OR_EQUAL_DOUBLE (threshold, actual)`

Asserts that the `actual` parameter is less than or equal to `threshold`.
The rules for equality are the same as for `TEST_ASSERT_EQUAL_DOUBLE`.

#### `TEST_ASSERT_GREATER_THAN_DOUBLE (threshold, actual)`

Asserts that the `actual` parameter is greater than `threshold` (exclusive).
For example, if the threshold value is 1.0, the assertion will fail if it is
less than 1.0.

#### `TEST_ASSERT_GREATER_OR_EQUAL_DOUBLE (threshold, actual)`

Asserts that the `actual` parameter is greater than or equal to `threshold`.
The rules for equality are the same as for `TEST_ASSERT_EQUAL_DOUBLE`.

#### `TEST_ASSERT_DOUBLE_IS_INF (actual)`

Asserts that `actual` parameter is equivalent to positive infinity floating
point representation.

#### `TEST_ASSERT_DOUBLE_IS_NEG_INF (actual)`

Asserts that `actual` parameter is equivalent to negative infinity floating point
representation.

#### `TEST_ASSERT_DOUBLE_IS_NAN (actual)`

Asserts that `actual` parameter is a Not A Number floating point representation.

#### `TEST_ASSERT_DOUBLE_IS_DETERMINATE (actual)`

Asserts that `actual` parameter is a floating point representation usable for
mathematical operations. That is, the `actual` parameter is neither positive
infinity nor negative infinity nor Not A Number floating point representations.

#### `TEST_ASSERT_DOUBLE_IS_NOT_INF (actual)`

Asserts that `actual` parameter is a value other than positive infinity floating
point representation.

#### `TEST_ASSERT_DOUBLE_IS_NOT_NEG_INF (actual)`

Asserts that `actual` parameter is a value other than negative infinity floating
point representation.

#### `TEST_ASSERT_DOUBLE_IS_NOT_NAN (actual)`

Asserts that `actual` parameter is a value other than Not A Number floating
point representation.

#### `TEST_ASSERT_DOUBLE_IS_NOT_DETERMINATE (actual)`

Asserts that `actual` parameter is not usable for mathematical operations. That
is, the `actual` parameter is either positive infinity or negative infinity or
Not A Number floating point representations.

## Advanced Asserting: Details On Tricky Assertions

This section helps you understand how to deal with some of the trickier
assertion situations you may run into. It will give you a glimpse into some of
the under-the-hood details of Unity’s assertion mechanisms. If you’re one of
those people who likes to know what is going on in the background, read on. If
not, feel free to ignore the rest of this document until you need it.

### How do the EQUAL assertions work for FLOAT and DOUBLE?

As you may know, directly checking for equality between a pair of floats or a
pair of doubles is sloppy at best and an outright no-no at worst. Floating point
values can often be represented in multiple ways, particularly after a series of
operations on a value. Initializing a variable to the value of 2.0 is likely to
result in a floating point representation of 2 x 20,but a series of
mathematical operations might result in a representation of 8 x 2-2
that also evaluates to a value of 2. At some point repeated operations cause
equality checks to fail.

So Unity doesn’t do direct floating point comparisons for equality. Instead, it
checks if two floating point values are “really close.” If you leave Unity
running with defaults, “really close” means “within a significant bit or two.”
Under the hood, `TEST_ASSERT_EQUAL_FLOAT` is really `TEST_ASSERT_FLOAT_WITHIN`
with the `delta` parameter calculated on the fly. For single precision, delta is
the expected value multiplied by 0.00001, producing a very small proportional
range around the expected value.

If you are expecting a value of 20,000.0 the delta is calculated to be 0.2. So
any value between 19,999.8 and 20,000.2 will satisfy the equality check. This
works out to be roughly a single bit of range for a single-precision number, and
that’s just about as tight a tolerance as you can reasonably get from a floating
point value.

So what happens when it’s zero? Zero - even more than other floating point
values - can be represented many different ways. It doesn’t matter if you have
0x20 or 0x263. It’s still zero, right? Luckily, if you subtract these 
values from each other, they will always produce a difference of zero, which 
will still fall between 0 plus or minus a delta of 0. So it still works!

Double precision floating point numbers use a much smaller multiplier, again
approximating a single bit of error.

If you don’t like these ranges and you want to make your floating point equality
assertions less strict, you can change these multipliers to whatever you like by
defining UNITY_FLOAT_PRECISION and UNITY_DOUBLE_PRECISION. See Unity
documentation for more.

### How do we deal with targets with non-standard int sizes?

It’s “fun” that C is a standard where something as fundamental as an integer
varies by target. According to the C standard, an `int` is to be the target’s
natural register size, and it should be at least 16-bits and a multiple of a
byte. It also guarantees an order of sizes:

```C
char <= short <= int <= long <= long long
```

Most often, `int` is 32-bits. In many cases in the embedded world, `int` is
16-bits. There are rare microcontrollers out there that have 24-bit integers,
and this remains perfectly standard C.

To make things even more interesting, there are compilers and targets out there
that have a hard choice to make. What if their natural register size is 10-bits
or 12-bits? Clearly they can’t fulfill _both_ the requirement to be at least
16-bits AND the requirement to match the natural register size. In these
situations, they often choose the natural register size, leaving us with
something like this:

```C
char (8 bit) <= short (12 bit) <= int (12 bit) <= long (16 bit)
```

Um… yikes. It’s obviously breaking a rule or two… but they had to break SOME
rules, so they made a choice.

When the C99 standard rolled around, it introduced alternate standard-size types.
It also introduced macros for pulling in MIN/MAX values for your integer types.
It’s glorious! Unfortunately, many embedded compilers can’t be relied upon to
use the C99 types (Sometimes because they have weird register sizes as described
above. Sometimes because they don’t feel like it?).

A goal of Unity from the beginning was to support every combination of
microcontroller or microprocessor and C compiler. Over time, we’ve gotten really
close to this. There are a few tricks that you should be aware of, though, if
you’re going to do this effectively on some of these more idiosyncratic targets.

First, when setting up Unity for a new target, you’re going to want to pay
special attention to the macros for automatically detecting types
(where available) or manually configuring them yourself. You can get information
on both of these in Unity’s documentation.

What about the times where you suddenly need to deal with something odd, like a
24-bit `int`? The simplest solution is to use the next size up. If you have a
24-bit `int`, configure Unity to use 32-bit integers. If you have a 12-bit
`int`, configure Unity to use 16 bits. There are two ways this is going to
affect you:

1. When Unity displays errors for you, it’s going to pad the upper unused bits
   with zeros.
2. You’re going to have to be careful of assertions that perform signed
   operations, particularly `TEST_ASSERT_INT_WITHIN`. Such assertions might wrap
   your `int` in the wrong place, and you could experience false failures. You can
   always back down to a simple `TEST_ASSERT` and do the operations yourself.

*Find The Latest of This And More at [ThrowTheSwitch.org][]*

[assert() macro]: http://en.wikipedia.org/wiki/Assert.h
[ThrowTheSwitch.org]: https://throwtheswitch.org
