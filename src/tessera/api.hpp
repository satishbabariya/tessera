/*************************************************************************
 *
 * Copyright 2026 The Tessera Authors
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

#ifndef TESSERA_API_HPP
#define TESSERA_API_HPP

/// \file api.hpp
///
/// Tier 1: the high-level API.
///
/// This is what most users write against: schemas, typed objects, live results
/// and change notifications. It is the stable public surface, and from v1.0
/// onwards it follows semantic versioning.
///
/// For direct access to the storage engine -- tables, queries, transactions --
/// include <tessera/engine.hpp> instead. That surface is also stable, but it is
/// lower level and assumes you know what a transaction boundary is.

#include <tessera/object-store/shared_db.hpp>
#include <tessera/object-store/object.hpp>
#include <tessera/object-store/object_schema.hpp>
#include <tessera/object-store/results.hpp>
#include <tessera/object-store/schema.hpp>
#include <tessera/object-store/list.hpp>
#include <tessera/object-store/set.hpp>
#include <tessera/object-store/dictionary.hpp>
#include <tessera/object-store/property.hpp>
#include <tessera/object-store/collection_notifications.hpp>
#include <tessera/object-store/object_accessor.hpp>
#include <tessera/object-store/thread_safe_reference.hpp>

#endif // TESSERA_API_HPP
