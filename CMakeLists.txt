cmake_minimum_required(VERSION 3.16.3)

project(RayTracer LANGUAGES C CXX)



add_subdirectory(libs/imgui)


# =================== EXAMPLE ===================

add_executable(esempio)
target_sources(esempio PUBLIC src/main.cpp)
target_link_libraries(esempio IMGUI)
set_target_properties(esempio PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})