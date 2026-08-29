if(TESSERA_ENABLE_SYNC)
    add_executable(tessera-benchmark-sync bench_transform.cpp ../test_all.cpp)
    add_dependencies(benchmarks tessera-benchmark-sync)
    # Sync lib is included with SyncServer
    target_link_libraries(tessera-benchmark-sync TestUtil SyncServer)
endif()