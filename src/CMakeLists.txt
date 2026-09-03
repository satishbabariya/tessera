add_subdirectory(tessera)
add_subdirectory(external/IntelRDFPMathLib20U2)
if (NOT TESSERA_BUILD_LIB_ONLY)
    add_subdirectory(external/bson EXCLUDE_FROM_ALL)
endif()

if (TESSERA_ENABLE_GEOSPATIAL)
    add_subdirectory(external/s2)
endif()

if(CMAKE_SYSTEM_NAME MATCHES "^Windows")
  add_subdirectory(win32)
endif()

# tessera.hpp only. It is the umbrella header, documented as such in its own
# docstring, included by a dozen test files, and it forwards to
# <tessera/engine.hpp>.
#
# tessera/engine.hpp used to be listed here too, which installed a second,
# byte-identical copy of it as <engine.hpp> at the root of the include path.
# Nothing referenced that copy and the README names the entry point as
# <tessera/engine.hpp>, so it was a duplicate occupying an extremely generic
# name in every consumer's include path -- the sort of thing that collides with
# somebody else's engine.hpp and is hard to diagnose from the other side.
install(FILES tessera.hpp
        DESTINATION include
        COMPONENT devel)
