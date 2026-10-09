#!/bin/bash
# Builds Nuxpad and installs it for the current user (no root needed).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "$here/build"
(cd "$here/build" && qmake6 ../nuxpad.pro >/dev/null && make -j"$(nproc)" >/dev/null)

install -Dm755 "$here/build/nuxpad" "$HOME/.local/bin/nuxpad"
install -Dm644 "$here/res/nuxpad.svg" "$HOME/.local/share/icons/hicolor/scalable/apps/nuxpad.svg"
install -Dm644 "$here/res/nuxpad.desktop" "$HOME/.local/share/applications/nuxpad.desktop"
command -v update-desktop-database >/dev/null && update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true
echo "Installed: $HOME/.local/bin/nuxpad"
