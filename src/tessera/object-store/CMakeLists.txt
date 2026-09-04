
set(SOURCES
    collection.cpp
    collection_notifications.cpp
    dictionary.cpp
    index_set.cpp
    list.cpp
    class.cpp
    object.cpp
    object_changeset.cpp
    object_schema.cpp
    object_store.cpp
    results.cpp
    schema.cpp
    sectioned_results.cpp
    set.cpp
    shared_db.cpp
    thread_safe_reference.cpp

    impl/collection_change_builder.cpp
    impl/collection_notifier.cpp
    impl/deep_change_checker.cpp
    impl/list_notifier.cpp
    impl/object_notifier.cpp
    impl/realm_coordinator.cpp
    impl/results_notifier.cpp
    impl/transact_log_handler.cpp
    impl/weak_realm_notifier.cpp
    util/scheduler.cpp
    util/uuid.cpp

    util/android/scheduler.hpp
    util/apple/scheduler.hpp
    util/generic/scheduler.hpp
    util/uv/scheduler.hpp)

set(HEADERS
    audit.hpp
    audit_serializer.hpp
    binding_context.hpp
    collection.hpp
    collection_notifications.hpp
    dictionary.hpp
    feature_checks.hpp
    index_set.hpp
    keypath_helpers.hpp
    list.hpp
    class.hpp
    object.hpp
    object_accessor.hpp
    object_changeset.hpp
    object_schema.hpp
    object_store.hpp
    property.hpp
    results.hpp
    schema.hpp
    sectioned_results.hpp
    set.hpp
    shared_db.hpp
    thread_safe_reference.hpp

    impl/apple/external_commit_helper.hpp
    impl/apple/keychain_helper.hpp
    impl/epoll/external_commit_helper.hpp
    impl/generic/external_commit_helper.hpp

    impl/collection_change_builder.hpp
    impl/collection_notifier.hpp
    impl/deep_change_checker.hpp
    impl/external_commit_helper.hpp
    impl/list_notifier.hpp
    impl/notification_wrapper.hpp
    impl/object_accessor_impl.hpp
    impl/object_notifier.hpp
    impl/realm_coordinator.hpp
    impl/results_notifier.hpp
    impl/transact_log_handler.hpp
    impl/weak_realm_notifier.hpp

    util/aligned_union.hpp
    util/atomic_shared_ptr.hpp
    util/copyable_atomic.hpp
    util/event_loop_dispatcher.hpp
    util/scheduler.hpp
    util/tagged_string.hpp
    util/uuid.hpp

)

if(TESSERA_ENABLE_SYNC)
    list(APPEND HEADERS
        sync/async_open_task.hpp
        sync/generic_network_transport.hpp
        sync/jwt.hpp
        sync/sync_client_config.hpp
        sync/sync_manager.hpp
        sync/sync_session.hpp
        sync/sync_user.hpp

        sync/impl/network_reachability.hpp
        sync/impl/sync_client.hpp
        sync/impl/sync_file.hpp)

    list(APPEND SOURCES
        sync/async_open_task.cpp
        sync/generic_network_transport.cpp
        sync/impl/sync_file.cpp
        sync/jwt.cpp
        sync/sync_manager.cpp
        sync/sync_session.cpp)


    if(APPLE)
        list(APPEND HEADERS
            sync/impl/apple/network_reachability_observer.hpp
            sync/impl/apple/system_configuration.hpp)

        list(APPEND SOURCES
            sync/impl/apple/network_reachability_observer.cpp
            sync/impl/apple/system_configuration.cpp)
    elseif(EMSCRIPTEN)
        list(APPEND HEADERS
            sync/impl/emscripten/network_transport.hpp
            sync/impl/emscripten/socket_provider.hpp)
        list(APPEND SOURCES
            sync/impl/emscripten/network_transport.cpp
            sync/impl/emscripten/socket_provider.cpp)
    endif()
endif()

add_library(ObjectStore STATIC ${SOURCES} ${HEADERS})
add_library(Tessera::ObjectStore ALIAS ObjectStore)

set_target_properties(ObjectStore PROPERTIES
    OUTPUT_NAME tessera-object-store
)

check_symbol_exists(epoll_create sys/epoll.h TESSERA_HAVE_EPOLL)

if(APPLE)
    target_sources(ObjectStore PRIVATE impl/apple/external_commit_helper.cpp impl/apple/keychain_helper.cpp)
    target_link_options(ObjectStore INTERFACE "SHELL:-framework Security")
elseif(TESSERA_HAVE_EPOLL)
    target_compile_definitions(ObjectStore PUBLIC TESSERA_HAVE_EPOLL=1)
    target_sources(ObjectStore PRIVATE impl/epoll/external_commit_helper.cpp)
elseif(CMAKE_SYSTEM_NAME MATCHES "^Windows")
    target_sources(ObjectStore PRIVATE impl/windows/external_commit_helper.cpp)
elseif(EMSCRIPTEN)
    target_sources(ObjectStore PRIVATE impl/emscripten/external_commit_helper.cpp)
else()
    target_sources(ObjectStore PRIVATE impl/generic/external_commit_helper.cpp)
endif()

if(TESSERA_ENABLE_SYNC AND NOT TESSERA_SYNC_MULTIPLEXING)
    target_compile_definitions(ObjectStore PUBLIC TESSERA_DISABLE_SYNC_MULTIPLEXING=1)
endif()

