cmake_minimum_required(VERSION 3.25)

project(Renderer)

find_package(glfw3 REQUIRED)
find_package(Vulkan REQUIRED)

message(STATUS "Vulkan_INCLUDE_DIRS = ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan_LIBRARIES    = ${Vulkan_LIBRARIES}")
message(STATUS "Vulkan_LIBRARY      = ${Vulkan_LIBRARY}")

add_library(Renderer3D STATIC
    src/renderer_3D.cpp
    src/window.cpp
    src/vertex.cpp 
    src/camera.cpp
    src/mesh.cpp
    include/stb_image.h 
    include/tiny_obj_loader.h
)

target_include_directories(Renderer3D PUBLIC ${PROJECT_SOURCE_DIR}/include ${CMAKE_CURRENT_BINARY_DIR})

add_executable(boat_test tests/boat/FirstTest.cpp)

add_test(
    NAME boat_test
    COMMAND boat_test
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/boat
)

add_dependencies(boat_test compile_shaders)

set_target_properties(boat_test PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/tests/boat)

set_property(TARGET boat_test PROPERTY CXX_STANDARD 20)

set_property(TARGET boat_test PROPERTY CMAKE_VERBOSE_MAKEFILE 1)

target_link_libraries(boat_test Renderer3D glfw Vulkan::Vulkan)

set(SHADER_SRC ${CMAKE_CURRENT_SOURCE_DIR}/src/shader.slang)
set(SHADER_SPV ${CMAKE_CURRENT_SOURCE_DIR}/tests/boat/assets/shader.spv)

add_custom_command(
    OUTPUT ${SHADER_SPV}
    COMMAND slangc ${SHADER_SRC} -target spirv -profile spirv_1_4 -emit-spirv-directly -fvk-use-entrypoint-name -entry vertMain -entry fragMain -o ${SHADER_SPV}
    DEPENDS ${SHADER_SRC}
    COMMENT "Compiling Slang shader"
)

add_custom_target(compile_shaders ALL DEPENDS ${SHADER_SPV})