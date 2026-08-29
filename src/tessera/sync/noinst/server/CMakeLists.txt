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
