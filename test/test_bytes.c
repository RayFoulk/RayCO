//------------------------------------------------------------------------|
// Copyright (c) 2018-2023 by Raymond M. Foulk IV (rfoulk@gmail.com)
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

#include "blammo.h"
#include "bytes.h"
#include "prng.h"
#include "mut.h"

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_bytes.log");
    BLAMMO(INFO, "bytes tests...");

TEST_BEGIN("create/size/empty")
    bytes_t * bytes = bytes_pub.create("hello", 5);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);
    CHECK(!bytes->empty(bytes));
    CHECK(bytes->size(bytes) == 5);
    bytes->destroy(bytes);

    bytes = bytes_pub.create(NULL, 0);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);
    CHECK(bytes->empty(bytes));
    CHECK(bytes->size(bytes) == 0);
    bytes->destroy(bytes);

    bytes = bytes_pub.create(NULL, 64);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);
    CHECK(!bytes->empty(bytes));
    CHECK(bytes->size(bytes) == 64);
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("destroy")
    // Would need to mock malloc/free for this.
    // Maybe by LD_PRELOAD voodoo TBD later.
    // or else use some kind of MALLOC macro.
    // Alternatively use valgrind on tests.
TEST_END

TEST_BEGIN("data")
    const uint8_t stuff[] = {
        0xDE, 0xAD, 0xBE, 0xEF,
        0xCA, 0xFE, 0xBA, 0xBE,
    };

    bytes_t * bytes = bytes_pub.create(stuff, sizeof(stuff));
    CHECK(bytes != NULL);
    CHECK(memcmp(bytes->data(bytes), stuff, sizeof(stuff)) == 0);
    CHECK(bytes->size(bytes) == sizeof(stuff));
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("cstr")
    const char * str = "The quick brown fox jumped over the lazy dog.";
    bytes_t * bytes = bytes_pub.create(str, strlen(str));
    CHECK(bytes != NULL);
    CHECK(strncmp(bytes->cstr(bytes), str, strlen(str)) == 0);
    CHECK(bytes->size(bytes) == strlen(str));
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("empty/clear")
    const char * str = "The quick brown fox jumped over the lazy dog.";
    size_t len = strlen(str);
    bytes_t * bytes = bytes_pub.create(str, len);
    CHECK(bytes != NULL);
    CHECK(bytes->size(bytes) == len);
    CHECK(!bytes->empty(bytes));
    bytes->clear(bytes);
    CHECK(bytes != NULL);
    CHECK(bytes->size(bytes) == 0);
    CHECK(bytes->empty(bytes));
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("resize/size")
    const char * str = "The quick brown fox jumped over the lazy dog.";
    size_t len = strlen(str);
    bytes_t * bytes = bytes_pub.create(str, len);
    CHECK(bytes->size(bytes) == len);

    bytes->resize(bytes, len * 2);
    CHECK(bytes->size(bytes) == len * 2);
    CHECK(strncmp(bytes->cstr(bytes), str, strlen(str)) == 0);

    bytes->resize(bytes, len / 2);
    CHECK(bytes->size(bytes) == len / 2);
    CHECK(strncmp(bytes->cstr(bytes), str, strlen(str) / 2) == 0);
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("print")
    bytes_t * bytes = bytes_pub.create(NULL, 0);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);

    // Test some common format types. but no float/double!
    // TODO expand these later,
    const char * expect = "hello -9 9 55 AA 2147483648";
    size_t len = strlen(expect);

    ssize_t result = bytes->print(bytes,
            "%s %d %d %x %X %u", "hello",
            -9, 9, 0x55, 0xAA, 0x1 << 31);

    BLAMMO(INFO, "size: %zu  len: %zu", bytes->size(bytes), len);
    BLAMMO(INFO, "cstr: \'%s\'", bytes->cstr(bytes));
    BLAMMO(INFO, "hexdump:\n%s\n", bytes->hexdump(bytes));

    CHECK(strcmp(expect, bytes->cstr(bytes)) == 0);
    CHECK(bytes->size(bytes) == len);
    CHECK(result == len);

    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("assign")
    bytes_t * bytes = bytes_pub.create(NULL, 0);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);

    const char * astr = "one two three";
    const char * bstr = "one two three";
    const uint8_t data[] = { 'o', 'n', 'e', ' ',
                             't', 'w', 'o', ' ',
                             't', 'h', 'r', 'e',
                             'e',  0,   0,   0,
                              0,   0,   0,   0  };

    // buffer is intentionally oversized
    // TODO: Turns out this does exactly what one
    // should expect.  Problem is it gets the missing 6
    // bytes from an overrun of the data segment!
    // Would need to have a separate assign_str method
    //bytes->assign(bytes, astr, 20);
    bytes->assign(bytes, astr, 13);
    bytes->resize(bytes, 20);
    BLAMMO(INFO, "hexdump:\n%s", bytes->hexdump(bytes));

    CHECK(strcmp(bytes->cstr(bytes), bstr) == 0);
    CHECK(bytes->size(bytes) == 20);

    CHECK(memcmp(bytes->data(bytes), data, 20) == 0);
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("append")
    bytes_t * bytes = bytes_pub.create("abc", 3);
    bytes_t * suffix = bytes_pub.create("defg", 4);
    CHECK(bytes->size(bytes) == 3);
    CHECK(suffix->size(suffix) == 4);

    bytes->append(bytes, suffix->data(suffix), suffix->size(suffix));
    CHECK(bytes->size(bytes) == 7);
    CHECK(suffix->size(suffix) == 4);
    //BLAMMO(INFO, "bytes->cstr() is %s", bytes->cstr(bytes));
    CHECK(strcmp(bytes->cstr(bytes), "abcdefg") == 0);

    bytes->destroy(bytes);
    suffix->destroy(suffix);

