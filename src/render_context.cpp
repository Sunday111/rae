#include "render_context.hpp"

#include <webgpu/webgpu_glfw.h>

#include <iostream>
#include <print>

#include "dawn/webgpu_cpp_print.h"

#if defined(__EMSCRIPTEN__)
#include <GLFW/glfw3.h>
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

static const char shaderCode[] = R"(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f {
        const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }
    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(1, 0, 0, 1);
    }
)";

void RenderContext::CreateRenderPipeline()
{
    wgpu::ShaderSourceWGSL wgsl{{.code = shaderCode}};
    wgpu::ShaderModuleDescriptor smd{.nextInChain = &wgsl};
    wgpu::ShaderModule shaderModule = device.CreateShaderModule(&smd);

    wgpu::ColorTargetState color{.format = format};
    wgpu::FragmentState frag{
        .module = shaderModule,
        .entryPoint = "fragmentMain",
        .targetCount = 1,
        .targets = &color};

    wgpu::RenderPipelineDescriptor desc{
        .vertex = {.module = shaderModule, .entryPoint = "vertexMain"},
        .fragment = &frag};
    pipeline = device.CreateRenderPipeline(&desc);
}

void RenderContext::Init()
{
    static constexpr auto kTimedWaitAny =
        wgpu::InstanceFeatureName::TimedWaitAny;
    constexpr wgpu::InstanceDescriptor idesc{
        .requiredFeatureCount = 1,
        .requiredFeatures = &kTimedWaitAny};
    instance = wgpu::CreateInstance(&idesc);

    wgpu::Future f1 = instance.RequestAdapter(
        nullptr,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestAdapterStatus status,
           wgpu::Adapter adapter,
           wgpu::StringView msg,
           RenderContext* app)
        {
            if (status != wgpu::RequestAdapterStatus::Success)
            {
                std::println("RequestAdapter: {}", msg.data);
                std::exit(1);
            }
            app->adapter = std::move(adapter);
        },
        this);
    instance.WaitAny(f1, UINT64_MAX);

    wgpu::DeviceDescriptor ddesc{};
    ddesc.SetUncapturedErrorCallback(
        [](const wgpu::Device&, wgpu::ErrorType type, wgpu::StringView msg)
        { std::cout << "Device error (" << type << "): " << msg << "\n"; });

    wgpu::Future f2 = adapter.RequestDevice(
        &ddesc,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestDeviceStatus status,
           wgpu::Device d,
           wgpu::StringView msg,
           RenderContext* app)
        {
            if (status != wgpu::RequestDeviceStatus::Success)
            {
                std::cout << "RequestDevice: " << msg << "\n";
                std::exit(1);
            }
            app->device = std::move(d);
        },
        this);
    instance.WaitAny(f2, UINT64_MAX);
}

void RenderContext::ChooseSurfaceFormatOnce()
{
    if (format != wgpu::TextureFormat::Undefined) return;

    wgpu::SurfaceCapabilities caps;
    surface.GetCapabilities(adapter, &caps);

    // Prefer BGRA8 or RGBA8; fall back to first advertised format.
    format = wgpu::TextureFormat::Undefined;
    for (uint32_t i = 0; i < caps.formatCount; ++i)
    {
        auto f = caps.formats[i];
        if (f == wgpu::TextureFormat::BGRA8Unorm ||
            f == wgpu::TextureFormat::RGBA8Unorm)
        {
            format = f;
            break;
        }
    }
    if (format == wgpu::TextureFormat::Undefined && caps.formatCount > 0)
    {
        format = caps.formats[0];
    }
}

void RenderContext::ConfigureSurfaceToSize(int pxW, int pxH)
{
    if (pxW <= 0 || pxH <= 0) return;

    ChooseSurfaceFormatOnce();

    wgpu::SurfaceConfiguration cfg{
        .device = device,
        .format = format,
        .usage = wgpu::TextureUsage::RenderAttachment,
        .width = static_cast<uint32_t>(pxW),
        .height = static_cast<uint32_t>(pxH),
        .presentMode = wgpu::PresentMode::Fifo};
    surface.Configure(&cfg);
    surface_width = pxW;
    surface_height = pxH;
}

#if defined(__EMSCRIPTEN__)
void RenderContext::UpdateCanvasAndSurfaceSize(
    GLFWwindow* window,
    const char* canvas)
{
    double cssW = 0.0, cssH = 0.0;
    emscripten_get_element_css_size(canvas, &cssW, &cssH);
    double dpr = emscripten_get_device_pixel_ratio();
    int pxW = static_cast<int>(std::round(cssW * dpr));
    int pxH = static_cast<int>(std::round(cssH * dpr));
    if (pxW <= 0 || pxH <= 0) return;
    glfwSetWindowSize(window, cssW, cssH);

    // Ensure canvas backing store matches CSS × DPR.
    emscripten_set_canvas_element_size(canvas, pxW, pxH);
    if (pxW != surface_width || pxH != surface_height)
    {
        ConfigureSurfaceToSize(pxW, pxH);
    }
}
#endif
