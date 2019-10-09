//------------------------------------------------------------------------|
// Copyright (c) 2018-2019 by Raymond M. Foulk IV
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

#pragma once

//------------------------------------------------------------------------|
// only really enable the MSGLOG() macro if the preprocessor directive
// MSGLOG_ENABLE is defined on the command line during build.  otherwise
// MSGLOG() will be an empty macro.
#ifndef MSGLOG_ENABLE
#define MSGLOG(mt, fm, __VA_ARGS__)
#else
#define MSGLOG(mt, fm, __VA_ARGS__)    msg_log(mt, fm, __VA_ARGS__)

//------------------------------------------------------------------------|
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>

//------------------------------------------------------------------------|
typedef enum msg_log_t
{
    INFO    = 0,
    DEBUG   = 1,
    WARNING = 2,
    ERROR   = 3
}

//typedef struct
//{
//}
//msg_log_data_t;

//------------------------------------------------------------------------|
int msg_log(const msg_log_t msg_type, const char * format, ...);


#endif
