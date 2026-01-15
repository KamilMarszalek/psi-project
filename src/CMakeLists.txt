add_compile_options(-Wall -Wextra -Wpedantic -g)

add_library(route STATIC route.c)
add_library(ring STATIC ring.c)

target_link_libraries(ring PRIVATE route)

add_executable(main main.c)
target_link_libraries(main PRIVATE ring)
target_link_libraries(main PRIVATE route)

