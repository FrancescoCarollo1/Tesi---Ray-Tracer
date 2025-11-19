cmake_minimum_required(VERSION 3.16.3)

project(RayTracer LANGUAGES C CXX)

# --- 1. TROVA IL PACCHETTO OPENMP ---
find_package(OpenMP REQUIRED)
# ------------------------------------

add_subdirectory(libs)


# =================== EXAMPLE ===================

add_executable(esempio)
target_sources(esempio PUBLIC src/main.cpp)

# --- LINKA LE LIBRERIE NECESSARIE ---
target_link_libraries(esempio 
    IMGUI 
    stb_image 
    ray_tracer 
    OpenMP::OpenMP_C
    OpenMP::OpenMP_CXX
)
# ----------------------------------------

set_target_properties(esempio PROPERTIES RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR})