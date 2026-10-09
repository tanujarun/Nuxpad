# Nuxpad

A native Linux clone of the Windows 11 Notepad (not Notepad++), written in C++ with Qt 6 Widgets.
Nuxpad is an independent work-alike. It is not affiliated with or endorsed by Microsoft; Windows and
Notepad are trademarks of the Microsoft group of companies.

- Tabs: `Ctrl+N` new tab, `Ctrl+W` close tab, `Ctrl+Tab` to switch, drag to reorder, middle-click to close.
  An unsaved note is named after its first words and carries a dot instead of the close button.
- Session memory: closing Nuxpad never asks. Every tab, including unsaved text and unsaved edits to
  files, comes back on the next start, with its caret and the active tab. The state is written while
  you type, so it also survives a crash or a kill.
- `nuxpad file.txt` while Nuxpad is running opens the file as a new tab in that window.
- Light and dark theme (follows the desktop by default), the Settings page, the find/replace bar,
  Go to line, zoom, word wrap, the status bar with position, character count, zoom, line ending and
  encoding, printing with header/footer codes.
- Encodings: ANSI, UTF-16 LE/BE, UTF-8 with or without BOM, detected on open and kept on save.

The window has no title bar of its own: under Hyprland it is a tile like any other, and the tab
strip is the first row of the window.

## Build and install

    ./install.sh

Needs `qt6-base` and `qt6-svg`. Installs `~/.local/bin/nuxpad`, a launcher entry and the icon.

## Files

| What | Where |
| --- | --- |
| Settings | `~/.config/nuxpad/nuxpad.ini` |
| Session (tabs, unsaved text) | `~/.local/state/nuxpad/` |

`DefaultLineEnding` (`lf`, `crlf` or `cr`) in the ini sets the line ending of new documents.

## Fonts

Windows 11 Notepad uses Segoe UI Variable for the interface and Consolas for text. When those are
installed they are used. Otherwise the bundled stand-ins are Selawik (Microsoft's open,
metric-compatible fallback for Segoe UI) and "Nuxpad Mono", an instance of Inconsolata widened and
given Consolas' advance width and line height. Both are under the SIL OFL (`res/fonts`).

## Test build

    mkdir build-dev && cd build-dev && qmake6 ../nuxpad.pro CONFIG+=devhooks && make

adds `NUXPAD_DEV_SCRIPT`, a list of scripted steps (keys, clicks, window grabs, state dumps) used to
check behaviour and looks without touching the real pointer or keyboard. See `src/devscript.cpp`.
Set `XDG_CONFIG_HOME` and `XDG_STATE_HOME` to scratch directories for such runs; an instance with its
own state directory never talks to the Nuxpad you have open.
