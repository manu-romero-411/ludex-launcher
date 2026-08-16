cmake_minimum_required(VERSION 3.26)

project(ludex_launcher
    VERSION 0.1.0
    DESCRIPTION "10-foot game launcher"
    LANGUAGES CXX
)

# ---------------------------------------------------------------------------
# Configuración global
# ---------------------------------------------------------------------------
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Opciones del proyecto
option(LUDEX_ENABLE_LIRC    "Enable LIRC IR remote support"    ON)
option(LUDEX_ENABLE_RSVG    "Enable high-quality SVG via librsvg" ON)
option(LUDEX_ENABLE_I18N    "Build gettext translations (.mo)"  ON)
option(LUDEX_STRICT_WARNINGS "Enable strict compiler warnings"  ON)

# ---------------------------------------------------------------------------
# Dependencias del sistema (pkg-config)
# ---------------------------------------------------------------------------
find_package(PkgConfig REQUIRED)

pkg_check_modules(SDL2        REQUIRED IMPORTED_TARGET sdl2)
pkg_check_modules(SDL2_IMAGE  REQUIRED IMPORTED_TARGET SDL2_image)
pkg_check_modules(SDL2_MIXER  REQUIRED IMPORTED_TARGET SDL2_mixer)

if(LUDEX_ENABLE_RSVG)
    pkg_check_modules(RSVG IMPORTED_TARGET librsvg-2.0)
    if(NOT RSVG_FOUND)
        message(WARNING "librsvg no encontrada: se usará el fallback nanosvg (SVG de menor calidad)")
        set(LUDEX_ENABLE_RSVG OFF)
    endif()
endif()

# ---------------------------------------------------------------------------
# LIRC (IR remote) — opcional
# ---------------------------------------------------------------------------
if(LUDEX_ENABLE_LIRC)
    find_library(LIRC_CLIENT_LIBRARY NAMES lirc_client)
    find_path(LIRC_CLIENT_INCLUDE_DIR NAMES lirc/lirc_client.h)
    if(LIRC_CLIENT_LIBRARY AND LIRC_CLIENT_INCLUDE_DIR)
        message(STATUS "LIRC: habilitado (${LIRC_CLIENT_LIBRARY})")
    else()
        message(WARNING "liblirc_client no encontrada: soporte IR desactivado")
        set(LUDEX_ENABLE_LIRC OFF)
    endif()
endif()

# ---------------------------------------------------------------------------
# FetchContent: ImGui + stb (header-only)
# ---------------------------------------------------------------------------
include(FetchContent)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG        v1.91.0
    GIT_SHALLOW    TRUE
)

FetchContent_Declare(
    stb
    GIT_REPOSITORY https://github.com/nothings/stb.git
    GIT_TAG        master
    GIT_SHALLOW    TRUE
)

FetchContent_MakeAvailable(imgui stb)

# --- ImGui como librería estática (backend SDL2 + SDL_Renderer) ---
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
)

# ---------------------------------------------------------------------------
# Ejecutable principal
# ---------------------------------------------------------------------------
add_executable(ludex-launcher
    # entry point
    src/main.cpp
    src/application.cpp

    # core
    src/util.cpp
    src/core/config.cpp
    src/core/ini.cpp
    src/core/app_backends.cpp
    src/core/app_discovery.cpp
    src/core/launcher.cpp
    src/core/bluetooth_manager.cpp

    # audio
    src/audio/audio_manager.cpp

    # input
    src/input/input_manager.cpp
    src/input/ir_input.cpp

    # ui — common + assets
    src/ui/ui_common.cpp
    src/ui/assets.cpp
    src/ui/shell_orchestrator.cpp
    src/ui/ui_input_handlers.cpp

    # ui — shell state
    src/ui/shell/shell_state.cpp

    # ui — render layer
    src/ui/render/texture_handle.cpp
    src/ui/render/icon_cache.cpp
    src/ui/render/wallpaper_manager.cpp
    src/ui/render/renderer_sdl.cpp

    # ui — widgets
    src/ui/widgets/clock.cpp
    src/ui/widgets/player_indicators.cpp
    src/ui/widgets/help_hints.cpp
    src/ui/widgets/edge_fades.cpp
    src/ui/widgets/wallpaper.cpp
    src/ui/widgets/toast.cpp
    src/ui/widgets/tile_carousel.cpp

    # ui — panels
    src/ui/panels/panel_renderer.cpp
    src/ui/panels/pan_settings.cpp
    src/ui/panels/pan_shutdown.cpp
    src/ui/panels/pan_controllers.cpp
    src/ui/panels/pan_mainmenu.cpp
    src/ui/panels/pan_pin.cpp
    src/ui/panels/pan_bluetooth.cpp
    src/ui/panels/pan_bluetooth_scan.cpp
)

