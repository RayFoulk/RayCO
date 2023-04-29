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
#include <stdio.h>
#include <string.h>
#include <stddef.h>         // ssize_t
#include <stdbool.h>
#include <ctype.h>

#include "sparser.h"
#include "utils.h"          // generic_print_f, memzero()

//------------------------------------------------------------------------|
typedef struct
{
    // The expression and pointers within the expression
    const char * expr;          // The expression
    char * ptr;                 // Current location being evaluated
    char * error_ptr;           // Where last error occurred

    // Current recursion depth
    unsigned char depth;

    // Pointers to string terms along with their associated lengths
    // (so that no strdup/malloc() need occur).  If these are NULL
    // the the terms are to be evaluated as numerical.
    const char * first;
    size_t first_length;

    const char * second;
    size_t second_length;

    // Function and object context for error reporting
    generic_print_f errprintf;
    void * errprintf_object;
}
sparser_t;

//------------------------------------------------------------------------|
// Forward declarations for functions that need them because of recursion
static long sparser_expression(sparser_t * sparser);
static long sparser_extract_term(sparser_t * sparser);
static long sparser_extract_factor(sparser_t * sparser);

//------------------------------------------------------------------------|
// The entry point for evaluating expressions
long sparser_evaluate(generic_print_f errprintf,
                      void * errprintf_object,
                      const char * expr)
{
    // Short-lived stack object to help with parsing this expression
    sparser_t sparser;

    // Initialize the object
    memzero(&sparser, sizeof(sparser_t));
    sparser.expr = (char *) expr;
    sparser.ptr = (char *) expr;
    sparser.errprintf = errprintf;
    sparser.errprintf_object = errprintf_object;

    // Parse the expression.  Sparser evaporates.
    long result = sparser_expression(&sparser);

    // Check if an invalid expression was detected at some point
    if (sparser.error_ptr)
    {
        if (sparser.errprintf)
        {
            sparser.errprintf(sparser.errprintf_object,
                              "Invalid expression at \'%s\' offset %zu\n",
                              sparser.error_ptr,
                              sparser.error_ptr - sparser.expr);
        }

        return SPARSER_INVALID_EXPRESSION;
    }

    return result;
}

//------------------------------------------------------------------------|
// A default error printf function for projects that don't implement one.
int sparser_errprintf(void * stream, const char * format, ...)
{
    va_list args;
    int nchars = 0;

    va_start (args, format);
    nchars = vfprintf((FILE *) stream, format, args);
    va_end (args);

    return nchars;
}

static void sparser_track_term(sparser_t * sparser,
                               const char * start,
                               size_t length)
{
    // This is basically a 2-unit queue.
    // Move the queue down and add the new term.
    sparser->second = sparser->first;
    sparser->second_length = sparser->first_length;
    sparser->first = start;
    sparser->first_length = length;
}

// Helper function to skip whitespace
static void skip_whitespace(sparser_t * sparser)
{
    while (isspace(*sparser->ptr))
    {
        sparser->ptr++;
    }
}

// Looks ahead but does not consume the token
static bool peek_token(sparser_t * sparser, const char * token)
{
    skip_whitespace(sparser);
    return !strncmp(sparser->ptr, token, strlen(token));
}

// Matches and consumes the token
static bool match_token(sparser_t * sparser, const char * token)
{
    if (peek_token(sparser, token))
    {
        sparser->ptr += strlen(token);
        return true;
    }

    return false;
}

//------------------------------------------------------------------------|
// Look ahead to see if the next operation is addition or subtraction
static bool is_add_sub(sparser_t * sparser)
{
    return (peek_token(sparser, "+") ||
            peek_token(sparser, "-"));
}

// Look ahead to see if the next operation is multiplication or division
static bool is_mul_div(sparser_t * sparser)
{
    return (peek_token(sparser, "*") ||
            peek_token(sparser, "/"));
}

// Look ahead to see if the next operation is logical
static bool is_logical(sparser_t * sparser)
{
    return (peek_token(sparser, "&&") ||
            peek_token(sparser, "||"));
}

