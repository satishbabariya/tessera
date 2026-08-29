add_executable(tessera-benchmark-crud main.cpp)
add_dependencies(benchmarks tessera-benchmark-crud)
target_link_libraries(tessera-benchmark-crud TestUtil)
