#pragma once

#include <QColor>
#include <QFont>
#include <QIcon>
#include <QObject>
#include <QString>

class QPainter;

// Colours, fonts and icons of the Windows 11 (WinUI) look, in a light and a
// dark variant. Sizes are logical pixels at 100% scaling.
namespace Theme {

enum class Mode { Light, Dark, System };

struct Colors {
    bool dark;
    QColor mica;            // tab strip, settings page
    QColor layer;           // selected tab, menu row, status bar
    QColor content;         // text area
    QColor divider;
    QColor tabOutline;
    QColor text, textSecondary, textDisabled;
    QColor subtleHover, subtlePressed;
    QColor accent, accentHover, accentPressed, onAccent, link;
    QColor controlFill, controlFillHover, controlFillPressed, controlFillDisabled;
    QColor controlBorder, controlBorderStrong;
    QColor inputFill, inputFocusFill;
    QColor flyout, flyoutBorder;
    QColor card, cardBorder;
    QColor dialogBody, dialogFooter, dialogBorder;
    QColor selection, selectionText;
    QColor smoke;
    QColor scrollThumb;
};

void init();
const Colors &colors();

Mode mode();
void setMode(Mode mode);   // re-themes the running application
bool systemIsDark();

// Emits changed() after the theme switched.
class Notifier : public QObject
{
    Q_OBJECT
signals:
    void changed();
};
Notifier *notifier();

QString uiFamily();
QString monoFamily();
QFont uiFont(int pixelSize = 14, QFont::Weight weight = QFont::Normal);
QFont editorFont(const QString &family, int pointSize, int weight, bool italic, int zoomPercent);

QIcon appIcon();

enum class Icon { Close, Add, ChevronDown, ChevronUp, ChevronRight, Settings, ArrowUp, ArrowDown, Options, Back, Check, Search };

// Draws a line icon centred in a square of the given size.
void drawIcon(QPainter *p, Icon icon, const QPointF &center, qreal size, const QColor &color);

// Soft drop shadow around a rounded panel (flyouts, dialogs).
void drawShadow(QPainter *p, const QRectF &panel, qreal radius, int spread);

} // namespace Theme
