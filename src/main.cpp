#include <GLFW/glfw3.h>

#include <iostream>
#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#endif

// Dawn / WebGPU C++ wrappers
#include <dawn/webgpu_cpp_print.h>
#include <webgpu/webgpu_cpp.h>
#include <webgpu/webgpu_glfw.h>

// ImGui
#include <imgui.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"

wgpu::Instance instance;
wgpu::Adapter adapter;
wgpu::Device device;
wgpu::RenderPipeline pipeline;

wgpu::Surface surface;
wgpu::TextureFormat format;
const uint32_t kWidth = 512;
const uint32_t kHeight = 512;

static GLFWwindow* gWindow = nullptr;

// ---------------- ImGui helpers ----------------
void InitImGui(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    // No GL/Vulkan context; "Other" works with GLFW input only.
    ImGui_ImplGlfw_InitForOther(window, true);

    ImGui_ImplWGPU_InitInfo init{};
    init.Device = device.Get();  // WGPUDevice
    init.NumFramesInFlight = 3;
    init.RenderTargetFormat = static_cast<WGPUTextureFormat>(format);
    init.DepthStencilFormat = WGPUTextureFormat_Undefined;

    ImGui_ImplWGPU_Init(&init);
}

void ShutdownImGui()
{
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// ---------------- Your existing code (light edits) ----------------
void ConfigureSurface()
{
    wgpu::SurfaceCapabilities capabilities;
    surface.GetCapabilities(adapter, &capabilities);
    format = capabilities.formats[0];

    wgpu::SurfaceConfiguration config{
        .device = device,
        .format = format,
        .width = kWidth,
        .height = kHeight,
        .presentMode = wgpu::PresentMode::Fifo};
    surface.Configure(&config);
}

void Init()
{
    static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    wgpu::InstanceDescriptor instanceDesc{
        .requiredFeatureCount = 1,
        .requiredFeatures = &kTimedWaitAny};
    instance = wgpu::CreateInstance(&instanceDesc);

    wgpu::Future f1 = instance.RequestAdapter(
        nullptr,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestAdapterStatus status,
           wgpu::Adapter a,
           wgpu::StringView message)
        {
            if (status != wgpu::RequestAdapterStatus::Success)
            {
                std::cout << "RequestAdapter: " << message << "\n";
                std::exit(0);
            }
            adapter = std::move(a);
        });
    instance.WaitAny(f1, UINT64_MAX);

    wgpu::DeviceDescriptor desc{};
    desc.SetUncapturedErrorCallback(
        [](const wgpu::Device&,
           wgpu::ErrorType errorType,
           wgpu::StringView message)
        {
            std::cout << "Error: " << errorType << " - message: " << message
                      << "\n";
        });

    wgpu::Future f2 = adapter.RequestDevice(
        &desc,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestDeviceStatus status,
           wgpu::Device d,
           wgpu::StringView message)
        {
            if (status != wgpu::RequestDeviceStatus::Success)
            {
                std::cout << "RequestDevice: " << message << "\n";
                std::exit(0);
            }
            device = std::move(d);
        });
    instance.WaitAny(f2, UINT64_MAX);
}

const char shaderCode[] = R"(
    @vertex fn vertexMain(@builtin(vertex_index) i : u32) ->
      @builtin(position) vec4f {
        const pos = array(vec2f(0, 1), vec2f(-1, -1), vec2f(1, -1));
        return vec4f(pos[i], 0, 1);
    }
    @fragment fn fragmentMain() -> @location(0) vec4f {
        return vec4f(1, 0, 0, 1);
    }
)";

void CreateRenderPipeline()
{
    wgpu::ShaderSourceWGSL wgsl{{.code = shaderCode}};
    wgpu::ShaderModuleDescriptor shaderModuleDescriptor{.nextInChain = &wgsl};
    wgpu::ShaderModule shaderModule =
        device.CreateShaderModule(&shaderModuleDescriptor);

    wgpu::ColorTargetState colorTargetState{.format = format};
    wgpu::FragmentState fragmentState{
        .module = shaderModule,
        .entryPoint = "fragmentMain",
        .targetCount = 1,
        .targets = &colorTargetState,
    };

    wgpu::RenderPipelineDescriptor descriptor{
        .vertex = {.module = shaderModule, .entryPoint = "vertexMain"},
        .fragment = &fragmentState};
    pipeline = device.CreateRenderPipeline(&descriptor);
}

void Render()
{
    // (Optional) event pump – makes ImGui input responsive on native and wasm
    glfwPollEvents();

    // Begin ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();

    // Demo UI
    static bool show_demo = true;
    ImGui::Begin("WebGPU + ImGui");
    ImGui::Text("Hello from ImGui!");
    ImGui::Checkbox("Show Demo", &show_demo);
    ImGui::End();
    if (show_demo) ImGui::ShowDemoWindow(&show_demo);

    ImGui::Render();

    // Acquire the drawable
    wgpu::SurfaceTexture surfaceTexture;
    surface.GetCurrentTexture(&surfaceTexture);
    wgpu::TextureView view = surfaceTexture.texture.CreateView();

    // One pass: clear + triangle + ImGui
    wgpu::RenderPassColorAttachment attachment{
        .view = view,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = {0.10f, 0.14f, 0.18f, 1.0f}};

    wgpu::RenderPassDescriptor renderpass{
        .colorAttachmentCount = 1,
        .colorAttachments = &attachment};

    wgpu::CommandEncoder encoder = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = encoder.BeginRenderPass(&renderpass);

    // Render your triangle first
    pass.SetPipeline(pipeline);
    pass.Draw(3);

    // Render ImGui draw data into the same pass
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass.Get());

    pass.End();
    wgpu::CommandBuffer commands = encoder.Finish();
    device.GetQueue().Submit(1, &commands);

#if !defined(__EMSCRIPTEN__)
    surface.Present();
    instance.ProcessEvents();
#endif
}

void InitGraphics()
{
    ConfigureSurface();
    CreateRenderPipeline();
    InitImGui(gWindow);  // <-- ImGui needs device + surface format
}

void Start()
{
    if (!glfwInit()) return;

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    gWindow =
        glfwCreateWindow(kWidth, kHeight, "WebGPU window", nullptr, nullptr);
    surface = wgpu::glfw::CreateSurfaceForWindow(instance, gWindow);

    InitGraphics();

#if defined(__EMSCRIPTEN__)
    emscripten_set_main_loop(Render, 0, false);
#else
    while (!glfwWindowShouldClose(gWindow))
    {
        Render();
    }
    ShutdownImGui();
    glfwDestroyWindow(gWindow);
    glfwTerminate();
#endif
}

int main()
{
    Init();
    Start();
}
