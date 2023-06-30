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

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

//------------------------------------------------------------------------|
// Requirements:
//
// The final parser shall have the following limitations and features.
//
// - Integer arithmetic only.  No fractional numbers or results.
//   - Supported integer operators include Addition '+', Subtraction '-',
//     Multiplication '*', Division (remainder is discarded) '/' and Unary
//     Negation '-' (prefix) of a numeric values.
//   - The generated parser should use the C data type 'long' rather than
//     'int'.
//
// - Supported boolean operators will be logical Not "!", And "&&", and
//   Or "||"
//
// - Supported comparator operators for arithmetic expressions include
//   '==', '!=', '>=', "<=", ">", and "<"
//
// - The comparators "==" and "!=" will also support string and boolean
//   expression comparison.
//
// - The grammar and resulting parser should handle arbitrarily nested
//   parenthetical statements.
//
// - The order of precedence for operators shall be as normally expected,
//   with addition and subtraction being lower than multiplication and
//   division, and logical operators and comparison operators lower than
//   arithmetic.
//
// - Whitespace within expressions shall be ignored.

//------------------------------------------------------------------------|
// EBNF Context-Free Grammar Definition:
//
// <expression> ::= <term> {<addop> <term>}
// <term> ::= <factor> {<mulop> <factor>}
// <factor> ::= <number> | <string> | <paren> | <unaryop> <factor>
// <number> ::= <digit> {<digit>}
// <digit> ::= "0" | "1" | "2" | "3" | "4" | "5" | "6" | "7" | "8" | "9"
// <string> ::= '"' {<char>} '"'
// <char> ::= <digit> | <letter> | <symbol>
// <paren> ::= "(" <expression> ")"
// <addop> ::= "+" | "-"
// <mulop> ::= "*" | "/"
// <unaryop> ::= "-"
// <boolop> ::= "!" | "&&" | "||"
// <compop> ::= "==" | "!=" | ">=" | "<=" | ">" | "<"
// <boolean> ::= <expression>
// <zero> ::= "0"
// <nonzero> ::= <digit> {<digit>}
// <factor> ::= <boolean> | (<zero> | <nonzero>) <compop> (<zero> | <nonzero>)

//------------------------------------------------------------------------|
#include <stdarg.h>         // va_start, va_end
#include <limits.h>         // LONG_MIN

#include "utils.h"          // generic_print_f, memzero()

//------------------------------------------------------------------------|
// Limit expression recursion depth to avoid stack smash.  This could
// occur either due to flaws in the recursive parser itself of absurdly
// deep parenthetical expressions.
#define SPARSER_MAX_RECURSION_DEPTH         64

// Reserve a single special return value to indicate the expression is
// invalid.  All other return values are considered legitimate.  When
// a parsing error occurs, details will be reported to the print function
// as passed into the top level evaluator call.
#define SPARSER_INVALID_EXPRESSION          LONG_MIN

//------------------------------------------------------------------------|
// Do a simple dry-run evaluation.  Return true if successful.
// Makes the assumption that an expression must contain parenthesis,
// although this is not necessarily true in all cases.
//bool sparser_dryrun(const char * expr);
// NOTE: probably not a good idea b/c then we won't attempt to evaluate
// even when we should, and wouldn't provide helpful syntax error info...
// also this effectively evalutes things twice so is not efficient.
// Then again -- maybe just perform some simple checks.  Try it and see.
bool sparser_is_expr(const char * expr);

// The top-level expression evaluator.
long sparser_evaluate(generic_print_f errprintf,
                      void * errprintf_object,
                      const char * expr);

// Default error print function.  If your project doesn't have an
// implementation, this one can be used like so:
//
//   long value = sparser_evaluate(sparser_errprintf,
//                                 stderr,
//                                 myexpr);
//
// This will output any parsing error message to stderr.
int sparser_errprintf(void * stream, const char * format, ...);

#ifdef __cplusplus
}
#endif
