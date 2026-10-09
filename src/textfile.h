#pragma once

#include <QString>

// Reading and writing text files the way Notepad does: encoding detection by
// byte-order mark and content, and preservation of the file's line endings.
namespace TextFile {

enum Encoding { Ansi, Utf16LE, Utf16BE, Utf8, Utf8Bom, AutoDetect = -1 };
enum LineEnding { Crlf, Lf, Cr };

struct Document {
    QString text;                 // line endings normalised to '\n'
    Encoding encoding = Utf8;
    LineEnding lineEnding = Lf;
    bool ok = false;
    QString error;
};

Document load(const QString &path, Encoding forced = AutoDetect);
bool save(const QString &path, const QString &text, Encoding encoding, LineEnding lineEnding, QString *error);

// True when every character survives a round trip through the ANSI code page.
bool fitsAnsi(const QString &text);

QString encodingName(Encoding encoding);       // status bar / Save As combo text
QString lineEndingName(LineEnding lineEnding); // status bar text

} // namespace TextFile
