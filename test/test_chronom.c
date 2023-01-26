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

#include "blammo.h"
#include "chronom.h"
#include "mut.h"

#include <stdbool.h>
#include <string.h>
#include <limits.h>
#include <math.h>

TESTSUITE_BEGIN

    // Simple test of the blammo logger
    BLAMMO_LEVEL(INFO);
    BLAMMO_FILE("test_prng.log");
    BLAMMO(INFO, "chronometer tests...");

TEST_BEGIN("test start/stop/running")
    chronom_t * chm = chronom_pub.create();
    CHECK(chm->running(chm) == false);

    // not technically necessary as these are reset on creation
    chm->reset(chm);
    CHECK(chm->running(chm) == false);

    chm->start(chm);
    CHECK(chm->running(chm) == true);

    chm->stop(chm);
    CHECK(chm->running(chm) == false);

    chm->resume(chm);
    CHECK(chm->running(chm) == true);

    chm->reset(chm);
    CHECK(chm->running(chm) == false);

    chm->report(chm, "test chronometer");
    chm->destroy(chm);

TEST_END

TEST_BEGIN("test elapsed timespec")
    static const unsigned int test_increment = 1;
    static const double test_tolerance = 0.002;
    double test_seconds = 0;
    double test_error = 0;

    chronom_t * chm1 = chronom_pub.create();
    chronom_t * chm2 = chronom_pub.create();

    // not technically necessary as these are reset on creation	
    chm1->reset(chm1);
    chm2->reset(chm2);

    // begin both running
    chm1->start(chm1);
    chm2->start(chm2);

    // wait an increment
    sleep(test_increment);

    // stop chm1, chm2 keeps running...
    chm1->stop(chm1);

    // wait an increment
    sleep(test_increment);

    // get elapsed time from both
    struct timespec e1 = chm1->elapsed(chm1);
    struct timespec e2 = chm2->elapsed(chm2);

    // chronom 1 elapsed should be about 1 increment
    test_seconds = timespec_to_seconds(&e1);
    test_error = fabs(test_increment - test_seconds);
    BLAMMO(INFO, "chronom 1 elapsed %.9lf error %.9lf", test_seconds, test_error);
    CHECK(test_error <= test_tolerance);

    // chronom 2 elapsed should be about 2 increments
    test_seconds = timespec_to_seconds(&e2);
    test_error = fabs((2 * test_increment) - test_seconds);
    BLAMMO(INFO, "chronom 2 elapsed %.9lf error %.9lf", test_seconds, test_error);
    CHECK(test_error <= test_tolerance);
 
    // let chm1 continue...
    chm1->resume(chm1);

    // wait another increment
    sleep(test_increment);

    // get elapsed time from both
    e1 = chm1->elapsed(chm1);
    e2 = chm2->elapsed(chm2);

    // chronom 1 elapsed should be about 2 increments
    test_seconds = timespec_to_seconds(&e1);
    test_error = fabs((2 * test_increment) - test_seconds);
    BLAMMO(INFO, "chronom 1 elapsed %.9lf error %.9lf", test_seconds, test_error);
    CHECK(test_error <= test_tolerance);

    // chronom 2 elapsed should be about 3 increments
    test_seconds = timespec_to_seconds(&e2);
    test_error = fabs((3 * test_increment) - test_seconds);
    BLAMMO(INFO, "chronom 2 elapsed %.9lf error %.9lf", test_seconds, test_error);
    CHECK(test_error <= test_tolerance);
    
    // reset all back to 0
    chm1->reset(chm1);
    chm2->reset(chm2);

    e1 = chm1->elapsed(chm1);
    e2 = chm2->elapsed(chm2);

    // both had better be zero
    test_seconds = timespec_to_seconds(&e1);
	CHECK(test_seconds == 0.0);
    test_seconds = timespec_to_seconds(&e2);
	CHECK(test_seconds == 0.0);
	
    chm1->destroy(chm1);
    chm2->destroy(chm2);

TEST_END

TESTSUITE_END

