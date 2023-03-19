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
#include <errno.h>

#include "blammo.h"
#include "console.h"

#ifdef LINENOISE_ENABLE
#include "linenoise.h"
#endif

//------------------------------------------------------------------------|
// Console private data container
typedef struct
{
    // Lock to keep I/O thread-safe and prevent interleaving.
    pthread_mutex_t lock;

    // Pipes for user I/O
    FILE * input;
    FILE * output;

    // reprint buffers
    // TODO: make this dynamic heap allocated
    char newbuffer[CONSOLE_BUFFER_SIZE];
    char oldbuffer[CONSOLE_BUFFER_SIZE];

    // Callbacks for tab completion and argument hints if supported
    console_tab_completion_f tab_completion_callback;
    console_arg_hints_f arg_hints_callback;
    void * callback_object;

#ifdef LINENOISE_ENABLE
    // linenoise needs a little extra context to work properly
    linenoiseCompletions * lc;
#endif
}
console_priv_t;

//------------------------------------------------------------------------|
#ifdef LINENOISE_ENABLE

// Some object is going to be forced into a singleton-ish pattern in order
// to support the linenoise submodule.  This means when linenoise is
// enabled then only one console object per process can be supported.
// The restriction is not that bad of a price to pay.
static console_t * singleton_console_ptr = NULL;

// Surrogate callback functions for redirecting linenoise calls to our
// contextual calls.
static void surrogate_linenoise_completion(const char * buffer,
                                           linenoiseCompletions * lc)
{
    BLAMMO(DEBUG, "buffer: \'%s\'", buffer);

    console_t * console = singleton_console_ptr;
    console_priv_t * priv = (console_priv_t *) console->priv;

    // Set up context and do the callback
    priv->lc = lc;
    if (!priv->tab_completion_callback)
    {
        return;
    }

    priv->tab_completion_callback(priv->callback_object, buffer);
}

static char * surrogate_linenoise_hints(const char * buffer,
                                        int * color,
                                        int * bold)
{
    BLAMMO(DEBUG, "buffer: \'%s\'", buffer);

    console_t * console = singleton_console_ptr;
    console_priv_t * priv = (console_priv_t *) console->priv;

    if (!priv->arg_hints_callback)
    {
        return NULL;
    }

    return priv->arg_hints_callback(priv->callback_object,
                                    buffer,
                                    color,
                                    bold);
}
#endif

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

    // Reset the reprint buffers
    console->reprint(console, NULL);

    // Use a recursive lock
    pthread_mutexattr_t attr;
    pthread_mutexattr_init(&attr);
    pthread_mutexattr_settype(&attr, PTHREAD_MUTEX_RECURSIVE_NP);
    pthread_mutex_init(&priv->lock, &attr);
    pthread_mutexattr_destroy(&attr);

    // Setup the pipes
    priv->input = input;
    priv->output = output;

#ifdef LINENOISE_ENABLE
    // Set the completion callback, for when <tab> is pressed
    linenoiseSetCompletionCallback(surrogate_linenoise_completion);

    // Set the hints callback for when arguments are needed
    linenoiseSetHintsCallback(surrogate_linenoise_hints);

    // Set singleton pointer to console object.  WARNING: this will break
    // if multiple console objects are created.
    singleton_console_ptr = console;
#endif

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

#ifdef LINENOISE_ENABLE
    singleton_console_ptr = NULL;
#endif
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
static void console_set_line_callbacks(console_t * console,
                                       console_tab_completion_f tab_callback,
                                       console_arg_hints_f hints_callback,
                                       void * object)
{
    console_priv_t * priv = (console_priv_t *) console->priv;
    priv->tab_completion_callback = tab_callback;
    priv->arg_hints_callback = hints_callback;
    priv->callback_object = object;
}

//------------------------------------------------------------------------|
void console_add_tab_completion(console_t * console, const char * line)
{
#ifdef LINENOISE_ENABLE
    console_priv_t * priv = (console_priv_t *) console->priv;
    linenoiseAddCompletion(priv->lc, line);
#else
    BLAMMO(DEBUG, "Tab-completion not implemented");
#endif
}

