cmake_minimum_required(VERSION 3.10.2)

project("graffito")

add_compile_options(-std=c11 -Weverything -Werror)

# libgraffito.so
file(GLOB GRAFFITO_SRC sources/*/*.c)
add_library(graffito SHARED ${GRAFFITO_SRC})
target_include_directories(graffito PUBLIC include sources/app sources/io sources/linker sources/procfs)
target_link_libraries(graffito log dl)
