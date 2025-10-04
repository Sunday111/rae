#pragma once

#include <memory>

// Dawn / WebGPU C++ wrappers
#include <webgpu/webgpu_glfw.h>

class RenderContext;
struct GLFWwindow;

class Application
{
public:
    [[nodiscard]] static Application& Instance() noexcept
    {
        static Application app;
        return app;
    }

    Application();
    ~Application();

    void Init();
    void ConfigureSurfaceToSize(int pxW, int pxH);
    void InitGraphics();
    void Render();
    void Start();
    void ChooseSurfaceFormatOnce();
    void InitImGui();
    void ShutdownImGui();

#if defined(__EMSCRIPTEN__)
    void UpdateCanvasAndSurfaceSize();
#endif

private:
    GLFWwindow* window = nullptr;
    std::unique_ptr<RenderContext> render_context;
};