TEST_END

TEST_BEGIN("print/append")
    bytes_t * a = bytes_pub.create(NULL, 0);
    bytes_t * b = bytes_pub.create(NULL, 0);

    a->print(a, "abc%d", 1);
    BLAMMO(INFO, "a hexdump:\n%s\n", a->hexdump(a));

    b->print(b, "def%d", 2);
    BLAMMO(INFO, "b hexdump:\n%s\n", b->hexdump(b));

    a->append(a, b->data(b), b->size(b));
    BLAMMO(INFO, "a' hexdump:\n%s\n", a->hexdump(a));
    BLAMMO(INFO, "a' str: %s", a->cstr(a));

    a->destroy(a);
    b->destroy(b);

TEST_END



TEST_BEGIN("read_at")
    const char * str = "abc123";
    size_t len = strlen(str);
    char buffer[8] = { 0x00 };
    bytes_t * bytes = bytes_pub.create(str, len);
    bytes->read_at(bytes, buffer, 1, 3);

    BLAMMO(INFO, "read_at buffer: %s\n", buffer);
    CHECK(strcmp(buffer, "1") == 0);

    // TODO: more cases, overlapping
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("write_at")
TEST_END

TEST_BEGIN("rtrim")

    const char * str = "abc123  \t \n\n ";
    bytes_t * bytes = bytes_pub.create(str, strlen(str));
    bytes->rtrim(bytes, " \t\n");
    CHECK(strcmp(bytes->cstr(bytes), "abc123") == 0)
    bytes->destroy(bytes);

TEST_END

TEST_BEGIN("ltrim")

    const char * str = " \t\t  \n abc123";
    bytes_t * bytes = bytes_pub.create(str, strlen(str));
    bytes->ltrim(bytes, " \t\n");
    CHECK(strcmp(bytes->cstr(bytes), "abc123") == 0)
    bytes->destroy(bytes);

TEST_END

TEST_BEGIN("trim")

    const char * str = "  \t\n  \t abc123  \n \t   ";
    bytes_t * bytes = bytes_pub.create(str, strlen(str));
    bytes->trim(bytes, " \t\n");
    CHECK(strcmp(bytes->cstr(bytes), "abc123") == 0)
    bytes->destroy(bytes);

TEST_END

TEST_BEGIN("copy")
TEST_END

TEST_BEGIN("split")
TEST_END

TEST_BEGIN("join")
TEST_END

TEST_BEGIN("hexdump")
    bytes_t * bytes = bytes_pub.create(NULL, 37);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);

    prng_seed(0xDEADBEEFCAFEBABEULL);
    prng_fill((uint8_t *)bytes->data(bytes), bytes->size(bytes));
    //BLAMMO(INFO, "hexdump:\n%s", bytes->hexdump(bytes));

    bytes->destroy(bytes);
TEST_END

TESTSUITE_END