//------------------------------------------------------------------------|
static char * console_get_line(console_t * console,
                               const char * prompt,
                               bool interactive)
{
    // if interactive and linenoise enabled, use linenoise
    // else read from input FILE *???
    // tempting to just compare with stdin here, but that would be a
    // potential mistake leading to future problems...
#ifdef LINENOISE_ENABLE
    if (interactive)
    {
        // TODO: PR against linenoise to add setting of FILE *
        // so that we can, for example, redirect I/O to a tty
        return linenoise(prompt);
    }
#endif

    console_priv_t * priv = (console_priv_t *) console->priv;

    // wrap getline, and use for interactive if there is no linenoise
    // submodule.  This is always used for scripts, however.
    fprintf(stdout, "%s", prompt);

    char * line = NULL;
    size_t nalloc = 0;
    ssize_t nchars = getline(&line, &nalloc, priv->input);
    if (nchars < 0)
    {
        BLAMMO(ERROR, "getline() failed with errno %d strerror %s",
                      errno, strerror(errno));
        free(line);
        return NULL;
    }

    return line;
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
    console_priv_t * priv = (console_priv_t *) console->priv;

    // TODO: Consider finally making this dynamic heap alloc
    // beyond what librmf refprintf() does
    pthread_mutex_lock(&priv->lock);

    va_list args;
    int count = 0;
    int address = 0;

    // Null format string indicates a buffer reset
    if (!format)
    {
        priv->newbuffer[0] = 0;
        priv->oldbuffer[0] = 0;
        pthread_mutex_unlock(&priv->lock);
        return 0;

    }

    // map arguments into 'new' buffer
    va_start (args, format);
    vsnprintf (priv->newbuffer, CONSOLE_BUFFER_SIZE, format, args);
    va_end (args);

    // Check for first call after a reset
    if (priv->oldbuffer[0] == 0)
    {
        // this is the first call _after_ a reset
        // FIXME: console print has implicit newlines,
        //  need to implement bytes_t vprint()
        //console->print(console, "%s", priv->newbuffer);
        fprintf(priv->output, "%s", priv->newbuffer);
    }
    else
    {
        // Update the output terminal with new information.
        // We need to find where the first byte that's changed is.
        while ((priv->newbuffer[address] == priv->oldbuffer[address])
                && (priv->newbuffer[address] != 0))
        {
            address ++;
        }

        // Only print something if buffers differ somehow
        if (!((priv->newbuffer[address] == 0)
                && (priv->oldbuffer[address] == 0)))
        {
            // First delete what's differenent, working backwards
            // from the end of old buffer down to the 1st changed byte
            for (count = strnlen(priv->oldbuffer, CONSOLE_BUFFER_SIZE); count > address; count --)
            {
                // delete a character: delete-space-delete
                // FIXME: USING CONSOLE PRINT IS BROKEN
                //  DUE TO IMPLICIT CARRIAGE-RETURN-NEWLINE
                //console->print(console, "%c%c%c", (char) 8, (char) 32, (char) 8);
                fprintf(priv->output, "%c%c%c", (char) 8, (char) 32, (char) 8);

            }

            // Next print out the updated part of the new buffer
            // from where we left off working forwards
            address = strnlen(priv->newbuffer, CONSOLE_BUFFER_SIZE);
            while (count < address)
            {
                // FIXME: XXXXXXXXXXXX
                //console->print(console, "%c", priv->newbuffer[count]);
                fprintf(priv->output, "%c", priv->newbuffer[count]);
                count ++;
            }
        }
    }

    // New buffer becomes old, and always fflush the output stream
    strncpy (priv->oldbuffer, priv->newbuffer, CONSOLE_BUFFER_SIZE);
    fflush (priv->output);

    pthread_mutex_unlock(&priv->lock);
    return 0;
}

//------------------------------------------------------------------------|
const console_t console_pub = {
    &console_create,
    &console_destroy,
    &console_lock,
    &console_unlock,
    &console_set_line_callbacks,
    &console_add_tab_completion,
    &console_get_line,
    &console_print,
    &console_reprint,
    NULL
};
