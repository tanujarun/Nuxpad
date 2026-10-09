#include "tabbar.h"
#include "theme.h"

#include <QContextMenuEvent>
#include <QHelpEvent>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QToolTip>

namespace {
constexpr int kLeft = 8;
constexpr int kTop = 9;
constexpr int kTabHeight = 32;
constexpr int kMaxTabWidth = 240;
constexpr int kMinTabWidth = 56;
const Theme::Colors &C() { return Theme::colors(); }
}

TabBar::TabBar(QWidget *parent)
    : QWidget(parent)
{
    setFixedHeight(Height);
    setMouseTracking(true);
    setAttribute(Qt::WA_OpaquePaintEvent, true);
}

int TabBar::addTab(const QString &title)
{
    m_tabs.append(Tab{ title, QString(), false });
    update();
    return m_tabs.size() - 1;
}

void TabBar::removeTab(int index)
{
    if (index < 0 || index >= m_tabs.size())
        return;
    m_tabs.removeAt(index);
    if (m_current > index || m_current >= m_tabs.size())
        --m_current;
    m_hoverPart = Nothing;
    m_hoverIndex = -1;
    update();
}

void TabBar::setCurrentIndex(int index)
{
    if (index < 0 || index >= m_tabs.size() || index == m_current)
        return;
    m_current = index;
    update();
    emit currentChanged(index);
}

void TabBar::setTabTitle(int index, const QString &title)
{
    if (index >= 0 && index < m_tabs.size() && m_tabs[index].title != title) {
        m_tabs[index].title = title;
        update();
    }
}

void TabBar::setTabToolTip(int index, const QString &toolTip)
{
    if (index >= 0 && index < m_tabs.size())
        m_tabs[index].toolTip = toolTip;
}

void TabBar::setTabModified(int index, bool modified)
{
    if (index >= 0 && index < m_tabs.size() && m_tabs[index].modified != modified) {
        m_tabs[index].modified = modified;
        update();
    }
}

int TabBar::tabWidth() const
{
    if (m_tabs.isEmpty())
        return kMaxTabWidth;
    const int available = width() - kLeft - 9 - 32 - 8;   // room for the add button
    return qBound(kMinTabWidth, available / int(m_tabs.size()), kMaxTabWidth);
}

QRect TabBar::tabRect(int index) const
{
    const int w = tabWidth();
    return QRect(kLeft + index * w, kTop, w, kTabHeight);
}

QRect TabBar::closeRect(int index) const
{
    const QRect r = tabRect(index);
    return QRect(r.right() - 33, r.y() + 4, 24, 24);
}

QRect TabBar::addRect() const
{
    const int right = kLeft + int(m_tabs.size()) * tabWidth();
    return QRect(right + 9, kTop + 2, 32, 28);
}

TabBar::Part TabBar::hitTest(const QPoint &pos, int *index) const
{
    *index = -1;
    if (addRect().contains(pos))
        return AddButton;
    for (int i = 0; i < m_tabs.size(); ++i) {
        if (tabRect(i).contains(pos)) {
            *index = i;
            return closeRect(i).contains(pos) ? CloseButton : Body;
        }
    }
    return Nothing;
}

void TabBar::setHover(Part part, int index)
{
    if (part != m_hoverPart || index != m_hoverIndex) {
        m_hoverPart = part;
        m_hoverIndex = index;
        update();
    }
}

