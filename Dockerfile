FROM debian:trixie

ENV DEBIAN_FRONTEND=noninteractive
ENV TZ=Europe/Madrid

RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential \
    cmake \
    ninja-build \
    git \
    pkg-config \
    ca-certificates \
    gdb \
    libsdl2-dev \
    libvulkan-dev \
    vulkan-tools \
    vulkan-validationlayers \
    glslang-tools \
    liblirc-client-dev \
    libstb-dev \
    libsdl2-dev \
    libsdl2-image-dev \
    libglm-dev \
    librsvg2-dev \
    libsdl2-mixer-dev \
    && rm -rf /var/lib/apt/lists/*

WORKDIR /project

COPY . /project

RUN cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo \
    && cmake --build build

CMD ["./build/ludex-launcher"]
