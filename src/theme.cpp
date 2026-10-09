#include "theme.h"
#include "style.h"

#include <QApplication>
#include <QFontDatabase>
#include <QPainter>
#include <QPainterPath>
#include <QPalette>
#include <QProcess>
#include <QStyleHints>
#include <QToolTip>
#include <QWidget>

#include <cmath>

namespace Theme {

namespace {

QString s_ui, s_mono;
Mode s_mode = Mode::System;
Colors s_colors;
Notifier *s_notifier = nullptr;

QColor rgb(unsigned v) { return QColor((v >> 16) & 0xFF, (v >> 8) & 0xFF, v & 0xFF); }
QColor rgba(unsigned v, int alpha) { QColor c = rgb(v); c.setAlpha(alpha); return c; }

Colors lightColors()
{
    Colors c;
    c.dark = false;
    c.mica = rgb(0xD1DBEC);
    c.layer = rgb(0xF1F4F9);
    c.content = rgb(0xF9F9F9);
    c.divider = rgb(0xE4E7EB);
    c.tabOutline = rgb(0xC4CFDE);
    c.text = rgb(0x1A1A1A);
    c.textSecondary = rgb(0x5D5D5D);
    c.textDisabled = rgb(0xA0A0A0);
    c.subtleHover = rgba(0x000000, 14);
    c.subtlePressed = rgba(0x000000, 8);
    c.accent = rgb(0x005FB8);
    c.accentHover = rgb(0x196EBF);
    c.accentPressed = rgb(0x337FC6);
    c.onAccent = Qt::white;
    c.link = rgb(0x003E92);
    c.controlFill = rgb(0xFBFBFB);
    c.controlFillHover = rgb(0xF6F6F6);
    c.controlFillPressed = rgb(0xF5F5F5);
    c.controlFillDisabled = rgb(0xF5F5F5);
    c.controlBorder = rgb(0xE5E5E5);
    c.controlBorderStrong = rgb(0x868686);
    c.inputFill = rgb(0xFBFBFB);
    c.inputFocusFill = Qt::white;
    c.flyout = rgb(0xF9F9F9);
    c.flyoutBorder = rgb(0xDCDCDC);
    c.card = rgb(0xFBFBFB);
    c.cardBorder = rgb(0xE5E5E5);
    c.dialogBody = Qt::white;
    c.dialogFooter = rgb(0xF3F3F3);
    c.dialogBorder = rgb(0xB9B9B9);
    c.selection = rgb(0x0078D4);
    c.selectionText = Qt::white;
    c.smoke = rgba(0x000000, 77);
    c.scrollThumb = rgb(0x8A8A8A);
    return c;
}

Colors darkColors()
{
    Colors c;
    c.dark = true;
    c.mica = rgb(0x202020);
    c.layer = rgb(0x2C2C2C);
    c.content = rgb(0x272727);
    c.divider = rgb(0x1D1D1D);
    c.tabOutline = rgb(0x3A3A3A);
    c.text = Qt::white;
    c.textSecondary = rgb(0xCFCFCF);
    c.textDisabled = rgb(0x787878);
    c.subtleHover = rgba(0xFFFFFF, 15);
    c.subtlePressed = rgba(0xFFFFFF, 10);
    c.accent = rgb(0x60CDFF);
    c.accentHover = rgb(0x5BBDE9);
    c.accentPressed = rgb(0x56AFD5);
    c.onAccent = Qt::black;
    c.link = rgb(0x99EBFF);
    c.controlFill = rgb(0x373737);
    c.controlFillHover = rgb(0x3C3C3C);
    c.controlFillPressed = rgb(0x323232);
    c.controlFillDisabled = rgb(0x323232);
    c.controlBorder = rgb(0x424242);
    c.controlBorderStrong = rgb(0x9A9A9A);
    c.inputFill = rgb(0x373737);
    c.inputFocusFill = rgb(0x1F1F1F);
    c.flyout = rgb(0x2C2C2C);
    c.flyoutBorder = rgb(0x1C1C1C);
    c.card = rgb(0x2B2B2B);
    c.cardBorder = rgb(0x1D1D1D);
    c.dialogBody = rgb(0x2B2B2B);
    c.dialogFooter = rgb(0x202020);
    c.dialogBorder = rgb(0x4A4A4A);
    c.selection = rgb(0x0078D4);
    c.selectionText = Qt::white;
    c.smoke = rgba(0x000000, 77);
    c.scrollThumb = rgb(0x9F9F9F);
    return c;
}

QString firstAvailable(const QStringList &candidates, const QString &fallback)
{
    const QStringList families = QFontDatabase::families();
    for (const QString &c : candidates) {
        if (families.contains(c, Qt::CaseInsensitive))
            return c;
    }
    return fallback;
}

void applyPalette()
{
    const Colors &c = s_colors;
    QPalette pal;
    pal.setColor(QPalette::Window, c.dark ? c.dialogBody : c.dialogFooter);
    pal.setColor(QPalette::WindowText, c.text);
    pal.setColor(QPalette::Base, c.dark ? QColor(0x1F, 0x1F, 0x1F) : QColor(Qt::white));
    pal.setColor(QPalette::AlternateBase, c.card);
    pal.setColor(QPalette::Text, c.text);
    pal.setColor(QPalette::Button, c.controlFill);
    pal.setColor(QPalette::ButtonText, c.text);
    pal.setColor(QPalette::BrightText, c.onAccent);
    pal.setColor(QPalette::Highlight, c.selection);
    pal.setColor(QPalette::HighlightedText, c.selectionText);
    pal.setColor(QPalette::Light, c.dark ? QColor(0x45, 0x45, 0x45) : QColor(Qt::white));
    pal.setColor(QPalette::Midlight, c.controlBorder);
    pal.setColor(QPalette::Mid, c.controlBorder);
    pal.setColor(QPalette::Dark, c.dark ? QColor(0x15, 0x15, 0x15) : QColor(0xA0, 0xA0, 0xA0));
    pal.setColor(QPalette::Shadow, c.dark ? QColor(Qt::black) : QColor(0x69, 0x69, 0x69));
    pal.setColor(QPalette::Link, c.link);
    pal.setColor(QPalette::LinkVisited, c.link);
    pal.setColor(QPalette::ToolTipBase, c.flyout);
    pal.setColor(QPalette::ToolTipText, c.text);
    pal.setColor(QPalette::PlaceholderText, c.textSecondary);
    for (QPalette::ColorRole role : { QPalette::WindowText, QPalette::Text, QPalette::ButtonText })
        pal.setColor(QPalette::Disabled, role, c.textDisabled);
    pal.setColor(QPalette::Inactive, QPalette::Highlight, c.selection);
    pal.setColor(QPalette::Inactive, QPalette::HighlightedText, c.selectionText);
    QApplication::setPalette(pal);
    QToolTip::setPalette(pal);
}

void resolveColors()
{
    const bool dark = s_mode == Mode::Dark || (s_mode == Mode::System && systemIsDark());
    s_colors = dark ? darkColors() : lightColors();
}

} // namespace

bool systemIsDark()
{
    static int cached = -1;
    if (cached < 0) {
        cached = 0;
        QProcess proc;
        proc.start(QStringLiteral("gsettings"),
                   { QStringLiteral("get"), QStringLiteral("org.gnome.desktop.interface"), QStringLiteral("color-scheme") });
        if (proc.waitForFinished(800))
            cached = proc.readAllStandardOutput().contains("dark") ? 1 : 0;
    }
    return cached == 1;
}

void init()
{
    for (const char *f : { ":/fonts/selawk.ttf", ":/fonts/selawkb.ttf", ":/fonts/selawksb.ttf",
                           ":/fonts/NuxpadMono-Regular.ttf", ":/fonts/NuxpadMono-Bold.ttf" })
        QFontDatabase::addApplicationFont(QString::fromLatin1(f));

    s_ui = firstAvailable({ "Segoe UI Variable Text", "Segoe UI Variable", "Segoe UI", "Selawik" }, "sans-serif");
    s_mono = firstAvailable({ "Consolas", "Nuxpad Mono" }, "monospace");
    s_notifier = new Notifier;

    resolveColors();
    QApplication::setStyle(new FluentStyle);
    applyPalette();
    QApplication::setFont(uiFont());
    QApplication::setCursorFlashTime(1060);
    QApplication::setWindowIcon(appIcon());
}

const Colors &colors() { return s_colors; }
Mode mode() { return s_mode; }
Notifier *notifier() { return s_notifier; }

void setMode(Mode mode)
{
    const bool wasDark = s_colors.dark;
    s_mode = mode;
    resolveColors();
    if (s_colors.dark == wasDark)
        return;
    applyPalette();
    emit s_notifier->changed();
    const auto widgets = QApplication::allWidgets();
    for (QWidget *w : widgets)
        w->update();
}

QString uiFamily() { return s_ui; }
QString monoFamily() { return s_mono; }

QFont uiFont(int pixelSize, QFont::Weight weight)
{
    QFont f(s_ui);
    f.setPixelSize(pixelSize);
    f.setWeight(weight);
    return f;
}

QFont editorFont(const QString &family, int pointSize, int weight, bool italic, int zoomPercent)
{
    QFont f(family);
    // GDI: lfHeight = -MulDiv(points, 96, 72), scaled by the zoom factor.
    f.setPixelSize(qMax(1, qRound(pointSize * zoomPercent / 100.0 * 96.0 / 72.0)));
    f.setWeight(QFont::Weight(qBound(1, weight, 1000)));
    f.setItalic(italic);
    f.setHintingPreference(QFont::PreferFullHinting);
    f.setKerning(false);
    return f;
}

QIcon appIcon()
{
    static const QIcon icon(QStringLiteral(":/nuxpad.svg"));
    return icon;
}

void drawIcon(QPainter *p, Icon icon, const QPointF &c, qreal size, const QColor &color)
{
    const qreal h = size / 2;
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setBrush(Qt::NoBrush);
    p->setPen(QPen(color, 1.0, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    switch (icon) {
    case Icon::Close:
        p->drawLine(c + QPointF(-h, -h), c + QPointF(h, h));
        p->drawLine(c + QPointF(h, -h), c + QPointF(-h, h));
        break;
    case Icon::Add:
        p->drawLine(c + QPointF(-h, 0), c + QPointF(h, 0));
        p->drawLine(c + QPointF(0, -h), c + QPointF(0, h));
        break;
    case Icon::ChevronDown:
        p->drawPolyline(QPolygonF({ c + QPointF(-h, -h / 2), c + QPointF(0, h / 2), c + QPointF(h, -h / 2) }));
        break;
    case Icon::ChevronUp:
        p->drawPolyline(QPolygonF({ c + QPointF(-h, h / 2), c + QPointF(0, -h / 2), c + QPointF(h, h / 2) }));
        break;
    case Icon::ChevronRight:
        p->drawPolyline(QPolygonF({ c + QPointF(-h / 2, -h), c + QPointF(h / 2, 0), c + QPointF(-h / 2, h) }));
        break;
    case Icon::ArrowUp:
        p->drawLine(c + QPointF(0, -h), c + QPointF(0, h));
        p->drawPolyline(QPolygonF({ c + QPointF(-h * 0.7, -h * 0.3), c + QPointF(0, -h), c + QPointF(h * 0.7, -h * 0.3) }));
        break;
    case Icon::ArrowDown:
        p->drawLine(c + QPointF(0, -h), c + QPointF(0, h));
        p->drawPolyline(QPolygonF({ c + QPointF(-h * 0.7, h * 0.3), c + QPointF(0, h), c + QPointF(h * 0.7, h * 0.3) }));
        break;
    case Icon::Back:
        p->drawLine(c + QPointF(-h, 0), c + QPointF(h, 0));
        p->drawPolyline(QPolygonF({ c + QPointF(-h * 0.3, -h * 0.7), c + QPointF(-h, 0), c + QPointF(-h * 0.3, h * 0.7) }));
        break;
    case Icon::Check:
        p->setPen(QPen(color, 1.3, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p->drawPolyline(QPolygonF({ c + QPointF(-h, 0), c + QPointF(-h * 0.3, h * 0.7), c + QPointF(h, -h * 0.6) }));
        break;
    case Icon::Search:
        p->drawEllipse(c + QPointF(h * 0.15, -h * 0.15), h * 0.75, h * 0.75);
        p->drawLine(c + QPointF(-h * 0.4, h * 0.4), c + QPointF(-h, h));
        break;
    case Icon::Options:   // two sliders
        p->drawLine(c + QPointF(-h, -h * 0.5), c + QPointF(h, -h * 0.5));
        p->drawLine(c + QPointF(-h, h * 0.5), c + QPointF(h, h * 0.5));
        p->setBrush(color);
        p->drawEllipse(c + QPointF(h * 0.35, -h * 0.5), h * 0.28, h * 0.28);
        p->drawEllipse(c + QPointF(-h * 0.35, h * 0.5), h * 0.28, h * 0.28);
        break;
    case Icon::Settings: {
        // an eight-toothed gear outline with a hub
        QPainterPath gear;
        const int teeth = 8;
        const qreal outer = h, inner = h * 0.74;
        for (int i = 0; i < teeth; ++i) {
            const qreal a = i * 2 * M_PI / teeth;
            const qreal w1 = 0.30, w2 = 0.16;
            const QPointF pts[4] = {
                c + QPointF(std::cos(a - w1) * inner, std::sin(a - w1) * inner),
                c + QPointF(std::cos(a - w2) * outer, std::sin(a - w2) * outer),
                c + QPointF(std::cos(a + w2) * outer, std::sin(a + w2) * outer),
                c + QPointF(std::cos(a + w1) * inner, std::sin(a + w1) * inner),
            };
            if (i == 0)
                gear.moveTo(pts[0]);
            else
                gear.lineTo(pts[0]);
            gear.lineTo(pts[1]);
            gear.lineTo(pts[2]);
            gear.lineTo(pts[3]);
        }
        gear.closeSubpath();
        p->drawPath(gear);
        p->drawEllipse(c, h * 0.34, h * 0.34);
        break;
    }
    }
    p->restore();
}

void drawShadow(QPainter *p, const QRectF &panel, qreal radius, int spread)
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    p->setPen(Qt::NoPen);
    const int maxAlpha = s_colors.dark ? 26 : 13;
    for (int i = spread; i >= 1; --i) {
        const qreal t = qreal(spread - i + 1) / spread;
        p->setBrush(QColor(0, 0, 0, int(maxAlpha * t * t)));
        // the shadow falls slightly downwards
        p->drawRoundedRect(panel.adjusted(-i, -i + spread * 0.25, i, i + spread * 0.25), radius + i, radius + i);
    }
    p->restore();
}

} // namespace Theme
