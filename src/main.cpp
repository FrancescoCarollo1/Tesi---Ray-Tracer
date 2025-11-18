// Dear ImGui: standalone example application for SDL3 + OpenGL

#include "imgui.h"
#include "imgui_impl_sdl3.h"
#include "imgui_impl_opengl3.h"
#include <stdio.h>
#include <SDL3/SDL.h>
#if defined(IMGUI_IMPL_OPENGL_ES2)
#include <SDL3/SDL_opengles2.h>
#else
#include <SDL3/SDL_opengl.h>
#endif

#include <cstdlib>
#include <chrono>
#include <vector>

extern "C"
{
#include "scene.h"
#include "render.h"
#include "ppm.h"
#include "color.h"
}

// Main code
int main(int, char **)
{
    // --- 1. SETUP INIZIALE ---
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

#if defined(IMGUI_IMPL_OPENGL_ES2)
    const char *glsl_version = "#version 100";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_ES);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 2);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#else
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window *window = SDL_CreateWindow("Tesi Ray Tracer - Camera Mobile", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    
    if (window == nullptr) { printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError()); return 1; }
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr) { printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError()); return 1; }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); 
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // --- 2. SETUP IMGUI ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- 3. STATO DELL'APPLICAZIONE ---
    double last_render_time_ms = 0.0;
    int width = 980;
    int height = 480;

    // Camera
    // (Posizione Z negativa per guardare verso l'origine e le sfere positive)
    Vec3 camera_origin = {0.0f, 1.0f, -3.0f}; 
    Vec3 camera_lookat = {0.0f, 0.0f, 0.0f};
    Vec3 camera_vup = {0.0f, 1.0f, 0.0f};
    double camera_vfov = 90.0;
    float camera_speed = 0.5f;

    // Buffer Pixel
    Color *pixel_data = (Color *)malloc(width * height * sizeof(Color));
    if (pixel_data == NULL) { exit(EXIT_FAILURE); }
    memset(pixel_data, 0, width * height * sizeof(Color));

    // Scena
    Scene *scene = create_empty_scene();
    if (read_scene("../libs/Ray-Tracing/prove_txt/prova1.txt", scene) != 0) { exit(EXIT_FAILURE); }

    // Texture GPU 
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL); // Vuota!
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); 

    bool show_demo_window = true;
    bool done = false;
    bool first_frame = true; 

    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) done = true;
        }

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        // --- GESTIONE INPUT CAMERA ---
        bool camera_moved = false;
        const bool *state = SDL_GetKeyboardState(NULL);
        
        // Se un tasto è premuto, muovi la camera E imposta il flag
        if (state[SDL_SCANCODE_W]) { camera_origin.z += camera_speed; camera_moved = true; }
        if (state[SDL_SCANCODE_S]) { camera_origin.z -= camera_speed; camera_moved = true; }
        if (state[SDL_SCANCODE_A]) { camera_origin.x -= camera_speed; camera_moved = true; }
        if (state[SDL_SCANCODE_D]) { camera_origin.x += camera_speed; camera_moved = true; }
        if (state[SDL_SCANCODE_SPACE]) { camera_origin.y += camera_speed; camera_moved = true; }
        if (state[SDL_SCANCODE_LCTRL]) { camera_origin.y -= camera_speed; camera_moved = true; }



        if (camera_moved || first_frame)
        {

            setup_camera(scene, camera_origin, camera_lookat, camera_vfov, (double)width / (double)height);
            
    
            auto start = std::chrono::high_resolution_clock::now();
            omp_render_scene(scene, pixel_data, width, height);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            last_render_time_ms = (double)duration.count();

            // 3. Carica sulla GPU
            glBindTexture(GL_TEXTURE_2D, image_texture);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixel_data);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

            first_frame = false; // Fatto il primo frame
        }
        // -----------------------------------------


        // ImGui Frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // Finestra di Render
        ImGui::Begin("Viewport");
        ImGui::Text("Render Time: %.0f ms", last_render_time_ms);
        ImGui::Text("Posizione: %.1f, %.1f, %.1f", camera_origin.x, camera_origin.y, camera_origin.z);
        
        // Mostra la texture (contiene sempre l'ultimo render valido)
        ImGui::Image((ImTextureID)(intptr_t)image_texture, ImVec2(width, height));
        ImGui::End();

        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        // Render Finale
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    free(pixel_data);
    delete_scene(scene);
    glDeleteTextures(1, &image_texture);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}