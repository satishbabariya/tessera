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

#ifndef TESSERA_UTIL_FEATURES_H
#define TESSERA_UTIL_FEATURES_H

#ifdef _MSC_VER
#pragma warning(disable : 4800) // Visual Studio int->bool performance warnings
#endif

#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

#ifndef TESSERA_NO_CONFIG
#include <tessera/util/config.h>
#endif

/* The maximum number of elements in a B+-tree node. Applies to inner nodes and
 * to leaves. The minimum allowable value is 2.
 */
#ifndef TESSERA_MAX_BPNODE_SIZE
#define TESSERA_MAX_BPNODE_SIZE 1000
#endif


#define TESSERA_QUOTE_2(x) #x
#define TESSERA_QUOTE(x) TESSERA_QUOTE_2(x)

/* See these links for information about feature check macroes in GCC,
 * Clang, and MSVC:
 *
 * http://gcc.gnu.org/projects/cxx0x.html
 * http://clang.llvm.org/cxx_status.html
 * http://clang.llvm.org/docs/LanguageExtensions.html#checks-for-standard-language-features
 * http://msdn.microsoft.com/en-us/library/vstudio/hh567368.aspx
 * http://sourceforge.net/p/predef/wiki/Compilers
 */


/* Compiler is GCC and version is greater than or equal to the specified version */
#define TESSERA_HAVE_AT_LEAST_GCC(maj, min) \
    (__GNUC__ > (maj) || __GNUC__ == (maj) && __GNUC_MINOR__ >= (min))

#if defined(__clang__)
#define TESSERA_HAVE_CLANG_FEATURE(feature) __has_feature(feature)
#define TESSERA_HAVE_CLANG_WARNING(warning) __has_warning(warning)
#else
#define TESSERA_HAVE_CLANG_FEATURE(feature) 0
#define TESSERA_HAVE_CLANG_WARNING(warning) 0
#endif

#ifdef __has_cpp_attribute
#define TESSERA_HAS_CPP_ATTRIBUTE(attr) __has_cpp_attribute(attr)
#else
#define TESSERA_HAS_CPP_ATTRIBUTE(attr) 0
#endif

#if TESSERA_HAS_CPP_ATTRIBUTE(clang::fallthrough)
#define TESSERA_FALLTHROUGH [[clang::fallthrough]]
#elif TESSERA_HAS_CPP_ATTRIBUTE(gnu::fallthrough)
#define TESSERA_FALLTHROUGH [[gnu::fallthrough]]
#elif TESSERA_HAS_CPP_ATTRIBUTE(fallthrough)
#define TESSERA_FALLTHROUGH [[fallthrough]]
#else
#define TESSERA_FALLTHROUGH
#endif

// This should be renamed to TESSERA_UNREACHABLE as soon as TESSERA_UNREACHABLE is renamed to
// TESSERA_ASSERT_NOT_REACHED which will better reflect its nature
#if defined(__GNUC__) || defined(__clang__)
#define TESSERA_COMPILER_HINT_UNREACHABLE __builtin_unreachable
#else
#define TESSERA_COMPILER_HINT_UNREACHABLE abort
#endif

#if defined(__GNUC__) // clang or GCC
#define TESSERA_PRAGMA(v) _Pragma(TESSERA_QUOTE_2(v))
#elif defined(_MSC_VER) // VS
#define TESSERA_PRAGMA(v) __pragma(v)
#else
#define TESSERA_PRAGMA(v)
#endif

#if defined(__clang__)
#define TESSERA_DIAG(v) TESSERA_PRAGMA(clang diagnostic v)
#elif defined(__GNUC__)
#define TESSERA_DIAG(v) TESSERA_PRAGMA(GCC diagnostic v)
#else
#define TESSERA_DIAG(v)
#endif

#define TESSERA_DIAG_PUSH() TESSERA_DIAG(push)
#define TESSERA_DIAG_POP() TESSERA_DIAG(pop)

#ifdef _MSC_VER
#define TESSERA_VS_WARNING_DISABLE #pragma warning (default: 4297)
#endif

