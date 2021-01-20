cmake_minimum_required(VERSION 3.10.2)

project("graffito")

add_compile_options(-std=c11 -Weverything -Werror)

file(GLOB GRAFFITO_SRC app/*.c linker/*.c proc/*.c util/*.c)
add_library(graffito SHARED ${GRAFFITO_SRC})
target_include_directories(graffito PUBLIC app linker proc util)
target_link_libraries(graffito log dl)
