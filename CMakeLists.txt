cmake_minimum_required(VERSION 2.8.4)
project(fungewars C)

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -std=c99")
set(CMAKE_C_FLAGS_DEBUG "${CMAKE_C_FLAGS_DEBUG} -Og -g")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2")

include_directories(${CMAKE_SOURCE_DIR})

find_package(OpenGL REQUIRED)
find_package(PNG REQUIRED)
find_package(Threads REQUIRED)

set(GLFW_BUILD_EXAMPLES OFF)
set(GLFW_BUILD_TESTS OFF)
set(GLFW_BUILD_DOCS OFF)
add_subdirectory(glfw)
include_directories(glfw/include)

add_executable(fungewars
	fungewars.c
	png_to_tex
	interpreter
	graphics
)

target_link_libraries(fungewars m pthread png glfw ${OPENGL_glu_LIBRARY} ${GLFW_LIBRARIES})

