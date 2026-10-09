#include "style.h"
#include "theme.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QComboBox>
#include <QDateTime>
#include <QDialogButtonBox>
#include <QHash>
#include <QLineEdit>
#include <QMenuBar>
#include <QPainter>
#include <QPainterPath>
#include <QScrollBar>
#include <QStyleFactory>
#include <QStyleOption>
#include <QTimer>
#include <QWindow>

namespace {

constexpr int kShadow = FluentMenu::Shadow;
constexpr int kMenuItemHeight = 32;      // 28 + a 2 pixel margin above and below
constexpr int kMenuSeparatorHeight = 5;
constexpr int kMenuTextInset = 15;       // 4 margin + 11 padding
constexpr int kMenuCheckColumn = 28;
constexpr int kMenuShortcutGap = 24;
constexpr int kMenuBarHeight = 32;
constexpr int kScrollBarExtent = 12;
constexpr qint64 kScrollBarLingerMs = 1500;

const Theme::Colors &C() { return Theme::colors(); }

QString withoutMnemonic(const QString &text)
{
    QString out;
    out.reserve(text.size());
    for (qsizetype i = 0; i < text.size(); ++i) {
        if (text.at(i) == u'&' && i + 1 < text.size())
            ++i;
        out.append(text.at(i));
    }
    return out;
}

QFont shortcutFont(const QFont &base)
{
    QFont f(base);
    f.setPixelSize(12);
    return f;
}

struct MenuMetrics {
    int width = 0;            // of every item
    int shortcutColumn = 0;   // what QMenu adds on its own
    bool checkable = false;
};

MenuMetrics measureMenu(const QWidget *widget, const QFont &font)
{
    MenuMetrics m;
    const auto *menu = qobject_cast<const QMenu *>(widget);
    if (!menu)
        return m;
    const QFontMetricsF fm(font);
    const QFontMetricsF sfm(shortcutFont(font));
    const QFontMetrics ifm(font);
    qreal widest = 0;
    bool hasSubMenu = false;
    const auto actions = menu->actions();
    for (const QAction *a : actions) {
        if (a->isSeparator() || !a->isVisible())
            continue;
        m.checkable = m.checkable || a->isCheckable();
        hasSubMenu = hasSubMenu || a->menu();
        const QString text = a->text();
        const qsizetype tab = text.indexOf(u'\t');
        qreal width = fm.horizontalAdvance(withoutMnemonic(tab >= 0 ? text.left(tab) : text));
        if (tab >= 0) {
            const QString shortcut = text.mid(tab + 1);
            width += kMenuShortcutGap + sfm.horizontalAdvance(shortcut);
            m.shortcutColumn = qMax(m.shortcutColumn, ifm.horizontalAdvance(shortcut));
        }
        widest = qMax(widest, width);
    }
    m.width = 2 * kMenuTextInset + qRound(widest) + (m.checkable ? kMenuCheckColumn : 0) + (hasSubMenu ? 24 : 0);
    m.width = qMax(m.width, 120);
    return m;
}

void fillRounded(QPainter *p, const QRectF &r, qreal radius, const QColor &fill, const QColor &border = QColor())
{
    p->save();
    p->setRenderHint(QPainter::Antialiasing, true);
    if (border.isValid()) {
        p->setPen(QPen(border, 1));
        p->setBrush(fill);
        p->drawRoundedRect(r.adjusted(0.5, 0.5, -0.5, -0.5), radius, radius);
    } else {
        p->setPen(Qt::NoPen);
        p->setBrush(fill);
        p->drawRoundedRect(r, radius, radius);
    }
    p->restore();
}

bool isAccentButton(const QStyleOption *option, const QWidget *widget)
{
    if (widget && widget->property("accent").toBool())
        return true;
    if (const auto *btn = qstyleoption_cast<const QStyleOptionButton *>(option))
        return btn->features & QStyleOptionButton::DefaultButton;
    return false;
}

// Overlay scroll bars stay visible for a moment after the last activity.
struct ScrollLinger {
    qint64 last = 0;
    bool timerArmed = false;
};

QHash<const QWidget *, ScrollLinger> &scrollActivity()
{
    static QHash<const QWidget *, ScrollLinger> map;
    return map;
}

void armScrollHide(QWidget *widget)
{
    ScrollLinger &state = scrollActivity()[widget];
    if (state.timerArmed)
        return;
    state.timerArmed = true;
    QTimer::singleShot(kScrollBarLingerMs + 60, widget, [widget] {
        auto it = scrollActivity().find(widget);
        if (it == scrollActivity().end())
            return;
        it->timerArmed = false;
        if (QDateTime::currentMSecsSinceEpoch() - it->last < kScrollBarLingerMs)
            armScrollHide(widget);     // there was activity in the meantime
        else
            widget->update();
    });
}

} // namespace

