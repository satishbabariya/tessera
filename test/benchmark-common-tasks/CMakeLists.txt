add_executable(tessera-benchmark-common-tasks main.cpp)
target_link_libraries(tessera-benchmark-common-tasks TestUtil QueryParser)
add_dependencies(benchmarks tessera-benchmark-common-tasks)

