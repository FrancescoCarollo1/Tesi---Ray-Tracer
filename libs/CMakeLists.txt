add_subdirectory(imgui)


add_library(stb_image STATIC )
set(STB_IMAGE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/stb)
target_include_directories(stb_image PUBLIC ${STB_IMAGE_DIR})
target_sources(stb_image PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/stb_image.cpp) 
