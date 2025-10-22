#include renderer.h

void Renderer::Render()
{
    for (uint32_t i = 0; i < width * height; ++i)
    {
        framebuffer[i] = rand() % 256; // Placeholder rendering logic
    }
}