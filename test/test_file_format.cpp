/*************************************************************************
 *
 * Copyright 2026 Tessera contributors
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

// Tessera's file format is its own, and it opens nothing else. README.md and
// ARCHITECTURE.md both state that plainly -- it is the clean break from Realm
// made concrete, and the reason there is no upgrade path.
//
// Until these tests existed the claim rested on reading the code. The magic
// mnemonic was checked by the consumer smoke test, which asserts that a file
// Tessera writes says TESS; that is the writing half. Nothing exercised the
// reading half: that a file which says something else is refused, and refused
// with an error a person can act on.

#include <tessera/db.hpp>
#include <tessera/group.hpp>
#include <tessera/table.hpp>
#include <tessera/transaction.hpp>
#include <tessera/util/file.hpp>

#include <cstdint>
#include <cstring>

#include "test.hpp"

using namespace tessera;

namespace {

// A copy of SlabAlloc::Header, which is private. Twenty-four bytes: two top
// refs, then an eight-byte info block holding the mnemonic, the two file format
// slots, a reserved byte, and flags whose bit 0 selects which of the two slots
// and top refs is live.
struct FileHeader {
    uint64_t m_top_ref[2];
    uint8_t m_mnemonic[4];
    uint8_t m_file_format[2];
    uint8_t m_reserved;
    uint8_t m_flags;
};
static_assert(sizeof(FileHeader) == 24, "the on-disk header is 24 bytes");

// Writes a database with one table, closes it, and returns its header.
FileHeader make_database(const std::string& path)
{
    {
        DBRef db = DB::create(path);
        auto wt = db->start_write();
        auto table = wt->add_table("Thing");
        table->add_column(type_Int, "value");
        wt->commit();
    }
    FileHeader header;
    util::File file(path, util::File::mode_Read);
    file.read(0, reinterpret_cast<char*>(&header), sizeof header);
    return header;
}

void overwrite_header(const std::string& path, const FileHeader& header)
{
    util::File file(path, util::File::mode_Update);
    file.write(0, reinterpret_cast<const char*>(&header), sizeof header);
    file.sync();
}

} // unnamed namespace


// What Tessera writes. The counterpart of the magic-byte assertion in
// tools/verify/consumer-smoke-test.sh, which checks the same thing from outside
// the project against an installed package.
TEST(FileFormat_WritesItsOwnIdentity)
{
    SHARED_GROUP_TEST_PATH(path);
    FileHeader header = make_database(path);

    CHECK_EQUAL('T', char(header.m_mnemonic[0]));
    CHECK_EQUAL('E', char(header.m_mnemonic[1]));
    CHECK_EQUAL('S', char(header.m_mnemonic[2]));
    CHECK_EQUAL('S', char(header.m_mnemonic[3]));

    int slot = (header.m_flags & 1) ? 1 : 0;
    CHECK_EQUAL(Group::get_current_file_format_version(), int(header.m_file_format[slot]));
    CHECK_EQUAL(1, Group::get_current_file_format_version());
}


// A Realm or TightDB file, which begins T-DB. This is the case the fork exists
// to refuse: it must not be opened, and must not be silently upgraded.
TEST(FileFormat_RejectsRealmMnemonic)
{
    SHARED_GROUP_TEST_PATH(path);
    FileHeader header = make_database(path);

    header.m_mnemonic[0] = 'T';
    header.m_mnemonic[1] = '-';
    header.m_mnemonic[2] = 'D';
    header.m_mnemonic[3] = 'B';
    overwrite_header(path, header);

    CHECK_THROW(DB::create(path), InvalidDatabase);
}


// Anything else that is not a database at all.
TEST(FileFormat_RejectsForeignMnemonic)
{
    SHARED_GROUP_TEST_PATH(path);
    FileHeader header = make_database(path);

    std::memcpy(header.m_mnemonic, "SQLi", 4);
    overwrite_header(path, header);

    CHECK_THROW(DB::create(path), InvalidDatabase);
}


// A file that really is a Tessera file but records a format this build does not
// implement. Version 24 is in the range Realm reached; version 2 is a format
// Tessera has not defined yet. Both must be refused rather than guessed at.
TEST(FileFormat_RejectsOtherVersions)
{
    for (int foreign_version : {2, 10, 24, 255}) {
        SHARED_GROUP_TEST_PATH(path);
        FileHeader header = make_database(path);

        int slot = (header.m_flags & 1) ? 1 : 0;
        // Only the live slot matters; the other is allowed to differ.
        header.m_file_format[slot] = uint8_t(foreign_version);
        overwrite_header(path, header);

        CHECK_THROW(DB::create(path), UnsupportedFileFormatVersion);
    }
}


// The rejection must say what is wrong. A file refused with a message that does
// not name the format is a support request.
TEST(FileFormat_RejectionNamesTheProblem)
{
    SHARED_GROUP_TEST_PATH(path);
    FileHeader header = make_database(path);

    int slot = (header.m_flags & 1) ? 1 : 0;
    header.m_file_format[slot] = 24;
    overwrite_header(path, header);

    std::string message;
    try {
        DB::create(path);
        CHECK(false); // must not reach here
    }
    catch (const UnsupportedFileFormatVersion& e) {
        message = e.what();
    }
    CHECK_NOT_EQUAL(std::string::npos, message.find("24"));
}
