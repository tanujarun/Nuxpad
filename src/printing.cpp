#include "printing.h"

#include <QDateTime>
#include <QLocale>
#include <QPageLayout>
#include <QPainter>
#include <QPrintDialog>
#include <QPrinter>
#include <QTextLayout>

PageSetup PageSetup::defaults()
{
    PageSetup setup;
    if (QLocale::system().measurementSystem() == QLocale::MetricSystem) {
        setup.paper = QPageSize::A4;
        setup.left = setup.right = 20.0;
        setup.top = setup.bottom = 25.0;
    }
    return setup;
}

namespace Printing {

namespace {

struct Banner {
    QString left, center, right;
    bool isEmpty() const { return left.isEmpty() && center.isEmpty() && right.isEmpty(); }
};

// Expands Notepad's header/footer codes for one page.
Banner expand(const QString &format, const QString &documentName, int page)
{
    Banner banner;
    QString *target = &banner.center;
    const QLocale locale = QLocale::system();
    const QDateTime now = QDateTime::currentDateTime();
    for (qsizetype i = 0; i < format.size(); ++i) {
        const QChar c = format.at(i);
        if (c != u'&' || i + 1 >= format.size()) {
            target->append(c);
            continue;
        }
        const QChar code = format.at(++i);
        switch (code.toLower().unicode()) {
        case 'f': target->append(documentName); break;
        case 'p': target->append(QString::number(page)); break;
        case 'd': target->append(locale.toString(now.date(), QLocale::LongFormat)); break;
        case 't': target->append(locale.toString(now.time(), QLocale::ShortFormat)); break;
        case 'l': target = &banner.left; break;
        case 'c': target = &banner.center; break;
        case 'r': target = &banner.right; break;
        case '&': target->append(u'&'); break;
        default: break;
        }
    }
    return banner;
}

void drawBanner(QPainter &painter, const Banner &banner, const QRectF &line)
{
    painter.drawText(line, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, banner.left);
    painter.drawText(line, Qt::AlignHCenter | Qt::AlignVCenter | Qt::TextSingleLine, banner.center);
    painter.drawText(line, Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, banner.right);
}

} // namespace

void print(QWidget *parent, const QString &text, const QFont &font, const QString &documentName,
           const PageSetup &setup)
{
    QPrinter printer(QPrinter::HighResolution);
    printer.setDocName(documentName);
    printer.setPageLayout(QPageLayout(QPageSize(setup.paper),
                                      setup.landscape ? QPageLayout::Landscape : QPageLayout::Portrait,
                                      QMarginsF(setup.left, setup.top, setup.right, setup.bottom),
                                      QPageLayout::Millimeter));

    QPrintDialog dialog(&printer, parent);
    dialog.setWindowTitle(QObject::tr("Print"));
    if (dialog.exec() != QDialog::Accepted)
        return;

    QPainter painter;
    if (!painter.begin(&printer))
        return;

    // The printout uses the editor's typeface at its nominal (unzoomed) size.
    QFont printFont(font);
    printFont.setPointSizeF(font.pixelSize() > 0 ? font.pixelSize() * 72.0 / 96.0 : font.pointSizeF());
    printFont.setHintingPreference(QFont::PreferNoHinting);
    painter.setFont(printFont);
    const QFontMetricsF fm(printFont, &printer);
    const qreal lineHeight = fm.lineSpacing();

    const QRectF page(QPointF(0, 0), printer.pageLayout().paintRectPixels(printer.resolution()).size());
    const bool hasHeader = !setup.header.isEmpty();
    const bool hasFooter = !setup.footer.isEmpty();
    const qreal bodyTop = page.top() + (hasHeader ? 2 * lineHeight : 0);
    const qreal bodyBottom = page.bottom() - (hasFooter ? 2 * lineHeight : 0);

    const int fromPage = printer.fromPage();
    const int toPage = printer.toPage() > 0 ? printer.toPage() : INT_MAX;
    int pageNumber = 1;
    bool pageStarted = false;

    auto inRange = [&] { return pageNumber >= fromPage && pageNumber <= toPage; };
    auto beginPage = [&] {
        if (!inRange())
            return;
        if (pageStarted)
            printer.newPage();
        pageStarted = true;
        if (hasHeader)
            drawBanner(painter, expand(setup.header, documentName, pageNumber),
                       QRectF(page.left(), page.top(), page.width(), lineHeight));
        if (hasFooter)
            drawBanner(painter, expand(setup.footer, documentName, pageNumber),
                       QRectF(page.left(), page.bottom() - lineHeight, page.width(), lineHeight));
    };

    QTextOption option;
    option.setWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    option.setTabStopDistance(8 * fm.horizontalAdvance(u' '));

    beginPage();
    qreal y = bodyTop;
    const QStringList paragraphs = text.split(u'\n');
    for (const QString &paragraph : paragraphs) {
        QTextLayout layout(paragraph, printFont, &printer);
        layout.setTextOption(option);
        layout.beginLayout();
        QList<QTextLine> lines;
        for (QTextLine line = layout.createLine(); line.isValid(); line = layout.createLine()) {
            line.setLineWidth(page.width());
            lines.append(line);
        }
        layout.endLayout();
        if (lines.isEmpty()) {   // empty paragraph still takes a line
            if (y + lineHeight > bodyBottom) {
                ++pageNumber;
                if (pageNumber > toPage)
                    break;
                beginPage();
                y = bodyTop;
            }
            y += lineHeight;
            continue;
        }
        for (const QTextLine &line : std::as_const(lines)) {
            if (y + lineHeight > bodyBottom) {
                ++pageNumber;
                if (pageNumber > toPage)
                    break;
                beginPage();
                y = bodyTop;
            }
            if (inRange())
                line.draw(&painter, QPointF(page.left(), y - line.y()));
            y += lineHeight;
        }
        if (pageNumber > toPage)
            break;
    }
    painter.end();
}

} // namespace Printing
