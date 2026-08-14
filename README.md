# Ludex Launcher

A modern 10-foot launcher for Linux gaming and media centers, designed for TV screens and game controllers.

![Ludex Launcher](screenshot.png)

## Features

- **Controller-first UI**: Navigate with D-pad or analog stick, optimized for couch gaming. It should work with TV remotes attached to the PC, too.
- **Multiple backends**: Launch web apps, emulators, or custom commands via configurable backends.
- **Dynamic wallpapers**: Ken Burns effect with smooth crossfade transitions.
- **Audio support**: Background music and UI sound effects.
- **Multi-controller support**: Up to 8 players with individual controller assignment.
- **Vertical/horizontal layouts**: Configurable drawer position (left/right/top/bottom).
- **Dark/light themes**: not very elaborate, but work fine.
- **Wayland native**. X11 is also working.

## Building from Source

### Build dependencies

#### Debian/Ubuntu

```bash
sudo apt update
sudo apt install \
    libsdl2-dev \
    libsdl2-image-dev \
    libsdl2-mixer-dev \
    libvulkan-dev \
    librsvg2-dev \
    libcairo2-dev \
    cmake \
    ninja-build \
    g++
```

#### Fedora

```bash
sudo dnf install \
    SDL2-devel \
    SDL2_image-devel \
    SDL2_mixer-devel \
    vulkan-loader-devel \
    librsvg2-devel \
    cairo-devel \
    cmake \
    ninja-build \
    gcc-c++
```

#### Arch Linux

```bash
sudo pacman -S \
    sdl2 \
    sdl2_image \
    sdl2_mixer \
    vulkan-icd-loader \
    librsvg \
    cairo \
    cmake \
    ninja \
    gcc
```
### Build instructions:

Clone and build:

```bash
git clone https://github.com/manu-romero-411/ludex-launcher.git
cd ludex-launcher

cmake -B build -G Ninja
cmake --build build
```
Run:

```
./build/ludex-launcher
```

### Build Options

```bash
# Release build
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release

# Install to /opt/ludex
cmake --install build --prefix /opt/ludex
```

### Docker

A Dockerfile is included for easy deployment.

```bash
# Build image
docker build -t ludex-launcher .

# Run with X11 forwarding
docker run -it \
    --env DISPLAY=$DISPLAY \
    --volume /tmp/.X11-unix:/tmp/.X11-unix \
    --device /dev/dri \
    ludex-launcher
```

## Configuration

Configuration is stored in `~/.config/ludex/ludex-launcher.ini`

### Key Settings

```ini
[ludex-launcher]
# UI Layout
side=left                    # left, right, top, bottom
visible_items=7              # Number of visible app tiles (odd number)
theme=dark                   # dark or light

# Audio
music_dir=/path/to/music     # Background music directory
# Leave empty to use default locations

# Controller assignment
# Empty = auto-assign by connection order
[controllers]
p1_guid=030000006d04000012c2000011010000
p1_name=Logitech F310
```

## Directory Structure

```
/opt/ludex/
├── ludex-launcher              # Executable
├── resources/
│   ├── icons/                  # UI icons (SVG/PNG)
│   │   ├── settings.svg
│   │   ├── exit.svg
│   │   └── help/
│   │       ├── xbox/
│   │       └── playstation/
│   └── sounds/
│       ├── scroll.wav
│       └── select.wav
└── backends/                   # Backend definitions
    ├── webapp.backend
    └── retroarch.backend
```

### App Definitions

Create `.webapp` files in your apps directory:

```ini
[ludex-element]
name=RetroArch
backend=retroarch
run=
icon=retroarch.png
tile_type=flat
color1=#333333
```

### Backend Example

```ini
[backend]
name=retroarch
exec_start=retroarch -f %CONTROLLERSCONFIG%
exec_end=
```

## Controller Support

- **Any SDL-compatible controller**: Button labels are only from Xbox and PlayStation for now.
- **IR remotes (not tested)**: LIRC support (compile with `-DHAVE_LIRC=ON`)

### Assigning Controllers

1. Press `HOME` or `F1` to open system menu
2. Select "CONTROLLERS"
3. Assign each connected controller to a player slot
4. Configuration is saved automatically

## Keyboard Shortcuts

- `↑↓←→` or `WASD`: Navigate
- `Enter` or `Space`: Select
- `Escape`: Back/Close menu
- `F1` or `Home`: Open system menu
- `F5`: Reload apps and backends

## Dependencies

| Library | Version | Purpose |
|---------|---------|---------|
| SDL2 | 2.0.14+ | Window, input, events |
| SDL2_image | 2.0.5+ | Image loading |
| SDL2_mixer | 2.0.4+ | Audio playback |
| Dear ImGui | 1.89+ | Immediate mode GUI |
| stb_image | 2.28+ | Image decoding |
| stb_image_resize | 2.0+ | High-quality resizing |
| librsvg | 2.50+ | SVG rendering |

## License

This software is distributed under the terms of the GNU GPLv2 license - see [LICENSE](LICENSE) file for details

## Credits

- **Dear ImGui**: [https://github.com/ocornut/imgui](https://github.com/ocornut/imgui)
- **SDL2**: [https://www.libsdl.org](https://www.libsdl.org)
- **stb libraries**: [https://github.com/nothings/stb](https://github.com/nothings/stb)

## Troubleshooting

### No audio
- Check SDL2_mixer installation
- Verify audio files exist in music/sounds directories
- Check system audio settings (PulseAudio/PipeWire)

### Controllers not detected
- Ensure SDL2 gamecontrollerdb.txt is installed
- Check `SDL_GAMECONTROLLERCONFIG` environment variable
- Verify controller is in XInput or DirectInput mode

### Blank screen
- Check Vulkan drivers are installed
- Try `SDL_VIDEODRIVER=x11` for X11 fallback
- Check logs in terminal output

### Apps not appearing
- Verify `.webapp` files are in the correct directory
- Check backend definitions exist
- Run with terminal to see error messages