# ---------------------------------------------------------------------------
# Propiedades del target
# ---------------------------------------------------------------------------
target_include_directories(ludex-launcher PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${stb_SOURCE_DIR}
)

target_link_libraries(ludex-launcher PRIVATE
    imgui
    PkgConfig::SDL2
    PkgConfig::SDL2_IMAGE
    PkgConfig::SDL2_MIXER
)

# Warnings estrictos
if(LUDEX_STRICT_WARNINGS)
    target_compile_options(ludex-launcher PRIVATE
        -Wall -Wextra -Wpedantic
        -Wshadow -Wnon-virtual-dtor -Wold-style-cast
        -Wcast-align -Woverloaded-virtual
        -Wconversion -Wsign-conversion
        -Wnull-dereference -Wdouble-promotion
        -Wformat=2
    )
    target_compile_options(imgui PRIVATE -w) # silenciar warnings de imgui
endif()

# ---------------------------------------------------------------------------
# Definiciones de compilación
# ---------------------------------------------------------------------------
if(LUDEX_ENABLE_RSVG)
    target_compile_definitions(ludex-launcher PRIVATE LUDEX_HAVE_RSVG=1)
    target_link_libraries(ludex-launcher PRIVATE PkgConfig::RSVG)
endif()

if(LUDEX_ENABLE_LIRC)
    target_compile_definitions(ludex-launcher PRIVATE HAVE_LIRC=1)
    target_include_directories(ludex-launcher PRIVATE ${LIRC_CLIENT_INCLUDE_DIR})
    target_link_libraries(ludex-launcher PRIVATE ${LIRC_CLIENT_LIBRARY})
endif()

# ---------------------------------------------------------------------------
# Build info (git commit + fecha) — generado en build/generated/build_info.h
# ---------------------------------------------------------------------------
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
target_include_directories(ludex-launcher PRIVATE ${CMAKE_BINARY_DIR}/generated)

# ---------------------------------------------------------------------------
# i18n: locale/<lang>/LC_MESSAGES/ludex.po → build/locale/<lang>/LC_MESSAGES/ludex.mo
# ---------------------------------------------------------------------------
if(LUDEX_ENABLE_I18N)
    find_program(MSGFMT_EXECUTABLE msgfmt)
    if(MSGFMT_EXECUTABLE)
        file(GLOB_RECURSE PO_FILES CONFIGURE_DEPENDS
            ${CMAKE_SOURCE_DIR}/locale/*.po
        )

        set(MO_OUTPUTS "")
        foreach(PO_FILE IN LISTS PO_FILES)
            get_filename_component(PO_NAME ${PO_FILE} NAME_WE)
            get_filename_component(PO_DIR  ${PO_FILE} DIRECTORY)
            get_filename_component(LANG_DIR ${PO_DIR} DIRECTORY)
            get_filename_component(LANG ${LANG_DIR} NAME)

            set(MO_DIR  ${CMAKE_BINARY_DIR}/locale/${LANG}/LC_MESSAGES)
            set(MO_FILE ${MO_DIR}/${PO_NAME}.mo)

            add_custom_command(
                OUTPUT  ${MO_FILE}
                COMMAND ${CMAKE_COMMAND} -E make_directory ${MO_DIR}
                COMMAND ${MSGFMT_EXECUTABLE} --check-format -o ${MO_FILE} ${PO_FILE}
                DEPENDS ${PO_FILE}
                COMMENT "msgfmt: ${LANG}/${PO_NAME}.mo"
                VERBATIM
            )
            list(APPEND MO_OUTPUTS ${MO_FILE})

            install(FILES ${MO_FILE}
                DESTINATION share/locale/${LANG}/LC_MESSAGES
            )
        endforeach()

        if(MO_OUTPUTS)
            add_custom_target(translations ALL DEPENDS ${MO_OUTPUTS})
            add_dependencies(ludex-launcher translations)
        endif()
    else()
        message(WARNING "msgfmt no encontrado: traducciones desactivadas")
    endif()
endif()

# LOCALEDIR: en dev apunta al build; al empaquetar se sobreescribe
if(NOT DEFINED LUDEX_LOCALEDIR)
    set(LUDEX_LOCALEDIR "${CMAKE_INSTALL_PREFIX}/share/locale")
endif()
target_compile_definitions(ludex-launcher PRIVATE
    LOCALEDIR="${LUDEX_LOCALEDIR}"
)

# ---------------------------------------------------------------------------
# Install
# ---------------------------------------------------------------------------
install(TARGETS ludex-launcher RUNTIME DESTINATION bin)