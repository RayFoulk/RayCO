//------------------------------------------------------------------------|
// Copyright (c) 2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
//------------------------------------------------------------------------|

#include "blammo.h"
#include "utils.h"
#include "sparser.h"
#include "prng.h"
#include "mut.h"

#include <string.h>
#include <limits.h>
#include <stdbool.h>

// Simple evaluator using default error print function
static long evalexpr(const char * expression)
{
    return sparser_evaluate(sparser_errprintf,
                            stderr,
                            expression);
}

TESTSUITE_BEGIN

    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_sparser.log");
    BLAMMO(INFO, "sparser tests...");

TEST_BEGIN("addition")
    CHECK(evalexpr("2 + 3") == 5);
    CHECK(evalexpr("5678 + 998877") == 1004555);
    CHECK(evalexpr("((1 + 2) + 4)") == 7)
    CHECK(evalexpr("((5 + 5) + (4 + 4))") == 18)
    CHECK(evalexpr("1 + 2 + 3 + 4 + 5") == 15)
    // TODO: Check a bunch of randomly generated values
TEST_END

TEST_BEGIN("subtraction")
    CHECK(evalexpr("3 - 2") == 1);
    CHECK(evalexpr("999 - 777") == 222);
    CHECK(evalexpr("((9 - 5) - 1)") == 3)
    CHECK(evalexpr("((99 - 1) - (55 - 4))") == 47)
    CHECK(evalexpr("77 - 55 - 33 - 11") == -22)
TEST_END

TEST_BEGIN("multiplication")
    CHECK(evalexpr("2 * 3") == 6);
    CHECK(evalexpr("1 + 2 * 3") == 7);
    CHECK(evalexpr("5 * 3 + 2 * 3") == 21);
    CHECK(evalexpr("11 * 13") == 143);
TEST_END

TEST_BEGIN("division")
    CHECK(evalexpr("3 / 2") == 1);
    CHECK(evalexpr("4 / 2") == 2);
    CHECK(evalexpr("222 / 11") == 20);
    CHECK(evalexpr("3 / 2 + 5 / 2") == 3);
TEST_END

TEST_BEGIN("boolean logical")
    CHECK(evalexpr("1 && 1"));
    CHECK(evalexpr("30 && 50"));
    CHECK(!evalexpr("000 && 999"));
    CHECK(!evalexpr("(0 && 1)"));

    CHECK(evalexpr("0 || 1"));
    CHECK(evalexpr("1 || 0"));
    CHECK(!evalexpr("0 || 0"));
    CHECK(evalexpr("1 || 1"));

    CHECK(evalexpr("((0 && 1) || (7 && 5))"));
    CHECK(evalexpr("((3 && 1) || (7 && 0))"));
    CHECK(!evalexpr("((0 && 1) || (7 && 0))"));

    CHECK(evalexpr("((1 || 0) && (22 || 44))"));
    CHECK(evalexpr("((3 || 1) && (7 || 0))"));
    CHECK(evalexpr("((77 || 99) && (5 || 4))"));
    CHECK(!evalexpr("((0 || 0) && (0 || 0))"));
TEST_END

TEST_BEGIN("whitespace")
    CHECK(evalexpr("11+22") == 33);
    CHECK(evalexpr("  11+22") == 33);
    CHECK(evalexpr("11  +  22") == 33);
    CHECK(evalexpr("11+22  \t\n    ") == 33);
TEST_END

TEST_BEGIN("bad parenthesis")
    CHECK(evalexpr("(((1 && 1))") == SPARSER_INVALID_EXPRESSION);
    CHECK(evalexpr("((((1 + 1) + (2 + 2))") == SPARSER_INVALID_EXPRESSION);
    //CHECK(evalexpr("(((1 || 1)))))") == SPARSER_INVALID_EXPRESSION);
    CHECK(evalexpr("(1))") == SPARSER_INVALID_EXPRESSION);
TEST_END

TEST_BEGIN("bad characters")
    CHECK(evalexpr("(^%# == !@#%)") == SPARSER_INVALID_EXPRESSION);
    CHECK(evalexpr("((9 == 9.0) && (1))") == SPARSER_INVALID_EXPRESSION);

    // TODO: This is allowed because the lack of outer parenthesis
    // and evaluation stops when '.' is detected as not-a-digit.
    // is this acceptable or a problem?
    CHECK(evalexpr("9 == 9.0"));
TEST_END

TEST_BEGIN("numeric comparison")
    CHECK(evalexpr("1 > 0"));
    CHECK(evalexpr("99 > 77"));
    CHECK(!evalexpr("55 > 77"));
    CHECK(evalexpr("-5 < 5"));
    CHECK(evalexpr("1 < 2"));
    CHECK(!evalexpr("3 < 2"));
    CHECK(evalexpr("2 >= 1"));
    CHECK(evalexpr("55 >= 55"));
    CHECK(!evalexpr("74 >= 75"));
    CHECK(evalexpr("5 <= 6"));
    CHECK(evalexpr("66 <= 66"));
    CHECK(!evalexpr("44 <= 43"));
    CHECK(evalexpr("123 == 123"));
    CHECK(!evalexpr("321 == 123"));
    CHECK(evalexpr("333 != 555"));
    CHECK(!evalexpr("333 != 333"));
TEST_END

TEST_BEGIN("string comparison")
    CHECK(evalexpr("valid == valid"));
    CHECK(!evalexpr("valid == invalid"));
    CHECK(evalexpr("\"quoted\" == \"quoted\""));
    CHECK(evalexpr("\"quoted\" == quoted"));
    CHECK(evalexpr("quarks != muons"));
    CHECK(evalexpr("valid != invalid"));
    CHECK(!evalexpr("roses != roses"));
TEST_END

TESTSUITE_END
