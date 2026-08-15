#!/usr/bin/env bash
set -euo pipefail

PREFIX="${1:-/var/penguin/aplicaciones/ludex-launcher}"
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"   # funciona desde cualquier cwd

# --- Build Release ---
# LOCALEDIR apuntando al destino: gettext encontrará las traducciones desplegadas
cmake -S "$SRC" -B "$SRC/build-release" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DLOCALEDIR="$PREFIX/locale"
cmake --build "$SRC/build-release"

BIN="$SRC/build-release/ludex-launcher"
strip --strip-unneeded "$BIN"
# Opcional, si quieres backtraces útiles en producción:
# objcopy --only-keep-debug "$BIN" "$BIN.debug"
# objcopy --add-gnu-debuglink="$BIN.debug" "$BIN"

# --- Instalación limpia de directorios gestionados por el dev (sin restos) ---
install -d "$PREFIX" "$HOME/.local/bin"
rm -rf "$PREFIX/apps" "$PREFIX/resources" "$PREFIX/backends" "$PREFIX/locale"

# Binario con reemplazo atómico (seguro si hay una instancia corriendo)
install -m 755 "$BIN" "$PREFIX/.ludex-launcher.new"
mv -f "$PREFIX/.ludex-launcher.new" "$PREFIX/ludex-launcher"

for d in apps resources backends; do
    if [ -d "$SRC/$d" ]; then
        cp -a "$SRC/$d" "$PREFIX/"
    fi
done

# Traducciones compiladas por CMake (build-release/locale/<lang>/LC_MESSAGES/*.mo)
if [ -d "$SRC/build-release/locale" ]; then
    cp -a "$SRC/build-release/locale" "$PREFIX/"
fi

# Música: se MEZCLA, no se borra (puede tener pistas propias del usuario)
if [ -d "$SRC/music" ]; then
    install -d "$PREFIX/music"
    cp -a "$SRC/music/." "$PREFIX/music/"
fi

# Symlink en el PATH de usuario (sin root)
ln -sf "$PREFIX/ludex-launcher" "$HOME/.local/bin/ludex-launcher"

echo "Desplegado en $PREFIX"