// ---------------------------------------------------------------------------

FluentMenu::FluentMenu(const QString &title, QWidget *parent)
    : QMenu(title, parent)
{
    setFont(Theme::uiFont());
    // Room for the drop shadow. Wayland places a menu by its top-left corner
    // against the item that opened it, so the panel itself starts there.
    setContentsMargins(0, 0, Shadow, Shadow);
}

FluentMenu *FluentMenu::addFluentMenu(const QString &title)
{
    auto *menu = new FluentMenu(title, this);
    addMenu(menu);
    return menu;
}

// ---------------------------------------------------------------------------

FluentStyle::FluentStyle()
    : QProxyStyle(QStyleFactory::create(QStringLiteral("fusion")))
{
}

void FluentStyle::polish(QWidget *widget)
{
    QProxyStyle::polish(widget);
    if (qobject_cast<QMenu *>(widget)) {
        widget->setAttribute(Qt::WA_TranslucentBackground, true);
        widget->setWindowFlag(Qt::NoDropShadowWindowHint, true);
    }
    if (qobject_cast<QLineEdit *>(widget) || qobject_cast<QScrollBar *>(widget))
        widget->setAttribute(Qt::WA_Hover, true);
    if (auto *bar = qobject_cast<QScrollBar *>(widget))
        connect(bar, &QObject::destroyed, this, [bar] { scrollActivity().remove(bar); });
}

int FluentStyle::styleHint(StyleHint hint, const QStyleOption *option, const QWidget *widget,
                           QStyleHintReturn *returnData) const
{
    switch (hint) {
    case SH_MenuBar_AltKeyNavigation:
    case SH_MenuBar_MouseTracking:
    case SH_Menu_MouseTracking:
    case SH_Menu_AllowActiveAndDisabled:
    case SH_Menu_SloppySubMenus:
    case SH_Menu_SelectionWrap:
    case SH_ComboBox_ListMouseTracking:
    case SH_ScrollBar_Transient:
        return 1;
    case SH_Menu_SubMenuPopupDelay:
        return 300;
    case SH_ToolTip_WakeUpDelay:
        return 600;
    case SH_UnderlineShortcut:
    case SH_Menu_KeyboardSearch:
    case SH_Menu_FlashTriggeredItem:
    case SH_Menu_FadeOutOnHide:
    case SH_ComboBox_Popup:
    case SH_ScrollBar_LeftClickAbsolutePosition:
    case SH_ScrollBar_MiddleClickAbsolutePosition:
    case SH_ScrollBar_ContextMenu:
    case SH_ScrollView_FrameOnlyAroundContents:
    case SH_DialogButtonBox_ButtonsHaveIcons:
    case SH_EtchDisabledText:
    case SH_DitherDisabledText:
    case SH_ItemView_ActivateItemOnSingleClick:
        return 0;
    case SH_DialogButtonLayout:
        return QDialogButtonBox::WinLayout;
    default:
        return QProxyStyle::styleHint(hint, option, widget, returnData);
    }
}

int FluentStyle::pixelMetric(PixelMetric metric, const QStyleOption *option, const QWidget *widget) const
{
    switch (metric) {
    case PM_ScrollBarExtent:
    case PM_ScrollView_ScrollBarOverlap:
        return kScrollBarExtent;
    case PM_ScrollBarSliderMin:
        return 30;
    case PM_MenuHMargin:
        return 1;
    case PM_MenuVMargin:
        return 3;
    case PM_ScrollView_ScrollBarSpacing:
    case PM_MenuPanelWidth:
    case PM_MenuBarPanelWidth:
    case PM_MenuBarItemSpacing:
    case PM_MenuBarHMargin:
    case PM_MenuBarVMargin:
    case PM_MenuDesktopFrameWidth:
    case PM_SubMenuOverlap:
    case PM_ButtonShiftHorizontal:
    case PM_ButtonShiftVertical:
    case PM_ButtonDefaultIndicator:
        return 0;
    case PM_DefaultFrameWidth:
    case PM_ToolTipLabelFrameWidth:
        return 1;
    case PM_IndicatorWidth:
    case PM_IndicatorHeight:
    case PM_ExclusiveIndicatorWidth:
    case PM_ExclusiveIndicatorHeight:
        return 20;
    case PM_CheckBoxLabelSpacing:
    case PM_RadioButtonLabelSpacing:
        return 8;
    default:
        return QProxyStyle::pixelMetric(metric, option, widget);
    }
}

