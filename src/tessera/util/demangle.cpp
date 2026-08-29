#include <cstdlib>
#include <memory>

#include <tessera/util/features.h>
#include <tessera/util/assert.hpp>
#include <tessera/util/demangle.hpp>
#include <tessera/util/backtrace.hpp>

#if TESSERA_HAVE_AT_LEAST_GCC(3, 2)
#define TESSERA_HAVE_CXXABI_DEMANGLE
#include <cxxabi.h>
#endif


// See http://gcc.gnu.org/onlinedocs/libstdc++/latest-doxygen/namespaceabi.html
//
// FIXME: Could use the Autoconf macro 'ax_cxx_gcc_abi_demangle'. See
// http://autoconf-archive.cryp.to.
std::string tessera::util::demangle(const std::string& mangled_name)
{
#ifdef TESSERA_HAVE_CXXABI_DEMANGLE
    int status = 0;
    char* unmangled_name = abi::__cxa_demangle(mangled_name.c_str(), nullptr, nullptr, &status);
    switch (status) {
        case 0:
            TESSERA_ASSERT(unmangled_name);
            goto demangled;
        case -1:
            TESSERA_ASSERT(!unmangled_name);
            throw util::bad_alloc{};
    }
    TESSERA_ASSERT(!unmangled_name);
    return mangled_name; // Throws
demangled:
    class Free {
    public:
        void operator()(char* p) const
        {
            std::free(p);
        }
    };
    std::unique_ptr<char[], Free> owner{unmangled_name};
    std::string demangled_name_2{unmangled_name}; // Throws
    return demangled_name_2;
#else
    return mangled_name; // Throws
#endif
}
