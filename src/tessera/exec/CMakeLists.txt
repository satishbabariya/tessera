add_executable(TesseraTrawler tessera_trawler.cpp)
set_target_properties(TesseraTrawler PROPERTIES
    OUTPUT_NAME "tessera-trawler"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(TesseraTrawler Storage)
if (EMSCRIPTEN)
    set_target_properties(TesseraTrawler PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(TesseraEnumerate tessera_enumerate.cpp)
set_target_properties(TesseraEnumerate PROPERTIES
    OUTPUT_NAME "tessera-enumerate"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(TesseraEnumerate ObjectStore)
# FIXME can be fixed for others, but requires link and install fixes for libuv target
if (NOT APPLE)
    set_target_properties(TesseraEnumerate PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(TesseraDecrypt tessera_decrypt.cpp)
set_target_properties(TesseraDecrypt PROPERTIES
    OUTPUT_NAME "tessera-decrypt"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(TesseraDecrypt Storage)
if (NOT TESSERA_ENABLE_ENCRYPTION)
    set_target_properties(TesseraDecrypt PROPERTIES EXCLUDE_FROM_ALL TRUE)
endif()

add_executable(TesseraEncrypt tessera_encrypt.cpp)
set_target_properties(TesseraEncrypt PROPERTIES
    OUTPUT_NAME "tessera-encrypt"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(TesseraEncrypt Storage)

if(TESSERA_ENABLE_SYNC)
add_executable(Tessera2JSON tessera2json.cpp )
set_target_properties(Tessera2JSON PROPERTIES
    OUTPUT_NAME "tessera2json"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)
target_link_libraries(Tessera2JSON Sync QueryParser)
list(APPEND ExecTargetsToInstall Tessera2JSON)
endif()

add_executable(TesseraDump tessera_dump.c)
set_target_properties(TesseraDump PROPERTIES
    OUTPUT_NAME "tessera-dump"
    DEBUG_POSTFIX ${CMAKE_DEBUG_POSTFIX}
)

install(TARGETS ${ExecTargetsToInstall}
        COMPONENT runtime
        DESTINATION bin)
