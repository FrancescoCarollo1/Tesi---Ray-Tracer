// File: main.cpp
// Francesco Carollo - Tesi Ray Tracer CPU/GPU Hybrid

#define GL_GLEXT_PROTOTYPES
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
#include <fstream>
#include <sstream>
#include <string>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

extern "C"
{
#include "scene.h"
#include "render.h"
#include "ppm.h"
#include "color.h"
#include "vec3.h"
}

// --- CLASSE SHADER ---
class Shader {
public:
    unsigned int ID;
    bool isValid = false;
    Shader(const char* fragmentPath) {
        std::string fragmentCode;
        std::ifstream fShaderFile;
        fShaderFile.exceptions(std::ifstream::failbit | std::ifstream::badbit);
        try {
            fShaderFile.open(fragmentPath);
            std::stringstream fShaderStream;
            fShaderStream << fShaderFile.rdbuf();
            fShaderFile.close();
            fragmentCode = fShaderStream.str();
        } catch (std::ifstream::failure& e) {
            printf("ERROR::SHADER::FILE_NOT_FOUND: %s\n", fragmentPath); return;
        }
        const char* fShaderCode = fragmentCode.c_str();
        unsigned int fragment = glCreateShader(GL_FRAGMENT_SHADER);
        glShaderSource(fragment, 1, &fShaderCode, NULL);
        glCompileShader(fragment);
        if(!checkCompileErrors(fragment, "FRAGMENT")) return;

        const char* vShaderCode = "#version 330 core\n"
            "layout (location = 0) in vec3 aPos;\n"
            "void main() { gl_Position = vec4(aPos, 1.0); }";
        unsigned int vertex = glCreateShader(GL_VERTEX_SHADER);
        glShaderSource(vertex, 1, &vShaderCode, NULL);
        glCompileShader(vertex);
        if(!checkCompileErrors(vertex, "VERTEX")) return;

        ID = glCreateProgram();
        glAttachShader(ID, vertex);
        glAttachShader(ID, fragment);
        glLinkProgram(ID);
        if(!checkCompileErrors(ID, "PROGRAM")) return;
        glDeleteShader(vertex); glDeleteShader(fragment);
        isValid = true; printf("Shader Loaded Successfully!\n");
    }
    void use() { if(isValid) glUseProgram(ID); }
    void setVec2(const std::string &name, float x, float y) const { if(isValid) glUniform2f(glGetUniformLocation(ID, name.c_str()), x, y); }
    void setVec3(const std::string &name, float x, float y, float z) const { if(isValid) glUniform3f(glGetUniformLocation(ID, name.c_str()), x, y, z); }
    void setFloat(const std::string &name, float value) const { if(isValid) glUniform1f(glGetUniformLocation(ID, name.c_str()), value); }
    void setInt(const std::string &name, int value) const { if(isValid) glUniform1i(glGetUniformLocation(ID, name.c_str()), value); }
private:
    bool checkCompileErrors(unsigned int shader, std::string type) {
        int success; char infoLog[1024];
        if (type != "PROGRAM") { glGetShaderiv(shader, GL_COMPILE_STATUS, &success); if (!success) { glGetShaderInfoLog(shader, 1024, NULL, infoLog); printf("SHADER ERROR (%s): %s\n", type.c_str(), infoLog); return false; } }
        else { glGetProgramiv(shader, GL_LINK_STATUS, &success); if (!success) { glGetProgramInfoLog(shader, 1024, NULL, infoLog); printf("LINKING ERROR: %s\n", infoLog); return false; } }
        return true;
    }
};

