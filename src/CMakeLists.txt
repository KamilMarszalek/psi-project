add_library(project_warnings INTERFACE)
target_compile_options(project_warnings INTERFACE -Wall -Wextra -Wpedantic -g)

add_library(logger STATIC logger.c)
target_link_libraries(logger PUBLIC project_warnings)
target_include_directories(logger PUBLIC
  ${CMAKE_CURRENT_SOURCE_DIR}/include
)

add_library(broadcast_collect_targets STATIC broadcast_targets.c)
target_link_libraries(broadcast_collect_targets PUBLIC logger)

add_library(cli STATIC cli.c)
target_link_libraries(cli PUBLIC logger)

add_library(rudp STATIC rudp.c)
target_link_libraries(rudp PUBLIC logger)

add_library(unicast STATIC unicast.c)
target_link_libraries(unicast PUBLIC rudp logger)

add_library(join STATIC join.c)
target_link_libraries(join PUBLIC logger)

add_library(broadcast STATIC broadcast.c)
target_link_libraries(broadcast PUBLIC broadcast_collect_targets logger)

add_library(ring STATIC ring.c)
target_link_libraries(ring PUBLIC unicast broadcast join logger)

add_executable(core core.c)
target_link_libraries(core PRIVATE ring cli)

add_executable(writer writer.c)
target_link_libraries(writer PRIVATE cli)

