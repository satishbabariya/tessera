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

#ifndef TESSERA_UTIL_ASSERT_HPP
#define TESSERA_UTIL_ASSERT_HPP

#include <tessera/util/features.h>
#include <tessera/util/terminate.hpp>

#if TESSERA_ENABLE_ASSERTIONS || defined(TESSERA_DEBUG)
#define TESSERA_ASSERTIONS_ENABLED 1
#else
#define TESSERA_ASSERTIONS_ENABLED 0
#endif

#define TESSERA_ASSERT_RELEASE(condition)                                                                              \
    (TESSERA_LIKELY(condition) ? static_cast<void>(0)                                                                  \
                             : tessera::util::terminate("Assertion failed: " #condition, __FILE__, __LINE__))

#if TESSERA_ASSERTIONS_ENABLED
#define TESSERA_ASSERT(condition) TESSERA_ASSERT_RELEASE(condition)
#else
#define TESSERA_ASSERT(condition) static_cast<void>(sizeof bool(condition))
#endif

#ifdef TESSERA_DEBUG
#define TESSERA_ASSERT_DEBUG(condition) TESSERA_ASSERT_RELEASE(condition)
#else
#define TESSERA_ASSERT_DEBUG(condition) static_cast<void>(sizeof bool(condition))
#endif

#define TESSERA_STRINGIFY(X) #X

#define TESSERA_ASSERT_RELEASE_EX(condition, ...)                                                                      \
    (TESSERA_LIKELY(condition) ? static_cast<void>(0)                                                                  \
                             : tessera::util::terminate_with_info("Assertion failed: " #condition, __LINE__, __FILE__, \
                                                                TESSERA_STRINGIFY((__VA_ARGS__)), __VA_ARGS__))

#ifdef TESSERA_DEBUG
#define TESSERA_ASSERT_DEBUG_EX TESSERA_ASSERT_RELEASE_EX
#else
#define TESSERA_ASSERT_DEBUG_EX(condition, ...) static_cast<void>(sizeof bool(condition))
#endif

// Becase the assert is used in noexcept methods, it's a bad idea to allocate
// buffer space for the message so therefore we must pass it to terminate which
// will 'cerr' it for us without needing any buffer
#if TESSERA_ENABLE_ASSERTIONS || defined(TESSERA_DEBUG)

#define TESSERA_ASSERT_EX TESSERA_ASSERT_RELEASE_EX

#define TESSERA_ASSERT_3(left, cmp, right)                                                                             \
    (TESSERA_LIKELY((left)cmp(right)) ? static_cast<void>(0)                                                           \
                                    : tessera::util::terminate("Assertion failed: "                                    \
                                                             "" #left " " #cmp " " #right,                           \
                                                             __FILE__, __LINE__, left, right))

#define TESSERA_ASSERT_7(left1, cmp1, right1, logical, left2, cmp2, right2)                                            \
    (TESSERA_LIKELY(((left1)cmp1(right1))logical((left2)cmp2(right2)))                                                 \
         ? static_cast<void>(0)                                                                                      \
         : tessera::util::terminate("Assertion failed: "                                                               \
                                  "" #left1 " " #cmp1 " " #right1 " " #logical " "                                   \
                                  "" #left2 " " #cmp2 " " #right2,                                                   \
                                  __FILE__, __LINE__, left1, right1, left2, right2))

#define TESSERA_ASSERT_11(left1, cmp1, right1, logical1, left2, cmp2, right2, logical2, left3, cmp3, right3)           \
    (TESSERA_LIKELY(((left1)cmp1(right1))logical1((left2)cmp2(right2)) logical2((left3)cmp3(right3)))                  \
         ? static_cast<void>(0)                                                                                      \
         : tessera::util::terminate("Assertion failed: "                                                               \
                                  "" #left1 " " #cmp1 " " #right1 " " #logical1 " "                                  \
                                  "" #left2 " " #cmp2 " " #right2 " " #logical2 " "                                  \
                                  "" #left3 " " #cmp3 " " #right3,                                                   \
                                  __FILE__, __LINE__, left1, right1, left2, right2, left3, right3))
#else
#define TESSERA_ASSERT_EX(condition, ...) static_cast<void>(sizeof bool(condition))
#define TESSERA_ASSERT_3(left, cmp, right) static_cast<void>(sizeof bool((left)cmp(right)))
#define TESSERA_ASSERT_7(left1, cmp1, right1, logical, left2, cmp2, right2)                                            \
    static_cast<void>(sizeof bool(((left1)cmp1(right1))logical((left2)cmp2(right2))))
#define TESSERA_ASSERT_11(left1, cmp1, right1, logical1, left2, cmp2, right2, logical2, left3, cmp3, right3)           \
    static_cast<void>(sizeof bool(((left1)cmp1(right1))logical1((left2)cmp2(right2)) logical2((left3)cmp3(right3))))
#endif

#define TESSERA_UNREACHABLE() tessera::util::terminate("Unreachable code", __FILE__, __LINE__)
#ifdef TESSERA_COVER
#define TESSERA_COVER_NEVER(x) false
#define TESSERA_COVER_ALWAYS(x) true
#else
#define TESSERA_COVER_NEVER(x) (x)
#define TESSERA_COVER_ALWAYS(x) (x)
#endif

#endif // TESSERA_UTIL_ASSERT_HPP
