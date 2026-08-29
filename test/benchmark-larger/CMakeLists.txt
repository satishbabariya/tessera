add_executable(tessera-benchmark-larger EXCLUDE_FROM_ALL main.cpp)
add_dependencies(benchmarks tessera-benchmark-larger)
target_link_libraries(tessera-benchmark-larger TestUtil)
