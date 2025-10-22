#pragma once

class Renderer 
{
public:
    Renderer() = default;
    void Render();

private:

    uint32_t* framebuffer = nullptr;
}