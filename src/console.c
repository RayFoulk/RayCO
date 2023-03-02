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

//#include <stdlib.h>
//#include <stdbool.h>
//#include <string.h>
#include <pthread.h>

#include "blammo.h"
#include "console.h"

//------------------------------------------------------------------------|
// Console private data container
typedef struct
{
    pthread_mutex_t lock;



}
console_priv_t;

//------------------------------------------------------------------------|
static console_t * console_create(FILE * input, FILE * output)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");

//    pthread_mutexattr_t attr;
//    pthread_mutexattr_init(&attr);
//    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);


    return NULL;
}

//------------------------------------------------------------------------|
static void console_destroy(void * console)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
}

//------------------------------------------------------------------------|
static bool console_lock(console_t * console)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return true;
}

//------------------------------------------------------------------------|
static void console_unlock(console_t * console)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
}

//------------------------------------------------------------------------|
static ssize_t console_getline(console_t * console, char ** lineptr, size_t * nchars)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return -1;
}

//------------------------------------------------------------------------|
static int console_print(console_t * console, const char * format, ...)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return -1;
}

//------------------------------------------------------------------------|
static int console_reprint(console_t * console, const char * format, ...)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return -1;
}

//------------------------------------------------------------------------|
const console_t console_pub = {
    &console_create,
    &console_destroy,
    &console_lock,
    &console_unlock,
    &console_getline,
    &console_print,
    &console_reprint,
    NULL
};
