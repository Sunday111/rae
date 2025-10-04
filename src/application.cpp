#include "application.hpp"

#include <GLFW/glfw3.h>
#include <dawn/webgpu_cpp_print.h>
#include <imgui.h>

#include "backends/imgui_impl_glfw.h"
#include "backends/imgui_impl_wgpu.h"
#include "render_context.hpp"

#if defined(__EMSCRIPTEN__)
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#endif

Application::Application() = default;
Application::~Application() = default;

void Application::InitImGui()
{
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOther(window, true);

    ImGui_ImplWGPU_InitInfo init{};
    init.Device = render_context->device.Get();
    init.NumFramesInFlight = 3;
    init.RenderTargetFormat =
        static_cast<WGPUTextureFormat>(render_context->format);
    init.DepthStencilFormat = WGPUTextureFormat_Undefined;
    ImGui_ImplWGPU_Init(&init);
}

void Application::ShutdownImGui()
{
    ImGui_ImplWGPU_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
}

// ---------------- WebGPU init ----------------
void Application::ChooseSurfaceFormatOnce()
{
    render_context->ChooseSurfaceFormatOnce();
}

void Application::ConfigureSurfaceToSize(int pxW, int pxH)
{
    render_context->ConfigureSurfaceToSize(pxW, pxH);
}

void Application::Init()
{
    render_context = std::make_unique<RenderContext>();
    render_context->Init();
}

#if defined(__EMSCRIPTEN__)
void Application::UpdateCanvasAndSurfaceSize()
{
    render_context->UpdateCanvasAndSurfaceSize(window, "#canvas");
}
#endif

// ---------------- Render loop ----------------
void Application::Render()
{
    // Always pump events (web + native) so ImGui input is fresh.
    glfwPollEvents();

#if defined(__EMSCRIPTEN__)
    UpdateCanvasAndSurfaceSize();
#endif
    // Native: update ImGui IO for correct scaling and reconfigure on size
    // change.
    int winW = 0, winH = 0, fbW = 0, fbH = 0;
    glfwGetWindowSize(window, &winW, &winH);
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW > 0 && fbH > 0 &&
        (fbW != render_context->surface_width ||
         fbH != render_context->surface_height))
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
    ImGui::Text(
        "Framebuffer: %d x %d",
        render_context->surface_width,
        render_context->surface_height);
    ImGui::Checkbox("Show Demo", &show_demo);
    ImGui::End();
    if (show_demo) ImGui::ShowDemoWindow(&show_demo);
    ImGui::Render();

    // Acquire current texture; skip frame if unavailable (e.g., minimized)
    wgpu::SurfaceTexture st;
    render_context->surface.GetCurrentTexture(&st);
    if (!st.texture)
    {
#if !defined(__EMSCRIPTEN__)
        render_context->instance.ProcessEvents();
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

    wgpu::CommandEncoder enc = render_context->device.CreateCommandEncoder();
    wgpu::RenderPassEncoder pass = enc.BeginRenderPass(&rp);
    pass.SetPipeline(render_context->pipeline);
    pass.Draw(3);
    ImGui_ImplWGPU_RenderDrawData(ImGui::GetDrawData(), pass.Get());
    pass.End();

    wgpu::CommandBuffer cmd = enc.Finish();
    render_context->device.GetQueue().Submit(1, &cmd);

#if !defined(__EMSCRIPTEN__)
    render_context->surface.Present();
    render_context->instance.ProcessEvents();
#endif
}

// ---------------- App bootstrap ----------------
void Application::InitGraphics()
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
    glfwGetFramebufferSize(window, &fbW, &fbH);
    if (fbW == 0 || fbH == 0)
    {
        fbW = 800;
        fbH = 600;
    }
    ConfigureSurfaceToSize(fbW, fbH);
#endif
    render_context->CreateRenderPipeline();
    InitImGui();
}

void Application::Start()
{
    if (!glfwInit()) return;
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    window = glfwCreateWindow(800, 600, "WebGPU window", nullptr, nullptr);
    render_context->surface =
        wgpu::glfw::CreateSurfaceForWindow(render_context->instance, window);

    // Configure surface + pipeline + ImGui
    InitGraphics();

#if defined(__EMSCRIPTEN__)
    // Register resize callback AFTER ImGui exists, then enter main loop
    emscripten_set_resize_callback(
        EMSCRIPTEN_EVENT_TARGET_WINDOW,
        nullptr,
        false,
        [](int, const EmscriptenUiEvent*, void*)
        {
            Application::Instance().UpdateCanvasAndSurfaceSize();
            return EM_TRUE;
        });
    emscripten_set_main_loop(
        []() { Application::Instance().Render(); },
        0,
        false);
#else
    while (!glfwWindowShouldClose(window))
    {
        Render();
    }
    ShutdownImGui();
    glfwDestroyWindow(window);
    glfwTerminate();
#endif
}
