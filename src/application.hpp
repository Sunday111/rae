#pragma once

#include <memory>

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
    void InitImGui();
    void Run();
    void Tick();
    void ShutdownImGui();

#if defined(__EMSCRIPTEN__)
    void UpdateCanvasAndSurfaceSize();
#endif

private:
    GLFWwindow* window = nullptr;
    std::unique_ptr<RenderContext> render_context;
};