QSize FluentStyle::sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                                    const QWidget *widget) const
{
    switch (type) {
    case CT_MenuBarItem:
        if (const auto *mbi = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            const qreal advance = QFontMetricsF(mbi->font).horizontalAdvance(withoutMnemonic(mbi->text));
            return QSize(qRound(advance) + 28, kMenuBarHeight);   // 10 + text + 10, then 8 to the next item
        }
        break;
    case CT_MenuItem:
        if (const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            if (mi->menuItemType == QStyleOptionMenuItem::Separator)
                return QSize(10, kMenuSeparatorHeight);
            const MenuMetrics m = measureMenu(widget, mi->font);
            const int width = m.width > 0 ? m.width : size.width() + 2 * kMenuTextInset;
            return QSize(width - m.shortcutColumn, kMenuItemHeight);
        }
        break;
    case CT_PushButton:
        return QSize(qMax(size.width() + 24, 80), qMax(size.height() + 10, 32));
    case CT_LineEdit:
        return QSize(size.width() + 20, qMax(size.height() + 10, 32));
    case CT_ComboBox:
        return QSize(size.width() + 44, qMax(size.height() + 10, 32));
    default:
        break;
    }
    return QProxyStyle::sizeFromContents(type, option, size, widget);
}

QRect FluentStyle::subControlRect(ComplexControl control, const QStyleOptionComplex *option,
                                  SubControl subControl, const QWidget *widget) const
{
    if (control == CC_ScrollBar) {
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            const QRect r = sb->rect;
            const bool horizontal = sb->orientation == Qt::Horizontal;
            const int length = horizontal ? r.width() : r.height();
            const int button = qMin(kScrollBarExtent, length / 2);
            const int maxLength = length - 2 * button;
            int sliderLength = maxLength;
            if (sb->maximum != sb->minimum) {
                const uint range = uint(sb->maximum - sb->minimum);
                sliderLength = int((qint64(sb->pageStep) * maxLength) / (qint64(range) + sb->pageStep));
                if (sliderLength < 30 || range > uint(INT_MAX / 2))
                    sliderLength = 30;
                if (sliderLength > maxLength)
                    sliderLength = maxLength;
            }
            const int sliderStart = button + sliderPositionFromValue(sb->minimum, sb->maximum, sb->sliderPosition,
                                                                     maxLength - sliderLength, sb->upsideDown);
            auto along = [&](int pos, int len) {
                return horizontal ? QRect(r.x() + pos, r.y(), len, r.height())
                                  : QRect(r.x(), r.y() + pos, r.width(), len);
            };
            QRect ret;
            switch (subControl) {
            case SC_ScrollBarSubLine: ret = along(0, button); break;
            case SC_ScrollBarAddLine: ret = along(length - button, button); break;
            case SC_ScrollBarSubPage: ret = along(button, sliderStart - button); break;
            case SC_ScrollBarAddPage:
                ret = along(sliderStart + sliderLength, length - button - sliderStart - sliderLength);
                break;
            case SC_ScrollBarGroove: ret = along(button, maxLength); break;
            case SC_ScrollBarSlider: ret = along(sliderStart, sliderLength); break;
            default: break;
            }
            return visualRect(sb->direction, r, ret);
        }
    }
    if (control == CC_ComboBox) {
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            const QRect r = cb->rect;
            switch (subControl) {
            case SC_ComboBoxFrame: return r;
            case SC_ComboBoxArrow: return QRect(r.right() - 31, r.y(), 32, r.height());
            case SC_ComboBoxEditField: return r.adjusted(11, 1, -32, -1);
            case SC_ComboBoxListBoxPopup: return r;
            default: break;
            }
        }
    }
    return QProxyStyle::subControlRect(control, option, subControl, widget);
}

