#include <tessera/sync/noinst/server/server_file_access_cache.hpp>

using namespace tessera;
using namespace _impl;


void ServerFileAccessCache::proper_close_all()
{
    while (m_first_open_file)
        m_first_open_file->proper_close(); // Throws
}


void ServerFileAccessCache::access(Slot& slot)
{
    if (slot.is_open()) {
        m_logger.trace(util::LogCategory::server, "Using already open database file: %1", slot.realm_path); // Throws

        // Move to front
        TESSERA_ASSERT(m_first_open_file);
        if (&slot != m_first_open_file) {
            remove(slot);
            insert(slot); // At front
            m_first_open_file = &slot;
        }
        return;
    }

    // Close least recently accessed Realm file
    if (m_num_open_files == m_max_open_files) {
        TESSERA_ASSERT(m_first_open_file);
        Slot& least_recently_accessed = *m_first_open_file->m_prev_open_file;
        least_recently_accessed.proper_close(); // Throws
    }

    slot.open(); // Throws
}

void ServerFileAccessCache::Slot::proper_close()
{
    if (is_open()) {
        m_cache.m_logger.detail("Closing database file: %1", realm_path); // Throws
        do_close();
    }
}


void ServerFileAccessCache::Slot::open()
{
    TESSERA_ASSERT(!is_open());

    m_cache.m_logger.detail("Opening database file: %1", realm_path); // Throws

    m_file.reset(new File{*this}); // Throws

    m_cache.insert(*this);
    m_cache.m_first_open_file = this;
    ++m_cache.m_num_open_files;
}
