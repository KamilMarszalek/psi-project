add_compile_options(-Wall -Wextra -Wpedantic -g)

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/include)

add_library(unicast STATIC unicast.c)
add_library(cli STATIC cli.c)

add_library(ring STATIC ring.c)
target_link_libraries(ring PRIVATE unicast)

add_executable(main main.c)
target_link_libraries(main PRIVATE ring)
target_link_libraries(main PRIVATE cli)

