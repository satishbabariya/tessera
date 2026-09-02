set(SERVER_SOURCES
    access_control.cpp
    access_token.cpp
    server.cpp
    server_dir.cpp
    server_file_access_cache.cpp
    server_history.cpp
)

set(SERVER_HEADERS
    access_control.hpp
    access_token.hpp
    clock.hpp
    crypto_server.hpp
    permissions.hpp
    server.hpp
    server_dir.hpp
    server_file_access_cache.hpp
    server_history.hpp
    server_impl_base.hpp
)


add_library(SyncServer STATIC ${SERVER_SOURCES} ${SERVER_HEADERS})
add_library(Tessera::SyncServer ALIAS SyncServer)

set_target_properties(SyncServer PROPERTIES
    OUTPUT_NAME "tessera-server"
)
target_link_libraries(SyncServer PUBLIC Sync QueryParser)

if(APPLE AND NOT TESSERA_FORCE_OPENSSL)
    target_sources(SyncServer PRIVATE crypto_server_apple.mm)
elseif(TESSERA_HAVE_OPENSSL)
    target_sources(SyncServer PRIVATE crypto_server_openssl.cpp)
else()
    target_sources(SyncServer PRIVATE crypto_server_stub.cpp)
endif()

# The public face of the server: exactly what is needed to compile server.hpp,
# and nothing else. Publishing a header is a promise about it, so the promise is
# kept as small as the API allows.
#
#   server.hpp        the API -- Server, its Config, start/stop
#   clock.hpp         Config::token_expiration_clock is a Clock*
#   crypto_server.hpp the constructor takes util::Optional<PKey>
#
# The remaining seven headers stay private: they are the history, the file
# cache, the directory layout and the access-control internals, none of which a
# caller names.
set(SERVER_INSTALL_HEADERS
    server.hpp
    clock.hpp
    crypto_server.hpp
)

install(TARGETS SyncServer EXPORT tessera
    ARCHIVE DESTINATION lib
    COMPONENT devel)

install(FILES ${SERVER_INSTALL_HEADERS}
        DESTINATION include/tessera/sync/server
        COMPONENT devel)

# A server you can run, not only link. Installed to bin alongside the inspector
# tools. Guarded: tools/check-server-not-shipped-unauthenticated.sh treats an
# executable in this directory as making the server reachable, and refuses that
# combination unless the server authenticates -- which is why this could not
# have been added before #29.
if(NOT TESSERA_BUILD_LIB_ONLY AND NOT CMAKE_SYSTEM_NAME MATCHES "^Windows")
    add_executable(TesseraSyncServer tessera_sync_server.cpp)
    set_target_properties(TesseraSyncServer PROPERTIES
                          OUTPUT_NAME "tessera-sync-server"
                          DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
    target_link_libraries(TesseraSyncServer PRIVATE SyncServer)
    install(TARGETS TesseraSyncServer
            COMPONENT runtime
            DESTINATION bin)

    # Mints the tokens the server verifies. v0.3.0 shipped a server that refuses
    # to start without a public key and nothing that could produce a token for
    # it, which made the authentication unusable rather than merely incomplete.
    add_executable(TesseraToken tessera_token.cpp)
    set_target_properties(TesseraToken PROPERTIES
                          OUTPUT_NAME "tessera-token"
                          DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
    target_link_libraries(TesseraToken PRIVATE SyncServer)
    install(TARGETS TesseraToken
            COMPONENT runtime
            DESTINATION bin)
endif()
