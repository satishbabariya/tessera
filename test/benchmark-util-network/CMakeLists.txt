# This benchmark existed for years with no CMakeLists and no add_subdirectory,
# so nothing built it. It stopped compiling at some point during an upstream
# namespace refactor that moved end_of_input from network:: to MiscExtErrors::
# and did not touch a file the build never mentions.
#
# It is wired up here rather than deleted so that the next such refactor breaks
# the build instead of the file.
if(TESSERA_ENABLE_SYNC)
    add_executable(tessera-benchmark-util-network main.cpp)
    add_dependencies(benchmarks tessera-benchmark-util-network)
    target_link_libraries(tessera-benchmark-util-network TestUtil Sync)
endif()
