cmake_minimum_required(VERSION 3.15)

project(ld46)
add_executable(ld46 main.c)

set_property(TARGET ld46 PROPERTY C_STANDARD 99)

file(COPY assets DESTINATION ${CMAKE_BINARY_DIR})

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/Modules")

find_package(SDL2 REQUIRED)
find_package(SDL2_image REQUIRED)
find_package(SDL2_ttf REQUIRED)
find_package(SDL2_mixer REQUIRED)

include_directories(${SDL2_INCLUDE_DIR}
                    ${SDL2_IMAGE_INCLUDE_DIR}
                    ${SDL2_MIXER_INCLUDE_DIR}
                    ${SDL2_TTF_INCLUDE_DIR})

target_link_libraries(ld46 m
                           ${SDL2_LIBRARY}
                           ${SDL2_IMAGE_LIBRARIES}
                           ${SDL2_MIXER_LIBRARIES}
                           ${SDL2_TTF_LIBRARIES})
