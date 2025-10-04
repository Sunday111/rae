#pragma once

#include <GLFW/glfw3.h>

#include <cmath>

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

// Dawn / WebGPU C++ wrappers
#include <dawn/webgpu_cpp_print.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

// ImGui
#include <imgui.h>

class Application
{
public:
    [[nodiscard]] static Application& Instance() noexcept
    {
        static Application app;
        return app;
    }

    void Init();
    void ConfigureSurfaceToSize(int pxW, int pxH);
    void InitGraphics();
    void CreateRenderPipeline();
    void Render();
    void Start();
    void ChooseSurfaceFormatOnce();
    void InitImGui(GLFWwindow* window);
    void ShutdownImGui();

#if defined(__EMSCRIPTEN__)
    void UpdateCanvasAndSurfaceSize();
#endif

private:
    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::RenderPipeline pipeline;

    wgpu::Surface surface;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;

    GLFWwindow* gWindow = nullptr;
    int gCfgWidth = 0;   // last configured backing width  (framebuffer px)
    int gCfgHeight = 0;  // last configured backing height (framebuffer px)
};
