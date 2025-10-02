#include <GLFW/glfw3.h>

#include <cmath>
#include <iostream>

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

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"

wgpu::Instance instance;
wgpu::Adapter adapter;
wgpu::Device device;
wgpu::RenderPipeline pipeline;

wgpu::Surface surface;
wgpu::TextureFormat format = wgpu::TextureFormat::Undefined;

static GLFWwindow* gWindow = nullptr;
static int gCfgWidth = 0;   // last configured backing width  (framebuffer px)
static int gCfgHeight = 0;  // last configured backing height (framebuffer px)

// ---------------- ImGui helpers ----------------
static void InitImGui(GLFWwindow* window)
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window, true);

    ImGui_ImplWGPU_InitInfo init{};
    init.Device = device.Get();
    init.NumFramesInFlight = 3;
    init.RenderTargetFormat = static_cast<WGPUTextureFormat>(format);
    init.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&init);
}

static void ShutdownImGui()
{
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// ---------------- WebGPU init ----------------
static void ChooseSurfaceFormatOnce()
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

static void ConfigureSurfaceToSize(int pxW, int pxH)
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
    gCfgWidth = pxW;
    gCfgHeight = pxH;
}

#if defined(__EMSCRIPTEN__)
static void UpdateCanvasAndSurfaceSize()
{
    double cssW = 0.0, cssH = 0.0;
    emscripten_get_element_css_size("#canvas", &cssW, &cssH);
    double dpr = emscripten_get_device_pixel_ratio();
    int pxW = static_cast<int>(std::round(cssW * dpr));
    int pxH = static_cast<int>(std::round(cssH * dpr));
    if (pxW <= 0 || pxH <= 0) return;
    glfwSetWindowSize(gWindow, cssW, cssH);

    // Ensure canvas backing store matches CSS × DPR.
    emscripten_set_canvas_element_size("#canvas", pxW, pxH);
    if (pxW != gCfgWidth || pxH != gCfgHeight)
    {
        ConfigureSurfaceToSize(pxW, pxH);
    }
}

static EM_BOOL OnResize(int, const EmscriptenUiEvent*, void*)
{
    UpdateCanvasAndSurfaceSize();
    return EM_TRUE;
}
#endif

static void Init()
{
    static const auto kTimedWaitAny = wgpu::InstanceFeatureName::TimedWaitAny;
    wgpu::InstanceDescriptor idesc{
        .requiredFeatureCount = 1,
        .requiredFeatures = &kTimedWaitAny};
    instance = wgpu::CreateInstance(&idesc);

    wgpu::Future f1 = instance.RequestAdapter(
        nullptr,
        wgpu::CallbackMode::WaitAnyOnly,
        [](wgpu::RequestAdapterStatus status,
           wgpu::Adapter a,
           wgpu::StringView msg)
        {
            if (status != wgpu::RequestAdapterStatus::Success)
            {
                std::cout << "RequestAdapter: " << msg << "\n";
                std::exit(1);
            }
            adapter = std::move(a);
        });
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
           wgpu::StringView msg)
        {
            if (status != wgpu::RequestDeviceStatus::Success)
            {
                std::cout << "RequestDevice: " << msg << "\n";
                std::exit(1);
            }
            device = std::move(d);
        });
    instance.WaitAny(f2, UINT64_MAX);
}

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

static void CreateRenderPipeline()
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

// ---------------- Render loop ----------------
static void Render()
{
    // Always pump events (web + native) so ImGui input is fresh.
    glfwPollEvents();

#if defined(__EMSCRIPTEN__)
    UpdateCanvasAndSurfaceSize();
#endif
    // Native: update ImGui IO for correct scaling and reconfigure on size
    // change.
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(gWindow, &winW, &winH);
    glfwGetFramebufferSize(gWindow, &fbW, &fbH);
    if (fbW > 0 && fbH > 0 && (fbW != gCfgWidth || fbH != gCfgHeight))
    {
        ConfigureSurfaceToSize(fbW, fbH);
    }
    if (ImGui::GetCurrentContext())
    {
        ImGuiIO& io = ImGui::GetIO();
        io.DisplaySize =
            ImVec2(static_cast<float>(winW), static_cast<float>(winH));
        io.DisplayFramebufferScale = ImVec2(
            winW ? static_cast<float>(fbW) / winW : 1.f,
            winH ? static_cast<float>(fbH) / winH : 1.f);
    }

    // ImGui frame
    ImGui_ImplWGPU_NewFrame();
    ImGui_ImplGlfw_NewFrame();

    ImGui::NewFrame();

    static bool show_demo = true;
    ImGui::Begin("WebGPU + ImGui");
    ImGui::Text("Framebuffer: %d x %d", gCfgWidth, gCfgHeight);
    ImGui::Checkbox("Show Demo", &show_demo);
    ImGui::End();
    if (show_demo) ImGui::ShowDemoWindow(&show_demo);
    ImGui::Render();

    // Acquire current texture; skip frame if unavailable (e.g., minimized)
    wgpu::SurfaceTexture st;
    surface.GetCurrentTexture(&st);
    if (!st.texture)
    {
#if !defined(__EMSCRIPTEN__)
        instance.ProcessEvents();
#endif
        return;
    }
    wgpu::TextureView view = st.texture.CreateView();

    // One pass: clear + triangle + ImGui
    wgpu::RenderPassColorAttachment ca{
        .view = view,
        .loadOp = wgpu::LoadOp::Clear,
        .storeOp = wgpu::StoreOp::Store,
        .clearValue = {0.10f, 0.14f, 0.18f, 1.0f}};
    wgpu::RenderPassDescriptor rp{
        .colorAttachmentCount = 1,
        .colorAttachments = &ca};

    wgpu::CommandEncoder enc = device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.SetPipeline(pipeline);
    pass.Draw(3);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass.Get());
    pass.End();

    wgpu::CommandBuffer cmd = enc.Finish();
    device.GetQueue().Submit(1, &cmd);

#if !defined(__EMSCRIPTEN__)
    surface.Present();
    instance.ProcessEvents();
#endif
}

// ---------------- App bootstrap ----------------
static void InitGraphics()
{
#if defined(__EMSCRIPTEN__)
    // Make the canvas focusable & focused so keyboard works; disable context
    // menu
    EM_ASM({
        if (Module['canvas'])
        {
            Module['canvas'].setAttribute('tabindex', '0');
            Module['canvas'].style.outline = 'none';
            Module['canvas'].focus();
            Module['canvas'].oncontextmenu = function(e)
            {
                e.preventDefault();
            };
        }
    });
    // Ensure surface is configured to CSS × DPR before pipeline/UI init
    UpdateCanvasAndSurfaceSize();
#else
    int fbW = 0, fbH = 0;
    glfwGetFramebufferSize(gWindow, &fbW, &fbH);
    if (fbW == 0 || fbH == 0)
    {
        fbW = 800;
        fbH = 600;
    }
    ConfigureSurfaceToSize(fbW, fbH);
#endif
    CreateRenderPipeline();
    InitImGui(gWindow);
}

static void Start()
{
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    gWindow = glfwCreateWindow(800, 600, "WebGPU window", nullptr, nullptr);
    surface = wgpu::glfw::CreateSurfaceForWindow(instance, gWindow);

    // Configure surface + pipeline + ImGui
    InitGraphics();

#if defined(__EMSCRIPTEN__)
    // Register resize callback AFTER ImGui exists, then enter main loop
    emscripten_set_resize_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW,
        nullptr,
        false,
        OnResize);
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
