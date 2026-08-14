#!/usr/bin/env bash
set -euo pipefail
PREFIX="${1:-/var/penguin/aplicaciones/ludex-launcher}"

cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build-release
strip build-release/ludex-launcher

install -d "$PREFIX/resources" "$PREFIX/backends" "$HOME/.local/bin"
install -m 755 build-release/ludex-launcher "$PREFIX/"
cp -r apps  "$PREFIX/apps"
cp -r resources/icons "$PREFIX/resources/"
cp -r backends/*.backend "$PREFIX/backends/"

# symlink en tu PATH de usuario (sin root)
ln -sf "$PREFIX/ludex-launcher" "$HOME/.local/bin/ludex-launcher"

echo "Desplegado en $PREFIX"