void TabBar::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), C().mica);
    p.setRenderHint(QPainter::Antialiasing, true);

    const QFont regular = Theme::uiFont(12);
    const QFont strong = Theme::uiFont(12, QFont::DemiBold);

    for (int i = 0; i < m_tabs.size(); ++i) {
        const Tab &tab = m_tabs.at(i);
        const QRect r = tabRect(i);
        const bool selected = i == m_current;
        const bool hovered = m_hoverIndex == i && m_hoverPart != AddButton;

        if (selected) {
            const qreal x0 = r.left() + 0.5, x1 = r.right() + 0.5, y0 = r.top() + 0.5, y1 = Height;
            QPainterPath shape;
            shape.moveTo(x0 - 4, y1);
            shape.quadTo(x0, y1, x0, y1 - 4);
            shape.lineTo(x0, y0 + 8);
            shape.quadTo(x0, y0, x0 + 8, y0);
            shape.lineTo(x1 - 8, y0);
            shape.quadTo(x1, y0, x1, y0 + 8);
            shape.lineTo(x1, y1 - 4);
            shape.quadTo(x1, y1, x1 + 4, y1);
            p.setPen(QPen(C().tabOutline, 1));
            p.setBrush(C().layer);
            p.drawPath(shape);
            p.fillRect(QRectF(x0 - 4, y1 - 0.6, x1 - x0 + 8, 1), C().layer);   // no outline along the bottom
        } else {
            if (hovered) {
                QPainterPath shape;
                shape.addRoundedRect(QRectF(r).adjusted(1, 1, -1, -3), 6, 6);
                p.fillPath(shape, C().subtleHover);
            } else if (i + 1 < m_tabs.size() && i + 1 != m_current && m_hoverIndex != i + 1) {
                // hairline between two resting tabs
                p.fillRect(QRect(r.right(), r.y() + 8, 1, 16), C().dark ? QColor(255, 255, 255, 28) : QColor(0, 0, 0, 28));
            }
        }

        // close button, or the dot of an unsaved document
        const QRect cr = closeRect(i);
        const bool closeHot = m_hoverIndex == i && m_hoverPart == CloseButton;
        int textRight = r.right() - 9;
        if (closeHot) {
            p.setPen(Qt::NoPen);
            p.setBrush(m_pressPart == CloseButton && m_pressIndex == i ? C().subtlePressed : C().subtleHover);
            p.drawRoundedRect(QRectF(cr), 4, 4);
            Theme::drawIcon(&p, Theme::Icon::Close, QRectF(cr).center(), 8, C().text);
        } else if (tab.modified && !hovered) {
            p.setPen(Qt::NoPen);
            p.setBrush(selected ? C().text : C().textSecondary);
            p.drawEllipse(QRectF(cr).center(), 2.5, 2.5);
        } else if (selected || hovered) {
            Theme::drawIcon(&p, Theme::Icon::Close, QRectF(cr).center(), 8, selected ? C().text : C().textSecondary);
        }
        if (selected || hovered || tab.modified)
            textRight = cr.left() - 4;

        p.setFont(selected ? strong : regular);
        p.setPen(selected ? C().text : C().textSecondary);
        const QRect textRect(r.left() + 9, r.top(), qMax(0, textRight - r.left() - 9), r.height());
        p.drawText(textRect, Qt::AlignLeft | Qt::AlignVCenter | Qt::TextSingleLine,
                   p.fontMetrics().elidedText(tab.title, Qt::ElideRight, textRect.width()));
    }

    const QRect ar = addRect();
    if (m_hoverPart == AddButton) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_pressPart == AddButton ? C().subtlePressed : C().subtleHover);
        p.drawRoundedRect(QRectF(ar), 4, 4);
    }
    Theme::drawIcon(&p, Theme::Icon::Add, QRectF(ar).center(), 11, C().textSecondary);
}

void TabBar::mousePressEvent(QMouseEvent *event)
{
    int index = -1;
    const Part part = hitTest(event->pos(), &index);
    if (event->button() == Qt::MiddleButton) {
        m_pressPart = part == Nothing || part == AddButton ? Nothing : CloseButton;
        m_pressIndex = index;
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    m_pressPart = part;
    m_pressIndex = index;
    m_pressPos = event->pos();
    m_dragging = false;
    if (part == Body)
        setCurrentIndex(index);
    update();
}

void TabBar::mouseMoveEvent(QMouseEvent *event)
{
    if ((event->buttons() & Qt::LeftButton) && m_pressPart == Body && m_pressIndex >= 0) {
        if (!m_dragging && (event->pos() - m_pressPos).manhattanLength() > 10)
            m_dragging = true;
        if (m_dragging) {
            const int target = qBound(0, (event->pos().x() - kLeft) / tabWidth(), int(m_tabs.size()) - 1);
            if (target != m_pressIndex) {
                const int from = m_pressIndex;
                m_tabs.move(from, target);
                m_current = target;
                m_pressIndex = target;
                update();
                emit tabMoved(from, target);
            }
        }
        return;
    }
    int index = -1;
    const Part part = hitTest(event->pos(), &index);
    setHover(part, index);
}

void TabBar::mouseReleaseEvent(QMouseEvent *event)
{
    int index = -1;
    const Part part = hitTest(event->pos(), &index);
    const Part pressed = m_pressPart;
    const int pressedIndex = m_pressIndex;
    m_pressPart = Nothing;
    m_pressIndex = -1;
    m_dragging = false;
    update();
    if (event->button() == Qt::MiddleButton) {
        if (pressed == CloseButton && index == pressedIndex && index >= 0)
            emit tabCloseRequested(index);
        return;
    }
    if (event->button() != Qt::LeftButton)
        return;
    if (pressed == CloseButton && part == CloseButton && index == pressedIndex)
        emit tabCloseRequested(index);
    else if (pressed == AddButton && part == AddButton)
        emit newTabRequested();
}

void TabBar::leaveEvent(QEvent *)
{
    setHover(Nothing, -1);
}

void TabBar::contextMenuEvent(QContextMenuEvent *event)
{
    int index = -1;
    const Part part = hitTest(event->pos(), &index);
    if (part == Body || part == CloseButton)
        emit contextMenuRequested(index, event->globalPos());
}

bool TabBar::event(QEvent *event)
{
    if (event->type() == QEvent::ToolTip) {
        auto *help = static_cast<QHelpEvent *>(event);
        int index = -1;
        const Part part = hitTest(help->pos(), &index);
        if (part == AddButton)
            QToolTip::showText(help->globalPos(), tr("New tab (Ctrl+N)"), this);
        else if (part == CloseButton)
            QToolTip::showText(help->globalPos(), tr("Close tab (Ctrl+W)"), this);
        else if (index >= 0 && !m_tabs.at(index).toolTip.isEmpty())
            QToolTip::showText(help->globalPos(), m_tabs.at(index).toolTip, this);
        else
            QToolTip::hideText();
        return true;
    }
    return QWidget::event(event);
}
