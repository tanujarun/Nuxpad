#include "widgets.h"

#include <QAction>
#include <QEventLoop>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QShortcut>
#include <QVBoxLayout>

namespace {
const Theme::Colors &C() { return Theme::colors(); }
}

// ---------------------------------------------------------------------------
// IconButton
// ---------------------------------------------------------------------------

IconButton::IconButton(Theme::Icon icon, QWidget *parent, const QSize &size, qreal iconSize)
    : QAbstractButton(parent), m_icon(icon), m_size(size), m_iconSize(iconSize)
{
    setFixedSize(size);
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::ArrowCursor);
}

void IconButton::setIconKind(Theme::Icon icon)
{
    m_icon = icon;
    update();
}

void IconButton::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void IconButton::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void IconButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    if (isEnabled() && (isDown() || m_hover || isChecked())) {
        p.setPen(Qt::NoPen);
        p.setBrush(isDown() ? C().subtlePressed : C().subtleHover);
        p.drawRoundedRect(QRectF(rect()), 4, 4);
    }
    Theme::drawIcon(&p, m_icon, QRectF(rect()).center(), m_iconSize, isEnabled() ? C().text : C().textDisabled);
}

// ---------------------------------------------------------------------------
// ToggleSwitch
// ---------------------------------------------------------------------------

ToggleSwitch::ToggleSwitch(QWidget *parent)
    : QAbstractButton(parent)
{
    setCheckable(true);
    setFixedSize(40, 20);
    setCursor(Qt::ArrowCursor);
}

void ToggleSwitch::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void ToggleSwitch::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void ToggleSwitch::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF track = QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5);
    const qreal knob = m_hover ? 7 : 6;
    if (isChecked()) {
        p.setPen(Qt::NoPen);
        p.setBrush(m_hover ? C().accentHover : C().accent);
        p.drawRoundedRect(track, 10, 10);
        p.setBrush(C().onAccent);
        p.drawEllipse(QPointF(width() - 10, height() / 2.0), knob, knob);
    } else {
        p.setPen(QPen(C().controlBorderStrong, 1));
        p.setBrush(m_hover ? C().subtleHover : QColor(0, 0, 0, 0));
        p.drawRoundedRect(track, 10, 10);
        p.setPen(Qt::NoPen);
        p.setBrush(C().textSecondary);
        p.drawEllipse(QPointF(10, height() / 2.0), knob, knob);
    }
}

// ---------------------------------------------------------------------------
// ContentDialog
// ---------------------------------------------------------------------------

ContentDialog::ContentDialog(QWidget *host, const QString &title)
    : QWidget(host), m_host(host)
{
    setFocusPolicy(Qt::StrongFocus);
    setGeometry(host->rect());
    host->installEventFilter(this);

    m_card = new QWidget(this);
    auto *cardLayout = new QVBoxLayout(m_card);
    cardLayout->setContentsMargins(0, 0, 0, 0);
    cardLayout->setSpacing(0);

    auto *body = new QWidget(m_card);
    m_bodyLayout = new QVBoxLayout(body);
    m_bodyLayout->setContentsMargins(24, 22, 24, 24);
    m_bodyLayout->setSpacing(12);
    auto *titleLabel = new QLabel(title, body);
    titleLabel->setFont(Theme::uiFont(20, QFont::DemiBold));
    titleLabel->setWordWrap(true);
    m_bodyLayout->addWidget(titleLabel);
    cardLayout->addWidget(body);

    m_footer = new QWidget(m_card);
    m_buttonLayout = new QHBoxLayout(m_footer);
    m_buttonLayout->setContentsMargins(24, 24, 24, 24);
    m_buttonLayout->setSpacing(8);
    cardLayout->addWidget(m_footer);
    hide();
}

void ContentDialog::setText(const QString &text)
{
    if (!m_text) {
        m_text = new QLabel(m_card);
        m_text->setWordWrap(true);
        m_text->setFont(Theme::uiFont(14));
        m_text->setTextInteractionFlags(Qt::NoTextInteraction);
        m_bodyLayout->addWidget(m_text);
    }
    m_text->setText(text);
}

void ContentDialog::setContent(QWidget *content)
{
    content->setParent(m_card);
    m_bodyLayout->addWidget(content);
}

QPushButton *ContentDialog::addButton(const QString &text, int result, bool accent)
{
    auto *button = new QPushButton(text, m_footer);
    button->setFont(Theme::uiFont(14));
    button->setProperty("accent", accent);
    button->setAutoDefault(false);
    button->setMinimumHeight(32);
    m_buttonLayout->addWidget(button, 1);
    m_buttons.append(button);
    connect(button, &QPushButton::clicked, this, [this, result] { done(result); });
    return button;
}

void ContentDialog::done(int result)
{
    m_result = result;
    if (m_loop)
        m_loop->quit();
}

