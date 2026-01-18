add_compile_options(-Wall -Wextra -Wpedantic -g)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(logger STATIC logger.c)

add_library(cli STATIC cli.c)
target_link_libraries(cli PRIVATE logger)

add_library(rudp STATIC rudp.c)
target_link_libraries(rudp PRIVATE logger)

add_library(unicast STATIC unicast.c)
add_library(join STATIC join.c)
add_library(broadcast STATIC broadcast.c)
target_link_libraries(unicast PRIVATE rudp logger)

add_library(ring STATIC ring.c)
target_link_libraries(ring PRIVATE unicast logger)
target_link_libraries(ring PRIVATE broadcast)
target_link_libraries(ring PRIVATE join)

add_executable(core core.c)
target_link_libraries(core PRIVATE ring cli)

add_executable(writer writer.c)
target_link_libraries(writer PRIVATE cli)

