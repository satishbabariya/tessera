add_executable(RealmTrawler tessera_trawler.cpp)
set_target_properties(RealmTrawler PROPERTIES
    OUTPUT_NAME "tessera-trawler"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmTrawler Storage)
if (EMSCRIPTEN)
    set_target_properties(RealmTrawler PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(RealmEnumerate tessera_enumerate.cpp)
set_target_properties(RealmEnumerate PROPERTIES
    OUTPUT_NAME "tessera-enumerate"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmEnumerate ObjectStore)
# FIXME can be fixed for others, but requires link and install fixes for libuv target
if (NOT APPLE)
    set_target_properties(RealmEnumerate PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(RealmDecrypt tessera_decrypt.cpp)
set_target_properties(RealmDecrypt PROPERTIES
    OUTPUT_NAME "tessera-decrypt"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmDecrypt Storage)
if (NOT TESSERA_ENABLE_ENCRYPTION)
    set_target_properties(RealmDecrypt PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(RealmEncrypt tessera_encrypt.cpp)
set_target_properties(RealmEncrypt PROPERTIES
    OUTPUT_NAME "tessera-encrypt"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(RealmEncrypt Storage)

if(TESSERA_ENABLE_SYNC)
add_executable(Realm2JSON tessera2json.cpp )
set_target_properties(Realm2JSON PROPERTIES
    OUTPUT_NAME "tessera2json"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(Realm2JSON Sync QueryParser)
list(APPEND ExecTargetsToInstall Realm2JSON)
endif()

add_executable(RealmDump tessera_dump.c)
set_target_properties(RealmDump PROPERTIES
    OUTPUT_NAME "tessera-dump"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

install(TARGETS ${ExecTargetsToInstall}
        COMPONENT runtime
        DESTINATION bin)
