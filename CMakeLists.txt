cmake_minimum_required(VERSION 3.10)
project(LiveShader)

enable_language(C CXX ASM)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(SDL3 REQUIRED CONFIG)
find_package(OpenGL REQUIRED)

include_directories(
  ${SDL3_INCLUDE_DIRS}
  ${OPENGL_INCLUDE_DIR}
  ${CMAKE_SOURCE_DIR}/src/external/imgui
  ${CMAKE_SOURCE_DIR}/src/external/imgui/backends
  ${CMAKE_SOURCE_DIR}/include
)

# https://glad.dav1d.de/
# https://glad.dav1d.de/generated/tmpnwrw19qoglad/

add_executable(${PROJECT_NAME}
  src/main.cpp
  src/glad.c
  # src/png.c
  src/resource.S
  src/external/imgui/imgui.cpp
  src/external/imgui/imgui_draw.cpp
  src/external/imgui/imgui_widgets.cpp
  src/external/imgui/imgui_tables.cpp
  src/external/imgui/backends/imgui_impl_sdl3.cpp
  src/external/imgui/backends/imgui_impl_opengl3.cpp
  src/external/imgui/backends/imgui_impl_glfw.cpp
)

# Optional: on Windows link additional libraries
if (WIN32)
  target_link_libraries(${PROJECT_NAME}
    #${SDL3_LIBRARIES}
    ${OPENGL_LIBRARIES}
    png16
    SDL3
    winmm
    setupapi
    version
    opengl32
    glfw3
    opengl32
    gdi32
    imm32
    z
  )

  set_target_properties(${PROJECT_NAME}
    PROPERTIES LINK_FLAGS "-static -lgcc -lstdc++ -lwinpthread -lSDL3"
  )
else()
  target_link_libraries(${PROJECT_NAME}
    ${SDL3_LIBRARIES}
    ${OPENGL_LIBRARIES}
    glfw
    dl
    GL
    z
  )
endif()