// Look ahead to see if the next operation is comparison
static bool is_comparison(sparser_t * sparser)
{
    return (peek_token(sparser, "==") ||
            peek_token(sparser, "!=") ||
            peek_token(sparser, ">=") ||
            peek_token(sparser, "<=") ||
            peek_token(sparser, ">") ||
            peek_token(sparser, "<"));
}

//------------------------------------------------------------------------|
static long sparser_handle_add_sub(sparser_t * sparser, long left)
{
    while (peek_token(sparser, "+") || peek_token(sparser, "-"))
    {
        char op = *sparser->ptr;
        sparser->ptr++;     // Consume '+' or '-'
        skip_whitespace(sparser);
        long right = sparser_extract_term(sparser);

        if (op == '+')
        {
            left += right;
        }
        else
        {
            left -= right;
        }

        skip_whitespace(sparser);
    }

    return left;
}

static long sparser_handle_mul_div(sparser_t * sparser, long left)
{
    while (peek_token(sparser, "*") || peek_token(sparser, "/"))
    {
        char op = *sparser->ptr;
        sparser->ptr++;     // Consume '*' or '/'
        skip_whitespace(sparser);
        long right = sparser_extract_factor(sparser);

        if (op == '*')
        {
            left *= right;
        }
        else
        {
            left /= right;
        }

        skip_whitespace(sparser);
    }

    return left;
}

static long sparser_handle_comparison(sparser_t * sparser, long left)
{
    if (match_token(sparser, "=="))
    {
        long right = sparser_expression(sparser);

        // Do string comparison if the last two terms were strings
        if (sparser->first && sparser->second)
        {
            // Unequal lengths cannot be equal strings
            if (sparser->first_length != sparser->second_length)
            {
                return 0;
            }
            else
            {
                return !strncmp(sparser->first, sparser->second, sparser->first_length);
            }
        }

        // Fall back to numeric comparison otherwise
        return left == right;
    }
    else if (match_token(sparser, "!="))
    {
        long right = sparser_expression(sparser);

        // Do string comparison if the last two terms were strings
        if (sparser->first && sparser->second)
        {
            // Unequal lengths cannot be equal strings
            if (sparser->first_length != sparser->second_length)
            {
                return sparser->first_length - sparser->second_length;
            }
            else
            {
                return strncmp(sparser->first, sparser->second, sparser->first_length);
            }
        }

        // Fall back to numeric comparison otherwise
        return left != right;
    }
    else if (match_token(sparser, ">="))
    {
        long right = sparser_expression(sparser);
        return left >= right;
    }
    else if (match_token(sparser, "<="))
    {
        long right = sparser_expression(sparser);
        return left <= right;
    }
    else if (match_token(sparser, ">"))
    {
        long right = sparser_expression(sparser);
        return left > right;
    }
    else if (match_token(sparser, "<"))
    {
        long right = sparser_expression(sparser);
        return left < right;
    }

    return left;
}

static long sparser_handle_logical(sparser_t * sparser, long left)
{
    if (match_token(sparser, "&&"))
    {
        long right = sparser_expression(sparser);
        return left && right;
    }
    else if (match_token(sparser, "||"))
    {
        long right = sparser_expression(sparser);
        return left || right;
    }

    return left;
}

// Parse a number - Terminal node in parse tree
static long sparser_terminal_number(sparser_t * sparser)
{
    long result = 0;

    const char * start = sparser->ptr;

    while (isdigit(*sparser->ptr))
    {
        result = 10 * result + (*sparser->ptr - '0');
        sparser->ptr++;
    }

    // If any numeric value was stored (even zero)
    // then track this as a number and not a string
    if (sparser->ptr != start)
    {
        sparser_track_term(sparser, NULL, 0);
    }

    return result;
}

// Parse a string - Terminal node in parse tree
static long sparser_terminal_string(sparser_t * sparser)
{
    // skip the opening double quote
    bool quoted = match_token(sparser, "\"");

    const char * start = sparser->ptr;

    //while (*sparser->ptr != '"' && **input != '\0')
    while (isalpha(*sparser->ptr) || *sparser->ptr == '_')
    {
        sparser->ptr++;
    }

    size_t length = sparser->ptr - start;

    // consume closing quote if present
    quoted &= match_token(sparser, "\"");

    // If any string value was stored (even empty "")
    // then track this as a string and not a number.
    if ((sparser->ptr != start) || quoted)
    {
        sparser_track_term(sparser, start, length);
    }

    // Allow for alphabetization up to 3 chars deep
    // when using greater/less-than comparators
    long result = length >= 1 ? (long) start[0] << 16 : 0;
    result += length >= 2 ? (long) start[1] << 8 : 0;
    result += length >= 3 ? (long) start[2] : 0;
    return result;
}

