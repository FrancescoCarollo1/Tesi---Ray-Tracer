// File: main.cpp
// Francesco Carollo - Tesi Ray Tracer CPU/GPU

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
    
#include <cmath> 
#include <cstdlib>
#include <chrono>
#include <vector>
#include <cstring> 

// Definizione di sicurezza per M_PI
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

// Inclusione headers C
extern "C"
{
#include "scene.h"
#include "render.h"
#include "ppm.h"
#include "color.h"
#include "vec3.h"
}


Vec3 cross_product(Vec3 u, Vec3 v) {
    return (Vec3){
        u.y * v.z - u.z * v.y,
        u.z * v.x - u.x * v.z,
        u.x * v.y - u.y * v.x
    };
}

typedef struct {
    float r, g, b;
} Vec3Color;

// Main code
int main(int, char **)
{
    // --- 1. SETUP SDL3 & OPENGL ---
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        printf("Error: SDL_Init(): %s\n", SDL_GetError()); return 1;
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
    SDL_Window *window = SDL_CreateWindow("Tesi Ray Tracer - Progressive Rendering", (int)(1280 * main_scale), (int)(800 * main_scale), window_flags);
    
    if (!window) { printf("Error: SDL_CreateWindow(): %s\n", SDL_GetError()); return 1; }
    
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    if (!gl_context) { printf("Error: SDL_GL_CreateContext(): %s\n", SDL_GetError()); return 1; }

    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(1); 
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    // --- 2. SETUP IMGUI ---
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    ImGui::StyleColorsDark();
    ImGuiStyle &style = ImGui::GetStyle();
    style.ScaleAllSizes(main_scale);
    ImGui_ImplSDL3_InitForOpenGL(window, gl_context);
    ImGui_ImplOpenGL3_Init(glsl_version);

    // --- 3. INIZIALIZZAZIONE ---
    double last_render_time_ms = 0.0;
    int width = 980;
    int height = 480;

    // Allocazione Buffer
    Color *pixel_data = (Color *)malloc(width * height * sizeof(Color));
    Color *current_frame_data = (Color *)malloc(width * height * sizeof(Color));
    Vec3Color *accumulation_buffer = (Vec3Color *)malloc(width * height * sizeof(Vec3Color));

    if (!pixel_data || !current_frame_data || !accumulation_buffer) exit(EXIT_FAILURE);
    
    memset(pixel_data, 0, width * height * sizeof(Color));
    memset(current_frame_data, 0, width * height * sizeof(Color));
    memset(accumulation_buffer, 0, width * height * sizeof(Vec3Color));

    // Caricamento Scena
    Scene *scene = create_empty_scene();
    if (read_scene("../libs/Ray-Tracing/prove_txt/prova1.txt", scene) != 0) { 
        printf("Errore caricamento scena! Controlla il percorso.\n");
        exit(EXIT_FAILURE); 
    }

    // --- Inizializzazione Variabili Camera e Stato ---
    Vec3 camera_origin = scene->camera.cam_lookfrom;
    Vec3 camera_lookat = scene->camera.cam_lookat;
    
    double camera_aspect_ratio = scene->camera.cam_aspect_ratio;

    const double base_speed = 0.2;

    // Calcolo rotazione iniziale basata sul file
    Vec3 dir_start = normalize(sub(camera_lookat, camera_origin));
    float cam_pitch = asin(dir_start.y) * (180.0 / M_PI);
    float cam_yaw = atan2(dir_start.z, dir_start.x) * (180.0 / M_PI);

    // Parametri Lente
    float f_fov = (float)scene->camera.cam_vfov;
    float f_aperture = 0.0f; // Default: tutto a fuoco
    
    // Distanza focale automatica iniziale
    Vec3 dist_vec = sub(camera_lookat, camera_origin);
    float f_focus_dist = (float)sqrt(dist_vec.x*dist_vec.x + dist_vec.y*dist_vec.y + dist_vec.z*dist_vec.z);

    // Texture GPU
    GLuint image_texture;
    glGenTextures(1, &image_texture);
    glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, NULL); 

    bool show_demo_window = false;
    bool done = false;
    bool first_frame = true; 
    int frame_index = 1; 

    printf("Starting Main Loop.\n");

    // --- 4. MAIN LOOP ---
    while (!done)
    {
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
            if (event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window)) done = true;
        }
        if (SDL_GetWindowFlags(window) & SDL_WINDOW_MINIMIZED) { SDL_Delay(10); continue; }

        // Iniziamo il frame ImGui
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplSDL3_NewFrame();
        ImGui::NewFrame();

        bool camera_changed = false;

        // 
        // GESTIONE INPUT (Movimento + Rotazione)
        
        // Blocchiamo il movimento se stiamo usando il mouse sulla GUI
        if (!io.WantCaptureKeyboard) 
        {
            const bool *keys = SDL_GetKeyboardState(NULL);
            double speed = base_speed;
            if (keys[SDL_SCANCODE_LSHIFT]) speed *= 3.0; 

            // --- Rotazione (Frecce) ---
            float rot_speed = 2.0f;
            bool rotated = false;
            if (keys[SDL_SCANCODE_RIGHT]) { cam_yaw += rot_speed; rotated = true; }
            if (keys[SDL_SCANCODE_LEFT])  { cam_yaw -= rot_speed; rotated = true; }
            if (keys[SDL_SCANCODE_DOWN])  { cam_pitch -= rot_speed; rotated = true; }
            if (keys[SDL_SCANCODE_UP])    { cam_pitch += rot_speed; rotated = true; }

            if (cam_pitch > 89.0f) cam_pitch = 89.0f;
            if (cam_pitch < -89.0f) cam_pitch = -89.0f;

            // Calcolo vettori base
            Vec3 front;
            double rad_yaw = cam_yaw * (M_PI / 180.0);
            double rad_pitch = cam_pitch * (M_PI / 180.0);
            front.x = cos(rad_yaw) * cos(rad_pitch);
            front.y = sin(rad_pitch);
            front.z = sin(rad_yaw) * cos(rad_pitch);
            front = normalize(front);

            Vec3 world_up = {0, 1, 0};
            Vec3 right = normalize(cross_product(front, world_up)); 
            
            // --- Movimento (WASD) ---
            Vec3 movement = {0, 0, 0};
            if (keys[SDL_SCANCODE_W]) movement = add(movement, front);
            if (keys[SDL_SCANCODE_S]) movement = sub(movement, front);
            if (keys[SDL_SCANCODE_D]) movement = add(movement, right);
            if (keys[SDL_SCANCODE_A]) movement = sub(movement, right);
            if (keys[SDL_SCANCODE_SPACE]) movement.y += 1.0;
            if (keys[SDL_SCANCODE_LCTRL]) movement.y -= 1.0;

            if (rotated || movement.x != 0 || movement.y != 0 || movement.z != 0) 
            {
                if (movement.x != 0 || movement.y != 0 || movement.z != 0) {
                    movement = normalize(movement);
                    movement = mul_scalar(movement, speed);
                    camera_origin = add(camera_origin, movement);
                }
                camera_lookat = add(camera_origin, front);
                camera_changed = true;
            }
        }

        
        // GUI CONTROLS (ImGui)
        ImGui::Begin("Ray Tracer Control");
        
        ImGui::Text("Performance: %.1f ms/frame (%.1f FPS)", last_render_time_ms, 1000.0f/last_render_time_ms);
        ImGui::Text("Accumulated Samples: %d", frame_index);
        
        ImGui::Separator();
        ImGui::Text("Camera Lens");
        
        // Slider per FOV
        if (ImGui::SliderFloat("FOV (Zoom)", &f_fov, 10.0f, 120.0f)) camera_changed = true;
        
        // Slider per Aperture (DoF)
        if (ImGui::SliderFloat("Aperture (Blur)", &f_aperture, 0.0f, 2.0f)) camera_changed = true;
        
        // Slider per Focus Distance
        if (ImGui::SliderFloat("Focus Dist", &f_focus_dist, 0.1f, 20.0f)) camera_changed = true;

        // Bottone Auto-Focus
        if (ImGui::Button("Auto-Focus Center")) {
            Vec3 v = sub(camera_lookat, camera_origin);
            f_focus_dist = (float)sqrt(v.x*v.x + v.y*v.y + v.z*v.z);
            camera_changed = true;
        }

        ImGui::Separator();
        ImGui::Text("Position: %.2f %.2f %.2f", camera_origin.x, camera_origin.y, camera_origin.z);
        
        // Mostriamo l'immagine dentro la finestra ImGui
        ImVec2 avail_size = ImGui::GetContentRegionAvail();
        float aspect = (float)width / (float)height;
        float draw_w = avail_size.x;
        float draw_h = draw_w / aspect;
        ImGui::Image((ImTextureID)(intptr_t)image_texture, ImVec2(draw_w, draw_h));
        
        ImGui::End(); 

        //  PROGRESSIVE RENDERING

        if (camera_changed || first_frame)
        {
            frame_index = 1;
            memset(accumulation_buffer, 0, width * height * sizeof(Vec3Color));
            
            setup_camera(scene, 
                         camera_origin, 
                         camera_lookat, 
                         scene->camera.cam_vup, 
                         (double)f_fov, 
                         camera_aspect_ratio, 
                         (double)f_aperture, 
                         (double)f_focus_dist);
            
            first_frame = false;
        }

        // CALCOLO RAY TRACING 
        auto start = std::chrono::high_resolution_clock::now();
        
        omp_render_scene(scene, current_frame_data, width, height, 1, frame_index);

        auto end = std::chrono::high_resolution_clock::now();
        last_render_time_ms = (double)std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

        // ACCUMULO
        for (int i = 0; i < width * height; i++)
        {
            accumulation_buffer[i].r += (float)current_frame_data[i].r;
            accumulation_buffer[i].g += (float)current_frame_data[i].g;
            accumulation_buffer[i].b += (float)current_frame_data[i].b;

            float r = accumulation_buffer[i].r / (float)frame_index;
            float g = accumulation_buffer[i].g / (float)frame_index;
            float b = accumulation_buffer[i].b / (float)frame_index;

            pixel_data[i].r = (uint8_t)(r > 255.0f ? 255.0f : r);
            pixel_data[i].g = (uint8_t)(g > 255.0f ? 255.0f : g);
            pixel_data[i].b = (uint8_t)(b > 255.0f ? 255.0f : b);
        }
        frame_index++;

        // UPLOAD TEXTURE
        glBindTexture(GL_TEXTURE_2D, image_texture);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixel_data);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);

    
        if (show_demo_window) ImGui::ShowDemoWindow(&show_demo_window);

        ImGui::Render();
        glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    // --- CLEANUP ---
    free(pixel_data);
    free(current_frame_data);
    free(accumulation_buffer);
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