add_compile_options(-Wall -Wextra -Wpedantic -g)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(cli STATIC cli.c)
add_library(rudp STATIC rudp.c)

add_library(unicast STATIC unicast.c)
target_link_libraries(unicast PRIVATE rudp)

add_library(ring STATIC ring.c)
target_link_libraries(ring PRIVATE unicast)

add_executable(core core.c)
target_link_libraries(core PRIVATE ring)
target_link_libraries(core PRIVATE cli)

add_executable(writer writer.c)
target_link_libraries(writer PRIVATE cli)

