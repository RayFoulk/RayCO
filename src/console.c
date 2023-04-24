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

#include "console.h"
#include "utils.h"              // memzero()
#include "blammo.h"
#include "bytes.h"

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

    // Double-buffer for reprinting messages,
    // or for general use in other message printing.
    bytes_t * buffer[2];
    int which;

    // TODO: Assume ownership of the input line buffer
    // as well, rather than relying on the caller to free()
    // Problem of how to manage linenoise's allocation
    // versus our own buffer: answer copy it immediately
    // and free() the linenoise buffer here.

    // Callbacks for tab completion and argument hints if supported
    console_tab_completion_f tab_completion_callback;
    console_arg_hints_f arg_hints_callback;
    void * callback_object;

#ifdef LINENOISE_ENABLE
    // linenoise needs a little extra context to work properly
    linenoiseCompletions * lc;

    // File name for command history
    bytes_t * history_file;
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
static console_t * console_create(FILE * input,
                                  FILE * output,
                                  const char * history_file)
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

    memzero(console->priv, sizeof(console_priv_t));
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

    // Initialize the message buffer(s).  Both should be initially
    // clear()ed, so no need to do a reprint(NULL) here.
    priv->buffer[0] = bytes_pub.create(NULL, 0);
    priv->buffer[1] = bytes_pub.create(NULL, 0);
    priv->which = 0;

#ifdef LINENOISE_ENABLE
    // Set the completion callback, for when <tab> is pressed
    linenoiseSetCompletionCallback(surrogate_linenoise_completion);

    // Set the hints callback for when arguments are needed
    linenoiseSetHintsCallback(surrogate_linenoise_hints);

    // Configure and load history from file
    priv->history_file = bytes_pub.create(history_file,
                                          history_file ?
                                              strlen(history_file) :
                                              0);
    linenoiseHistoryLoad(priv->history_file->cstr(priv->history_file));

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
    priv->buffer[0]->destroy(priv->buffer[0]);
    priv->buffer[1]->destroy(priv->buffer[1]);
    pthread_mutex_destroy(&priv->lock);

#ifdef LINENOISE_ENABLE
    priv->history_file->destroy(priv->history_file);
    singleton_console_ptr = NULL;
#endif

    // zero out and destroy the private data
    memzero(console->priv, sizeof(console_priv_t));
    free(console->priv);

    // zero out and destroy the public interface
    memzero(console, sizeof(console_t));
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
static inline FILE * console_get_inputf(console_t * console)
{
    console_priv_t * priv = (console_priv_t *) console->priv;
    return priv->input;
}

//------------------------------------------------------------------------|
static inline FILE * console_get_outputf(console_t * console)
{
    console_priv_t * priv = (console_priv_t *) console->priv;
    return priv->output;
}

//------------------------------------------------------------------------|
static bool console_set_inputf(console_t * console, FILE * input)
{
    if (!console->lock(console))
    {
        BLAMMO(ERROR, "console->lock() failed");
        return false;
    }

    console_priv_t * priv = (console_priv_t *) console->priv;
    priv->input = input;

    console->unlock(console);
    return true;
}

//------------------------------------------------------------------------|
static bool console_set_outputf(console_t * console, FILE * output)
{
    if (!console->lock(console))
    {
        BLAMMO(ERROR, "console->lock() failed");
        return false;
    }

    console_priv_t * priv = (console_priv_t *) console->priv;
    priv->output = output;

    console->unlock(console);
    return true;

}

//------------------------------------------------------------------------|
bool console_inputf_eof(console_t * console)
{
    console_priv_t * priv = (console_priv_t *) console->priv;
    return (bool) feof(priv->input);
}

//------------------------------------------------------------------------|
// Private helper for adding line to history
static void console_add_history(console_t * console,
                                const char * line)
{
#ifdef LINENOISE_ENABLE
    console_priv_t * priv = (console_priv_t *) console->priv;

    if (line && line[0] && !priv->history_file->empty(priv->history_file))
    {
        // Add line to history, and save to disk
        linenoiseHistoryAdd(line);
        linenoiseHistorySave(priv->history_file->cstr(priv->history_file));
    }

#else
    BLAMMO(DEBUG, "Line history not implemented");
#endif
}

