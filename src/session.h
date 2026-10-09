#pragma once

#include <QList>
#include <QString>

// One tab as remembered between runs. Unsaved text lives in its own buffer
// file, so closing Nuxpad never asks and never loses anything.
struct SessionTab {
    QString id;
    QString path;            // empty for an untitled document
    int encoding = 0;
    int lineEnding = 0;
    bool modified = false;
    bool hasBuffer = false;  // unsaved text is kept in a buffer file
    QString text;            // save: null = buffer unchanged; load: the buffer's text
    int cursor = 0;
    int anchor = 0;
};

namespace Session {

QString directory();                               // ~/.local/state/nuxpad
QList<SessionTab> load(int *activeIndex);
void save(const QList<SessionTab> &tabs, int activeIndex);

} // namespace Session
