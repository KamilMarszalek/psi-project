add_compile_options(-Wall -Wextra -Wpedantic -g)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(logger STATIC logger.c)

add_library(cli STATIC cli.c)
target_link_libraries(cli PRIVATE logger)

add_library(rudp STATIC rudp.c)

add_library(unicast STATIC unicast.c)
target_link_libraries(unicast PRIVATE rudp logger)

add_library(ring STATIC ring.c)
target_link_libraries(ring PRIVATE unicast logger)

add_executable(core core.c)
target_link_libraries(core PRIVATE ring cli)

add_executable(writer writer.c)
target_link_libraries(writer PRIVATE cli)

