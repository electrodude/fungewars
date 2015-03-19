cmake_minimum_required(VERSION 2.8.4)
project(fungewars)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

include_directories(${CMAKE_SOURCE_DIR})

find_package(OpenGL REQUIRED)
find_package(GLUT REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)

add_executable(fungewars
    fungewars.o
    interpreter
    graphics
)

target_link_libraries(fungewars glut GLU GL m pthread png)

