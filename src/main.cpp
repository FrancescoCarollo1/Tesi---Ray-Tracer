// Dear ImGui: standalone example application for SDL3 + OpenGL
// (SDL is a cross-platform general purpose library for handling windows, inputs, OpenGL/Vulkan/Metal graphics context creation, etc.)

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
    // Setup SDL
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD))
    {
        printf("Error: SDL_Init(): %s\n", SDL_GetError());
        return 1;
    }

    // Decide GL+GLSL versions
#if defined(IMGUI_IMPL_OPENGL_ES2)
    // ... (codice GL ES) ...
#else
    // GL 3.0 + GLSL 130
    const char *glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 0);
#endif

    // Create window with graphics context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);
    float main_scale = SDL_GetDisplayContentScale(SDL_GetPrimaryDisplay());
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN | SDL_WINDOW_HIGH_PIXEL_DENSITY;
    SDL_Window *window = SDL_CreateWindow("Tesi Ray Tracer - Francis", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    if (window == nullptr)
    {
        printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError());
        return 1;
    }
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (gl_context == nullptr)
    {
        printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError());
        return 1;
    }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); // Enable vsync
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);


    IMGUI_CHECKVERSION();
    ImGui::CreateContext(); 
    
    ImGuiIO& io = ImGui::GetIO(); (void)io; 
    
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // Enable Keyboard Controls
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;  // Enable Gamepad Controls

    // Setup Dear ImGui style
    ImGui::StyleColorsDark();

    // Setup scaling
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    style.FontScaleDpi = main_scale;

    // Setup Platform/Renderer backends
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context); 
    ImGui_ImplOpenGL3_Init(glsl_version);
  
    double last_render_time_ms = 0.0;
    int width = 980;
    int height = 480;

    int current_scene_index = 1;
    const int max_scenes = 11;
    char random_image_data[256 * 256 * 3]; 
    char scene_file_buffer[256];
    snprintf(scene_file_buffer, 256, "../libs/Ray-Tracing/prove_txt/prova%d.txt", current_scene_index);


    Vec3 camera_origin = {0.0f, 0.0f, 10.0f}; // Posizione iniziale
    Vec3 camera_lookat = {0.0f, 0.0f, 0.0f};  // Punto che guarda
    Vec3 camera_vup = {0.0f, 1.0f, 0.0f};     // Vettore "Su"
    double camera_vfov = 90.0;                // Campo visivo
    float camera_speed = 0.5f;                // Velocità di movimento

    // Buffer per i pixel 
    Color *pixel_data = (Color *)malloc(width * height * sizeof(Color));
    if (pixel_data == NULL) { printf("Errore malloc\n"); exit(EXIT_FAILURE); }
    memset(pixel_data, 0, width * height * sizeof(Color));

    // Carica la Scena 
    Scene *scene = create_empty_scene();
    if (scene == NULL) { printf("Errore create_empty_scene\n"); exit(EXIT_FAILURE); }
    if (read_scene(scene_file_buffer, scene) != 0) { printf("Errore read_scene\n"); exit(EXIT_FAILURE); }

   

    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); 

    // (Texture2 per l'immagine random)
    GLuint image_texture2;
    glGenTextures(1, &image_texture2);
    glBindTexture(GL_TEXTURE_2D, image_texture2);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4); 

    // Our state
    bool show_demo_window = true;
    bool show_another_window = false;
    bool show_render_window = true; 
    bool show_random_image_window = false;
    ImVec4 clear_color = ImVec4(0.45f, 0.55f, 0.60f, 1.00f);

    // Main loop
    bool done = false;
    while (!done)
    {
        // Poll and handle events
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT)
                done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window))
                done = true;
        }

        // --- CONTROLLO TASTIERA  ---
        const bool *state = SDL_GetKeyboardState(NULL);
        if (state[SDL_SCANCODE_W]) camera_origin.z -= camera_speed;
        if (state[SDL_SCANCODE_S]) camera_origin.z += camera_speed;
        if (state[SDL_SCANCODE_A]) camera_origin.x -= camera_speed;
        if (state[SDL_SCANCODE_D]) camera_origin.x += camera_speed;
        if (state[SDL_SCANCODE_SPACE]) camera_origin.y += camera_speed;
        if (state[SDL_SCANCODE_LCTRL]) camera_origin.y -= camera_speed;
        // -------------------------

        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED)
        {
            SDL_Delay(10);
            continue;
        }

        setup_camera(scene, camera_origin, camera_lookat, camera_vfov, (double)width / (double)height);

        auto start = std::chrono::high_resolution_clock::now();
        omp_render_scene(scene, pixel_data, width, height);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        last_render_time_ms = (double)duration.count();

        glBindTexture(GL_TEXTURE_2D, image_texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixel_data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
 


        // Start the Dear ImGui frame
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        // 1. Show the big demo window
        if (show_demo_window)
            ImGui::ShowDemoWindow(&show_demo_window);

        // 2. Show a simple window that we create ourselves.
        {
            static float f = 0.0f;
            static int counter = 0;

            ImGui::Begin("UEEE");
            ImGui::Text("CIAO FRANCIS ");
            ImGui::Checkbox("Demo Window", &show_demo_window);
            ImGui::Checkbox("Another Window", &show_another_window);
            ImGui::Checkbox("Random Image", &show_random_image_window);
            ImGui::SliderFloat("float", &f, 0.0f, 1.0f);
            ImGui::ColorEdit3("clear color", (float *)&clear_color);
            if (ImGui::Button("Button"))
                counter++;
            ImGui::SameLine();
            ImGui::Text("counter = %d", counter);
            ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
            ImGui::End();
        }

        // 3. Show another simple window.
        if (show_another_window)
        {
            ImGui::Begin("Another Window", &show_another_window);
            ImGui::Text("FRANCIS");
            if (ImGui::Button("Close Me"))
                show_another_window = false;
            ImGui::End();
        }

        // 4. Show random image window
        if (show_random_image_window)
        {
            ImGui::Begin("Random Image", &show_random_image_window);
            ImGui::Text("This is a random image.");
            for (int i = 0; i < 256 * 256 * 3; i++)
                random_image_data[i] = rand() % 256;
            
            //texture 2 per l'immagine random
            glBindTexture(GL_TEXTURE_2D, image_texture2);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, 256, 256, 0, GL_RGB, GL_UNSIGNED_BYTE, random_image_data);
            ImGui::Image((ImTextureID)(intptr_t)image_texture2, ImVec2(256, 256));
            ImGui::End();
        }


        ImGui::Checkbox("Render Scene", &show_render_window);
        if (show_render_window)
        {
            ImGui::Begin("OpenGL Texture Window", &show_render_window);
            ImGui::Text("Mostro la Scena %d", current_scene_index);
            ImGui::Text("Muoviti con W, A, S, D, Spazio, Ctrl");
            ImGui::Image((ImTextureID)(intptr_t)image_texture, ImVec2(width, height));
            ImGui::Text("Tempo per il render: %.0f ms", last_render_time_ms);
            ImGui::End();
        }

        // Rendering
        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(clear_color.x * clear_color.w, clear_color.y * clear_color.w, clear_color.z * clear_color.w, clear_color.w);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // Cleanup
    free(pixel_data); 
    delete_scene(scene);
    glDeleteTextures(1, &image_texture);
    glDeleteTextures(1, &image_texture2);
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();

    SDL_GL_DestroyContext(gl_context);
    SDL_DestroyWindow(window);
    SDL_Quit();

    return 0;
}