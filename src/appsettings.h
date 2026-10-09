#pragma once

#include "findbar.h"
#include "printing.h"
#include "textfile.h"
#include "theme.h"

#include <QSize>

struct FontChoice {
    QString family;
    int pointSize = 11;
    int weight = 400;
    bool italic = false;
};

// Everything Nuxpad remembers between runs (~/.config/nuxpad/nuxpad.ini).
struct AppSettings {
    Theme::Mode theme = Theme::Mode::System;
    FontChoice font;
    bool customFont = false;
    bool wordWrap = false;
    bool statusBar = true;
    bool openInNewTab = true;       // "Opening files": new tab instead of a new window
    bool continueSession = true;    // "When Nuxpad starts": continue previous session
    TextFile::LineEnding defaultLineEnding = TextFile::Lf;
    QSize windowSize = QSize(1136, 640);
    QString lastDirectory;
    SearchState search;
    PageSetup pageSetup;

    void load();
    void save() const;
};
