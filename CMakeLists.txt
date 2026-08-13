cmake_minimum_required(VERSION 3.26)

project(tenfoot_shell_cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TENFOOT_ENABLE_LIRC "Enable LIRC IR remote support" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)

find_package(Vulkan REQUIRED)

find_program(GLSLANG_VALIDATOR NAMES glslangValidator)

include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.0
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG master
)

FetchContent_MakeAvailable(imgui stb)

add_library(imgui STATIC
    ${imgui_SOURCE_DIR}/imgui.cpp
    ${imgui_SOURCE_DIR}/imgui_demo.cpp
    ${imgui_SOURCE_DIR}/imgui_draw.cpp
    ${imgui_SOURCE_DIR}/imgui_tables.cpp
    ${imgui_SOURCE_DIR}/imgui_widgets.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdl2.cpp
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp  # <--- AÑADIR ESTO
)

target_include_directories(imgui PUBLIC
    ${imgui_SOURCE_DIR}
    ${imgui_SOURCE_DIR}/backends
)

target_link_libraries(imgui PUBLIC
    PkgConfig::SDL2
    Vulkan::Vulkan
)

add_executable(ludex-launcher
    src/main.cpp
    src/config.cpp
    src/app_discovery.cpp
    src/launcher.cpp
    src/input_manager.cpp
    src/ir_input.cpp
    src/shell_state.cpp
    src/shell_ui.cpp
    src/renderer_vulkan.cpp
    src/assets.cpp
)

target_include_directories(ludex-launcher PRIVATE
    src
    ${stb_SOURCE_DIR}
)

target_link_libraries(ludex-launcher PRIVATE
    imgui
    PkgConfig::SDL2
    PkgConfig::SDL2_IMAGE
    Vulkan::Vulkan
)

if(TENFOOT_ENABLE_LIRC)
    find_library(LIRC_CLIENT_LIBRARY NAMES lirc_client)
    find_path(LIRC_CLIENT_INCLUDE_DIR NAMES lirc/lirc_client.h)

    if(LIRC_CLIENT_LIBRARY AND LIRC_CLIENT_INCLUDE_DIR)
        target_compile_definitions(ludex-launcher PRIVATE HAVE_LIRC=1)
        target_include_directories(ludex-launcher PRIVATE ${LIRC_CLIENT_INCLUDE_DIR})
        target_link_libraries(ludex-launcher PRIVATE ${LIRC_CLIENT_LIBRARY})
        message(STATUS "LIRC support enabled")
    else()
        message(WARNING "liblirc_client not found; IR support disabled")
    endif()
endif()

# ---------------------------------------------------------------------------
# Shaders para ImGui/Vulkan
# ---------------------------------------------------------------------------

find_program(GLSLANG_VALIDATOR NAMES glslangValidator)

if(GLSLANG_VALIDATOR)
    set(SHADER_OUT ${CMAKE_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${SHADER_OUT})

    # Shaders locales del proyecto.
    set(LOCAL_VERT ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.vert)
    set(LOCAL_FRAG ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.frag)

    # Shaders que vienen con Dear ImGui.
    set(IMGUI_VERT ${imgui_SOURCE_DIR}/backends/vulkan/glsl_shader.vert)
    set(IMGUI_FRAG ${imgui_SOURCE_DIR}/backends/vulkan/glsl_shader.frag)

    # Fallback por si alguna versión antigua los tuviera en otra ruta.
    if(NOT EXISTS ${IMGUI_VERT})
        set(IMGUI_VERT ${imgui_SOURCE_DIR}/backends/glsl_shader.vert)
    endif()

    if(NOT EXISTS ${IMGUI_FRAG})
        set(IMGUI_FRAG ${imgui_SOURCE_DIR}/backends/glsl_shader.frag)
    endif()

    # Preferimos los shaders locales. Si no existen, usamos los de ImGui.
    if(EXISTS ${LOCAL_VERT} AND EXISTS ${LOCAL_FRAG})
        set(VERT_SRC ${LOCAL_VERT})
        set(FRAG_SRC ${LOCAL_FRAG})
        message(STATUS "Using local ImGui Vulkan shaders")
    elseif(EXISTS ${IMGUI_VERT} AND EXISTS ${IMGUI_FRAG})
        set(VERT_SRC ${IMGUI_VERT})
        set(FRAG_SRC ${IMGUI_FRAG})
        message(STATUS "Using Dear ImGui Vulkan shaders: ${VERT_SRC}")
    else()
        message(FATAL_ERROR
            "No se han encontrado shaders para ImGui/Vulkan.\n"
            "Crea shaders/imgui.vert y shaders/imgui.frag en la raíz del proyecto."
        )
    endif()

    add_custom_command(
        OUTPUT ${SHADER_OUT}/imgui.vert.spv
        COMMAND ${GLSLANG_VALIDATOR}
                -V
                ${VERT_SRC}
                -o ${SHADER_OUT}/imgui.vert.spv
        DEPENDS ${VERT_SRC}
        COMMENT "Compiling ImGui vertex shader"
        VERBATIM
    )

    add_custom_command(
        OUTPUT ${SHADER_OUT}/imgui.frag.spv
        COMMAND ${GLSLANG_VALIDATOR}
                -V
                ${FRAG_SRC}
                -o ${SHADER_OUT}/imgui.frag.spv
        DEPENDS ${FRAG_SRC}
        COMMENT "Compiling ImGui fragment shader"
        VERBATIM
    )

    add_custom_target(tenfoot_shaders DEPENDS
        ${SHADER_OUT}/imgui.vert.spv
        ${SHADER_OUT}/imgui.frag.spv
    )

    if(TARGET imgui-populate)
        add_dependencies(tenfoot_shaders imgui-populate)
    endif()

    add_dependencies(ludex-launcher tenfoot_shaders)
else()
    message(FATAL_ERROR
        "glslangValidator no encontrado.\n"
        "Instala el paquete: sudo apt install glslang-tools"
    )
endif()
