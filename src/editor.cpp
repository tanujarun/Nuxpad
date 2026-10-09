#include "editor.h"
#include "style.h"
#include "theme.h"

#include <QContextMenuEvent>
#include <QFileInfo>
#include <QMimeData>
#include <QScrollBar>
#include <QTextBlock>
#include <QUrl>

namespace {
// The text starts 16 pixels from the left and top edges of the text area.
constexpr int kDocumentMargin = 4;
constexpr int kPadding = 12;
}

Editor::Editor(QWidget *parent)
    : QPlainTextEdit(parent)
{
    setFrameShape(QFrame::NoFrame);
    setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setHorizontalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    setLineWrapMode(NoWrap);
    setWordWrapMode(QTextOption::WrapAtWordBoundaryOrAnywhere);
    setCursorWidth(1);
    setCenterOnScroll(false);
    setAcceptDrops(true);
    document()->setDocumentMargin(kDocumentMargin);
    setViewportMargins(kPadding, kPadding, 0, 0);
    setAutoFillBackground(true);
    applyTheme();
}

void Editor::applyTheme()
{
    const Theme::Colors &c = Theme::colors();
    QPalette pal = palette();
    for (QPalette::ColorGroup group : { QPalette::Active, QPalette::Inactive, QPalette::Disabled }) {
        pal.setColor(group, QPalette::Base, c.content);
        pal.setColor(group, QPalette::Window, c.content);
        pal.setColor(group, QPalette::Text, c.text);
        pal.setColor(group, QPalette::Highlight, c.selection);
        pal.setColor(group, QPalette::HighlightedText, c.selectionText);
    }
    setPalette(pal);
    viewport()->setPalette(pal);
}

void Editor::setWordWrap(bool wrap)
{
    setLineWrapMode(wrap ? WidgetWidth : NoWrap);
}

void Editor::applyFont(const QFont &font)
{
    setFont(font);
    setTabStopDistance(8 * QFontMetricsF(font).horizontalAdvance(u' '));
}

int Editor::currentLine() const
{
    return textCursor().blockNumber() + 1;
}

int Editor::currentColumn() const
{
    return textCursor().positionInBlock() + 1;
}

void Editor::deleteSelection()
{
    textCursor().removeSelectedText();
}

void Editor::goToLine(int line)
{
    const QTextBlock block = document()->findBlockByNumber(line - 1);
    if (!block.isValid())
        return;
    setTextCursor(QTextCursor(block));
    ensureCursorVisible();
}

void Editor::keyPressEvent(QKeyEvent *event)
{
    // Keys Qt binds on Linux that the Windows edit control does not have.
    const bool plainInsert = event->key() == Qt::Key_Insert
                             && !(event->modifiers() & (Qt::ShiftModifier | Qt::ControlModifier));
    if (plainInsert || event->matches(QKeySequence::DeleteEndOfLine)
        || event->matches(QKeySequence::DeleteCompleteLine)) {
        event->ignore();
        return;
    }
    QPlainTextEdit::keyPressEvent(event);
}

void Editor::wheelEvent(QWheelEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        const int delta = event->angleDelta().y();
        if (delta != 0)
            emit zoomRequested(delta > 0 ? 1 : -1);
        event->accept();
        return;
    }
    QPlainTextEdit::wheelEvent(event);
}

void Editor::mousePressEvent(QMouseEvent *event)
{
    // A click inside the selection moves the caret instead of starting a drag.
    if (event->button() == Qt::LeftButton && !(event->modifiers() & Qt::ShiftModifier)
        && textCursor().hasSelection()) {
        const int pos = cursorForPosition(event->pos()).position();
        QTextCursor cursor = textCursor();
        if (pos >= cursor.selectionStart() && pos <= cursor.selectionEnd()) {
            cursor.setPosition(pos);
            setTextCursor(cursor);
        }
    }
    QPlainTextEdit::mousePressEvent(event);
}

