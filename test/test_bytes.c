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
    BLAMMO_LEVEL(DEBUG);
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

TEST_BEGIN("print_create")
    bytes_t * bytes = bytes_pub.print_create("%s %s %d", "yankee", "doodle", 76);
    CHECK(bytes != NULL);
    CHECK(bytes->priv != NULL);
    CHECK(!bytes->empty(bytes));
    CHECK(strcmp(bytes->cstr(bytes), "yankee doodle 76") == 0);
    bytes->destroy(bytes);
TEST_END


TEST_BEGIN("destroy")
    // Would need to mock malloc/free for this.
    // Maybe by LD_PRELOAD voodoo TBD later.
    // or else use some kind of MALLOC macro.
    // Alternatively use valgrind on tests.
TEST_END

TEST_BEGIN("compare")
    bytes_t * a = bytes_pub.create("asdfvcxz", 8);
    bytes_t * b = bytes_pub.create("asdfvcxz", 8);
    bytes_t * c = bytes_pub.create("qwertyui", 8);
    bytes_t * d = bytes_pub.create("qwerty", 6);

    CHECK(a->compare(a, b) == 0);
    CHECK(a->compare(a, c) < 0);
    CHECK(c->compare(c, d) > 0);

    a->destroy(a);
    b->destroy(b);
    c->destroy(c);
    d->destroy(d);
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

    //BLAMMO(INFO, "size: %zu  len: %zu", bytes->size(bytes), len);
    //BLAMMO(INFO, "cstr: \'%s\'", bytes->cstr(bytes));
    //BLAMMO(INFO, "hexdump:\n%s\n", bytes->hexdump(bytes));

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
    //BLAMMO(INFO, "hexdump:\n%s", bytes->hexdump(bytes));

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
    //BLAMMO(INFO, "a hexdump:\n%s\n", a->hexdump(a));

    b->print(b, "def%d", 2);
    //BLAMMO(INFO, "b hexdump:\n%s\n", b->hexdump(b));

    a->append(a, b->data(b), b->size(b));
    //BLAMMO(INFO, "a' hexdump:\n%s\n", a->hexdump(a));
    //BLAMMO(INFO, "a' str: %s", a->cstr(a));
    CHECK(memcmp(a->data(a), "abc1def2", a->size(a)) == 0);

    a->destroy(a);
    b->destroy(b);

TEST_END

TEST_BEGIN("read_at")
    const char * str = "abc123";
    size_t len = strlen(str);
    char buffer[8] = { 0x00 };
    bytes_t * bytes = bytes_pub.create(str, len);
    bytes->read_at(bytes, buffer, 1, 3);

    //BLAMMO(INFO, "read_at buffer: %s\n", buffer);
    CHECK(strcmp(buffer, "1") == 0);

    // TODO: more cases, overlapping
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("write_at")
    BLAMMO(INFO, "FIXME: TEST NOT IMPLEMENTED");
TEST_END

TEST_BEGIN("trim_left")
    const char * str = " \t\t  \n abc123";
    bytes_t * bytes = bytes_pub.create(str, strlen(str));
    bytes->trim_left(bytes, " \t\n");
    CHECK(strcmp(bytes->cstr(bytes), "abc123") == 0)
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("trim_right")
    const char * str = "abc123  \t \n\n ";
    bytes_t * bytes = bytes_pub.create(str, strlen(str));
    bytes->trim_right(bytes, " \t\n");
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

TEST_BEGIN("find_forward")
    bytes_t * bytes = bytes_pub.create("abcdefghijkl", 12);
    CHECK(bytes->find_forward(bytes, 0, "ghi", 3) == 6);
    CHECK(bytes->find_forward(bytes, 0, "zzz", 3) < 0);
    CHECK(bytes->find_forward(bytes, 0, "kl", 2) == 10);
    CHECK(bytes->find_forward(bytes, 0, "abc", 3) == 0);
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("find_reverse")
    bytes_t * bytes = bytes_pub.create("mnopqrstuvwxyz", 14);
    CHECK(bytes->find_reverse(bytes, 99, "pqrs", 4) == 3);
    CHECK(bytes->find_reverse(bytes, 99, "mnomnop", 7) < 0);
    CHECK(bytes->find_reverse(bytes, bytes->size(bytes), "mnop", 4) == 0);
    CHECK(bytes->find_reverse(bytes, bytes->size(bytes), "xyz", 3) == 11);
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("fill")
    bytes_t * bytes = bytes_pub.create(NULL, 20);
    bytes->fill(bytes, 'A');
    CHECK(strcmp((const char *) bytes->data(bytes), "AAAAAAAAAAAAAAAAAAAA") == 0);
    bytes->resize(bytes, 10);
    bytes->fill(bytes, '5');
    CHECK(strcmp((const char *) bytes->data(bytes), "5555555555") == 0);
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("copy")
    bytes_t * a = bytes_pub.create("qwertyuiop", 10);
    bytes_t * b = a->copy(a);

    CHECK(a->compare(a, b) == 0);
    CHECK(a->data(a) != b->data(b));

    a->destroy(a);
    b->destroy(b);