//------------------------------------------------------------------------|
// Parse an expression -- recursion entry point for all expressions.
static long sparser_expression(sparser_t * sparser)
{
    sparser->depth++;

    // limit recursion depth
    if (sparser->depth >= SPARSER_MAX_RECURSION_DEPTH)
    {
        if (sparser->errprintf)
        {
            sparser->errprintf(sparser->errprintf_object,
                               "Maximum recursion depth %d reached\n",
                               sparser->depth);
        }

        sparser->depth--;
        sparser->error_ptr = sparser->ptr;
        return SPARSER_INVALID_EXPRESSION;
    }

    long left = sparser_extract_term(sparser);
    skip_whitespace(sparser);

    // Check for other conditions that should stop any further parsing
    if (sparser->error_ptr)
    {
        // Return to top level early if an error occurred
        sparser->depth--;
        return SPARSER_INVALID_EXPRESSION;
    }
    else if (*sparser->ptr == ')' && sparser->depth <= 1)
    {
        // Return early when unexpected end-parenthesis
        if (sparser->errprintf)
        {
            sparser->errprintf(sparser->errprintf_object,
                               "Unexpected ')'\n");
        }

        sparser->error_ptr = sparser->ptr;
        return SPARSER_INVALID_EXPRESSION;
    }
    else if (!*sparser->ptr)
    {
        // Return early at end-of-expression for efficiency
        sparser->depth--;
        return left;
    }

    if (is_add_sub(sparser))
    {
        left = sparser_handle_add_sub(sparser, left);
    }
    else if (is_comparison(sparser))
    {
        left = sparser_handle_comparison(sparser, left);
    }
    else if(is_logical(sparser))
    {
        left = sparser_handle_logical(sparser, left);
    }

    skip_whitespace(sparser);
    sparser->depth--;
    return left;
}

static long sparser_extract_term(sparser_t * sparser)
{
    long left = sparser_extract_factor(sparser);
    skip_whitespace(sparser);

    if (is_mul_div(sparser))
    {
        left = sparser_handle_mul_div(sparser, left);
    }

    return left;
}

static long sparser_extract_factor(sparser_t * sparser)
{
    skip_whitespace(sparser);
    long result;

    // Check for parenthetical sub-expression
    if (*sparser->ptr == '(')
    {
        sparser->ptr++;     // Consume '('
        result = sparser_expression(sparser);
        skip_whitespace(sparser);
        if (*sparser->ptr == ')')
        {
            sparser->ptr++;  // Consume ')'
            return result;
        }
        else if (sparser->errprintf)
        {
            sparser->errprintf(sparser->errprintf_object,
                               "Expected ')'\n");
        }

        sparser->error_ptr = sparser->ptr;
        return SPARSER_INVALID_EXPRESSION;
    }
    else if (*sparser->ptr == '!')
    {
        sparser->ptr++;  // Consume '!'
        return !sparser_extract_factor(sparser);
    }
    else if (*sparser->ptr == '-')
    {
        sparser->ptr++;  // Consume '-'
        return -sparser_extract_factor(sparser);
    }
    else if (isdigit(*sparser->ptr))
    {
        result = sparser_terminal_number(sparser);
        skip_whitespace(sparser);
        return result;
    }
    else if (*sparser->ptr == '"' || isalpha(*sparser->ptr) || *sparser->ptr == '_')
    {
        result = sparser_terminal_string(sparser);
        skip_whitespace(sparser);
        return result;
    }
    else if (sparser->errprintf)
    {
        sparser->errprintf(sparser->errprintf_object,
                           "Invalid character: %c\n",
                           *sparser->ptr);
    }

    sparser->error_ptr = sparser->ptr;
    return SPARSER_INVALID_EXPRESSION;
}
