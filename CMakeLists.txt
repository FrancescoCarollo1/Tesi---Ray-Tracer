cmake_minimum_required(VERSION 3.16.3)

project(RayTracer LANGUAGES C CXX)



add_subdirectory(libs)


# =================== EXAMPLE ===================

add_executable(esempio)
target_sources(esempio PUBLIC src/main.cpp)
target_link_libraries(esempio IMGUI stb_image)
set_target_properties(esempio PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})