void FluentStyle::drawScrollBar(const QStyleOptionSlider *sb, QPainter *p, const QWidget *widget) const
{
    const bool horizontal = sb->orientation == Qt::Horizontal;
    const bool scrollable = (sb->state & State_Enabled) && sb->maximum > sb->minimum;
    if (!scrollable)
        return;
    const bool hover = sb->state & State_MouseOver;
    const bool sunken = sb->state & State_Sunken;
    const qint64 now = QDateTime::currentMSecsSinceEpoch();

    // State_On marks a moment of activity (scrolling); linger a little after it.
    if (widget && ((sb->state & State_On) || hover || sunken)) {
        scrollActivity()[widget].last = now;
        armScrollHide(const_cast<QWidget *>(widget));
    }
    const bool expanded = hover || sunken;
    if (!expanded && widget && now - scrollActivity().value(widget).last > kScrollBarLingerMs)
        return;

    const QRect r = sb->rect;
    const QRect slider = subControlRect(CC_ScrollBar, sb, SC_ScrollBarSlider, widget);
    QColor thumb = C().scrollThumb;
    if (expanded) {
        QColor track = C().flyout;
        track.setAlpha(235);
        fillRounded(p, QRectF(r), 6, track);
        // small arrow heads at both ends
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        p->setPen(Qt::NoPen);
        p->setBrush(thumb);
        const QPointF a = QRectF(subControlRect(CC_ScrollBar, sb, SC_ScrollBarSubLine, widget)).center();
        const QPointF b = QRectF(subControlRect(CC_ScrollBar, sb, SC_ScrollBarAddLine, widget)).center();
        if (horizontal) {
            p->drawPolygon(QPolygonF({ a + QPointF(-2.5, 0), a + QPointF(1.5, -3.5), a + QPointF(1.5, 3.5) }));
            p->drawPolygon(QPolygonF({ b + QPointF(2.5, 0), b + QPointF(-1.5, -3.5), b + QPointF(-1.5, 3.5) }));
        } else {
            p->drawPolygon(QPolygonF({ a + QPointF(0, -2.5), a + QPointF(-3.5, 1.5), a + QPointF(3.5, 1.5) }));
            p->drawPolygon(QPolygonF({ b + QPointF(0, 2.5), b + QPointF(-3.5, -1.5), b + QPointF(3.5, -1.5) }));
        }
        p->restore();
        const QRectF t = horizontal ? QRectF(slider.x(), r.center().y() - 2.5, slider.width(), 6)
                                    : QRectF(r.center().x() - 2.5, slider.y(), 6, slider.height());
        fillRounded(p, t, 3, thumb);
    } else {
        thumb.setAlpha(190);
        const QRectF t = horizontal ? QRectF(slider.x(), r.bottom() - 4, slider.width(), 2)
                                    : QRectF(r.right() - 4, slider.y(), 2, slider.height());
        fillRounded(p, t, 1, thumb);
    }
}

void FluentStyle::drawMenuItem(const QStyleOptionMenuItem *mi, QPainter *p, const QWidget *widget) const
{
    const QRect r = mi->rect;
    const bool enabled = mi->state & State_Enabled;
    const bool selected = mi->state & State_Selected;

    if (mi->menuItemType == QStyleOptionMenuItem::Separator) {
        const QColor line = C().dark ? QColor(255, 255, 255, 21) : QColor(0, 0, 0, 15);
        p->fillRect(QRect(r.x() - 1, r.y() + r.height() / 2, r.width() + 2, 1), line);
        return;
    }

    if (selected && enabled)
        fillRounded(p, QRectF(r).adjusted(4, 2, -4, -2), 4, C().subtleHover);

    const MenuMetrics m = measureMenu(widget, mi->font);
    const QColor textColor = enabled ? C().text : C().textDisabled;
    int textLeft = r.x() + kMenuTextInset;
    if (m.checkable) {
        if (mi->checked)
            Theme::drawIcon(p, Theme::Icon::Check, QPointF(textLeft + 6.5, r.center().y() + 1), 10, textColor);
        textLeft += kMenuCheckColumn;
    }

    QString label = mi->text;
    QString shortcut;
    const qsizetype tab = label.indexOf(u'\t');
    if (tab >= 0) {
        shortcut = label.mid(tab + 1);
        label.truncate(tab);
    }
    const int flags = Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextDontClip | Qt::TextShowMnemonic
                      | Qt::TextHideMnemonic;
    p->save();
    p->setFont(mi->font);
    p->setPen(textColor);
    p->drawText(QRect(textLeft, r.y(), r.width(), r.height()), flags, label);
    if (!shortcut.isEmpty()) {
        p->setFont(shortcutFont(mi->font));
        p->setPen(enabled ? C().textSecondary : C().textDisabled);
        p->drawText(QRect(r.x(), r.y(), r.width() - kMenuTextInset, r.height()),
                    Qt::AlignRight | Qt::AlignVCenter | Qt::TextSingleLine, shortcut);
    }
    p->restore();

    if (mi->menuItemType == QStyleOptionMenuItem::SubMenu) {
        Theme::drawIcon(p, Theme::Icon::ChevronRight, QPointF(r.right() - kMenuTextInset - 3, r.center().y() + 0.5), 8,
                        enabled ? C().textSecondary : C().textDisabled);
    }
}

