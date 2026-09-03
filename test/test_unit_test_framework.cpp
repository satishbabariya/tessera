/*************************************************************************
 *
 * Copyright 2026 Realm Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 * http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 *
 **************************************************************************/

#include "testsettings.hpp"
#ifdef TEST_UNIT_TEST_FRAMEWORK

#include "test.hpp"

#include <chrono>
#include <iostream>
#include <string>
#include <thread>

using namespace tessera::test_util;

// Tests of the test framework itself.
//
// There were none until a bug made the whole suite report success while a test
// in it had failed: with more than one thread, the thread that goes on to run
// the nonconcurrent tests discarded its concurrent-phase counters -- including
// num_failed_tests, which the exit status is computed from. Whether a failure
// survived depended on which thread happened to finish last.
//
// Nothing detected that, because nothing checked that the framework fails when
// it should. Every other check in this repository is aimed at the code under
// test; this one is aimed at the thing doing the checking.

namespace {

void always_passes(unit_test::TestContext& test_context)
{
    CHECK(true);
}

// Deliberately slow, and deliberately first in the list below.
//
// The thread that picks this up is still working while the others run out of
// fast tests and finish, so it is the last thread to end -- which is exactly the
// thread whose counters used to be discarded. Without the delay the whole inner
// list is grabbed by whichever thread starts first, that thread finishes before
// any other reaches the end, and the failure is counted correctly even on a
// broken framework. The first version of this test did that, passed against the
// bug it was written for, and was useless.
void always_fails(unit_test::TestContext& test_context)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(60));
    CHECK(false);
}

bool always_enabled()
{
    return true;
}

// Builds a list holding plenty of concurrent work, one nonconcurrent test, and
// exactly one failing test, then runs it silently and reports whether the run
// was correctly judged a failure.
//
// The nonconcurrent test matters: without one, the framework never takes the
// path where a thread hands over to run the nonconcurrent tests, which is the
// path that used to drop that thread's counters.
bool run_reports_failure(int num_threads)
{
    unit_test::TestList list;
    list.add(always_fails, always_enabled, true, "Inner", "fails", __FILE__, __LINE__);
    for (int i = 0; i < 64; ++i)
        list.add(always_passes, always_enabled, true, "Inner", "pass_" + std::to_string(i), __FILE__, __LINE__);
    list.add(always_passes, always_enabled, false, "Inner", "nonconcurrent", __FILE__, __LINE__);

    unit_test::TestList::Config config;
    config.num_threads = num_threads;
    config.reporter = nullptr; // silent: the inner failure is expected

    return !list.run(config); // run() returns true when everything passed
}

} // unnamed namespace


// A failing test must fail the run, whichever thread happens to execute it.
//
// Repeated, because the bug this guards against was probabilistic: with two
// threads the failure was lost only when it landed on the thread that finished
// last, which was about half the time. One iteration would have passed cleanly
// on a broken framework often enough to be useless.
TEST(UnitTestFramework_FailureIsCountedFromEveryThread)
{
    for (int num_threads : {1, 2, 4}) {
        for (int attempt = 0; attempt < 3; ++attempt) {
            bool reported = run_reports_failure(num_threads);
            CHECK(reported);
            if (!reported) {
                // Say which configuration, once, rather than twelve times.
                std::cerr << "a failing inner test was not reported as a failure at " << num_threads
                          << " thread(s)\n";
                break;
            }
        }
    }
}

#endif // TEST_UNIT_TEST_FRAMEWORK