if(TESSERA_ENABLE_SYNC OR TESSERA_ENABLE_SERVER)
    # needed to disable assertions in external/json
    target_compile_definitions(ObjectStore PUBLIC
        $<$<CONFIG:Release>:NDEBUG>
        $<$<CONFIG:RelWithDebInfo>:NDEBUG>
    )
    target_include_directories(ObjectStore PRIVATE ${JSON_INCLUDE_DIR})
endif()

target_link_libraries(ObjectStore PUBLIC Storage QueryParser)

if(TESSERA_ENABLE_SYNC)
    target_link_libraries(ObjectStore PUBLIC Sync)
    target_compile_definitions(ObjectStore PUBLIC TESSERA_ENABLE_SYNC=1)
    if(EMSCRIPTEN)
        target_link_options(ObjectStore PUBLIC -sFETCH=1 -sFETCH_SUPPORT_INDEXEDDB=0 -lwebsocket.js)
    endif()
endif()


set_target_properties(ObjectStore PROPERTIES OUTPUT_NAME "tessera-object-store")

# HEADERS serves add_library, where listing every header is right: it is what
# gives IDEs the file tree and CMake its dependency edges. It was also driving
# the install, and those two lists want different things -- which is how
# thirteen headers under impl/ came to ship in the package.
#
# Nothing installed includes any of them, checked header by header, and `impl`
# says what they are. A consumer that reaches for realm_coordinator.hpp or
# transact_log_helper.hpp is reaching past the API into machinery that changes
# without notice, and shipping them advertises that as supported.
#
# So the install list is derived from HEADERS by excluding impl/. Deriving it
# rather than writing a second list by hand is deliberate: a hand-maintained
# copy drifts, and this project has a finding about a gate that enumerated its
# own members and fell behind them.
# The impl/ headers no installed header includes, measured by compiling every
# documented entry point with -H and diffing the reached set against the
# installed set. Not all of impl/ can go: object.hpp includes
# impl/collection_notifier.hpp, and collection_change_builder.hpp and
# deep_change_checker.hpp arrive the same way. Excluding the directory wholesale
# breaks the package, which is how that was discovered.
set(OBJECT_STORE_INTERNAL_HEADERS
    impl/apple/external_commit_helper.hpp
    impl/apple/keychain_helper.hpp
    impl/epoll/external_commit_helper.hpp
    impl/external_commit_helper.hpp
    impl/generic/external_commit_helper.hpp
    impl/list_notifier.hpp
    impl/notification_wrapper.hpp
    impl/object_accessor_impl.hpp
    impl/object_notifier.hpp
    impl/realm_coordinator.hpp
    impl/results_notifier.hpp
    impl/transact_log_handler.hpp
    impl/weak_realm_notifier.hpp

    # The sync half, found the same way. sync/impl/sync_client.hpp was the only
    # installed header reaching any of these, and nothing installed reaches it:
    # its four includers are all .cpp files in this library, plus one test
    # helper that compiles against the source tree.
    #
    # Removing it takes the rest with it. network_reachability.hpp was reachable
    # only through sync_client.hpp and the two Apple headers, which were
    # themselves reachable only through sync_client.hpp -- so the installed
    # surface stops being platform-dependent, which it had been solely because
    # of this one header.
    #
    # It was also broken where nobody looked. Under __EMSCRIPTEN__,
    # sync_client.hpp includes sync/impl/emscripten/socket_provider.hpp, which
    # never installed: a consumer building the package for Emscripten got a
    # missing header. No CI job builds that target, so the package was
    # unbuildable there for as long as it has existed.
    sync/impl/apple/network_reachability_observer.hpp
    sync/impl/apple/system_configuration.hpp
    sync/impl/emscripten/network_transport.hpp
    sync/impl/emscripten/socket_provider.hpp
    sync/impl/network_reachability.hpp
    sync/impl/sync_client.hpp
    sync/impl/sync_file.hpp

    # Not under impl/, and internal on stronger grounds than anything above it.
    # audit_serializer.hpp is included by nothing -- no header, no source, no
    # test. It declares AuditObjectSerializer, which has no definition anywhere
    # in the tree and no symbol in the built library, so a consumer who used it
    # would not link. It has no include guard, so including it twice does not
    # compile. And it was the only installed header needing
    # external/json/json.hpp, which is why a vendored third-party directory was
    # published at the root of the include path.
    #
    # Audit is an inert extension point: make_audit_context terminates with
    # "Audit is not implemented in this build" since the Apple-only App Services
    # implementation was deleted. audit.hpp stays installed as the documented
    # extension point; the serializer for a backend that no longer exists does
    # not.
    audit_serializer.hpp

    # Two headers that nothing in the repository includes -- no header, no
    # source, no test -- and that no documented entry point reaches. Unlike
    # audit_serializer.hpp they are self-contained and harmless, so they stay in
    # the tree as removal candidates rather than being deleted here; what stops
    # is publishing them as API nobody asked for.
    util/aligned_union.hpp
    util/tagged_string.hpp
)

set(OBJECT_STORE_INSTALL_HEADERS "")
foreach(FILE ${HEADERS})
    if(NOT FILE IN_LIST OBJECT_STORE_INTERNAL_HEADERS)
        list(APPEND OBJECT_STORE_INSTALL_HEADERS ${FILE})
    endif()
endforeach()

foreach(FILE ${OBJECT_STORE_INSTALL_HEADERS})
    get_filename_component(DIR ${FILE} DIRECTORY)
    INSTALL(FILES ${FILE}
            DESTINATION include/tessera/object-store/${DIR}
            COMPONENT devel)
endforeach()
install(TARGETS ObjectStore EXPORT tessera
        ARCHIVE DESTINATION lib
        COMPONENT devel)
