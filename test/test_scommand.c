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

#include <string.h>
#include <limits.h>
#include <stdbool.h>

#include "blammo.h"
#include "utils.h"
#include "scommand.h"
#include "mut.h"

static int bogus_scallcmd_handler(void * scallop,
                                     void * context,
                                     int argc,
                                     char ** argv)
{
    BLAMMO(INFO, "called");
    return 0;
}

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_scallcmd.log");
    BLAMMO(INFO, "scallcmd tests...");

TEST_BEGIN("test create/destroy")
    scallop_cmd_t * scallcmd = scallop_cmd_pub.create(bogus_scallcmd_handler,
                                                      NULL,
                                                      "test",
                                                      " <hint>",
                                                      "a bogus test command");
    CHECK(scallcmd != NULL);

    scallcmd->destroy(scallcmd);

TEST_END

TEST_BEGIN("test register/unregister")
    CHECK(true);
TEST_END

TEST_BEGIN("test deep destroy")
    CHECK(true);
TEST_END

TESTSUITE_END

