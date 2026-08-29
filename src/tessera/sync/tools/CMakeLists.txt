
add_executable(InspectorInspectClientRealm "inspect_client_realm.cpp")
set_target_properties(InspectorInspectClientRealm PROPERTIES
                      OUTPUT_NAME "tessera-inspect-client-db"
                      DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
target_link_libraries(InspectorInspectClientRealm Sync Storage)

add_executable(InspectorPrintChangeset "print_changeset.cpp")
set_target_properties(InspectorPrintChangeset PROPERTIES
                      OUTPUT_NAME "tessera-print-changeset"
                      DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
target_link_libraries(InspectorPrintChangeset Sync Storage)

add_executable(ApplyToStateCommand apply_to_state_command.cpp)
set_target_properties(ApplyToStateCommand PROPERTIES
                      OUTPUT_NAME "tessera-apply-to-state"
                      DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
target_link_libraries(ApplyToStateCommand Sync Storage)

add_executable(HistCommand hist_command.cpp)
set_target_properties(HistCommand PROPERTIES
                      OUTPUT_NAME "tessera-hist"
                      DEBUG_POSTFIX "${CMAKE_DEBUG_POSTFIX}")
target_link_libraries(HistCommand Sync Storage)

install(TARGETS
    HistCommand
    ApplyToStateCommand
    InspectorInspectClientRealm
    InspectorPrintChangeset
    COMPONENT runtime
    DESTINATION bin
)
