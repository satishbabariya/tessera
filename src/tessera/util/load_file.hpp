#ifndef TESSERA_UTIL_LOAD_FILE_HPP
#define TESSERA_UTIL_LOAD_FILE_HPP

#include <string>

namespace tessera {
namespace util {

// FIXME: These functions ought to be moved to <tessera/util/file.hpp> in the
// realm-core repository.
std::string load_file(const std::string& path);

} // namespace util
} // namespace tessera

#endif // TESSERA_UTIL_LOAD_FILE_HPP
