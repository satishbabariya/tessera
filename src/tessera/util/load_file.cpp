#include <tessera/util/buffer.hpp>
#include <tessera/util/file.hpp>
#include <tessera/util/load_file.hpp>

using namespace tessera;


std::string util::load_file(const std::string& path)
{
    util::File file{path}; // Throws
    util::Buffer<char> buffer;
    std::size_t used_size = 0;
    for (;;) {
        std::size_t min_extra_capacity = 256;
        buffer.reserve_extra(used_size, min_extra_capacity);                             // Throws
        std::size_t n = file.read(used_size, buffer.data() + used_size, buffer.size() - used_size); // Throws
        if (n == 0)
            break;
        used_size += n;
    }
    return std::string(buffer.data(), used_size); // Throws
}