TEST_END

TEST_BEGIN("tokenizer_basic")

    bool split = true;
    const char * a = "one two three #comment";
    const char * b = "token_one  \"token two quoted\" token_three";
    const char * c = "((x == y) && (w != z)) two three \"four is quoted\" five #comment";

    const char *encaps[] = {
            "\"\"",
            "()",
            NULL
    };

    bytes_t * bytes = bytes_pub.create(a, strlen(a));
    size_t ntokens = 0;
    char ** tokens = bytes->tokenizer(bytes, split, NULL, " ", "#", &ntokens);
    CHECK(ntokens == 3);
    CHECK(!strcmp(tokens[0], "one"));
    CHECK(!strcmp(tokens[1], "two"));
    CHECK(!strcmp(tokens[2], "three"));
    CHECK(tokens[3] == NULL);

    bytes->assign(bytes, b, strlen(b));
    tokens = bytes->tokenizer(bytes, split, encaps, " ", "#", &ntokens);
    CHECK(ntokens == 3);
    CHECK(!strcmp(tokens[0], "token_one"));
    CHECK(!strcmp(tokens[1], "token two quoted"));
    CHECK(!strcmp(tokens[2], "token_three"));
    CHECK(tokens[3] == NULL);

    bytes->assign(bytes, c, strlen(c));
    tokens = bytes->tokenizer(bytes, split, encaps, " ", "#", &ntokens);
    CHECK(ntokens == 5);
    CHECK(!strcmp(tokens[0], "((x == y) && (w != z))"));
    CHECK(!strcmp(tokens[1], "two"));
    CHECK(!strcmp(tokens[2], "three"));
    CHECK(!strcmp(tokens[3], "four is quoted"));
    CHECK(!strcmp(tokens[4], "five"));
    CHECK(tokens[5] == NULL);

    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("tokenizer_nested")

    bool split = true;
    const char * a = "expression is ({x} + {y})";
    const char * b = "variables are {var one} and {var two}";
    const char * c = "{(1+1) * (9-7)} {multiple spaces in name}";

    const char *encaps[] = {
            "\"\"",
            "()",
            "{}",
            NULL
    };

    bytes_t * bytes = bytes_pub.create(a, strlen(a));
    size_t ntokens = 0;
    char ** tokens = bytes->tokenizer(bytes, split, encaps, " ", "#", &ntokens);
    CHECK(ntokens == 3);
    CHECK(!strcmp(tokens[0], "expression"));
    CHECK(!strcmp(tokens[1], "is"));
    CHECK(!strcmp(tokens[2], "({x} + {y})"));
    CHECK(tokens[3] == NULL);

    bytes->assign(bytes, b, strlen(b));
    tokens = bytes->tokenizer(bytes, split, encaps, " ", "#", &ntokens);
    CHECK(ntokens == 5);
    CHECK(!strcmp(tokens[0], "variables"));
    CHECK(!strcmp(tokens[1], "are"));
    CHECK(!strcmp(tokens[2], "{var one}"));
    CHECK(!strcmp(tokens[3], "and"));
    CHECK(!strcmp(tokens[4], "{var two}"));
    CHECK(tokens[5] == NULL);

    bytes->assign(bytes, c, strlen(c));
    tokens = bytes->tokenizer(bytes, split, encaps, " ", "#", &ntokens);
    CHECK(ntokens == 2);
    CHECK(!strcmp(tokens[0], "{(1+1) * (9-7)}"));
    CHECK(!strcmp(tokens[1], "{multiple spaces in name}"));
    CHECK(tokens[3] == NULL);

    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("remove")
    const char * data = "abcdefghijklmnop";
    bytes_t * bytes = bytes_pub.create(data, strlen(data));
    ssize_t newsize = bytes->remove(bytes, 6, 3);

    BLAMMO(DEBUG, "newsize: %d", newsize);
    BLAMMO(DEBUG, "original size: %zu", strlen(data));
    BLAMMO(DEBUG, "new data: %s", bytes->cstr(bytes));

    CHECK(newsize == strlen(data) - 3);
    CHECK(strcmp(bytes->cstr(bytes), "abcdefjklmnop") == 0);
    bytes->destroy(bytes);
TEST_END

TEST_BEGIN("insert")
    const char * data = "aaaaccccc";
    bytes_t * bytes = bytes_pub.create(data, strlen(data));
    ssize_t newsize = bytes->insert(bytes, 4, "bbbbbbb", 7);

    BLAMMO(DEBUG, "newsize: %d", newsize);
    BLAMMO(DEBUG, "original size: %zu", strlen(data));
    BLAMMO(DEBUG, "new data: %s", bytes->cstr(bytes));

    CHECK(newsize == strlen(data) + 7);
    CHECK(strcmp(bytes->cstr(bytes), "aaaabbbbbbbccccc") == 0);
    bytes->destroy(bytes);
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
