cmake_minimum_required(VERSION 3.16.3)

project(RayTracer LANGUAGES C CXX)



add_subdirectory(libs/imgui)


#=================== EXAMPLE ===================

add_executable(example_sdl3_opengl3)
target_sources(example_sdl3_opengl3 PUBLIC src/main.cpp)
target_link_libraries(example_sdl3_opengl3 IMGUI)
set_target_properties(example_sdl3_opengl3 PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})