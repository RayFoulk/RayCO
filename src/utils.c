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

//-----------------------------------------------------------------------------+
#include <stdio.h>
#include <string.h>

#include "utils.h"
#include "blammo.h"

//-----------------------------------------------------------------------------+
// A very simple hexdump function.  bytes_t already has one, but it's OK to be
// a little redundant while I figure out what to do with that object.  This is
// just too useful not to have around.  The address is meant to indicate the
// offset at the beginning of the data.  Pass 0 if you don't care.
void hexdump(const void * buf, size_t len, size_t addr)
{
    unsigned char * bufptr = (unsigned char *) buf;
    char line[128] = { 0 };
    char ascii[17] = { 0 };
    int offset = 0;
    size_t i = 0;
    size_t j = 0;

    while(i < len)
    {
        offset = sprintf(line, "0x%08x%08x: ",
                         (unsigned int) (addr >> 32),
                         (unsigned int) addr);

        // The format of this dump is 16 bytes per printed line
        for(j = 0; j < 16; ++j)
        {
            // advance one data bytes at a time
            if(i < len)
            {
                offset += sprintf(line + offset, "%02X ", bufptr[i]);

                // append printable ascii chars after hex bytes.
                // non-printable bytes represented by '.'
                if((bufptr[i] >= ' ') && (bufptr[i] < '~'))
                {
                    ascii[j] = bufptr[i];
                }
                else
                {
                    ascii[j] = '.';
                }
            }

            // when we've run out of data, pad out with spaces
            else
            {
                offset += sprintf(line + offset, "   ");
                ascii[j] = ' ';
            }

            i++;
        }

        ascii[sizeof(ascii) - 1] = '\0';

        // Print the ascii chars last, and delineate between data
        // bytes and ascii by including using extra space
        offset += sprintf(line + offset, " %s", ascii);
        BLAMMO(DEBUG, "%s", line);
        addr += 16;
    }
}

size_t splitstr(char ** tokens, size_t max_tokens,
                char * str, const char * delim)
{
    size_t ntok = 0;
    char * saveptr = NULL;
    char * ptr = strtok_r (str, delim, &saveptr);

    while ((ptr != NULL) && (ntok < max_tokens))
    {
        BLAMMO(DEBUG, "ptr: %s", ptr);
        tokens[ntok++] = ptr;
        ptr = strtok_r (NULL, delim, &saveptr);
    }

    return ntok;
}

// Very simple helper function to identify delimiters
static bool isdelim(const char c, const char * delim)
{
    char * d = NULL;

    for (d = (char *) delim; *d != 0; d++)
    {
        if (c == *d)
        {
            return true;
        }
    }

    return false;
}

size_t markstr(char ** markers, size_t max_markers,
               const char * str, const char * delim)
{
    size_t nmark = 0;
    bool within_delim = true;
    char * ptr = (char *) str;

    while ((*ptr != 0) && (nmark < max_markers))
    {
        if (isdelim(*ptr, delim))
        {
            if (!within_delim)
            {
                // transition to whitespace at the end of a token
                BLAMMO(DEBUG, "end of token at \'%s\'", ptr);
            }

            within_delim = true;
        }
        else
        {
            if (within_delim)
            {
                // transition to token at the end of whitespace
                BLAMMO(DEBUG, "beginning of token at \'%s\'", ptr);
                markers[nmark++] = ptr;
            }

            within_delim = false;
        }

        ptr++;
    }

    BLAMMO(DEBUG, "number of markers: %u", nmark);
    return nmark;
}
