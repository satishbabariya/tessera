if(TESSERA_ENABLE_SYNC)
    add_executable(tessera-benchmark-sync bench_transform.cpp ../test_all.cpp)
    add_dependencies(benchmarks tessera-benchmark-sync)
    # Sync lib is included with SyncServer
    target_link_libraries(tessera-benchmark-sync TestUtil SyncServer)

    # The load generator. Built, because the file that stood here before was not
    # -- it included a load_tester.hpp that exists nowhere in this repository or
    # its history, so it could never have compiled, and nothing noticed because
    # nothing built it.
    add_executable(TesseraLoadTest load_test.cpp)
    set_target_properties(TesseraLoadTest PROPERTIES
                          OUTPUT_NAME "tessera-load-test"
                          DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
    add_dependencies(benchmarks TesseraLoadTest)
    target_link_libraries(TesseraLoadTest Sync Storage)
endif()