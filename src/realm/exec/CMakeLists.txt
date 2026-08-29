add_executable(RealmTrawler realm_trawler.cpp)
set_target_properties(RealmTrawler PROPERTIES
    OUTPUT_NAME "realm-trawler"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmTrawler Storage)
if (EMSCRIPTEN)
    set_target_properties(RealmTrawler PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(RealmEnumerate realm_enumerate.cpp)
set_target_properties(RealmEnumerate PROPERTIES
    OUTPUT_NAME "realm-enumerate"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmEnumerate ObjectStore)
# FIXME can be fixed for others, but requires link and install fixes for libuv target
if (NOT APPLE)
    set_target_properties(RealmEnumerate PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(RealmDecrypt realm_decrypt.cpp)
set_target_properties(RealmDecrypt PROPERTIES
    OUTPUT_NAME "realm-decrypt"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmDecrypt Storage)
if (NOT REALM_ENABLE_ENCRYPTION)
    set_target_properties(RealmDecrypt PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(RealmEncrypt realm_encrypt.cpp)
set_target_properties(RealmEncrypt PROPERTIES
    OUTPUT_NAME "realm-encrypt"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmEncrypt Storage)

if(REALM_ENABLE_SYNC)
add_executable(Realm2JSON realm2json.cpp )
set_target_properties(Realm2JSON PROPERTIES
    OUTPUT_NAME "realm2json"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(Realm2JSON Sync QueryParser)
list(APPEND ExecTargetsToInstall Realm2JSON)
endif()

add_executable(RealmDump realm_dump.c)
set_target_properties(RealmDump PROPERTIES
    OUTPUT_NAME "realm-dump"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

install(TARGETS ${ExecTargetsToInstall}
        COMPONENT runtime
        DESTINATION bin)
