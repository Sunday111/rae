cmake_minimum_required(VERSION 3.25)
project(app)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# ---- Dawn (as subdir; your working approach) ----
set(DAWN_FETCH_DEPENDENCIES ON)
set(DAWN_BUILD_MONOLITHIC_LIBRARY STATIC)
set(DAWN_BUILD_SAMPLES OFF)
add_subdirectory("dawn/repo" EXCLUDE_FROM_ALL)

# ---- ImGui via FetchContent ----
include(FetchContent)
FetchContent_Declare(
  imgui
  GIT_REPOSITORY https://github.com/ocornut/imgui.git
  GIT_TAG        master
)
FetchContent_MakeAvailable(imgui)

# ---- Executable ----
add_executable(app src/main.cpp)

# ImGui core + backends (add demo or remove ShowDemoWindow from your code)
target_sources(app PRIVATE
  ${imgui_SOURCE_DIR}/imgui.cpp
  ${imgui_SOURCE_DIR}/imgui_draw.cpp
  ${imgui_SOURCE_DIR}/imgui_tables.cpp
  ${imgui_SOURCE_DIR}/imgui_widgets.cpp
  ${imgui_SOURCE_DIR}/imgui_demo.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_glfw.cpp
  ${imgui_SOURCE_DIR}/backends/imgui_impl_wgpu.cpp
)

# if (EMSCRIPTEN)
# else()
# target_sources(app PRIVATE
# )
# endif()

target_include_directories(app PRIVATE
  ${imgui_SOURCE_DIR}
  ${imgui_SOURCE_DIR}/backends
)

# ImGui backend selection
if(EMSCRIPTEN)
  target_compile_definitions(app PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN IMGUI_IMPL_WEBGPU_BACKEND_WGPU)
else()
  target_compile_definitions(app PRIVATE IMGUI_IMPL_WEBGPU_BACKEND_DAWN)
endif()

# Platform gates so Dawn exposes native surface structs in headers
if(WIN32)
  target_compile_definitions(app PRIVATE WGPU_ENABLE_BACKEND_WIN32=1)
elseif(UNIX AND NOT APPLE)
  target_compile_definitions(app PRIVATE WGPU_ENABLE_BACKEND_XLIB=1)
endif()

# ---- Linkage (mirror your working setup) ----
if(EMSCRIPTEN)
  set(content_dir "${CMAKE_CURRENT_SOURCE_DIR}/content")
  set(html_shell_file "${content_dir}/shell.html")
  set_target_properties(app PROPERTIES
    LINK_DEPENDS "${html_shell_file}"
    OUTPUT_NAME "index"
    SUFFIX ".html"
  )

  target_link_libraries(app PRIVATE emdawnwebgpu_cpp webgpu_glfw)
  target_link_options(app PRIVATE "-sASYNCIFY=1" "-sUSE_GLFW=3" --shell-file "${html_shell_file}")
else()
  target_link_libraries(app PRIVATE webgpu_dawn webgpu_glfw glfw)

  # Static GLFW from Dawn needs X11 libs explicitly on Linux
  if(UNIX AND NOT APPLE)
    find_package(X11 REQUIRED)
    target_link_libraries(app PRIVATE X11::X11)
    if(TARGET X11::Xrandr)
      target_link_libraries(app PRIVATE X11::Xrandr)
    endif()
    if(TARGET X11::Xi)
      target_link_libraries(app PRIVATE X11::Xi)
    endif()
    if(TARGET X11::Xinerama)
      target_link_libraries(app PRIVATE X11::Xinerama)
    endif()
    if(TARGET X11::Xcursor)
      target_link_libraries(app PRIVATE X11::Xcursor)
    endif()
    if(TARGET X11::Xxf86vm)
      target_link_libraries(app PRIVATE X11::Xxf86vm)
    endif()
  endif()
endif()
