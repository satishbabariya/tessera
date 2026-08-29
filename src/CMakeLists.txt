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

install(FILES tessera.hpp tessera/engine.hpp
        DESTINATION include
        COMPONENT devel)