void ContentDialog::layoutCard()
{
    setGeometry(m_host->rect());
    const int maxWidth = qMax(200, qMin(548, width() - 32));
    const int wanted = qBound(qMin(320, maxWidth), m_card->sizeHint().width(), maxWidth);
    int height = m_card->layout()->hasHeightForWidth() ? m_card->layout()->heightForWidth(wanted)
                                                         : m_card->sizeHint().height();
    height = qMin(height, this->height() - 16);
    m_card->setGeometry((width() - wanted) / 2, qMax(8, (this->height() - height) / 2), wanted, height);
}

int ContentDialog::exec()
{
    // Nothing behind the dialog may react to its accelerators meanwhile.
    QList<QPointer<QAction>> actions;
    for (QAction *a : m_host->window()->findChildren<QAction *>()) {
        if (a->isEnabled() && !a->shortcut().isEmpty()) {
            a->setEnabled(false);
            actions.append(a);
        }
    }
    QList<QPointer<QShortcut>> shortcuts;
    for (QShortcut *s : m_host->window()->findChildren<QShortcut *>()) {
        if (s->isEnabled()) {
            s->setEnabled(false);
            shortcuts.append(s);
        }
    }
    QPointer<QWidget> previousFocus = m_host->window()->focusWidget();

    layoutCard();
    show();
    raise();
    if (m_initialFocus)
        m_initialFocus->setFocus();
    else if (!m_buttons.isEmpty())
        m_buttons.constFirst()->setFocus();
    else
        setFocus();

    QEventLoop loop;
    m_loop = &loop;
    QPointer<ContentDialog> guard(this);
    loop.exec();
    if (!guard)
        return m_cancelResult;
    m_loop = nullptr;
    hide();

    for (const auto &a : std::as_const(actions)) {
        if (a)
            a->setEnabled(true);
    }
    for (const auto &s : std::as_const(shortcuts)) {
        if (s)
            s->setEnabled(true);
    }
    if (previousFocus)
        previousFocus->setFocus();
    return m_result;
}

void ContentDialog::message(QWidget *host, const QString &title, const QString &text)
{
    ask(host, title, text, { tr("OK") }, 0, 0);
}

int ContentDialog::ask(QWidget *host, const QString &title, const QString &text, const QStringList &buttons,
                       int accentIndex, int cancelIndex)
{
    ContentDialog dialog(host, title);
    dialog.setText(text);
    for (int i = 0; i < buttons.size(); ++i)
        dialog.addButton(buttons.at(i), i, i == accentIndex);
    dialog.setCancelResult(cancelIndex);
    return dialog.exec();
}

bool ContentDialog::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == m_host && event->type() == QEvent::Resize && isVisible())
        layoutCard();
    if (watched == m_host && event->type() == QEvent::Close && m_loop) {
        // the window is going away underneath us: treat as cancel
        done(m_cancelResult);
    }
    return QWidget::eventFilter(watched, event);
}

void ContentDialog::resizeEvent(QResizeEvent *)
{
    if (isVisible())
        layoutCard();
}

void ContentDialog::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape) {
        done(m_cancelResult);
        return;
    }
    if (event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) {
        if (auto *focused = qobject_cast<QPushButton *>(focusWidget())) {
            focused->click();
            return;
        }
        for (QPushButton *b : std::as_const(m_buttons)) {
            if (b->property("accent").toBool()) {
                b->click();
                return;
            }
        }
    }
    event->accept();   // keys never fall through to the window behind
}

bool ContentDialog::focusNextPrevChild(bool next)
{
    QList<QWidget *> chain;
    for (QWidget *w : m_card->findChildren<QWidget *>()) {
        if (w->isVisibleTo(this) && w->isEnabled() && (w->focusPolicy() & Qt::TabFocus))
            chain.append(w);
    }
    if (chain.isEmpty())
        return true;
    int index = chain.indexOf(QWidget::focusWidget());
    index = index < 0 ? 0 : (index + (next ? 1 : chain.size() - 1)) % chain.size();
    chain.at(index)->setFocus(next ? Qt::TabFocusReason : Qt::BacktabFocusReason);
    return true;
}

void ContentDialog::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), C().smoke);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF card(m_card->geometry());
    Theme::drawShadow(&p, card, 8, 24);
    QPainterPath shape;
    shape.addRoundedRect(card, 8, 8);
    p.save();
    p.setClipPath(shape);
    p.fillRect(card, C().dialogBody);
    const QRectF footer(card.x(), card.y() + m_footer->y(), card.width(), m_footer->height());
    p.fillRect(footer, C().dialogFooter);
    p.fillRect(QRectF(footer.x(), footer.y(), footer.width(), 1), C().dark ? QColor(0, 0, 0, 60) : QColor(0, 0, 0, 20));
    p.restore();
    p.setPen(QPen(C().dialogBorder, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
}
