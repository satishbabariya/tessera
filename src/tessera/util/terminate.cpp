/*************************************************************************
 *
 * Copyright 2016 Realm Inc.
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

#include <tessera/util/terminate.hpp>

#include <iostream>
#include <sstream>
#include <tessera/util/features.h>
#include <tessera/util/thread.hpp>
#include <tessera/util/backtrace.hpp>
#include <tessera/version.hpp>

#if TESSERA_PLATFORM_APPLE
#include <os/log.h>

#include <dlfcn.h>
#include <execinfo.h>
#include <CoreFoundation/CoreFoundation.h>
#endif

#if TESSERA_ANDROID
#include <android/log.h>
#endif

// extern "C" and noinline so that a readable message shows up in the stack trace
// of the crash
// prototype here to silence warning
// The macro indirection here puts the core version number in the actual stack trace

// LCOV_EXCL_START
#define TESSERA_DEFINE_TERMINATE_VERSIONED_(x)                                                                         \
    extern "C" TESSERA_NORETURN TESSERA_NOINLINE void please_report_this_issue_in_github_satishbabariya_tessera_v##x();        \
                                                                                                                     \
    extern "C" TESSERA_NORETURN TESSERA_NOINLINE void please_report_this_issue_in_github_satishbabariya_tessera_v##x()         \
    {                                                                                                                \
        std::abort();                                                                                                \
    }

#define TESSERA_DEFINE_TERMINATE_VERSIONED(x) TESSERA_DEFINE_TERMINATE_VERSIONED_(x)
#define TESSERA_EVALUATE_(x) _##x
#define TESSERA_EVALUATE(x) TESSERA_EVALUATE_(x)
#define TESSERA_MACRO_CONCAT(A, B) TESSERA_MACRO_CONCAT_(A, B)
#define TESSERA_MACRO_CONCAT_(A, B) A##B
#define TESSERA_VERSION_SUFFIX_CONCAT                                                                                  \
    TESSERA_MACRO_CONCAT(TESSERA_EVALUATE(TESSERA_VERSION_MAJOR),                                                          \
                       TESSERA_MACRO_CONCAT(TESSERA_EVALUATE(TESSERA_VERSION_MINOR), TESSERA_EVALUATE(TESSERA_VERSION_PATCH)))

TESSERA_DEFINE_TERMINATE_VERSIONED(TESSERA_VERSION_SUFFIX_CONCAT)

#define TESSERA_TERMINATE_VERSIONED_(x) please_report_this_issue_in_github_satishbabariya_tessera_v##x()
#define TESSERA_TERMINATE_VERSIONED(x) TESSERA_TERMINATE_VERSIONED_(x)
#define TESSERA_TERMINATE_AUTO_VERSIONED() TESSERA_TERMINATE_VERSIONED(TESSERA_VERSION_SUFFIX_CONCAT)

// LCOV_EXCL_STOP

namespace {

#if TESSERA_PLATFORM_APPLE
void nslog(const char* message) noexcept
{
    // Standard error goes nowhere for applications managed by launchd,
    // so log to ASL/unified logging system logs as well.
    fputs(message, stderr);
    // The unified logging system considers dynamic strings to be private in
    // order to protect users. This means we must specify "%{public}s" to get
    // the message here. See `man os_log` for more details.
    os_log_error(OS_LOG_DEFAULT, "%{public}s", message);
    // Log the message to Crashlytics if it's loaded into the process
    void* addr = dlsym(RTLD_DEFAULT, "CLSLog");
    if (addr) {
        CFStringRef str =
            CFStringCreateWithCStringNoCopy(kCFAllocatorDefault, message, kCFStringEncodingUTF8, kCFAllocatorNull);
        auto fn = reinterpret_cast<void (*)(CFStringRef, ...)>(reinterpret_cast<size_t>(addr));
        fn(CFSTR("%@"), str);
        CFRelease(str);
    }
}

void (*termination_notification_callback)(const char*) noexcept = nslog;

#elif TESSERA_ANDROID

void android_log(const char* message) noexcept
{
    __android_log_write(ANDROID_LOG_ERROR, "REALM", message);
}

void (*termination_notification_callback)(const char*) noexcept = android_log;

#else

void (*termination_notification_callback)(const char*) noexcept = nullptr;

#endif

} // unnamed namespace

namespace tessera {
namespace util {

// LCOV_EXCL_START
TESSERA_NORETURN static void terminate_internal(std::stringstream& ss) noexcept
{
    util::Backtrace::capture().print(ss);

    ss << "\n!!! IMPORTANT: Please report this at https://github.com/realm/realm-core/issues/new/choose";

    if (termination_notification_callback) {
        termination_notification_callback(ss.str().c_str());
    }
    else {
        std::cerr << ss.rdbuf();
        std::string thread_name;
        if (Thread::get_name(thread_name))
            std::cerr << "\nThread name: " << thread_name;
    }

    TESSERA_TERMINATE_AUTO_VERSIONED();
}

TESSERA_NORETURN void terminate(const char* message, const char* file, long line,
                              std::initializer_list<Printable>&& values) noexcept
{
    terminate_with_info(message, file, line, nullptr, std::move(values));
}

TESSERA_NORETURN void terminate_with_info(const char* message, const char* file, long line,
                                        const char* interesting_names,
                                        std::initializer_list<Printable>&& values) noexcept
{
    std::stringstream ss;
    ss << file << ':' << line << ": " TESSERA_VER_CHUNK " " << message;
    if (interesting_names)
        ss << " with " << interesting_names << " = ";
    Printable::print_all(ss, values, bool(interesting_names));
    ss << '\n';
    terminate_internal(ss);
}
// LCOV_EXCL_STOP

} // namespace util
} // namespace tessera
