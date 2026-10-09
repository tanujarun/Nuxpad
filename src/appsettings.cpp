#include "appsettings.h"

#include <QFontDatabase>
#include <QSettings>

namespace {
QSettings settingsFile()
{
    return QSettings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("nuxpad"), QStringLiteral("nuxpad"));
}
}

void AppSettings::load()
{
    QSettings s = settingsFile();
    const QString themeName = s.value("Theme", "system").toString().toLower();
    theme = themeName == "light" ? Theme::Mode::Light : themeName == "dark" ? Theme::Mode::Dark : Theme::Mode::System;

    font.family = Theme::monoFamily();
    const QString face = s.value("lfFaceName").toString();
    if (!face.isEmpty() && QFontDatabase::families().contains(face, Qt::CaseInsensitive)) {
        customFont = true;
        font.family = face;
        font.pointSize = qBound(1, s.value("iPointSize", 110).toInt() / 10, 1638);
        font.weight = s.value("lfWeight", 400).toInt();
        font.italic = s.value("lfItalic", false).toBool();
    }
    wordWrap = s.value("fWrap", false).toBool();
    statusBar = s.value("StatusBar", true).toBool();
    openInNewTab = s.value("OpenInNewTab", true).toBool();
    continueSession = s.value("ContinueSession", true).toBool();
    const QString eol = s.value("DefaultLineEnding", "lf").toString().toLower();
    defaultLineEnding = eol == "crlf" ? TextFile::Crlf : eol == "cr" ? TextFile::Cr : TextFile::Lf;
    windowSize = QSize(s.value("iWindowPosDX", 1136).toInt(), s.value("iWindowPosDY", 640).toInt());
    lastDirectory = s.value("LastDirectory").toString();

    search.findText = s.value("searchString").toString();
    search.replaceText = s.value("replaceString").toString();
    search.matchCase = s.value("fMatchCase", false).toBool();
    search.wrapAround = s.value("fWrapAround", false).toBool();

    const PageSetup defaults = PageSetup::defaults();
    pageSetup.paper = QPageSize::PageSizeId(s.value("PaperSize", int(defaults.paper)).toInt());
    pageSetup.landscape = s.value("Landscape", false).toBool();
    pageSetup.left = s.value("iMarginLeft", defaults.left).toDouble();
    pageSetup.right = s.value("iMarginRight", defaults.right).toDouble();
    pageSetup.top = s.value("iMarginTop", defaults.top).toDouble();
    pageSetup.bottom = s.value("iMarginBottom", defaults.bottom).toDouble();
    pageSetup.header = s.value("szHeader", defaults.header).toString();
    pageSetup.footer = s.value("szTrailer", defaults.footer).toString();
}

void AppSettings::save() const
{
    QSettings s = settingsFile();
    s.setValue("Theme", theme == Theme::Mode::Light ? "light" : theme == Theme::Mode::Dark ? "dark" : "system");
    if (customFont) {
        s.setValue("lfFaceName", font.family);
        s.setValue("iPointSize", font.pointSize * 10);
        s.setValue("lfWeight", font.weight);
        s.setValue("lfItalic", font.italic);
    }
    s.setValue("fWrap", wordWrap);
    s.setValue("StatusBar", statusBar);
    s.setValue("OpenInNewTab", openInNewTab);
    s.setValue("ContinueSession", continueSession);
    s.setValue("iWindowPosDX", windowSize.width());
    s.setValue("iWindowPosDY", windowSize.height());
    s.setValue("LastDirectory", lastDirectory);
    s.setValue("searchString", search.findText);
    s.setValue("replaceString", search.replaceText);
    s.setValue("fMatchCase", search.matchCase);
    s.setValue("fWrapAround", search.wrapAround);
    s.setValue("PaperSize", int(pageSetup.paper));
    s.setValue("Landscape", pageSetup.landscape);
    s.setValue("iMarginLeft", pageSetup.left);
    s.setValue("iMarginRight", pageSetup.right);
    s.setValue("iMarginTop", pageSetup.top);
    s.setValue("iMarginBottom", pageSetup.bottom);
    s.setValue("szHeader", pageSetup.header);
    s.setValue("szTrailer", pageSetup.footer);
}
