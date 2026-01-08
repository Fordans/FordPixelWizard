cmake_minimum_required(VERSION 3.20)
# Enable RC (Resource Compiler) for Windows icon embedding
if(WIN32)
  project(FordPixelWizard LANGUAGES CXX RC)
else()
  project(FordPixelWizard LANGUAGES CXX)
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# ---- Options ----
option(FPW_USE_FETCHCONTENT "Fetch Dear ImGui and GLFW via FetchContent" ON)

# ---- OpenCV ----
find_package(OpenCV REQUIRED)

# ---- OpenGL ----
find_package(OpenGL REQUIRED)

include(FetchContent)

if(FPW_USE_FETCHCONTENT)
  # GLFW
  FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
  )
  FetchContent_MakeAvailable(glfw)

  # Dear ImGui (docking is not required; master is ok for prototype)
  FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.1
  )
  FetchContent_MakeAvailable(imgui)
else()
  message(FATAL_ERROR "FPW_USE_FETCHCONTENT=OFF is not supported in this prototype template.")
endif()

# ---- ImGui sources (built as part of this target) ----
set(IMGUI_DIR ${imgui_SOURCE_DIR})
add_library(imgui_impl STATIC
  ${IMGUI_DIR}/imgui.cpp
  ${IMGUI_DIR}/imgui_demo.cpp
  ${IMGUI_DIR}/imgui_draw.cpp
  ${IMGUI_DIR}/imgui_tables.cpp
  ${IMGUI_DIR}/imgui_widgets.cpp
  ${IMGUI_DIR}/backends/imgui_impl_glfw.cpp
  ${IMGUI_DIR}/backends/imgui_impl_opengl2.cpp
)
target_include_directories(imgui_impl PUBLIC
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)
target_link_libraries(imgui_impl PUBLIC glfw OpenGL::GL)

# ---- App ----
add_executable(FordPixelWizard
  src/main.cpp
  src/App.cpp
  src/App.h
  src/ImageLoader.cpp
  src/ImageLoader.h
  src/PixelArtProcessor.cpp
  src/PixelArtProcessor.h
  src/GLTexture.cpp
  src/GLTexture.h
)

# Add Windows icon resource file (icon.rc references icon.ico)
# CMake will automatically use the Windows Resource Compiler (rc.exe) for .rc files
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icon.rc")
  target_sources(FordPixelWizard PRIVATE icon.rc)
endif()

target_include_directories(FordPixelWizard PRIVATE
  ${OpenCV_INCLUDE_DIRS}
  ${IMGUI_DIR}
  ${IMGUI_DIR}/backends
)

target_link_libraries(FordPixelWizard PRIVATE
  imgui_impl
  glfw
  OpenGL::GL
  ${OpenCV_LIBS}
)

if(MSVC)
  target_compile_definitions(FordPixelWizard PRIVATE _CRT_SECURE_NO_WARNINGS)
  target_compile_options(FordPixelWizard PRIVATE /W4)
  # Set Windows subsystem for Release builds (hide console window)
  # Use mainCRTStartup entry point so we can still use main() function
  set_target_properties(FordPixelWizard PROPERTIES
    LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup"
  )
else()
  target_compile_options(FordPixelWizard PRIVATE -Wall -Wextra -Wpedantic)
  # For single-config generators, check CMAKE_BUILD_TYPE
  if(CMAKE_BUILD_TYPE STREQUAL "Release")
    set_target_properties(FordPixelWizard PROPERTIES
      WIN32_EXECUTABLE TRUE
      LINK_FLAGS "-Wl,--subsystem,windows"
    )
  endif()
endif()

# Copy icon.png to output directory so the window icon can be loaded at runtime
if(WIN32 AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/icon.png")
  add_custom_command(TARGET FordPixelWizard POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
    "${CMAKE_CURRENT_SOURCE_DIR}/icon.png"
    $<TARGET_FILE_DIR:FordPixelWizard>
    COMMENT "Copying icon.png to output directory"
  )
endif()


