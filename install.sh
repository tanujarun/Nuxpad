#!/bin/bash
# Builds Notepad and installs it for the current user (no root needed).
set -euo pipefail
here="$(cd "$(dirname "$0")" && pwd)"

mkdir -p "$here/build"
(cd "$here/build" && qmake6 ../notepad.pro >/dev/null && make -j"$(nproc)" >/dev/null)

install -Dm755 "$here/build/notepad" "$HOME/.local/bin/notepad"
install -Dm644 "$here/res/notepad.svg" "$HOME/.local/share/icons/hicolor/scalable/apps/notepad.svg"
install -Dm644 "$here/res/notepad.desktop" "$HOME/.local/share/applications/notepad.desktop"
command -v update-desktop-database >/dev/null && update-desktop-database "$HOME/.local/share/applications" >/dev/null 2>&1 || true
command -v gtk-update-icon-cache >/dev/null && gtk-update-icon-cache -q -t "$HOME/.local/share/icons/hicolor" >/dev/null 2>&1 || true
echo "Installed: $HOME/.local/bin/notepad"
