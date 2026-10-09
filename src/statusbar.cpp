#include "statusbar.h"
#include "theme.h"

#include <QLocale>
#include <QPainter>

namespace {
// widths of the right-hand parts
constexpr int kEncodingWidth = 130;
constexpr int kLineEndingWidth = 160;
constexpr int kZoomWidth = 88;
}

StatusBar::StatusBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Height);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setPosition(1, 1);
    setCharacters(0, 0);
    setZoom(100);
}

void StatusBar::setPosition(int line, int column)
{
    const QString text = tr("Ln %1, Col %2").arg(line).arg(column);
    if (text != m_position) {
        m_position = text;
        update();
    }
}

void StatusBar::setCharacters(qsizetype total, qsizetype selected)
{
    const QLocale locale;
    QString text = selected > 0 ? tr("%1 of %2 characters").arg(locale.toString(selected), locale.toString(total))
                   : total == 1 ? tr("1 character") : tr("%1 characters").arg(locale.toString(total));
    if (text != m_characters) {
        m_characters = text;
        update();
    }
}

void StatusBar::setZoom(int percent)
{
    m_zoom = QStringLiteral("%1%").arg(percent);
    update();
}

void StatusBar::setLineEnding(const QString &text)
{
    m_lineEnding = text;
    update();
}

void StatusBar::setEncoding(const QString &text)
{
    m_encoding = text;
    update();
}

void StatusBar::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.fillRect(rect(), c.layer);
    p.fillRect(QRect(0, 0, width(), 1), c.divider);
    p.setFont(Theme::uiFont(12));
    p.setPen(c.textSecondary);
    const QColor line = c.dark ? QColor(255, 255, 255, 24) : QColor(0, 0, 0, 22);
    const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine;
    const QRect row(0, 1, width(), Height - 1);

    // left: position | characters
    int x = 22;
    p.drawText(QRect(x, row.y(), width(), row.height()), flags, m_position);
    x += p.fontMetrics().horizontalAdvance(m_position) + 16;
    p.fillRect(QRect(x, 9, 1, 16), line);
    x += 14;
    const int leftLimit = width() - kEncodingWidth - kLineEndingWidth - kZoomWidth;
    if (x + p.fontMetrics().horizontalAdvance(m_characters) < leftLimit - 8)
        p.drawText(QRect(x, row.y(), width(), row.height()), flags, m_characters);

    // right: zoom | line ending | encoding
    int right = width();
    auto part = [&](int partWidth, const QString &text) {
        const int left = right - partWidth;
        if (left > 120) {
            p.fillRect(QRect(left, 9, 1, 16), line);
            p.drawText(QRect(left + 14, row.y(), partWidth - 16, row.height()), flags, text);
        }
        right = left;
    };
    part(kEncodingWidth, m_encoding);
    part(kLineEndingWidth, m_lineEnding);
    part(kZoomWidth, m_zoom);
}