//------------------------------------------------------------------------|
static char * console_get_line(console_t * console,
                               const char * prompt,
                               bool interactive)
{
    char * line = NULL;

    // if interactive and linenoise enabled, use linenoise
    // else read from input FILE *???
    // tempting to just compare with stdin here, but that would be a
    // potential mistake leading to future problems...
#ifdef LINENOISE_ENABLE
    if (interactive)
    {
        // TODO: PR against linenoise to add setting of FILE *
        // so that we can, for example, redirect I/O to a tty
        line = linenoise(prompt);
        console_add_history(console, line);
        return line;
    }
#endif

    console_priv_t * priv = (console_priv_t *) console->priv;

    // wrap getline, and use for interactive if there is no linenoise
    // submodule.  This is always used for scripts, however.
    // Do not show prompt if not interactive
    if (interactive)
    {
        fprintf(priv->output, "%s", prompt);
    }

    size_t nalloc = 0;
    ssize_t nchars = getline(&line, &nalloc, priv->input);
    if (nchars < 0)
    {
        BLAMMO(ERROR, "getline() failed with errno %d strerror %s",
                      errno, strerror(errno));
        free(line);
        return NULL;
    }

    console_add_history(console, line);
    return line;
}

//------------------------------------------------------------------------|
static int console_warning(void * console_ptr, const char * format, ...)
{
    console_t * console = (console_t *) console_ptr;
    console_priv_t * priv = (console_priv_t *) console->priv;
    bytes_t * buffer = priv->buffer[0];
    bytes_t * wformat = priv->buffer[1];
    const char * warning = "warning: ";
    ssize_t nchars = 0;
    va_list args;

    if (!console->lock(console))
    {
        BLAMMO(ERROR, "console->lock() failed");
        return -1;
    }

    // Re-use secondary buffer as a modified/prepended format string
    wformat->assign(wformat, warning, strlen(warning));
    wformat->append(wformat, format, strlen(format));

    va_start(args, format);
    nchars = buffer->vprint(buffer, wformat->cstr(wformat), args);
    va_end(args);

    if (nchars < 0)
    {
        BLAMMO(ERROR, "bytes->vprint() returned %d", nchars);
        console->unlock(console);
        return nchars;
    }

    // Send the formatted message over the output pipe
    buffer->append(buffer, "\r\n\0", 3);
    nchars = fwrite(buffer->data(buffer),
                    sizeof(char),
                    buffer->size(buffer),
                    priv->output);

    // Also send it to blammo if enabled
    BLAMMO(WARNING, "\'%s\'", buffer->cstr(buffer));
    console->unlock(console);
    return (int) nchars;
}

//------------------------------------------------------------------------|
static int console_error(void * console_ptr, const char * format, ...)
{
    console_t * console = (console_t *) console_ptr;
    console_priv_t * priv = (console_priv_t *) console->priv;
    bytes_t * buffer = priv->buffer[0];
    bytes_t * eformat = priv->buffer[1];
    const char * error = "error: ";
    ssize_t nchars = 0;
    va_list args;

    if (!console->lock(console))
    {
        BLAMMO(ERROR, "console->lock() failed");
        return -1;
    }

    // Re-use secondary buffer as a modified/prepended format string
    eformat->assign(eformat, error, strlen(error));
    eformat->append(eformat, format, strlen(format));

    va_start(args, format);
    nchars = buffer->vprint(buffer, eformat->cstr(eformat), args);
    va_end(args);

    if (nchars < 0)
    {
        BLAMMO(ERROR, "bytes->vprint() returned %d", nchars);
        console->unlock(console);
        return nchars;
    }

    // Send the formatted message over the output pipe
    buffer->append(buffer, "\r\n\0", 3);
    nchars = fwrite(buffer->data(buffer),
                    sizeof(char),
                    buffer->size(buffer),
                    priv->output);

    // Also send it to blammo if enabled
    BLAMMO(ERROR, "\'%s\'", buffer->cstr(buffer));
    console->unlock(console);
    return (int) nchars;
}