#if TESSERA_HAVE_CLANG_WARNING("-Wtautological-compare") || TESSERA_HAVE_AT_LEAST_GCC(6, 0)
#define TESSERA_DIAG_IGNORE_TAUTOLOGICAL_COMPARE() TESSERA_DIAG(ignored "-Wtautological-compare")
#else
#define TESSERA_DIAG_IGNORE_TAUTOLOGICAL_COMPARE()
#endif

#ifdef _MSC_VER
#  define TESSERA_DIAG_IGNORE_UNSIGNED_MINUS() TESSERA_PRAGMA(warning(disable:4146))
#else
#define TESSERA_DIAG_IGNORE_UNSIGNED_MINUS()
#endif

/* The way to specify that a function never returns. */
#if TESSERA_HAVE_AT_LEAST_GCC(4, 8) || TESSERA_HAVE_CLANG_FEATURE(cxx_attributes)
#define TESSERA_NORETURN [[noreturn]]
#elif __GNUC__
#define TESSERA_NORETURN __attribute__((noreturn))
#elif defined(_MSC_VER)
#define TESSERA_NORETURN __declspec(noreturn)
#else
#define TESSERA_NORETURN
#endif


/* The way to specify that a variable or type is intended to possibly
 * not be used. Use it to suppress a warning from the compiler. */
#if __GNUC__
#define TESSERA_UNUSED __attribute__((unused))
#else
#define TESSERA_UNUSED
#endif

/* The way to specify that a function is deprecated
 * not be used. Use it to suppress a warning from the compiler. */
#if __GNUC__
#define TESSERA_DEPRECATED(x) [[deprecated(x)]]
#else
#define TESSERA_DEPRECATED(x) __declspec(deprecated(x))
#endif


#if __GNUC__ || defined __INTEL_COMPILER
#define TESSERA_UNLIKELY(expr) __builtin_expect(!!(expr), 0)
#define TESSERA_LIKELY(expr) __builtin_expect(!!(expr), 1)
#else
#define TESSERA_UNLIKELY(expr) (expr)
#define TESSERA_LIKELY(expr) (expr)
#endif


#if defined(__GNUC__) || defined(__HP_aCC)
#define TESSERA_FORCEINLINE inline __attribute__((always_inline))
#elif defined(_MSC_VER)
#define TESSERA_FORCEINLINE __forceinline
#else
#define TESSERA_FORCEINLINE inline
#endif


#if TESSERA_HAS_CPP_ATTRIBUTE(gnu::cold)
#define TESSERA_COLD [[gnu::cold]]
#else
#define TESSERA_COLD
#endif


#if TESSERA_HAS_CPP_ATTRIBUTE(gnu::noinline)
#define TESSERA_NOINLINE [[gnu::noinline]]
#elif defined(__GNUC__) || defined(__HP_aCC)
#define TESSERA_NOINLINE __attribute__((noinline))
#elif defined(_MSC_VER)
#define TESSERA_NOINLINE __declspec(noinline)
#else
#define TESSERA_NOINLINE
#endif


#if TESSERA_HAS_CPP_ATTRIBUTE(nodiscard)
#define TESSERA_NODISCARD [[nodiscard]]
#else
#if defined(__GNUC__) || defined(__HP_aCC)
#define TESSERA_NODISCARD __attribute__((warn_unused_result))
#elif defined(_MSC_VER)
#define TESSERA_NODISCARD _Check_return_
#else
#define TESSERA_NODISCARD
#endif
#endif

/* Thread specific data (only for POD types) */
#if defined __clang__
#define TESSERA_THREAD_LOCAL __thread
#else
#define TESSERA_THREAD_LOCAL thread_local
#endif


#if defined ANDROID || defined __ANDROID_API__
#define TESSERA_ANDROID 1
#define TESSERA_LINUX 0
#elif defined(__linux__)
#define TESSERA_ANDROID 0
#define TESSERA_LINUX 1
#else
#define TESSERA_ANDROID 0
#define TESSERA_LINUX 0
#endif

#if defined _WIN32
#include <winapifamily.h>
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP | WINAPI_PARTITION_SYSTEM)
#define TESSERA_WINDOWS 1
#define TESSERA_UWP 0
#elif WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_APP)
#define TESSERA_WINDOWS 0
#define TESSERA_UWP 1
#endif
#else
#define TESSERA_WINDOWS 0
#define TESSERA_UWP 0
#endif