void FluentStyle::drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *p,
                                const QWidget *widget) const
{
    const QRect r = option->rect;
    const bool enabled = option->state & State_Enabled;
    const bool hover = enabled && (option->state & State_MouseOver);
    const bool sunken = enabled && (option->state & State_Sunken);

    switch (element) {
    case PE_PanelMenu: {
        const QMargins margins = widget ? widget->contentsMargins() : QMargins();
        const QRectF panel = QRectF(r.marginsRemoved(margins));
        if (!margins.isNull())
            Theme::drawShadow(p, panel, 8, kShadow - 2);
        fillRounded(p, panel, 8, C().flyout, C().flyoutBorder);
        return;
    }
    case PE_FrameMenu:
    case PE_PanelMenuBar:
    case PE_FrameDefaultButton:
        return;
    case PE_PanelButtonCommand: {
        const bool accent = isAccentButton(option, widget);
        const bool down = sunken || (option->state & State_On);
        QColor fill, border;
        if (accent) {
            fill = !enabled ? C().controlFillDisabled : down ? C().accentPressed : hover ? C().accentHover : C().accent;
            border = fill;
        } else {
            fill = !enabled ? C().controlFillDisabled
                   : down ? C().controlFillPressed : hover ? C().controlFillHover : C().controlFill;
            border = C().controlBorder;
        }
        fillRounded(p, QRectF(r), 4, fill, border);
        if (!accent && enabled && !down && !C().dark) {
            // the slightly darker bottom edge of a WinUI button
            p->fillRect(QRect(r.x() + 4, r.bottom(), r.width() - 8, 1), QColor(0, 0, 0, 30));
        }
        return;
    }
    case PE_IndicatorCheckBox: {
        const bool on = option->state & (State_On | State_NoChange);
        if (on) {
            fillRounded(p, QRectF(r), 4, !enabled ? C().textDisabled : sunken ? C().accentPressed
                                                                   : hover ? C().accentHover : C().accent);
            Theme::drawIcon(p, Theme::Icon::Check, QRectF(r).center() + QPointF(0, 0.5), 10, C().onAccent);
        } else {
            fillRounded(p, QRectF(r), 4, hover ? C().subtleHover : QColor(0, 0, 0, 0),
                        enabled ? C().controlBorderStrong : C().textDisabled);
        }
        return;
    }
    case PE_IndicatorRadioButton: {
        const bool on = option->state & State_On;
        p->save();
        p->setRenderHint(QPainter::Antialiasing, true);
        const QRectF circle = QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5);
        if (on) {
            const QColor accent = !enabled ? C().textDisabled : sunken ? C().accentPressed
                                             : hover ? C().accentHover : C().accent;
            p->setPen(Qt::NoPen);
            p->setBrush(accent);
            p->drawEllipse(circle);
            p->setBrush(C().onAccent);
            const qreal dot = sunken ? 3.5 : hover ? 5.5 : 4.5;
            p->drawEllipse(circle.center(), dot, dot);
        } else {
            p->setPen(QPen(enabled ? C().controlBorderStrong : C().textDisabled, 1));
            p->setBrush(hover ? C().subtleHover : QColor(0, 0, 0, 0));
            p->drawEllipse(circle);
        }
        p->restore();
        return;
    }
    case PE_PanelLineEdit:
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            if (frame->lineWidth <= 0)
                return;
            const bool focus = option->state & State_HasFocus;
            const QColor fill = !enabled ? C().controlFillDisabled
                                : focus ? C().inputFocusFill : hover ? C().controlFillHover : C().inputFill;
            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);
            QPainterPath shape;
            shape.addRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
            p->fillPath(shape, fill);
            p->setPen(QPen(C().controlBorder, 1));
            p->drawPath(shape);
            // the bottom edge carries the state: grey at rest, accent with focus
            p->setClipPath(shape);
            if (focus)
                p->fillRect(QRect(r.x(), r.bottom() - 1, r.width(), 2), C().accent);
            else
                p->fillRect(QRect(r.x(), r.bottom(), r.width(), 1), C().controlBorderStrong);
            p->restore();
            return;
        }
        break;
    case PE_FrameLineEdit:
        return;
    case PE_Frame:
        if (const auto *frame = qstyleoption_cast<const QStyleOptionFrame *>(option)) {
            if (frame->lineWidth > 0) {
                p->save();
                p->setPen(QPen(C().controlBorder, 1));
                p->setBrush(Qt::NoBrush);
                p->drawRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5));
                p->restore();
            }
            return;
        }
        break;
    case PE_FrameFocusRect:
        if (option->state & State_KeyboardFocusChange) {
            p->save();
            p->setRenderHint(QPainter::Antialiasing, true);
            p->setPen(QPen(C().text, 2));
            p->setBrush(Qt::NoBrush);
            p->drawRoundedRect(QRectF(r).adjusted(-2, -2, 2, 2), 5, 5);
            p->restore();
        }
        return;
    case PE_PanelTipLabel:
        fillRounded(p, QRectF(r), 4, C().flyout, C().flyoutBorder);
        return;
    default:
        break;
    }
    QProxyStyle::drawPrimitive(element, option, p, widget);
}