//------------------------------------------------------------------------|
static int console_print(void * console_ptr, const char * format, ...)
{
    console_t * console = (console_t *) console_ptr;
    console_priv_t * priv = (console_priv_t *) console->priv;
    bytes_t * buffer = priv->buffer[0];
    ssize_t nchars = 0;
    va_list args;

    if (!console->lock(console))
    {
        BLAMMO(ERROR, "console->lock() failed");
        return -1;
    }

    va_start(args, format);
    nchars = buffer->vprint(buffer, format, args);
    va_end(args);

    if (nchars < 0)
    {
        BLAMMO(ERROR, "bytes->vprint() returned %d", nchars);
        console->unlock(console);
        return nchars;
    }

    // Send the formatted message over the output pipe
    buffer->append(buffer, "\r\n\0", 3);
    nchars = fwrite(buffer->data(buffer),
                    sizeof(char),
                    buffer->size(buffer),
                    priv->output);

    // Also send it to blammo if enabled
    BLAMMO(DEBUG, "\'%s\'", buffer->cstr(buffer));
    console->unlock(console);
    return (int) nchars;
}

//------------------------------------------------------------------------|
static int console_reprint(void * console_ptr, const char * format, ...)
{
    console_t * console = (console_t *) console_ptr;
    console_priv_t * priv = (console_priv_t *) console->priv;
    bytes_t * swap = NULL;
    bytes_t * backspaces = NULL;
    bytes_t * spaces = NULL;
    ssize_t nchars = 0;
    ssize_t diff_offset = 0;
    va_list args;

    if (!console->lock(console))
    {
        BLAMMO(ERROR, "console->lock() failed");
        return -1;
    }

    // Passing a NULL format string resets both buffers
    if (!format)
    {
        priv->buffer[0]->clear(priv->buffer[0]);
        priv->buffer[1]->clear(priv->buffer[1]);
        console->unlock(console);
        return 0;
    }

    // map arguments into 'new' buffer
    va_start(args, format);
    nchars = priv->buffer[0]->vprint(priv->buffer[0], format, args);
    va_end(args);

    // On the first call after a reset, the 'old' buffer
    // will be empty.  In this case just write the full new
    // buffer to the output pipe.
    if (priv->buffer[1]->empty(priv->buffer[1]))
    {
        nchars = fwrite(priv->buffer[0]->data(priv->buffer[0]),
                        sizeof(char),
                        priv->buffer[0]->size(priv->buffer[0]),
                        priv->output);
    }
    else
    {
        // Need to update the output terminal with any new information.
        // First need to find where the first different byte is.
        diff_offset = priv->buffer[0]->diff_byte(priv->buffer[0],
                                                 priv->buffer[1]);

        // Only print something if buffers differ somehow
        if (diff_offset >= 0)
        {
            // First delete what's different by overwriting old
            // data working backwards down to the 1st different byte
            nchars = priv->buffer[1]->size(priv->buffer[1]) - diff_offset;
            if (nchars > 0)
            {
                backspaces = bytes_pub.create(NULL, 0);
                spaces = bytes_pub.create(NULL, 0);

                backspaces->resize(backspaces, nchars);
                backspaces->fill(backspaces, 0x08);
                spaces->resize(spaces, nchars);
                spaces->fill(spaces, 0x20);

                fwrite(backspaces->data(backspaces), sizeof(char),
                       nchars, priv->output);
                fwrite(spaces->data(spaces), sizeof(char),
                       nchars, priv->output);
                fwrite(backspaces->data(backspaces), sizeof(char),
                       nchars, priv->output);

                spaces->destroy(spaces);
                backspaces->destroy(backspaces);
            }

            // Now write out the updated part of the new buffer,
            // from the point of difference to the end of new data.
            nchars = priv->buffer[0]->size(priv->buffer[0]) - diff_offset;
            if (nchars > 0)
            {
                nchars = fwrite(&priv->buffer[0]->data(priv->buffer[0])[diff_offset],
                                sizeof(char), nchars, priv->output);
            }
        }
    }

    // Swap 'old' and 'new' buffers
    swap = priv->buffer[1];
    priv->buffer[1] = priv->buffer[0];
    priv->buffer[0] = swap;

    // Always fflush() to keep terminal in sync
    fflush (priv->output);

    console->unlock(console);
    return (int) nchars;
}

//------------------------------------------------------------------------|
const console_t console_pub = {
    &console_create,
    &console_destroy,
    &console_lock,
    &console_unlock,
    &console_set_line_callbacks,
    &console_add_tab_completion,
    &console_get_inputf,
    &console_get_outputf,
    &console_set_inputf,
    &console_set_outputf,
    &console_inputf_eof,
    &console_get_line,
    &console_warning,
    &console_error,
    &console_print,
    &console_reprint,
    NULL
};