void Editor::contextMenuEvent(QContextMenuEvent *event)
{
    FluentMenu menu(QString(), this);
    const bool hasSelection = textCursor().hasSelection();

    menu.addAction(tr("&Undo\tCtrl+Z"), this, &QPlainTextEdit::undo)->setEnabled(document()->isUndoAvailable());
    menu.addSeparator();
    menu.addAction(tr("Cu&t\tCtrl+X"), this, &QPlainTextEdit::cut)->setEnabled(hasSelection);
    menu.addAction(tr("&Copy\tCtrl+C"), this, &QPlainTextEdit::copy)->setEnabled(hasSelection);
    menu.addAction(tr("&Paste\tCtrl+V"), this, &QPlainTextEdit::paste)->setEnabled(canPaste());
    menu.addAction(tr("&Delete\tDel"), this, &Editor::deleteSelection)->setEnabled(hasSelection);
    menu.addSeparator();
    menu.addAction(tr("Select &all\tCtrl+A"), this, &QPlainTextEdit::selectAll)->setEnabled(!document()->isEmpty());
    menu.addSeparator();

    QAction *rtl = menu.addAction(tr("&Right-to-left reading order"));
    rtl->setCheckable(true);
    rtl->setChecked(document()->defaultTextOption().textDirection() == Qt::RightToLeft);
    connect(rtl, &QAction::toggled, this, [this](bool on) {
        QTextOption option = document()->defaultTextOption();
        option.setTextDirection(on ? Qt::RightToLeft : Qt::LeftToRight);
        document()->setDefaultTextOption(option);
    });

    FluentMenu *insert = menu.addFluentMenu(tr("&Insert Unicode control character"));
    struct ControlChar { const char *name; char16_t code; };
    static const ControlChar chars[] = {
        { "LRM  Left-to-right mark", 0x200E }, { "RLM  Right-to-left mark", 0x200F },
        { "ZWJ  Zero width joiner", 0x200D }, { "ZWNJ  Zero width non-joiner", 0x200C },
        { "LRE  Start of left-to-right embedding", 0x202A }, { "RLE  Start of right-to-left embedding", 0x202B },
        { "LRO  Start of left-to-right override", 0x202D }, { "RLO  Start of right-to-left override", 0x202E },
        { "PDF  Pop directional formatting", 0x202C }, { "NADS  National digit shapes substitution", 0x206E },
        { "NODS  Nominal (European) digit shapes", 0x206F }, { "ASS  Activate symmetric swapping", 0x206B },
        { "ISS  Inhibit symmetric swapping", 0x206A }, { "AAFS  Activate Arabic form shaping", 0x206D },
        { "IAFS  Inhibit Arabic form shaping", 0x206C }, { "RS  Record Separator (Block separator)", 0x001E },
        { "US  Unit Separator (Segment separator)", 0x001F },
    };
    for (const ControlChar &c : chars) {
        const QChar ch(c.code);
        insert->addAction(tr(c.name), this, [this, ch] { insertPlainText(QString(ch)); });
    }

    menu.addSeparator();
    menu.addAction(tr("&Search with Bing\tCtrl+E"), this, &Editor::searchWithBingRequested)->setEnabled(hasSelection);

    menu.exec(event->globalPos());
}

void Editor::dragEnterEvent(QDragEnterEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void Editor::dragMoveEvent(QDragMoveEvent *event)
{
    if (event->mimeData()->hasUrls())
        event->acceptProposedAction();
    else
        event->ignore();
}

void Editor::dropEvent(QDropEvent *event)
{
    const QList<QUrl> urls = event->mimeData()->urls();
    bool any = false;
    for (const QUrl &url : urls) {
        if (url.isLocalFile() && QFileInfo(url.toLocalFile()).isFile()) {
            any = true;
            emit fileDropped(url.toLocalFile());
        }
    }
    if (any)
        event->acceptProposedAction();
    else
        event->ignore();
}

bool Editor::canInsertFromMimeData(const QMimeData *source) const
{
    return source && source->hasText();
}

void Editor::insertFromMimeData(const QMimeData *source)
{
    if (source && source->hasText())
        insertPlainText(source->text());
}
