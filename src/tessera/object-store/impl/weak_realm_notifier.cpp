////////////////////////////////////////////////////////////////////////////
//
// Copyright 2015 Realm Inc.
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

#include <tessera/object-store/impl/weak_realm_notifier.hpp>

#include <tessera/object-store/shared_db.hpp>
#include <tessera/object-store/util/scheduler.hpp>

using namespace tessera;
using namespace tessera::_impl;


WeakRealmNotifier::WeakRealmNotifier(const std::shared_ptr<Realm>& realm, bool cache)
    : m_realm(realm)
    , m_realm_key(realm.get())
    , m_cache(cache)
{
    bind_to_scheduler();
}

WeakRealmNotifier::~WeakRealmNotifier() = default;

void WeakRealmNotifier::notify()
{
    if (m_scheduler) {
        m_scheduler->invoke([weak_realm = m_realm] {
            if (auto realm = weak_realm.lock()) {
                realm->notify();
            }
        });
    }
}

void WeakRealmNotifier::bind_to_scheduler()
{
    TESSERA_ASSERT(!m_scheduler);
    m_scheduler = realm()->scheduler();
}

bool WeakRealmNotifier::is_cached_for_scheduler(std::shared_ptr<util::Scheduler> scheduler) const
{
    return m_cache && (m_scheduler && scheduler) && (m_scheduler->is_same_as(scheduler.get()));
}

bool WeakRealmNotifier::scheduler_is_on_thread() const
{
    return m_scheduler && m_scheduler->is_on_thread();
}
