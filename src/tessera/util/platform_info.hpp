#ifndef TESSERA_UTIL_PLATFORM_INFO_HPP
#define TESSERA_UTIL_PLATFORM_INFO_HPP

#include <string>


namespace tessera {
namespace util {

/// Get a description of the current system platform.
///
/// Returns a space-separated concatenation of `osname`, `sysname`, `release`,
/// `version`, and `machine` as returned by get_platform_info(PlatformInfo&).
std::string get_platform_info();


struct PlatformInfo {
    std::string osname;  ///< Equivalent to `uname -o` (Linux).
    std::string sysname; ///< Equivalent to `uname -s`.
    std::string release; ///< Equivalent to `uname -r`.
    std::string version; ///< Equivalent to `uname -v`.
    std::string machine; ///< Equivalent to `uname -m`.
};

/// Get a description of the current system platform.
void get_platform_info(PlatformInfo&);


// Implementation

inline std::string get_platform_info()
{
    PlatformInfo info;
    get_platform_info(info); // Throws
    return (info.osname + " " + info.sysname + " " + info.release + " " + info.version + " " +
            info.machine); // Throws
}

inline std::string get_library_platform()
{
#if TESSERA_ANDROID
    return "Android";
#elif TESSERA_WINDOWS
    return "Windows";
#elif TESSERA_UWP
    return "UWP";
#elif TESSERA_MACCATALYST // test Catalyst first because it's a subset of iOS
    return "Mac Catalyst";
#elif TESSERA_IOS
    return "iOS";
#elif TESSERA_TVOS
    return "tvOS";
#elif TESSERA_WATCHOS
    return "watchOS";
#elif TESSERA_PLATFORM_APPLE
    return "macOS";
#elif TESSERA_LINUX
    return "Linux";
#endif

    return "unknown";
}

inline std::string get_library_cpu_arch()
{
#if TESSERA_ARCHITECTURE_ARM32
    return "arm";
#elif TESSERA_ARCHITECTURE_ARM64
    return "arm64";
#elif TESSERA_ARCHITECTURE_X86_32
    return "x86";
#elif TESSERA_ARCHITECTURE_X86_64
    return "x86_64";
#endif

    return "unknown";
}

} // namespace util
} // namespace tessera

#endif // TESSERA_UTIL_PLATFORM_INFO_HPP
