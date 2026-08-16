# syntax=docker/dockerfile:1.6

# ===========================================================================
# Stage 1: builder — todo lo necesario para compilar
# ===========================================================================
FROM debian:trixie-slim AS builder

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Madrid

# Dependencias de build en una sola capa
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        build-essential \
        cmake \
        ninja-build \
        git \
        pkg-config \
        ca-certificates \
        gettext \
        gdb \
        # libs de desarrollo
        libsdl2-dev \
        libsdl2-image-dev \
        libsdl2-mixer-dev \
        librsvg2-dev \
        liblirc-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /src

# Copia solo lo necesario para el build
COPY CMakeLists.txt ./
COPY src            ./src
COPY locale         ./locale

# Configura y compila
RUN cmake -S . -B build -G Ninja \
        -DCMAKE_BUILD_TYPE=RelWithDebInfo \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DLUDEX_LOCALEDIR=/usr/share/locale \
    && cmake --build build --parallel \
    && cmake --install build --prefix /tmp/install

# ===========================================================================
# Stage 2: runtime — imagen mínima solo con lo necesario para ejecutar
# ===========================================================================
FROM debian:trixie-slim AS runtime

ENV DEBIAN_FRONTEND=noninteractive \
    TZ=Europe/Madrid \
    # Wayland por defecto con fallback a X11
    SDL_VIDEODRIVER=wayland \
    XDG_RUNTIME_DIR=/tmp/runtime-ludex

# Solo las shared libraries necesarias en runtime
RUN --mount=type=cache,target=/var/cache/apt,sharing=locked \
    --mount=type=cache,target=/var/lib/apt,sharing=locked \
    apt-get update && apt-get install -y --no-install-recommends \
        libsdl2-2.0-0 \
        libsdl2-image-2.0-0 \
        libsdl2-mixer-2.0-0 \
        librsvg2-2 \
        liblirc-client0 \
        bluez \
        bluetooth \
        dbus \
    && rm -rf /var/lib/apt/lists/* \
    && useradd --system --create-home --shell /bin/bash ludex

# Copia el binario + locales ya compilados del stage builder
COPY --from=builder /tmp/install /usr

# Recursos adicionales (si los tienes: wallpapers, música, iconos)
COPY resources /usr/share/ludex

WORKDIR /home/ludex
USER ludex

ENTRYPOINT ["/usr/bin/ludex-launcher"]