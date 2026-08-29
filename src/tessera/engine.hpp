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

#ifndef TESSERA_ENGINE_HPP
#define TESSERA_ENGINE_HPP

/// \file engine.hpp
///
/// Tier 2: the storage engine.
///
/// Direct access to the copy-on-write, memory-mapped MVCC engine underneath the
/// high-level API: databases, transactions, tables, queries and objects. This
/// surface is stable and supported, and it is where binding authors and users
/// with unusual requirements should start.
///
/// It is lower level than <tessera/tessera.hpp>: you manage transaction
/// boundaries yourself, and there is no schema layer or change-notification
/// machinery. Both tiers can be used together in one program.

#include <tessera/db.hpp>
#include <tessera/transaction.hpp>
#include <tessera/history.hpp>
#include <tessera/table.hpp>
#include <tessera/table_view.hpp>
#include <tessera/obj.hpp>
#include <tessera/keys.hpp>
#include <tessera/query.hpp>
#include <tessera/query_expression.hpp>
#include <tessera/list.hpp>
#include <tessera/set.hpp>
#include <tessera/dictionary.hpp>
#include <tessera/mixed.hpp>
#include <tessera/timestamp.hpp>
#include <tessera/decimal128.hpp>
#include <tessera/object_id.hpp>
#include <tessera/uuid.hpp>
#include <tessera/exceptions.hpp>

#endif // TESSERA_ENGINE_HPP