void FluentStyle::drawControl(ControlElement element, const QStyleOption *option, QPainter *p,
                              const QWidget *widget) const
{
    switch (element) {
    case CE_MenuBarEmptyArea:
        p->fillRect(option->rect, C().layer);
        return;
    case CE_MenuBarItem:
        if (const auto *mbi = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            const QRect r = mbi->rect;
            p->fillRect(r, C().layer);
            if ((mbi->state & State_Enabled) && (mbi->state & State_Selected)) {
                fillRounded(p, QRectF(r).adjusted(0, 4, -8, -4), 4,
                            (mbi->state & State_Sunken) ? C().subtlePressed : C().subtleHover);
            }
            p->save();
            p->setFont(mbi->font);
            p->setPen((mbi->state & State_Enabled) ? C().text : C().textDisabled);
            p->drawText(r.adjusted(10, 0, 0, -1), Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine
                                                      | Qt::TextShowMnemonic | Qt::TextHideMnemonic, mbi->text);
            p->restore();
            return;
        }
        break;
    case CE_MenuItem:
        if (const auto *mi = qstyleoption_cast<const QStyleOptionMenuItem *>(option)) {
            drawMenuItem(mi, p, widget);
            return;
        }
        break;
    case CE_MenuEmptyArea:
        return;
    case CE_PushButtonLabel:
        if (const auto *btn = qstyleoption_cast<const QStyleOptionButton *>(option)) {
            QStyleOptionButton copy(*btn);
            const bool enabled = btn->state & State_Enabled;
            copy.palette.setColor(QPalette::ButtonText, !enabled ? C().textDisabled
                                                        : isAccentButton(option, widget) ? C().onAccent : C().text);
            QProxyStyle::drawControl(element, &copy, p, widget);
            return;
        }
        break;
    case CE_ComboBoxLabel:
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            p->save();
            p->setPen((cb->state & State_Enabled) ? C().text : C().textDisabled);
            p->drawText(subControlRect(CC_ComboBox, cb, SC_ComboBoxEditField, widget),
                        Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine, cb->currentText);
            p->restore();
            return;
        }
        break;
    default:
        break;
    }
    QProxyStyle::drawControl(element, option, p, widget);
}

void FluentStyle::drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *p,
                                     const QWidget *widget) const
{
    switch (control) {
    case CC_ScrollBar:
        if (const auto *sb = qstyleoption_cast<const QStyleOptionSlider *>(option)) {
            drawScrollBar(sb, p, widget);
            return;
        }
        break;
    case CC_ComboBox:
        if (const auto *cb = qstyleoption_cast<const QStyleOptionComboBox *>(option)) {
            const bool enabled = cb->state & State_Enabled;
            const bool hover = enabled && (cb->state & State_MouseOver);
            const bool down = enabled && (cb->state & (State_Sunken | State_On));
            const QColor fill = !enabled ? C().controlFillDisabled
                                : down ? C().controlFillPressed : hover ? C().controlFillHover : C().controlFill;
            fillRounded(p, QRectF(cb->rect), 4, fill, C().controlBorder);
            Theme::drawIcon(p, Theme::Icon::ChevronDown, QPointF(cb->rect.right() - 15.5, cb->rect.center().y() + 1), 8,
                            enabled ? C().textSecondary : C().textDisabled);
            return;
        }
        break;
    default:
        break;
    }
    QProxyStyle::drawComplexControl(control, option, p, widget);
}
