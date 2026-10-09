#pragma once

#include <QFont>
#include <QPageSize>
#include <QString>

class QWidget;

// Page Setup values; margins are kept in millimetres.
struct PageSetup {
    QPageSize::PageSizeId paper = QPageSize::Letter;
    bool landscape = false;
    double left = 19.05, right = 19.05, top = 25.4, bottom = 25.4;   // 0.75" and 1"
    QString header = QStringLiteral("&f");
    QString footer = QStringLiteral("Page &p");

    static PageSetup defaults();   // paper size and margins of the current locale
};

namespace Printing {

// Shows the Print dialog and prints the text with Notepad's header and footer codes
// (&f file name, &p page number, &d date, &t time, &l &c &r alignment, && ampersand).
void print(QWidget *parent, const QString &text, const QFont &font, const QString &documentName,
           const PageSetup &setup);

} // namespace Printing
