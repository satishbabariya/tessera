# The convergent merge engine: changesets, instructions, and operational
# transform.
#
# Deliberately standalone. This code was previously compiled into the monolithic
# Sync library alongside the wire protocol, transport and session machinery,
# which meant nothing could depend on the merge engine without also pulling in a
# websocket stack. It depends on the storage engine and nothing else, enforced by
# tools/check-merge-deps.sh.
set(MERGE_SOURCES
    changeset.cpp
    changeset_encoder.cpp
    changeset_index.cpp
    changeset_parser.cpp
    instruction_applier.cpp
    instruction_replication.cpp
    instructions.cpp
    object_id.cpp
    transform.cpp)

set(MERGE_HEADERS
    changeset.hpp
    changeset_encoder.hpp
    changeset_index.hpp
    changeset_parser.hpp
    instruction_applier.hpp
    instruction_replication.hpp
    instructions.hpp
    integer_codec.hpp
    object_id.hpp
    transform.hpp)

add_library(Merge STATIC ${MERGE_SOURCES} ${MERGE_HEADERS})
add_library(Tessera::Merge ALIAS Merge)
set_target_properties(Merge PROPERTIES OUTPUT_NAME "tessera-merge")

# The layering contract, expressed as the build graph.
target_link_libraries(Merge PUBLIC Storage)

# integer_codec.hpp is a build input, not API: it is the varint encoding the
# changeset format uses internally, reachable from no declared entry point and
# included by no installed header. See tools/api-entry-points.txt.
set(MERGE_INTERNAL_HEADERS
    integer_codec.hpp
)

foreach(FILE ${MERGE_HEADERS})
    if(NOT FILE IN_LIST MERGE_INTERNAL_HEADERS)
        install(FILES ${FILE} DESTINATION include/tessera/merge COMPONENT devel)
    endif()
endforeach()
install(TARGETS Merge EXPORT tessera COMPONENT devel)
