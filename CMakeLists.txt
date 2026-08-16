cmake_minimum_required(VERSION 3.26)

project(ludex_launcher_cpp CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TENFOOT_ENABLE_LIRC "Enable LIRC IR remote support" ON)

find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2 REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMAGE REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(RSVG IMPORTED_TARGET librsvg-2.0)
pkg_check_modules(SDL2_MIXER REQUIRED SDL2_mixer)

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
    ${imgui_SOURCE_DIR}/backends/imgui_impl_sdlrenderer2.cpp
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
    src/util.cpp
    src/main.cpp
    src/ui/ui_common.cpp
    src/ui/widgets/clock.cpp
    src/ui/widgets/player_indicators.cpp
    src/ui/widgets/help_hints.cpp
    src/ui/widgets/edge_fades.cpp
    src/ui/widgets/wallpaper.cpp
    src/ui/widgets/toast.cpp
    src/ui/widgets/tile_carousel.cpp
    src/ui/panels/panel_renderer.cpp
    src/ui/panels/pan_settings.cpp
    src/ui/panels/pan_shutdown.cpp
    src/ui/panels/pan_controllers.cpp
    src/ui/panels/pan_mainmenu.cpp
    src/ui/panels/pan_pin.cpp
    src/ui/panels/pan_bluetooth.cpp
    src/ui/panels/pan_bluetooth_scan.cpp
    src/ui/panels/pan_about.cpp
    src/ui/render/texture_handle.cpp
    src/ui/render/fade_controller.cpp
    src/ui/render/icon_cache.cpp
    src/ui/render/wallpaper_manager.cpp
    src/ui/render/renderer_sdl.cpp
    src/ui/shell/shell_state.cpp
    src/ui/assets.cpp
    src/ui/shell_orchestrator.cpp
    src/ui/ui_input_handlers.cpp
    src/audio/audio_manager.cpp
    src/input/input_manager.cpp
    src/input/ir_input.cpp
    src/input/drag_controller.cpp
    src/core/app_backends.cpp
    src/core/app_discovery.cpp
    src/core/config.cpp
    src/core/ini.cpp
    src/core/launcher.cpp
    src/core/bluetooth_manager.cpp
    src/core/app_launcher.cpp
    src/core/system_info.cpp
    src/application.cpp
)

# ---------------------------------------------------------------------------
# i18n: locale/<lang>/LC_MESSAGES/ludex.po -> build/locale/<lang>/LC_MESSAGES/ludex.mo
# ---------------------------------------------------------------------------
find_program(MSGFMT_EXECUTABLE msgfmt)
if(MSGFMT_EXECUTABLE)
    file(GLOB_RECURSE PO_FILES ${CMAKE_SOURCE_DIR}/locale/*.po)
    set(MO_OUTPUTS "")
    foreach(PO_FILE ${PO_FILES})
        get_filename_component(PO_NAME ${PO_FILE} NAME_WE) # ludex
        get_filename_component(PO_DIR ${PO_FILE} DIRECTORY) # .../LC_MESSAGES
        get_filename_component(LANG_DIR ${PO_DIR} DIRECTORY) # .../es
        get_filename_component(LANG ${LANG_DIR} NAME) # es
        set(MO_DIR ${CMAKE_BINARY_DIR}/locale/${LANG}/LC_MESSAGES)
        set(MO_FILE ${MO_DIR}/${PO_NAME}.mo)
        add_custom_command(
            OUTPUT ${MO_FILE}
            COMMAND ${CMAKE_COMMAND} -E make_directory ${MO_DIR}
            COMMAND ${MSGFMT_EXECUTABLE} --check-format -o ${MO_FILE} ${PO_FILE}
            DEPENDS ${PO_FILE}
            COMMENT "msgfmt ${LANG}/${PO_NAME}.mo")
        list(APPEND MO_OUTPUTS ${MO_FILE})
        install(FILES ${MO_FILE} DESTINATION share/locale/${LANG}/LC_MESSAGES)
    endforeach()
    if(MO_OUTPUTS)
        add_custom_target(translations ALL DEPENDS ${MO_OUTPUTS})
        add_dependencies(ludex-launcher translations)
    endif()
else()
    message(WARNING "msgfmt no encontrado; traducciones desactivadas")
endif()

# En dev apuntamos al build; al empaquetar: cmake -DLOCALEDIR=<prefix>/share/locale
if(NOT DEFINED LOCALEDIR)
    set(LOCALEDIR "${CMAKE_BINARY_DIR}/locale")
endif()
target_compile_definitions(ludex-launcher PRIVATE LOCALEDIR="${LOCALEDIR}")

target_include_directories(ludex-launcher PRIVATE
    src
    ${stb_SOURCE_DIR}
)

target_link_libraries(ludex-launcher PRIVATE
    imgui
    PkgConfig::SDL2
    PkgConfig::SDL2_IMAGE
    Vulkan::Vulkan
    ${SDL2_MIXER_LIBRARIES}
)

# ---------------------------------------------------------------------------
# librsvg (SVG de alta calidad) - MOVIDO DESPUÉS de add_executable
# ---------------------------------------------------------------------------
if(RSVG_FOUND)
    target_compile_definitions(ludex-launcher PRIVATE LUDEX_HAVE_RSVG=1)
    target_link_libraries(ludex-launcher PRIVATE PkgConfig::RSVG)
    message(STATUS "SVG high quality: librsvg")
else()
    message(WARNING "librsvg not found, SVG rendering may fail!")
endif()

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

if(GLSLANG_VALIDATOR)
    set(SHADER_OUT ${CMAKE_BINARY_DIR}/shaders)
    file(MAKE_DIRECTORY ${SHADER_OUT})

    set(LOCAL_VERT ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.vert)
    set(LOCAL_FRAG ${CMAKE_CURRENT_SOURCE_DIR}/shaders/imgui.frag)

    set(IMGUI_VERT ${imgui_SOURCE_DIR}/backends/vulkan/glsl_shader.vert)
    set(IMGUI_FRAG ${imgui_SOURCE_DIR}/backends/vulkan/glsl_shader.frag)

    if(NOT EXISTS ${IMGUI_VERT})
        set(IMGUI_VERT ${imgui_SOURCE_DIR}/backends/glsl_shader.vert)
    endif()

    if(NOT EXISTS ${IMGUI_FRAG})
        set(IMGUI_FRAG ${imgui_SOURCE_DIR}/backends/glsl_shader.frag)
    endif()

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
# --- Build info (commit + fecha) ---
execute_process(
    COMMAND git rev-parse --short HEAD
    WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
    OUTPUT_VARIABLE LUDEX_GIT_COMMIT
    OUTPUT_STRIP_TRAILING_WHITESPACE
    ERROR_QUIET
)
if(NOT LUDEX_GIT_COMMIT)
    set(LUDEX_GIT_COMMIT "unknown")
endif()

string(TIMESTAMP LUDEX_BUILD_DATE "%Y-%m-%d")
string(TIMESTAMP LUDEX_BUILD_TIME "%H:%M:%S")

configure_file(
    ${CMAKE_SOURCE_DIR}/src/core/build_info.h.in
    ${CMAKE_BINARY_DIR}/generated/build_info.h
    @ONLY
)
include_directories(${CMAKE_BINARY_DIR}/generated)
