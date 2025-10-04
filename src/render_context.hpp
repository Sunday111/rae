#pragma once

#include <webgpu/webgpu_cpp.h>

struct GLFWwindow;

class RenderContext
{
public:
    void Init();
    void CreateRenderPipeline();
    void ChooseSurfaceFormatOnce();
    void ConfigureSurfaceToSize(int pxW, int pxH);

#if defined(__EMSCRIPTEN__)
    void UpdateCanvasAndSurfaceSize(GLFWwindow* window, const char* canvas);
#endif

    wgpu::Instance instance;
    wgpu::Adapter adapter;
    wgpu::Device device;
    wgpu::RenderPipeline pipeline;
    wgpu::Surface surface;
    wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;
    // last configured backing size  (framebuffer px)
    int surface_width = 0;
    int surface_height = 0;
};