// Some documentation of the defines provided by Apple:
// http://developer.apple.com/library/mac/documentation/Porting/Conceptual/PortingUnix/compiling/compiling.html#//apple_ref/doc/uid/TP40002850-SW13
#if defined __APPLE__ && defined __MACH__
#define TESSERA_PLATFORM_APPLE 1
/* Apple OSX and iOS (Darwin). */
#include <Availability.h>
#include <TargetConditionals.h>
#if TARGET_OS_IPHONE == 1 && TARGET_OS_IOS == 1
/* Device (iPhone or iPad) or simulator. */
#define TESSERA_IOS 1
#define TESSERA_APPLE_DEVICE !TARGET_OS_SIMULATOR
#define TESSERA_MACCATALYST TARGET_OS_MACCATALYST
#else
#define TESSERA_IOS 0
#define TESSERA_MACCATALYST 0
#endif
#if TARGET_OS_WATCH == 1
/* Device (Apple Watch) or simulator. */
#define TESSERA_WATCHOS 1
#define TESSERA_APPLE_DEVICE !TARGET_OS_SIMULATOR
#else
#define TESSERA_WATCHOS 0
#endif
#if TARGET_OS_TV
/* Device (Apple TV) or simulator. */
#define TESSERA_TVOS 1
#define TESSERA_APPLE_DEVICE !TARGET_OS_SIMULATOR
#else
#define TESSERA_TVOS 0
#endif
#else
#define TESSERA_PLATFORM_APPLE 0
#define TESSERA_MACCATALYST 0
#define TESSERA_IOS 0
#define TESSERA_WATCHOS 0
#define TESSERA_TVOS 0
#endif
#ifndef TESSERA_APPLE_DEVICE
#define TESSERA_APPLE_DEVICE 0
#endif

#if TESSERA_ANDROID || TESSERA_IOS || TESSERA_WATCHOS || TESSERA_TVOS || TESSERA_UWP
#define TESSERA_MOBILE 1
#else
#define TESSERA_MOBILE 0
#endif


#if defined(TESSERA_DEBUG) && !defined(TESSERA_COOKIE_CHECK)
#define TESSERA_COOKIE_CHECK
#endif

// We're in i686 mode
#if defined(__i386) || defined(__i386__) || defined(__i686__) || defined(_M_I86) || defined(_M_IX86)
#define TESSERA_ARCHITECTURE_X86_32 1
#else
#define TESSERA_ARCHITECTURE_X86_32 0
#endif

// We're in amd64 mode
#if defined(__amd64) || defined(__amd64__) || defined(__x86_64) || defined(__x86_64__) || defined(_M_X64) || \
    defined(_M_AMD64)
#define TESSERA_ARCHITECTURE_X86_64 1
#else
#define TESSERA_ARCHITECTURE_X86_64 0
#endif

#if defined(__arm__) || defined(_M_ARM)
#define TESSERA_ARCHITECTURE_ARM32 1
#else
#define TESSERA_ARCHITECTURE_ARM32 0
#endif

#if defined(__aarch64__) || defined(_M_ARM64) || defined(_M_ARM64EC)
#define TESSERA_ARCHITECTURE_ARM64 1
#else
#define TESSERA_ARCHITECTURE_ARM64 0
#endif

// Address Sanitizer
#if defined(__has_feature) // Clang
#  if __has_feature(address_sanitizer)
#    define TESSERA_SANITIZE_ADDRESS 1
#  else
#    define TESSERA_SANITIZE_ADDRESS 0
#  endif
#elif defined(__SANITIZE_ADDRESS__) && __SANITIZE_ADDRESS__ // GCC
#  define TESSERA_SANITIZE_ADDRESS 1
#else
#  define TESSERA_SANITIZE_ADDRESS 0
#endif

// Thread Sanitizer
#if defined(__has_feature) // Clang
#  if __has_feature(thread_sanitizer)
#    define TESSERA_SANITIZE_THREAD 1
#  else
#    define TESSERA_SANITIZE_THREAD 0
#  endif
#elif defined(__SANITIZE_THREAD__) && __SANITIZE_THREAD__ // GCC
#  define TESSERA_SANITIZE_THREAD 1
#else
#  define TESSERA_SANITIZE_THREAD 0
#endif

#endif /* TESSERA_UTIL_FEATURES_H */
