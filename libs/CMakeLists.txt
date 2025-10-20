# --- IMGUI ---

add_subdirectory(imgui)


# --- STB_IMAGE ---

add_library(stb_image STATIC )

set(STB_IMAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_include_directories(stb_image PUBLIC ${STB_IMAGE_DIR})
target_sources(stb_image PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stb_image.cpp) 


# --- RAY_TRACER  ---


set(RAY_TRACER_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/Ray-Tracing/src/raytracer_api.c 
    ${CMAKE_CURRENT_SOURCE_DIR}/Ray-Tracing/src/ppm.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Ray-Tracing/src/scene.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Ray-Tracing/src/render.c
    ${CMAKE_CURRENT_SOURCE_DIR}/Ray-Tracing/src/vec3.c

)

add_library(ray_tracer STATIC ${RAY_TRACER_SOURCES})

target_include_directories(ray_tracer PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/Ray-Tracing/src
)