// --- CLASSE FRAMEBUFFER ---
class FrameBuffer {
public:
    unsigned int FBO, Texture;
    FrameBuffer(int width, int height) {
        glGenFramebuffers(1, &FBO);
        glBindFramebuffer(GL_FRAMEBUFFER, FBO);
        glGenTextures(1, &Texture);
        glBindTexture(GL_TEXTURE_2D, Texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32F, width, height, 0, GL_RGBA, GL_FLOAT, NULL);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, Texture, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void bind() { glBindFramebuffer(GL_FRAMEBUFFER, FBO); }
    void unbind() { glBindFramebuffer(GL_FRAMEBUFFER, 0); }
};

Vec3 cross_product(Vec3 u, Vec3 v) { return (Vec3){ u.y*v.z - u.z*v.y, u.z*v.x - u.x*v.z, u.x*v.y - u.y*v.x }; }
typedef struct { float r, g, b; } Vec3Color;

int main(int, char **) {
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) return 1;
    const char* glsl_version = "#version 130";
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_FLAGS, 0);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    
    SDL_WindowFlags window_flags = SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN;
    SDL_Window *window = SDL_CreateWindow("Tesi Ray Tracer - Hybrid Engine", 1280, 720, window_flags);
    SDL_GLContext gl_context = SDL_GL_CreateContext(window);
    SDL_GL_MakeCurrent(window, gl_context);
    SDL_GL_SetSwapInterval(0); 
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION(); ImGui::CreateContext(); ImGuiIO& io = ImGui::GetIO();
    ImGui::StyleColorsDark(); ImGui_ImplSDL3_InitForOpenGL(window, gl_context); ImGui_ImplOpenGL3_Init(glsl_version);

    int width = 980; int height = 480;
    Color *pixel_data = (Color *)malloc(width * height * sizeof(Color));
    Color *current_frame_data = (Color *)malloc(width * height * sizeof(Color));
    Vec3Color *accumulation_buffer = (Vec3Color *)malloc(width * height * sizeof(Vec3Color));
    for(int i=0; i<width*height; i++) pixel_data[i] = (Color){0,0,0};

    Scene *scene = create_empty_scene();
    if (read_scene("../libs/Ray-Tracing/prove_txt/prova1.txt", scene) != 0) printf("Warning: Scene not loaded.\n");

    Vec3 camera_origin = scene->camera.cam_lookfrom;
    Vec3 camera_lookat = scene->camera.cam_lookat;
    Vec3 dir_start = normalize(sub(camera_lookat, camera_origin));
    float cam_pitch = asin(dir_start.y) * (180.0 / M_PI);
    float cam_yaw = atan2(dir_start.z, dir_start.x) * (180.0 / M_PI);
    float f_fov = (float)scene->camera.cam_vfov;

    GLuint image_texture;
    glGenTextures(1, &image_texture); glBindTexture(GL_TEXTURE_2D, image_texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR); glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, pixel_data);

    // GPU Resources
    Shader rayShader("../shaders/raytracer.frag"); 
    FrameBuffer accumFBO(1280, 720);
    float quadVertices[] = { -1.0f, 1.0f, 0.0f, -1.0f, -1.0f, 0.0f, 1.0f, -1.0f, 0.0f, -1.0f, 1.0f, 0.0f, 1.0f, -1.0f, 0.0f, 1.0f, 1.0f, 0.0f };
    unsigned int VAO, VBO;
    glGenVertexArrays(1, &VAO); glGenBuffers(1, &VBO); glBindVertexArray(VAO); glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0); glEnableVertexAttribArray(0);

    bool done = false; bool use_gpu_render = false; bool first_frame = true;
    int frame_index = 1;      // CPU Counter
    int gpu_frame_index = 1;  // GPU Counter

    while (!done) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            ImGui_ImplSDL3_ProcessEvent(&event);
            if (event.type == SDL_EVENT_QUIT) done = true;
        }
        
        bool camera_changed = false;
        if (!io.WantCaptureKeyboard) {
            const bool *keys = SDL_GetKeyboardState(NULL);
            double speed = 0.05; if (keys[SDL_SCANCODE_LSHIFT]) speed *= 3.0; 
            float rot_speed = 0.05f;
            if (keys[SDL_SCANCODE_RIGHT]) { cam_yaw -= rot_speed; camera_changed = true; }
            if (keys[SDL_SCANCODE_LEFT])  { cam_yaw += rot_speed; camera_changed = true; }
            if (keys[SDL_SCANCODE_DOWN])  { cam_pitch -= rot_speed; camera_changed = true; }
            if (keys[SDL_SCANCODE_UP])    { cam_pitch += rot_speed; camera_changed = true; }
            if (cam_pitch > 89.0f) cam_pitch = 89.0f; if (cam_pitch < -89.0f) cam_pitch = -89.0f;

            Vec3 front;
            front.x = cos(cam_yaw * (M_PI / 180.0)) * cos(cam_pitch * (M_PI / 180.0));
            front.y = sin(cam_pitch * (M_PI / 180.0));
            front.z = sin(cam_yaw * (M_PI / 180.0)) * cos(cam_pitch * (M_PI / 180.0));
            front = normalize(front);
            Vec3 right = normalize(cross_product(front, (Vec3){0,1,0}));
            Vec3 movement = {0,0,0};
            if (keys[SDL_SCANCODE_W]) movement = add(movement, front);
            if (keys[SDL_SCANCODE_S]) movement = sub(movement, front);
            if (keys[SDL_SCANCODE_D]) movement = sub(movement, right);
            if (keys[SDL_SCANCODE_A]) movement = add(movement, right);
            if (keys[SDL_SCANCODE_SPACE]) movement.y += 1.0;
            if (keys[SDL_SCANCODE_LCTRL]) movement.y -= 1.0;

            if (movement.x != 0 || movement.y != 0 || movement.z != 0) {
                movement = normalize(movement); movement = mul_scalar(movement, speed);
                camera_origin = add(camera_origin, movement); camera_changed = true;
            }
            camera_lookat = add(camera_origin, front);
        }

        // Se la camera cambia, resetta gli accumulatori
        if (camera_changed) {
            frame_index = 1;      // Reset CPU
            gpu_frame_index = 1;  // Reset GPU
        }

        // --- RENDER CPU ---
        if (!use_gpu_render) {
            if (camera_changed || first_frame) {
                frame_index = 1; memset(accumulation_buffer, 0, width * height * sizeof(Vec3Color));
                setup_camera(scene, camera_origin, camera_lookat, scene->camera.cam_vup, (double)f_fov, scene->camera.cam_aspect_ratio, 0.0, 10.0);
                first_frame = false;
            }
            omp_render_scene(scene, current_frame_data, width, height, 1, frame_index);
            for (int i = 0; i < width * height; i++) {
                accumulation_buffer[i].r += current_frame_data[i].r;
                accumulation_buffer[i].g += current_frame_data[i].g;
                accumulation_buffer[i].b += current_frame_data[i].b;
                float scale = 1.0f / frame_index;
                pixel_data[i].r = (uint8_t)(accumulation_buffer[i].r * scale);
                pixel_data[i].g = (uint8_t)(accumulation_buffer[i].g * scale);
                pixel_data[i].b = (uint8_t)(accumulation_buffer[i].b * scale);
            }
            frame_index++;
            glBindTexture(GL_TEXTURE_2D, image_texture);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, pixel_data);
        }

        // --- RENDER GPU ---
        // --- RENDER GPU ---
        if (use_gpu_render) {
            // 1. Disegna su Texture
            accumFBO.bind();
            
            // Se è il primo frame, pulisci
            if (gpu_frame_index == 1) {
                glClearColor(0,0,0,1); glClear(GL_COLOR_BUFFER_BIT);
            }

            // Abilita Blending per Accumulo
            glEnable(GL_BLEND);
            glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

            rayShader.use();
            
            // Passa Dati Camera
            rayShader.setVec2("u_resolution", (float)io.DisplaySize.x, (float)io.DisplaySize.y);
            rayShader.setVec3("u_cam_origin", camera_origin.x, camera_origin.y, camera_origin.z);
            
            Vec3 front;
            front.x = cos(cam_yaw*(M_PI/180.0))*cos(cam_pitch*(M_PI/180.0));
            front.y = sin(cam_pitch*(M_PI/180.0));
            front.z = sin(cam_yaw*(M_PI/180.0))*cos(cam_pitch*(M_PI/180.0));
            front = normalize(front);
            rayShader.setVec3("u_cam_lookat", camera_origin.x + front.x, camera_origin.y + front.y, camera_origin.z + front.z);
            rayShader.setFloat("u_fov", f_fov);
            
            // Passa Dati Accumulo
            rayShader.setFloat("u_time", SDL_GetTicks() / 1000.0f);
            rayShader.setInt("u_frame_index", gpu_frame_index);

            // --- INVIO SFERE ---
            int count = scene->num_spheres; 
            if (count > 64) count = 64;
            rayShader.setInt("u_num_spheres", count);
            
            for (int i = 0; i < count; i++) {
                std::string base = "u_spheres[" + std::to_string(i) + "]";
                rayShader.setVec3(base + ".center", scene->spheres[i].center.x, scene->spheres[i].center.y, scene->spheres[i].center.z);
                rayShader.setFloat(base + ".radius", scene->spheres[i].radius);
                rayShader.setVec3(base + ".color", (float)scene->spheres[i].color.r, (float)scene->spheres[i].color.g, (float)scene->spheres[i].color.b);
                rayShader.setInt(base + ".material", (int)scene->spheres[i].material);
                rayShader.setFloat(base + ".param", (float)scene->spheres[i].mat_param);
            }

            // --- INVIO TRIANGOLI---
            int tri_count = scene->num_triangles;
            if (tri_count > 64) tri_count = 64; // Max definito nello shader
            rayShader.setInt("u_num_triangles", tri_count);

            for (int i = 0; i < tri_count; i++) {
                std::string base = "u_triangles[" + std::to_string(i) + "]";
                
                // Vertici
                rayShader.setVec3(base + ".v0", scene->triangles[i].v0.x, scene->triangles[i].v0.y, scene->triangles[i].v0.z);
                rayShader.setVec3(base + ".v1", scene->triangles[i].v1.x, scene->triangles[i].v1.y, scene->triangles[i].v1.z);
                rayShader.setVec3(base + ".v2", scene->triangles[i].v2.x, scene->triangles[i].v2.y, scene->triangles[i].v2.z);
                
                // Materiale
                rayShader.setVec3(base + ".color", (float)scene->triangles[i].color.r, (float)scene->triangles[i].color.g, (float)scene->triangles[i].color.b);
                rayShader.setInt(base + ".material", (int)scene->triangles[i].material);
                rayShader.setFloat(base + ".param", (float)scene->triangles[i].mat_param);
            }
            // -------------------------------------

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 6);

            glDisable(GL_BLEND);
            accumFBO.unbind();
            
            // 2. COPIA FRAMEBUFFER A SCHERMO
            glBindFramebuffer(GL_READ_FRAMEBUFFER, accumFBO.FBO);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0); 
            glBlitFramebuffer(0, 0, 1280, 720, 0, 0, io.DisplaySize.x, io.DisplaySize.y, GL_COLOR_BUFFER_BIT, GL_NEAREST);
            glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);

            gpu_frame_index++;
        
        } else {
            // Se non usiamo la GPU, puliamo lo schermo normale per disegnare l'imgui
            glViewport(0, 0, (int)io.DisplaySize.x, (int)io.DisplaySize.y);
            glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);
        }

        // GUI
        ImGui_ImplOpenGL3_NewFrame(); ImGui_ImplSDL3_NewFrame(); ImGui::NewFrame();
        ImGui::Begin("Control Panel");
        ImGui::Text("FPS: %.1f", io.Framerate);
        if (use_gpu_render) ImGui::Text("GPU Samples: %d", gpu_frame_index);
        else ImGui::Text("CPU Samples: %d", frame_index);
        
        if (ImGui::Checkbox("USE GPU RENDER", &use_gpu_render)) {
            camera_changed = true; // Reset quando si cambia modalità
        }
        
        if (!use_gpu_render) {
            ImVec2 avail = ImGui::GetContentRegionAvail();
            float aspect = (float)width / height;
            ImGui::Image((ImTextureID)(intptr_t)image_texture, ImVec2(avail.x, avail.x / aspect));
        }
        ImGui::End();

        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        SDL_GL_SwapWindow(window);
    }

    free(pixel_data); free(current_frame_data); free(accumulation_buffer); delete_scene(scene);
    ImGui_ImplOpenGL3_Shutdown(); ImGui_ImplSDL3_Shutdown(); ImGui::DestroyContext();
    SDL_GL_DestroyContext(gl_context); SDL_DestroyWindow(window); SDL_Quit();
    return 0;
}