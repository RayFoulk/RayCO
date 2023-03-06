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

#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <stddef.h>
#include <stdarg.h>
#include <pthread.h>

#include "blammo.h"
#include "console.h"

//------------------------------------------------------------------------|
// Console private data container
typedef struct
{
    // Lock to keep I/O thread-safe and prevent interleaving.
    pthread_mutex_t lock;

    // Optional third-party getline function for line editing and
    // tab-completion
    thirdparty_getline_f thirdparty_getline;

    // Pipes for user I/O
    FILE * input;
    FILE * output;
}
console_priv_t;

//------------------------------------------------------------------------|
static console_t * console_create(FILE * input, FILE * output)
{
    // Allocate and initialize public interface
    console_t * console = (console_t *) malloc(sizeof(console_t));
    if (!console)
    {
        BLAMMO(FATAL, "malloc(sizeof(console_t)) failed\n");
        return NULL;
    }

    // bulk copy all function pointers and init opaque ptr
    memcpy(console, &console_pub, sizeof(console_t));

    // Allocate and initialize private implementation
    console->priv = malloc(sizeof(console_priv_t));
    if (!console->priv)
    {
        BLAMMO(FATAL, "malloc(sizeof(console_priv_t)) failed\n");
        free(console);
        return NULL;
    }

    memset(console->priv, 0, sizeof(console_priv_t));
    console_priv_t * priv = (console_priv_t *) console->priv;

    // Use a recursive lock
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&priv->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    // Setup the pipes
    priv->input = input;
    priv->output = output;

    return console;
}

//------------------------------------------------------------------------|
static void console_destroy(void * console_ptr)
{
    console_t * console = (console_t *) console_ptr;

    // guard against accidental double-destroy or early-destroy
    if (!console || !console->priv)
    {
        BLAMMO(WARNING, "attempt to early or double-destroy");
        return;
    }

    // tear down internal data...
    console_priv_t * priv = (console_priv_t *) console->priv;
    pthread_mutex_destroy(&priv->lock);

    // zero out and destroy the private data
    memset(console->priv, 0, sizeof(console_priv_t));
    free(console->priv);

    // zero out and destroy the public interface
    memset(console, 0, sizeof(console_t));
    free(console);
}

//------------------------------------------------------------------------|
static bool console_lock(console_t * console)
{
    console_priv_t * priv = (console_priv_t *) console->priv;
    int error = pthread_mutex_lock(&priv->lock);

    if (error)
    {
        BLAMMO(ERROR, "pthread_mutex_lock() returned %d", error);
    }

    return !error;
}

//------------------------------------------------------------------------|
static void console_unlock(console_t * console)
{
    console_priv_t * priv = (console_priv_t *) console->priv;
    pthread_mutex_unlock(&priv->lock);
}

//------------------------------------------------------------------------|
static char * console_get_line(console_t * console, char ** lineptr, size_t * nchars)
{




    BLAMMO(ERROR, "NOT IMPLEMENTED");
    return -1;
}

//------------------------------------------------------------------------|
static int console_print(console_t * console, const char * format, ...)
{
    console_priv_t * priv = (console_priv_t *) console->priv;

    int nchars = 0;
    size_t size = 0;
    char * message = NULL;
    va_list args;

    if (!console->lock(console))
    {
        BLAMMO(ERROR, "console->lock() failed");
        return -1;
    }

    // Determine message size
    va_start(args, format);
    nchars = vsnprintf(message, size, format, args);
    va_end(args);

    // Bail out if something went wrong
    if (nchars < 0)
    {
        BLAMMO(ERROR, "vsnprintf() returned %d on first pass", nchars);
        return nchars;
    }

    // Prepare memory: extra byte for terminator
    // Extra two bytes for implied \r\n at end.
    size = (size_t) nchars + 3;
    message = (char *) malloc(size);
    if (!message)
    {
        BLAMMO(FATAL, "malloc(%u) returned %d", size, nchars);
        return -2;
    }

    // Format the message
    va_start(args, format);
    nchars = vsnprintf(message, size, format, args);
    va_end(args);

    // Bail out if something went wrong
    if (nchars < 0)
    {
        BLAMMO(ERROR, "vsnprintf() returned %d on second pass", nchars);
        free(message);
        return nchars;
    }

    message[nchars++] = '\r';
    message[nchars++] = '\n';
    message[nchars++] = '\0';

    // Send the formatted message over the output pipe
    size = fwrite(message, sizeof(char), size, priv->output);

    // Also send it to blammo if enabled
    BLAMMO(DEBUG, "message: \'%s\'", message);

    free(message);
    console->unlock(console);

    return 0;
}

//------------------------------------------------------------------------|
static int console_reprint(console_t * console, const char * format, ...)
{
    BLAMMO(ERROR, "NOT IMPLEMENTED");

    // TODO: Consider finally making this dynamic heap alloc
    // beyond what librmf refprintf() does

    return -1;
}

//------------------------------------------------------------------------|
const console_t console_pub = {
    &console_create,
    &console_destroy,
    &console_lock,
    &console_unlock,
    &console_get_line,
    &console_print,
    &console_reprint,
    NULL
};
