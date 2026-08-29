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

#ifndef TESSERA_UMBRELLA_HPP
#define TESSERA_UMBRELLA_HPP

/// \file tessera.hpp
///
/// Convenience umbrella for the storage engine, retained because it predates
/// the tier split and has existing users. It forwards to <tessera/engine.hpp>.
///
/// New code should include one of the two tier entry points directly:
///   <tessera/api.hpp>     tier 1, the high-level API
///   <tessera/engine.hpp>  tier 2, the storage engine

#include <tessera/engine.hpp>
#include <tessera/query_engine.hpp>

#endif // TESSERA_UMBRELLA_HPP
