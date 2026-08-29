////////////////////////////////////////////////////////////////////////////
//
// Copyright 2016 Realm Inc.
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//
////////////////////////////////////////////////////////////////////////////

#ifndef TESSERA_EXTERNAL_COMMIT_HELPER_HPP
#define TESSERA_EXTERNAL_COMMIT_HELPER_HPP

#include <tessera/util/features.h>

#if TESSERA_PLATFORM_APPLE
#include <tessera/object-store/impl/apple/external_commit_helper.hpp>
#elif TESSERA_HAVE_EPOLL
#include <tessera/object-store/impl/epoll/external_commit_helper.hpp>
#elif defined(_WIN32)
#include <tessera/object-store/impl/windows/external_commit_helper.hpp>
#elif defined(__EMSCRIPTEN__)
#include <tessera/object-store/impl/emscripten/external_commit_helper.hpp>
#else
#warning "The GenericExternalCommitHelper has been selected. Notifications won't be delivered properly"
#include <tessera/object-store/impl/generic/external_commit_helper.hpp>
#endif

#endif // TESSERA_EXTERNAL_COMMIT_HELPER_HPP
