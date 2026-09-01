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
