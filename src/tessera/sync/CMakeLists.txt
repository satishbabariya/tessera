set(SYNC_SOURCES
    config.cpp
    noinst/client_history_impl.cpp
    noinst/client_impl_base.cpp
    noinst/client_reset.cpp
    noinst/client_reset_operation.cpp
    noinst/client_reset_recovery.cpp
    noinst/migration_store.cpp
    noinst/pending_bootstrap_store.cpp
    noinst/pending_reset_store.cpp
    noinst/protocol_codec.cpp
    noinst/sync_metadata_schema.cpp
    noinst/sync_schema_migration.cpp
    client.cpp
    history.cpp
    protocol.cpp
    subscriptions.cpp
    network/default_socket.cpp
    network/http.cpp
    network/network.cpp
    network/network_ssl.cpp
    network/websocket.cpp
)

set(IMPL_INSTALL_HEADERS
    impl/clock.hpp
    impl/clamped_hex_dump.hpp
)

set(SYNC_INSTALL_HEADERS
    binding_callback_thread_observer.hpp
    config.hpp
    client.hpp
    client_base.hpp
    socket_provider.hpp
    history.hpp
    protocol.hpp
    subscriptions.hpp
)

set(SYNC_NETWORK_INSTALL_HEADERS
    network/default_socket.hpp
    network/http.hpp
    network/network.hpp
    network/network_ssl.hpp
    network/websocket.hpp
    network/websocket_error.hpp
)

set(NOINST_HEADERS
    trigger.hpp

    noinst/client_history_impl.hpp
    noinst/client_impl_base.hpp
    noinst/client_reset.hpp
    noinst/client_reset_operation.hpp
    noinst/client_reset_recovery.hpp
    noinst/migration_store.hpp
    noinst/pending_bootstrap_store.hpp
    noinst/pending_reset_store.hpp
    noinst/protocol_codec.hpp
    noinst/root_certs.hpp
    noinst/sync_metadata_schema.hpp
    noinst/sync_schema_migration.hpp
)

set(SYNC_HEADERS ${IMPL_INSTALL_HEADERS}
    ${SYNC_INSTALL_HEADERS}
    ${SYNC_NETWORK_INSTALL_HEADERS}
    ${NOINST_HEADERS})

add_library(Sync STATIC ${SYNC_SOURCES} ${SYNC_HEADERS})
add_library(Tessera::Sync ALIAS Sync)

set_target_properties(Sync PROPERTIES
    OUTPUT_NAME "tessera-sync"
)

if(NOT TESSERA_SYNC_MULTIPLEXING)
    target_compile_definitions(Sync PUBLIC TESSERA_DISABLE_SYNC_MULTIPLEXING=1)
endif()

target_link_libraries(Sync PUBLIC Merge Storage)

if(APPLE AND NOT TESSERA_FORCE_OPENSSL)
    target_link_options(Sync INTERFACE "SHELL:-framework Security")
elseif(TESSERA_HAVE_OPENSSL)
    target_link_libraries(Sync PUBLIC OpenSSL::SSL)
endif()

if(WIN32)
    if(NOT WINDOWS_STORE)
        target_link_libraries(Sync INTERFACE Version.lib)
    endif()
    target_link_libraries(Sync INTERFACE Crypt32.lib)
endif()

install(TARGETS Sync EXPORT tessera
    ARCHIVE COMPONENT devel
    PUBLIC_HEADER COMPONENT devel)

install(FILES ${IMPL_INSTALL_HEADERS}
        DESTINATION include/tessera/impl
        COMPONENT devel)

install(FILES ${SYNC_INSTALL_HEADERS}
        DESTINATION include/tessera/sync
        COMPONENT devel)

install(FILES ${SYNC_NETWORK_INSTALL_HEADERS}
        DESTINATION include/tessera/sync/network
        COMPONENT devel)

install(FILES ../../external/mpark/variant.hpp
        DESTINATION include/external/mpark
        COMPONENT devel)
install(FILES ../../external/json/json.hpp
        DESTINATION include/external/json
        COMPONENT devel)

add_subdirectory(noinst/server)

if(NOT TESSERA_BUILD_LIB_ONLY AND NOT CMAKE_SYSTEM_NAME MATCHES "^Windows")
    add_subdirectory(tools)
    set_macos_only(tools)
endif()

