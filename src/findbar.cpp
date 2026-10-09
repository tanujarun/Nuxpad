#include "findbar.h"
#include "style.h"
#include "theme.h"
#include "widgets.h"

#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLineEdit>
#include <QPainter>
#include <QPushButton>
#include <QVBoxLayout>

namespace {
constexpr int kShadow = 10;
constexpr int kCardWidth = 452;
}

FindBar::FindBar(QWidget *parent, SearchState *state)
    : QWidget(parent), m_state(state)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(kShadow + 8, kShadow + 8, kShadow + 8, kShadow + 8);
    outer->setSpacing(8);

    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    m_toggle = new IconButton(Theme::Icon::ChevronDown, this, QSize(28, 32), 8);
    m_toggle->setToolTip(tr("Toggle replace"));
    m_find = new QLineEdit(this);
    m_find->setPlaceholderText(tr("Find"));
    m_find->setFixedHeight(32);
    m_find->setTextMargins(6, 0, 6, 0);
    m_next = new IconButton(Theme::Icon::ArrowDown, this, QSize(32, 32), 11);
    m_next->setToolTip(tr("Find next (F3)"));
    m_previous = new IconButton(Theme::Icon::ArrowUp, this, QSize(32, 32), 11);
    m_previous->setToolTip(tr("Find previous (Shift+F3)"));
    m_options = new IconButton(Theme::Icon::Options, this, QSize(32, 32), 12);
    m_options->setToolTip(tr("Search options"));
    auto *close = new IconButton(Theme::Icon::Close, this, QSize(32, 32), 9);
    close->setToolTip(tr("Close (Esc)"));
    row->addWidget(m_toggle);
    row->addWidget(m_find, 1);
    row->addWidget(m_next);
    row->addWidget(m_previous);
    row->addWidget(m_options);
    row->addWidget(close);
    outer->addLayout(row);

    m_replaceRow = new QWidget(this);
    auto *replaceLayout = new QHBoxLayout(m_replaceRow);
    replaceLayout->setContentsMargins(32, 0, 0, 0);
    replaceLayout->setSpacing(8);
    m_replace = new QLineEdit(m_replaceRow);
    m_replace->setPlaceholderText(tr("Replace"));
    m_replace->setFixedHeight(32);
    m_replace->setTextMargins(6, 0, 6, 0);
    m_replaceButton = new QPushButton(tr("Replace"), m_replaceRow);
    m_replaceAllButton = new QPushButton(tr("Replace all"), m_replaceRow);
    for (QPushButton *b : { m_replaceButton, m_replaceAllButton }) {
        b->setFixedHeight(32);
        b->setAutoDefault(false);
        b->setFocusPolicy(Qt::TabFocus);
    }
    replaceLayout->addWidget(m_replace, 1);
    replaceLayout->addWidget(m_replaceButton);
    replaceLayout->addWidget(m_replaceAllButton);
    outer->addWidget(m_replaceRow);
    m_replaceRow->hide();

    m_find->installEventFilter(this);
    m_replace->installEventFilter(this);

    connect(m_find, &QLineEdit::textChanged, this, [this](const QString &text) {
        m_state->findText = text;
        syncButtons();
    });
    connect(m_replace, &QLineEdit::textChanged, this, [this](const QString &text) { m_state->replaceText = text; });
    connect(m_toggle, &QAbstractButton::clicked, this, [this] { setReplaceVisible(!m_replaceRow->isVisibleTo(this)); });
    connect(m_next, &QAbstractButton::clicked, this, &FindBar::findNext);
    connect(m_previous, &QAbstractButton::clicked, this, &FindBar::findPrevious);
    connect(m_options, &QAbstractButton::clicked, this, &FindBar::showOptions);
    connect(close, &QAbstractButton::clicked, this, &FindBar::closeBar);
    connect(m_replaceButton, &QPushButton::clicked, this, &FindBar::replaceOne);
    connect(m_replaceAllButton, &QPushButton::clicked, this, &FindBar::replaceAll);
    hide();
}

void FindBar::syncButtons()
{
    const bool hasText = !m_find->text().isEmpty();
    m_next->setEnabled(hasText);
    m_previous->setEnabled(hasText);
    m_replaceButton->setEnabled(hasText);
    m_replaceAllButton->setEnabled(hasText);
}

void FindBar::setReplaceVisible(bool visible)
{
    m_replaceRow->setVisible(visible);
    m_toggle->setIconKind(visible ? Theme::Icon::ChevronUp : Theme::Icon::ChevronDown);
    reposition();
}

void FindBar::reposition()
{
    if (!parentWidget())
        return;
    const int width = qMin(kCardWidth, parentWidget()->width() - 16) + 2 * kShadow;
    const int height = layout()->heightForWidth(width) > 0 ? layout()->heightForWidth(width) : sizeHint().height();
    setGeometry((parentWidget()->width() - width) / 2, 8 - kShadow, width, qMax(height, sizeHint().height()));
    raise();
}

void FindBar::present(bool withReplace, const QString &seed)
{
    if (!seed.isEmpty())
        m_state->findText = seed;
    const SearchState state = *m_state;   // the setters below write back into it
    m_find->setText(state.findText);
    m_replace->setText(state.replaceText);
    syncButtons();
    show();
    setReplaceVisible(withReplace || (isVisible() && m_replaceRow->isVisibleTo(this) && withReplace));
    m_find->setFocus();
    m_find->selectAll();
}

void FindBar::openFind(const QString &seed)
{
    present(false, seed);
}

void FindBar::openReplace(const QString &seed)
{
    present(true, seed);
}

void FindBar::closeBar()
{
    if (!isVisible())
        return;
    hide();
    emit closed();
}

void FindBar::showOptions()
{
    FluentMenu menu(QString(), this);
    QAction *matchCase = menu.addAction(tr("Match case"));
    matchCase->setCheckable(true);
    matchCase->setChecked(m_state->matchCase);
    QAction *wrap = menu.addAction(tr("Wrap around"));
    wrap->setCheckable(true);
    wrap->setChecked(m_state->wrapAround);
    QAction *chosen = menu.exec(m_options->mapToGlobal(QPoint(0, m_options->height() + 2)));
    if (chosen == matchCase)
        m_state->matchCase = matchCase->isChecked();
    else if (chosen == wrap)
        m_state->wrapAround = wrap->isChecked();
}

bool FindBar::eventFilter(QObject *watched, QEvent *event)
{
    if (event->type() == QEvent::KeyPress) {
        auto *key = static_cast<QKeyEvent *>(event);
        if (key->key() == Qt::Key_Escape) {
            closeBar();
            return true;
        }
        if (key->key() == Qt::Key_Return || key->key() == Qt::Key_Enter) {
            if (watched == m_replace)
                emit replaceOne();
            else if (key->modifiers() & Qt::ShiftModifier)
                emit findPrevious();
            else
                emit findNext();
            return true;
        }
    }
    return QWidget::eventFilter(watched, event);
}

void FindBar::paintEvent(QPaintEvent *)
{
    const Theme::Colors &c = Theme::colors();
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF card = QRectF(rect()).adjusted(kShadow, kShadow, -kShadow, -kShadow);
    Theme::drawShadow(&p, card, 8, kShadow - 2);
    p.setPen(QPen(c.flyoutBorder, 1));
    p.setBrush(c.flyout);
    p.drawRoundedRect(card.adjusted(0.5, 0.5, -0.5, -0.5), 8, 8);
}
