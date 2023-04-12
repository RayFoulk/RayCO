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
#include "console.h"
#include "mut.h"

#include <string.h>
#include <limits.h>
#include <stdbool.h>
#include <unistd.h>

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_console.log");
    BLAMMO(INFO, "console tests...");

TEST_BEGIN("test console")
    console_t * console = console_pub.create(stdin, stdout, "test-history.txt");

    CHECK(console != NULL);

    console->destroy(console);
TEST_END

TEST_BEGIN("test lock/unlock")
TEST_END

TEST_BEGIN("test set callbacks")
TEST_END

TEST_BEGIN("test tab completion")
TEST_END

TEST_BEGIN("test get line")
TEST_END

TEST_BEGIN("test warning")
    console_t * console = console_pub.create(stdin, stdout, NULL);
    console->warning(console, "something could be wrong! %d", 777);
    console->destroy(console);
TEST_END

TEST_BEGIN("test error")
    console_t * console = console_pub.create(stdin, stdout, NULL);
    console->error(console, "something is definitely wrong! %d", 5555);
    console->destroy(console);
TEST_END

TEST_BEGIN("test print")
    console_t * console = console_pub.create(stdin, stdout, NULL);
    console->print(console, "howdy doody %d", 99);
    console->destroy(console);
TEST_END

TEST_BEGIN("test reprint")
    // This is an Inspection test
    console_t * console = console_pub.create(stdin, stdout, NULL);
    int i = 0;

    console->reprint(console, NULL);
    for(i = 0; i < 9999; i++)
    {
        console->reprint(console, "i: %d", i);
        usleep(100);
    }

    console->destroy(console);

TEST_END

TESTSUITE_END

