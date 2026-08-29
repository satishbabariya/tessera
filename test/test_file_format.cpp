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
//
// The same file carries the encryption-at-rest tests, for the same reason. Both
// are claims about what the bytes on disk are, as opposed to claims about what
// the engine does with them, and both were argued from the source rather than
// measured.

#include <tessera/db.hpp>
#include <tessera/group.hpp>
#include <tessera/table.hpp>
#include <tessera/transaction.hpp>
#include <tessera/util/file.hpp>

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstddef>
#include <cstring>
#include <vector>

#include "test.hpp"
#include "util/crypt_key.hpp"

using namespace tessera;
using tessera::test_util::crypt_key;

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

// A duplicated struct drifts silently. Size alone does not catch a reordering,
// and every offset below is depended on elsewhere: tools/verify/consumer-smoke-
// test.sh reads the mnemonic at 16 with dd, and the tests here patch the format
// slots and the flags byte directly.
//
// These assertions only establish that this copy is self-consistent. What binds
// it to SlabAlloc::Header, which is private and cannot be asserted against, is
// FileFormat_WritesItsOwnIdentity below: it reads a real file through this
// struct and finds TESS in m_mnemonic and the format version in the live slot.
// If the original were reordered, that test would fail rather than these
// assertions.
static_assert(offsetof(FileHeader, m_top_ref) == 0, "top refs are first");
static_assert(offsetof(FileHeader, m_mnemonic) == 16, "the mnemonic is at offset 16");
static_assert(offsetof(FileHeader, m_file_format) == 20, "the format slots follow the mnemonic");
static_assert(offsetof(FileHeader, m_reserved) == 22, "reserved byte");
static_assert(offsetof(FileHeader, m_flags) == 23, "flags are last; bit 0 selects the slot");

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


#if TESSERA_ENABLE_ENCRYPTION

// README.md: "Encryption at rest. Optional AES-256, applied per page below the
// engine."
//
// The encryption layer is thoroughly tested in test_encrypted_file_mapping.cpp,
// which covers the cryptor, page IVs, interrupted writes and concurrent
// mappings. All of that is about the mapping machinery being correct. None of it
// answers the question a user of the claim actually asks, which is whether their
// data is on the disk in the clear.

namespace {

std::vector<char> read_whole_file(const std::string& path)
{
    util::File file(path, util::File::mode_Read);
    std::vector<char> buffer(size_t(file.get_size()));
    if (!buffer.empty())
        file.read(0, buffer.data(), buffer.size());
    return buffer;
}

bool contains(const std::vector<char>& haystack, const std::string& needle)
{
    return std::search(haystack.begin(), haystack.end(), needle.begin(), needle.end()) != haystack.end();
}

} // unnamed namespace


// The control. Without a key the string is on the disk in the clear, which is
// what makes the encrypted case below meaningful rather than a test of whether
// std::search works.
TEST(EncryptionAtRest_UnencryptedFileContainsThePlaintext)
{
    SHARED_GROUP_TEST_PATH(path);
    const std::string secret = "correct-horse-battery-staple";
    {
        DBRef db = DB::create(path);
        auto wt = db->start_write();
        auto table = wt->add_table("Secrets");
        auto col = table->add_column(type_String, "value");
        table->create_object().set(col, secret);
        wt->commit();
    }
    CHECK(contains(read_whole_file(path), secret));
}


// The claim itself.
TEST(EncryptionAtRest_EncryptedFileDoesNotContainThePlaintext)
{
    SHARED_GROUP_TEST_PATH(path);
    const std::string secret = "correct-horse-battery-staple";
    {
        DBRef db = DB::create(path, DBOptions(crypt_key(true)));
        auto wt = db->start_write();
        auto table = wt->add_table("Secrets");
        auto col = table->add_column(type_String, "value");
        table->create_object().set(col, secret);
        wt->commit();
    }
    CHECK_NOT(contains(read_whole_file(path), secret));

    // And the column name, which is metadata rather than data: "below the
    // engine" means the engine's own structures are encrypted too, not only the
    // values a user stores.
    CHECK_NOT(contains(read_whole_file(path), "Secrets"));
}


// An encrypted file opened without the key. test_encrypted_file_mapping.cpp
// covers DecryptionFailed for corrupted pages; this is the case a person
// actually meets, and it goes through DB::create rather than the mapping layer.
TEST(EncryptionAtRest_RejectsOpenWithoutKey)
{
    SHARED_GROUP_TEST_PATH(path);
    {
        DBRef db = DB::create(path, DBOptions(crypt_key(true)));
        auto wt = db->start_write();
        wt->add_table("Thing");
        wt->commit();
    }
    CHECK_THROW(DB::create(path), InvalidDatabase);
}


// Positive control for the two tests above: the correct key must open the file.
// Without this, "throws when the key is wrong" is satisfied by an engine that
// throws whatever key it is given.
TEST(EncryptionAtRest_OpensWithCorrectKey)
{
    SHARED_GROUP_TEST_PATH(path);
    {
        DBRef db = DB::create(path, DBOptions(crypt_key(true)));
        auto wt = db->start_write();
        wt->add_table("Thing");
        wt->commit();
    }
    DBRef db = DB::create(path, DBOptions(crypt_key(true)));
    auto rt = db->start_read();
    CHECK(rt->has_table("Thing"));
}


// And with a key that is not the right one.
TEST(EncryptionAtRest_RejectsOpenWithWrongKey)
{
    SHARED_GROUP_TEST_PATH(path);
    {
        DBRef db = DB::create(path, DBOptions(crypt_key(true)));
        auto wt = db->start_write();
        wt->add_table("Thing");
        wt->commit();
    }

    // crypt_key() returns a fixed 64-byte key; flipping one byte of a copy gives
    // a key of the right shape that is not the right key.
    std::array<char, 64> wrong;
    std::memcpy(wrong.data(), crypt_key(true), wrong.size());
    wrong[0] = char(wrong[0] ^ 0xFF);

    CHECK_THROW(DB::create(path, DBOptions(wrong.data())), InvalidDatabase);
}

#endif // TESSERA_ENABLE_ENCRYPTION
