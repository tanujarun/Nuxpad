#include "mainwindow.h"
#include "editor.h"
#include "pagesetup.h"
#include "session.h"
#include "settingspage.h"
#include "statusbar.h"
#include "style.h"
#include "tabbar.h"
#include "widgets.h"

#include <QApplication>
#include <QClipboard>
#include <QCloseEvent>
#include <QComboBox>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QFileSystemModel>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenuBar>
#include <QPainter>
#include <QProcess>
#include <QPushButton>
#include <QRegularExpressionValidator>
#include <QShortcut>
#include <QStackedWidget>
#include <QStandardPaths>
#include <QTextBlock>
#include <QUrl>
#include <QUrlQuery>
#include <QUuid>
#include <QVBoxLayout>

namespace {

constexpr int kZoomMin = 10;
constexpr int kZoomMax = 500;

QString nativePath(const QString &path)
{
    return QDir::toNativeSeparators(path);
}

// A plain band in one of the theme's colours.
class Band : public QWidget
{
public:
    enum Kind { Layer, Divider, Content };
    Band(Kind kind, QWidget *parent) : QWidget(parent), m_kind(kind) {}

protected:
    void paintEvent(QPaintEvent *) override
    {
        const Theme::Colors &c = Theme::colors();
        QPainter p(this);
        p.fillRect(rect(), m_kind == Layer ? c.layer : m_kind == Divider ? c.divider : c.content);
    }

private:
    Kind m_kind;
};

} // namespace

MainWindow::MainWindow(bool ownsSession)
    : m_ownsSession(ownsSession)
{
    m_settings.load();
    Theme::setMode(m_settings.theme);
    setWindowIcon(Theme::appIcon());
    resize(m_settings.windowSize);

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(0, 0, 0, 0);
    m_pages = new QStackedWidget(this);
    root->addWidget(m_pages);

    auto *mainPage = new QWidget(m_pages);
    auto *layout = new QVBoxLayout(mainPage);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    m_tabBar = new TabBar(mainPage);
    layout->addWidget(m_tabBar);

    m_menuRow = new Band(Band::Layer, mainPage);
    m_menuRow->setFixedHeight(32);
    auto *menuLayout = new QHBoxLayout(m_menuRow);
    menuLayout->setContentsMargins(4, 0, 7, 0);
    menuLayout->setSpacing(0);
    m_menuBar = new QMenuBar(m_menuRow);
    m_menuBar->setNativeMenuBar(false);
    m_menuBar->setFont(Theme::uiFont(14));
    m_menuBar->setFixedHeight(32);
    m_menuBar->setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Fixed);
    menuLayout->addWidget(m_menuBar);
    menuLayout->addStretch(1);
    m_gear = new IconButton(Theme::Icon::Settings, m_menuRow, QSize(32, 24), 15);
    m_gear->setToolTip(tr("Settings"));
    menuLayout->addWidget(m_gear);
    layout->addWidget(m_menuRow);

    m_divider = new Band(Band::Divider, mainPage);
    m_divider->setFixedHeight(1);
    layout->addWidget(m_divider);

    auto *editorHost = new Band(Band::Content, mainPage);
    auto *hostLayout = new QVBoxLayout(editorHost);
    hostLayout->setContentsMargins(0, 0, 0, 0);
    m_editors = new QStackedWidget(editorHost);
    hostLayout->addWidget(m_editors);
    layout->addWidget(editorHost, 1);
    m_findBar = new FindBar(editorHost, &m_settings.search);

    m_statusBar = new StatusBar(mainPage);
    m_statusBar->setVisible(m_settings.statusBar);
    layout->addWidget(m_statusBar);

    m_pages->addWidget(mainPage);
    m_settingsPage = new SettingsPage(m_pages);
    m_pages->addWidget(m_settingsPage);

    buildMenus();

    connect(m_gear, &QAbstractButton::clicked, this, [this] { showSettings(true); });
    connect(m_tabBar, &TabBar::currentChanged, this, [this](int index) {
        if (index < 0 || index >= m_docs.size())
            return;
        m_editors->setCurrentWidget(m_docs.at(index)->editor);
        m_docs.at(index)->editor->setFocus();
        updateWindowTitle();
        updateStatus();
        updateEditActions();
        scheduleSessionSave();
    });
    connect(m_tabBar, &TabBar::tabCloseRequested, this, [this](int index) { closeTab(index); });
    connect(m_tabBar, &TabBar::newTabRequested, this, &MainWindow::fileNewTab);
    connect(m_tabBar, &TabBar::tabMoved, this, [this](int from, int to) {
        m_docs.move(from, to);
        scheduleSessionSave();
    });
    connect(m_tabBar, &TabBar::contextMenuRequested, this, [this](int index, const QPoint &pos) {
        FluentMenu menu(QString(), this);
        QAction *close = menu.addAction(tr("Close tab\tCtrl+W"));
        QAction *others = menu.addAction(tr("Close other tabs"));
        QAction *right = menu.addAction(tr("Close tabs to the right"));
        others->setEnabled(m_docs.size() > 1);
        right->setEnabled(index < m_docs.size() - 1);
        QAction *chosen = menu.exec(pos);
        if (!chosen || index >= m_docs.size())
            return;
        Document *keep = m_docs.at(index);
        if (chosen == close) {
            closeTab(index);
        } else if (chosen == others) {
            for (int i = m_docs.size() - 1; i >= 0; --i) {
                if (m_docs.at(i) != keep && !closeTab(i))
                    break;
            }
        } else if (chosen == right) {
            for (int i = m_docs.size() - 1; i > indexOf(keep); --i) {
                if (!closeTab(i))
                    break;
            }
        }
    });

    connect(m_findBar, &FindBar::findNext, this, [this] { find(false); });
    connect(m_findBar, &FindBar::findPrevious, this, [this] { find(true); });
    connect(m_findBar, &FindBar::replaceOne, this, &MainWindow::replaceOne);
    connect(m_findBar, &FindBar::replaceAll, this, &MainWindow::replaceAll);
    connect(m_findBar, &FindBar::closed, this, [this] {
        if (Editor *e = editor())
            e->setFocus();
    });

    connect(m_settingsPage, &SettingsPage::backRequested, this, [this] { showSettings(false); });
    connect(m_settingsPage, &SettingsPage::themeChanged, this, [this](Theme::Mode mode) {
        m_settings.theme = mode;
        Theme::setMode(mode);
        m_settings.save();
    });
    connect(m_settingsPage, &SettingsPage::fontChanged, this, [this](const FontChoice &font) {
        m_settings.font = font;
        m_settings.customFont = true;
        for (Document *doc : std::as_const(m_docs))
            applyEditorSettings(doc->editor);
        m_settings.save();
    });
    connect(m_settingsPage, &SettingsPage::wordWrapChanged, this, [this](bool wrap) {
        m_wordWrap->setChecked(wrap);
        setWordWrap(wrap);
    });
    connect(m_settingsPage, &SettingsPage::openInNewTabChanged, this, [this](bool newTab) {
        m_settings.openInNewTab = newTab;
        m_settings.save();
    });
    connect(m_settingsPage, &SettingsPage::continueSessionChanged, this, [this](bool on) {
        m_settings.continueSession = on;
        m_settings.save();
    });

    connect(Theme::notifier(), &Theme::Notifier::changed, this, &MainWindow::applyTheme);
    connect(QApplication::clipboard(), &QClipboard::dataChanged, this, &MainWindow::updateEditActions);

    m_sessionTimer.setSingleShot(true);
    m_sessionTimer.setInterval(800);
    connect(&m_sessionTimer, &QTimer::timeout, this, &MainWindow::saveSession);
    connect(qApp, &QCoreApplication::aboutToQuit, this, [this] {
        if (!m_closing)
            saveSession();
    });

    auto addShortcut = [this](const QKeySequence &sequence, auto slot) {
        auto *shortcut = new QShortcut(sequence, this);
        connect(shortcut, &QShortcut::activated, this, slot);
    };
    auto step = [this](int delta) {
        if (m_docs.size() > 1)
            m_tabBar->setCurrentIndex((m_tabBar->currentIndex() + delta + m_docs.size()) % m_docs.size());
    };
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_Tab), [step] { step(1); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Tab), [step] { step(-1); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageDown), [step] { step(1); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_PageUp), [step] { step(-1); });
    addShortcut(QKeySequence(Qt::CTRL | Qt::Key_T), [this] { fileNewTab(); });

    restoreSession();
    if (m_docs.isEmpty())
        createTab(true);
    updateWindowTitle();
    updateStatus();
    updateEditActions();
}

// ---------------------------------------------------------------------------
// Menus
// ---------------------------------------------------------------------------

void MainWindow::buildMenus()
{
    auto add = [this](QMenu *menu, const QString &text, auto slot, const QList<QKeySequence> &shortcuts = {}) {
        QAction *action = menu->addAction(text);
        if (!shortcuts.isEmpty())
            action->setShortcuts(shortcuts);
        connect(action, &QAction::triggered, this, slot);
        return action;
    };
    const auto key = [](QKeyCombination k) { return QList<QKeySequence>{ QKeySequence(k) }; };
    auto topMenu = [this](const QString &title) {
        auto *menu = new FluentMenu(title, m_menuBar);
        m_menuBar->addMenu(menu);
        return menu;
    };

    FluentMenu *file = topMenu(tr("&File"));
    add(file, tr("&New tab\tCtrl+N"), &MainWindow::fileNewTab, key(Qt::CTRL | Qt::Key_N));
    add(file, tr("New &window\tCtrl+Shift+N"), &MainWindow::fileNewWindow, key(Qt::CTRL | Qt::SHIFT | Qt::Key_N));
    add(file, tr("&Open\tCtrl+O"), &MainWindow::fileOpen, key(Qt::CTRL | Qt::Key_O));
    m_save = add(file, tr("&Save\tCtrl+S"), [this] { if (Document *d = current()) save(d); }, key(Qt::CTRL | Qt::Key_S));
    m_saveAs = add(file, tr("Save &as\tCtrl+Shift+S"), [this] { if (Document *d = current()) saveAs(d); },
                   key(Qt::CTRL | Qt::SHIFT | Qt::Key_S));
    m_saveAll = add(file, tr("Save a&ll\tCtrl+Alt+S"), &MainWindow::fileSaveAll, key(Qt::CTRL | Qt::ALT | Qt::Key_S));
    file->addSeparator();
    add(file, tr("Page set&up"), &MainWindow::filePageSetup);
    add(file, tr("&Print\tCtrl+P"), &MainWindow::filePrint, key(Qt::CTRL | Qt::Key_P));
    file->addSeparator();
    add(file, tr("&Close tab\tCtrl+W"), [this] { closeTab(m_tabBar->currentIndex()); }, key(Qt::CTRL | Qt::Key_W));
    add(file, tr("Close w&indow\tCtrl+Shift+W"), &QWidget::close, key(Qt::CTRL | Qt::SHIFT | Qt::Key_W));
    add(file, tr("E&xit"), &QWidget::close);

    // Undo, Cut, Copy, Paste, Del and Select all are handled by the text area
    // itself; the menu only shows their accelerators.
    FluentMenu *edit = topMenu(tr("&Edit"));
    m_undo = add(edit, tr("&Undo\tCtrl+Z"), [this] { if (Editor *e = editor()) e->undo(); });
    edit->addSeparator();
    m_cut = add(edit, tr("Cu&t\tCtrl+X"), [this] { if (Editor *e = editor()) e->cut(); });
    m_copy = add(edit, tr("&Copy\tCtrl+C"), [this] { if (Editor *e = editor()) e->copy(); });
    m_paste = add(edit, tr("&Paste\tCtrl+V"), [this] { if (Editor *e = editor()) e->paste(); });
    m_delete = add(edit, tr("&Delete\tDel"), [this] { if (Editor *e = editor()) e->deleteSelection(); });
    edit->addSeparator();
    m_bing = add(edit, tr("&Search with Bing\tCtrl+E"), &MainWindow::searchWithBing, key(Qt::CTRL | Qt::Key_E));
    edit->addSeparator();
    m_find = add(edit, tr("&Find\tCtrl+F"), &MainWindow::showFind, key(Qt::CTRL | Qt::Key_F));
    m_findNext = add(edit, tr("Find &next\tF3"), [this] { find(false); }, key(Qt::Key_F3));
    m_findPrevious = add(edit, tr("Find pre&vious\tShift+F3"), [this] { find(true); }, key(Qt::SHIFT | Qt::Key_F3));
    add(edit, tr("&Replace\tCtrl+H"), &MainWindow::showReplace, key(Qt::CTRL | Qt::Key_H));
    m_goTo = add(edit, tr("&Go to\tCtrl+G"), &MainWindow::goTo, key(Qt::CTRL | Qt::Key_G));
    edit->addSeparator();
    add(edit, tr("Select &all\tCtrl+A"), [this] { if (Editor *e = editor()) e->selectAll(); });
    add(edit, tr("Time/&Date\tF5"), &MainWindow::insertTimeDate, key(Qt::Key_F5));
    edit->addSeparator();
    add(edit, tr("F&ont"), [this] { showSettings(true); });
    connect(edit, &QMenu::aboutToShow, this, &MainWindow::updateEditActions);

    FluentMenu *view = topMenu(tr("&View"));
    FluentMenu *zoom = view->addFluentMenu(tr("&Zoom"));
    add(zoom, tr("Zoom &in\tCtrl+Plus"), [this] { zoomBy(1); },
        { QKeySequence(Qt::CTRL | Qt::Key_Plus), QKeySequence(Qt::CTRL | Qt::Key_Equal) });
    add(zoom, tr("Zoom &out\tCtrl+Minus"), [this] { zoomBy(-1); }, key(Qt::CTRL | Qt::Key_Minus));
    add(zoom, tr("&Restore default zoom\tCtrl+0"), [this] { zoomBy(0); }, key(Qt::CTRL | Qt::Key_0));
    m_statusBarAction = add(view, tr("&Status bar"), &MainWindow::setStatusBarVisible);
    m_statusBarAction->setCheckable(true);
    m_statusBarAction->setChecked(m_settings.statusBar);
    m_wordWrap = add(view, tr("&Word wrap"), &MainWindow::setWordWrap);
    m_wordWrap->setCheckable(true);
    m_wordWrap->setChecked(m_settings.wordWrap);
}

void MainWindow::updateEditActions()
{
    Editor *e = editor();
    if (!e)
        return;
    const bool hasSelection = e->textCursor().hasSelection();
    const bool hasText = !e->document()->isEmpty();
    m_undo->setEnabled(e->document()->isUndoAvailable());
    m_cut->setEnabled(hasSelection);
    m_copy->setEnabled(hasSelection);
    m_delete->setEnabled(hasSelection);
    m_bing->setEnabled(hasSelection);
    m_paste->setEnabled(e->canPaste());
    m_find->setEnabled(hasText);
    m_findNext->setEnabled(hasText);
    m_findPrevious->setEnabled(hasText);
    m_goTo->setEnabled(!m_settings.wordWrap);
}

// ---------------------------------------------------------------------------
// Tabs and documents
// ---------------------------------------------------------------------------

Editor *MainWindow::editor() const
{
    Document *doc = current();
    return doc ? doc->editor : nullptr;
}

MainWindow::Document *MainWindow::current() const
{
    return m_docs.value(m_tabBar->currentIndex(), nullptr);
}

int MainWindow::indexOf(const Document *doc) const
{
    return m_docs.indexOf(const_cast<Document *>(doc));
}

bool MainWindow::isPristine(const Document *doc) const
{
    return doc->path.isEmpty() && doc->editor->document()->isEmpty();
}

QString MainWindow::titleOf(const Document *doc) const
{
    if (!doc->path.isEmpty())
        return QFileInfo(doc->path).fileName();
    // an unsaved note is named after its first words
    for (QTextBlock block = doc->editor->document()->begin(); block.isValid(); block = block.next()) {
        const QString line = block.text().trimmed();
        if (!line.isEmpty())
            return line.left(60);
    }
    return tr("Untitled");
}

void MainWindow::applyEditorSettings(Editor *e)
{
    e->applyFont(Theme::editorFont(m_settings.font.family, m_settings.font.pointSize, m_settings.font.weight,
                                   m_settings.font.italic, m_zoom));
    e->setWordWrap(m_settings.wordWrap);
}

MainWindow::Document *MainWindow::createTab(bool select)
{
    auto *doc = new Document;
    doc->id = QUuid::createUuid().toString(QUuid::Id128);
    doc->lineEnding = m_settings.defaultLineEnding;
    doc->editor = new Editor(m_editors);
    applyEditorSettings(doc->editor);
    m_editors->addWidget(doc->editor);
    m_docs.append(doc);
    m_tabBar->addTab(tr("Untitled"));

    Editor *e = doc->editor;
    connect(e, &QPlainTextEdit::textChanged, this, [this, doc] {
        doc->bufferDirty = true;
        refreshTab(doc);
        if (doc == current()) {
            updateWindowTitle();
            updateStatus();
            updateEditActions();
        }
        scheduleSessionSave();
    });
    connect(e, &QPlainTextEdit::modificationChanged, this, [this, doc] { refreshTab(doc); });
    connect(e, &QPlainTextEdit::cursorPositionChanged, this, [this, doc] {
        if (doc == current())
            updateStatus();
    });
    connect(e, &QPlainTextEdit::selectionChanged, this, [this, doc] {
        if (doc == current()) {
            updateStatus();
            updateEditActions();
        }
    });
    connect(e, &QPlainTextEdit::undoAvailable, this, &MainWindow::updateEditActions);
    connect(e, &Editor::zoomRequested, this, &MainWindow::zoomBy);
    connect(e, &Editor::searchWithBingRequested, this, &MainWindow::searchWithBing);
    connect(e, &Editor::fileDropped, this, [this](const QString &path) { openPaths({ path }); });

    if (select || m_tabBar->currentIndex() < 0)
        m_tabBar->setCurrentIndex(m_docs.size() - 1);
    scheduleSessionSave();
    return doc;
}

void MainWindow::refreshTab(Document *doc)
{
    const int index = indexOf(doc);
    if (index < 0)
        return;
    m_tabBar->setTabTitle(index, titleOf(doc));
    m_tabBar->setTabToolTip(index, doc->path.isEmpty() ? titleOf(doc) : nativePath(doc->path));
    m_tabBar->setTabModified(index, doc->editor->document()->isModified() && !isPristine(doc));
    if (doc == current())
        updateWindowTitle();
}

void MainWindow::updateWindowTitle()
{
    Document *doc = current();
    setWindowTitle((doc ? titleOf(doc) : tr("Untitled")) + tr(" - Notepad"));
}

void MainWindow::updateStatus()
{
    Document *doc = current();
    if (!doc)
        return;
    const QTextCursor cursor = doc->editor->textCursor();
    m_statusBar->setPosition(doc->editor->currentLine(), doc->editor->currentColumn());
    m_statusBar->setCharacters(qMax(0, doc->editor->document()->characterCount() - 1),
                               cursor.selectionEnd() - cursor.selectionStart());
    m_statusBar->setZoom(m_zoom);
    m_statusBar->setLineEnding(TextFile::lineEndingName(doc->lineEnding));
    m_statusBar->setEncoding(TextFile::encodingName(doc->encoding));
}

void MainWindow::applyTheme()
{
    for (Document *doc : std::as_const(m_docs))
        doc->editor->applyTheme();
    update();
}

bool MainWindow::loadInto(Document *doc, const QString &path, TextFile::Encoding encoding)
{
    const TextFile::Document file = TextFile::load(path, encoding);
    if (!file.ok) {
        ContentDialog::message(this, tr("Notepad"), nativePath(path) + QLatin1Char('\n') + file.error + QLatin1Char('.'));
        return false;
    }
    doc->editor->setPlainText(file.text);
    doc->encoding = file.encoding;
    doc->lineEnding = file.lineEnding;
    doc->path = QFileInfo(path).absoluteFilePath();
    doc->editor->document()->setModified(false);
    m_settings.lastDirectory = QFileInfo(doc->path).absolutePath();

    // A file whose first line is .LOG gets the time appended every time it is opened.
    if (file.text.startsWith(QLatin1String(".LOG")) && (file.text.size() == 4 || file.text.at(4) == u'\n')) {
        QTextCursor cursor = doc->editor->textCursor();
        cursor.movePosition(QTextCursor::End);
        doc->editor->setTextCursor(cursor);
        const Document *previous = current();
        if (!file.text.endsWith(u'\n'))
            doc->editor->insertPlainText(QStringLiteral("\n"));
        if (previous == doc) {
            insertTimeDate();
        } else {
            m_tabBar->setCurrentIndex(indexOf(doc));
            insertTimeDate();
        }
        doc->editor->insertPlainText(QStringLiteral("\n"));
    }
    refreshTab(doc);
    updateStatus();
    scheduleSessionSave();
    return true;
}

void MainWindow::openPaths(const QStringList &paths)
{
    showSettings(false);
    for (const QString &argument : paths) {
        QString path = QFileInfo(argument).absoluteFilePath();
        if (QFileInfo(path).isDir()) {
            m_settings.lastDirectory = path;
            continue;
        }
        if (!QFileInfo::exists(path)) {
            // like Notepad, try the name with .txt before giving up
            if (QFileInfo(path).suffix().isEmpty() && QFileInfo::exists(path + QStringLiteral(".txt"))) {
                path += QStringLiteral(".txt");
            } else {
                const int answer = ContentDialog::ask(
                    this, tr("Notepad"),
                    tr("Cannot find the %1 file.\n\nDo you want to create a new file?").arg(nativePath(path)),
                    { tr("Yes"), tr("No") }, 0, 1);
                if (answer != 0)
                    continue;
                QString error;
                if (!TextFile::save(path, QString(), TextFile::Utf8, m_settings.defaultLineEnding, &error)) {
                    ContentDialog::message(this, tr("Notepad"), nativePath(path) + QLatin1Char('\n') + error + QLatin1Char('.'));
                    continue;
                }
            }
        }
        // already open: just show it
        bool found = false;
        for (int i = 0; i < m_docs.size(); ++i) {
            if (m_docs.at(i)->path == path) {
                m_tabBar->setCurrentIndex(i);
                found = true;
                break;
            }
        }
        if (found)
            continue;
        const bool reuse = current() && isPristine(current()) && !current()->editor->document()->isModified();
        Document *doc = reuse ? current() : createTab(true);
        if (loadInto(doc, path, TextFile::AutoDetect))
            m_tabBar->setCurrentIndex(indexOf(doc));
        else if (!reuse)
            closeTab(indexOf(doc), false);
    }
}

void MainWindow::bringToFront()
{
    show();
    raise();
    activateWindow();
}

bool MainWindow::writeFile(Document *doc, const QString &path, TextFile::Encoding encoding)
{
    const QString text = doc->editor->toPlainText();
    if (encoding == TextFile::Ansi && !TextFile::fitsAnsi(text)) {
        const int answer = ContentDialog::ask(
            this, tr("Notepad"),
            tr("This file contains characters in Unicode format which will be lost if you save this file as an "
               "ANSI encoded text file. To keep the Unicode information, click Cancel below and then select one "
               "of the Unicode options from the Encoding drop down list. Continue?"),
            { tr("OK"), tr("Cancel") }, 0, 1);
        if (answer != 0)
            return false;
    }
    QString error;
    if (!TextFile::save(path, text, encoding, doc->lineEnding, &error)) {
        ContentDialog::message(this, tr("Notepad"), nativePath(path) + QLatin1Char('\n') + error + QLatin1Char('.'));
        return false;
    }
    doc->encoding = encoding;
    doc->path = QFileInfo(path).absoluteFilePath();
    doc->editor->document()->setModified(false);
    m_settings.lastDirectory = QFileInfo(doc->path).absolutePath();
    refreshTab(doc);
    updateStatus();
    scheduleSessionSave();
    return true;
}

bool MainWindow::save(Document *doc)
{
    if (doc->path.isEmpty())
        return saveAs(doc);
    return writeFile(doc, doc->path, doc->encoding);
}

bool MainWindow::saveAs(Document *doc)
{
    QStringList paths;
    TextFile::Encoding encoding = doc->encoding;
    if (!runFileDialog(true, doc, &paths, &encoding) || paths.isEmpty())
        return false;
    return writeFile(doc, paths.constFirst(), encoding);
}

bool MainWindow::confirmClose(Document *doc)
{
    if (!doc->editor->document()->isModified() || isPristine(doc))
        return true;
    m_tabBar->setCurrentIndex(indexOf(doc));
    const QString name = doc->path.isEmpty() ? titleOf(doc) : nativePath(doc->path);
    const int answer = ContentDialog::ask(this, tr("Notepad"), tr("Do you want to save changes to %1?").arg(name),
                                          { tr("Save"), tr("Don't save"), tr("Cancel") }, 0, 2);
    if (answer == 0)
        return save(doc);
    return answer == 1;
}

bool MainWindow::closeTab(int index, bool confirm)
{
    Document *doc = m_docs.value(index, nullptr);
    if (!doc)
        return false;
    if (confirm && !confirmClose(doc))
        return false;
    index = indexOf(doc);
    m_docs.removeAt(index);
    m_tabBar->removeTab(index);
    m_editors->removeWidget(doc->editor);
    doc->editor->deleteLater();
    delete doc;

    if (m_docs.isEmpty()) {
        // closing the last tab closes the window
        m_closing = true;
        saveSession();
        m_settings.windowSize = size();
        m_settings.save();
        close();
        return true;
    }
    const int now = qBound(0, m_tabBar->currentIndex(), int(m_docs.size()) - 1);
    m_editors->setCurrentWidget(m_docs.at(now)->editor);
    m_docs.at(now)->editor->setFocus();
    m_tabBar->update();
    updateWindowTitle();
    updateStatus();
    updateEditActions();
    scheduleSessionSave();
    return true;
}

bool MainWindow::runFileDialog(bool save, const Document *doc, QStringList *paths, TextFile::Encoding *encoding)
{
    const QString textFilter = tr("Text documents (*.txt)");
    const QString allFilter = tr("All files (*.*)");
    const QString docPath = doc ? doc->path : QString();
    QString directory = docPath.isEmpty() ? m_settings.lastDirectory : QFileInfo(docPath).absolutePath();
    if (directory.isEmpty() || !QDir(directory).exists())
        directory = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString name = save ? (docPath.isEmpty() ? QStringLiteral("*.txt") : QFileInfo(docPath).fileName()) : QString();
    QString filter = (save && !docPath.isEmpty() && QFileInfo(docPath).suffix().toLower() != QLatin1String("txt"))
                         ? allFilter : textFilter;

    for (;;) {
        QFileDialog dialog(this, save ? tr("Save as") : tr("Open"));
        dialog.setOption(QFileDialog::DontUseNativeDialog, true);
        dialog.setOption(QFileDialog::DontConfirmOverwrite, true);
        dialog.setAcceptMode(save ? QFileDialog::AcceptSave : QFileDialog::AcceptOpen);
        dialog.setFileMode(save ? QFileDialog::AnyFile : QFileDialog::ExistingFiles);
        dialog.setNameFilters({ textFilter, allFilter });
        dialog.setDirectory(directory);
        dialog.selectNameFilter(filter);
        if (!name.isEmpty())
            dialog.selectFile(name);
        dialog.setLabelText(QFileDialog::Accept, save ? tr("&Save") : tr("&Open"));

        // "*.*" must also list files without a dot here
        auto applyFilter = [&dialog, allFilter](const QString &selected) {
            if (selected == allFilter) {
                if (auto *model = dialog.findChild<QFileSystemModel *>())
                    model->setNameFilters({});
            }
        };
        connect(&dialog, &QFileDialog::filterSelected, &dialog, applyFilter, Qt::QueuedConnection);
        QTimer::singleShot(0, &dialog, [&] { applyFilter(dialog.selectedNameFilter()); });

        // the Encoding drop-down next to the file type
        auto *combo = new QComboBox(&dialog);
        if (!save)
            combo->addItem(tr("Auto-Detect"), int(TextFile::AutoDetect));
        for (TextFile::Encoding e : { TextFile::Ansi, TextFile::Utf16LE, TextFile::Utf16BE, TextFile::Utf8,
                                      TextFile::Utf8Bom })
            combo->addItem(TextFile::encodingName(e), int(e));
        combo->setCurrentIndex(save ? qMax(0, combo->findData(int(*encoding))) : 0);
        if (auto *grid = qobject_cast<QGridLayout *>(dialog.layout())) {
            const int row = grid->rowCount();
            auto *label = new QLabel(tr("&Encoding:"), &dialog);
            label->setBuddy(combo);
            grid->addWidget(label, row, 0);
            grid->addWidget(combo, row, 1);
        }

        dialog.resize(860, 540);
        if (dialog.exec() != QDialog::Accepted || dialog.selectedFiles().isEmpty())
            return false;

        QStringList chosen = dialog.selectedFiles();
        filter = dialog.selectedNameFilter();
        directory = dialog.directory().absolutePath();
        *encoding = TextFile::Encoding(combo->currentData().toInt());

        if (save) {
            QString target = chosen.constFirst();
            if (filter == textFilter && QFileInfo(target).suffix().isEmpty())
                target += QStringLiteral(".txt");
            name = QFileInfo(target).fileName();
            if (QFileInfo::exists(target)) {
                const int answer = ContentDialog::ask(
                    this, tr("Confirm Save As"),
                    tr("%1 already exists.\nDo you want to replace it?").arg(QFileInfo(target).fileName()),
                    { tr("Yes"), tr("No") }, 1, 1);
                if (answer != 0)
                    continue;
            }
            chosen = { target };
        }
        *paths = chosen;
        m_settings.lastDirectory = directory;
        return true;
    }
}

// ---------------------------------------------------------------------------
// Session
// ---------------------------------------------------------------------------

void MainWindow::restoreSession()
{
    if (!m_ownsSession || !m_settings.continueSession)
        return;
    int active = 0;
    const QList<SessionTab> tabs = Session::load(&active);
    for (const SessionTab &tab : tabs) {
        const bool fromBuffer = tab.hasBuffer;
        if (!fromBuffer && (tab.path.isEmpty() || !QFileInfo::exists(tab.path)))
            continue;   // nothing left to show for this tab
        TextFile::Document file;
        if (!fromBuffer) {
            file = TextFile::load(tab.path, TextFile::AutoDetect);
            if (!file.ok)
                continue;
        }
        Document *doc = createTab(false);
        doc->id = tab.id;
        if (fromBuffer) {
            doc->path = tab.path;
            doc->encoding = TextFile::Encoding(tab.encoding);
            doc->lineEnding = TextFile::LineEnding(tab.lineEnding);
            doc->editor->setPlainText(tab.text);
            doc->editor->document()->setModified(tab.modified || tab.path.isEmpty());
            doc->bufferDirty = false;
        } else {
            doc->path = tab.path;
            doc->editor->setPlainText(file.text);
            doc->encoding = file.encoding;
            doc->lineEnding = file.lineEnding;
            doc->editor->document()->setModified(false);
        }
        QTextCursor cursor = doc->editor->textCursor();
        const int last = qMax(0, doc->editor->document()->characterCount() - 1);
        cursor.setPosition(qBound(0, tab.anchor, last));
        cursor.setPosition(qBound(0, tab.cursor, last), QTextCursor::KeepAnchor);
        doc->editor->setTextCursor(cursor);
        refreshTab(doc);
    }
    if (!m_docs.isEmpty()) {
        const int index = qBound(0, active, int(m_docs.size()) - 1);
        if (index == m_tabBar->currentIndex()) {
            m_editors->setCurrentWidget(m_docs.at(index)->editor);
            m_docs.at(index)->editor->setFocus();
        } else {
            m_tabBar->setCurrentIndex(index);
        }
        m_docs.at(index)->editor->ensureCursorVisible();
    }
    m_sessionTimer.stop();
}

void MainWindow::saveSession()
{
    m_sessionTimer.stop();
    if (!m_ownsSession)
        return;
    QList<SessionTab> tabs;
    if (m_settings.continueSession) {
        for (Document *doc : std::as_const(m_docs)) {
            const bool modified = doc->editor->document()->isModified() && !isPristine(doc);
            if (isPristine(doc) && !modified && m_docs.size() > 1)
                continue;   // an untouched "Untitled" tab is not worth remembering
            SessionTab tab;
            tab.id = doc->id;
            tab.path = doc->path;
            tab.encoding = int(doc->encoding);
            tab.lineEnding = int(doc->lineEnding);
            tab.modified = modified;
            tab.hasBuffer = modified || doc->path.isEmpty();
            if (tab.hasBuffer && doc->bufferDirty) {
                tab.text = doc->editor->toPlainText();
                if (tab.text.isNull())
                    tab.text = QStringLiteral("");
                doc->bufferDirty = false;
            }
            const QTextCursor cursor = doc->editor->textCursor();
            tab.cursor = cursor.position();
            tab.anchor = cursor.anchor();
            tabs.append(tab);
        }
    }
    int active = 0;
    if (Document *doc = current()) {
        for (int i = 0; i < tabs.size(); ++i) {
            if (tabs.at(i).id == doc->id)
                active = i;
        }
    }
    Session::save(tabs, active);
}

void MainWindow::scheduleSessionSave()
{
    if (m_ownsSession && !m_closing)
        m_sessionTimer.start();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    if (!m_closing) {
        const bool keepsSession = m_ownsSession && m_settings.continueSession;
        if (!keepsSession) {
            // nothing is remembered, so every unsaved tab gets its question
            const QList<Document *> docs = m_docs;
            for (Document *doc : docs) {
                if (m_docs.contains(doc) && !confirmClose(doc)) {
                    event->ignore();
                    return;
                }
            }
        }
        m_closing = true;
        saveSession();
        m_settings.windowSize = size();
        m_settings.save();
    }
    event->accept();
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    if (m_findBar && m_findBar->isVisible())
        m_findBar->reposition();
}

// ---------------------------------------------------------------------------
// File menu
// ---------------------------------------------------------------------------

void MainWindow::fileNewTab()
{
    showSettings(false);
    createTab(true);
}

void MainWindow::fileNewWindow()
{
    QProcess::startDetached(QCoreApplication::applicationFilePath(), { QStringLiteral("--new-window") });
}

void MainWindow::fileOpen()
{
    QStringList paths;
    TextFile::Encoding encoding = TextFile::AutoDetect;
    if (!runFileDialog(false, current(), &paths, &encoding))
        return;
    if (encoding == TextFile::AutoDetect) {
        openPaths(paths);
        return;
    }
    for (const QString &path : std::as_const(paths)) {
        const bool reuse = current() && isPristine(current()) && !current()->editor->document()->isModified();
        Document *doc = reuse ? current() : createTab(true);
        if (!loadInto(doc, path, encoding) && !reuse)
            closeTab(indexOf(doc), false);
    }
}

void MainWindow::fileSaveAll()
{
    const QList<Document *> docs = m_docs;
    for (Document *doc : docs) {
        if (!doc->editor->document()->isModified() || isPristine(doc))
            continue;
        if (doc->path.isEmpty())
            m_tabBar->setCurrentIndex(indexOf(doc));
        if (!save(doc))
            break;
    }
}

void MainWindow::filePageSetup()
{
    PageSetupDialog dialog(this, m_settings.pageSetup);
    if (dialog.exec() == QDialog::Accepted) {
        m_settings.pageSetup = dialog.setup();
        m_settings.save();
    }
}

void MainWindow::filePrint()
{
    Document *doc = current();
    if (!doc)
        return;
    const QFont font = Theme::editorFont(m_settings.font.family, m_settings.font.pointSize, m_settings.font.weight,
                                         m_settings.font.italic, 100);
    Printing::print(this, doc->editor->toPlainText(), font, titleOf(doc), m_settings.pageSetup);
}

// ---------------------------------------------------------------------------
// Edit menu
// ---------------------------------------------------------------------------

void MainWindow::searchWithBing()
{
    Editor *e = editor();
    if (!e)
        return;
    QString selection = e->textCursor().selectedText();
    if (selection.isEmpty())
        return;
    selection.replace(QChar::ParagraphSeparator, u' ');
    QUrl url(QStringLiteral("https://www.bing.com/search"));
    QUrlQuery query;
    query.addQueryItem(QStringLiteral("q"), QString::fromLatin1(QUrl::toPercentEncoding(selection)));
    url.setQuery(query);
    QDesktopServices::openUrl(url);
}

void MainWindow::showFind()
{
    Editor *e = editor();
    if (!e)
        return;
    const QString selection = e->textCursor().selectedText();
    m_findBar->openFind(selection.contains(QChar::ParagraphSeparator) ? QString() : selection);
    m_findBar->reposition();
}

void MainWindow::showReplace()
{
    Editor *e = editor();
    if (!e)
        return;
    const QString selection = e->textCursor().selectedText();
    m_findBar->openReplace(selection.contains(QChar::ParagraphSeparator) ? QString() : selection);
    m_findBar->reposition();
}

bool MainWindow::find(bool backward)
{
    Editor *e = editor();
    if (!e)
        return false;
    const QString needle = m_settings.search.findText;
    if (needle.isEmpty()) {
        showFind();
        return false;
    }
    QTextDocument::FindFlags flags;
    if (backward)
        flags |= QTextDocument::FindBackward;
    if (m_settings.search.matchCase)
        flags |= QTextDocument::FindCaseSensitively;

    QTextDocument *document = e->document();
    QTextCursor found = document->find(needle, e->textCursor(), flags);
    if (found.isNull() && m_settings.search.wrapAround) {
        QTextCursor edge(document);
        if (backward)
            edge.movePosition(QTextCursor::End);
        found = document->find(needle, edge, flags);
    }
    if (found.isNull()) {
        ContentDialog::message(this, tr("Notepad"), tr("Cannot find \"%1\"").arg(needle));
        return false;
    }
    e->setTextCursor(found);
    e->ensureCursorVisible();
    return true;
}

void MainWindow::replaceOne()
{
    Editor *e = editor();
    const QString needle = m_settings.search.findText;
    if (!e || needle.isEmpty())
        return;
    QTextCursor cursor = e->textCursor();
    const Qt::CaseSensitivity cs = m_settings.search.matchCase ? Qt::CaseSensitive : Qt::CaseInsensitive;
    if (cursor.hasSelection() && cursor.selectedText().compare(needle, cs) == 0) {
        cursor.insertText(m_settings.search.replaceText);
        e->setTextCursor(cursor);
    }
    find(false);
}

void MainWindow::replaceAll()
{
    Editor *e = editor();
    const QString needle = m_settings.search.findText;
    if (!e || needle.isEmpty())
        return;
    QTextDocument::FindFlags flags;
    if (m_settings.search.matchCase)
        flags |= QTextDocument::FindCaseSensitively;

    QTextDocument *document = e->document();
    QTextCursor group(document);
    group.beginEditBlock();
    QTextCursor position(document);
    for (;;) {
        QTextCursor found = document->find(needle, position, flags);
        if (found.isNull())
            break;
        found.insertText(m_settings.search.replaceText);
        position = found;
    }
    group.endEditBlock();
    e->setTextCursor(QTextCursor(document));
    e->ensureCursorVisible();
}

void MainWindow::goTo()
{
    Editor *e = editor();
    if (!e)
        return;
    QString text = QString::number(e->currentLine());
    for (;;) {
        ContentDialog dialog(this, tr("Go to line"));
        auto *content = new QWidget;
        auto *layout = new QVBoxLayout(content);
        layout->setContentsMargins(0, 0, 0, 0);
        layout->setSpacing(8);
        auto *label = new QLabel(tr("Line number"), content);
        label->setFont(Theme::uiFont(14));
        auto *edit = new QLineEdit(text, content);
        edit->setFixedHeight(32);
        edit->setTextMargins(6, 0, 6, 0);
        edit->setValidator(new QRegularExpressionValidator(QRegularExpression(QStringLiteral("[0-9]*")), edit));
        edit->setMaxLength(10);
        edit->selectAll();
        layout->addWidget(label);
        layout->addWidget(edit);
        dialog.setContent(content);
        dialog.addButton(tr("Go to"), 0, true);
        dialog.addButton(tr("Cancel"), 1);
        dialog.setCancelResult(1);
        dialog.setInitialFocus(edit);
        connect(edit, &QLineEdit::returnPressed, &dialog, [&dialog] { dialog.done(0); });
        if (dialog.exec() != 0)
            return;
        text = edit->text();
        bool ok = false;
        const qlonglong line = text.toLongLong(&ok);
        if (ok && line >= 1 && line <= e->blockCount()) {
            e->goToLine(int(line));
            e->setFocus();
            return;
        }
        ContentDialog::message(this, tr("Notepad - Go to line"),
                               tr("The line number is beyond the total number of lines"));
    }
}

void MainWindow::insertTimeDate()
{
    Editor *e = editor();
    if (!e)
        return;
    const QLocale locale = QLocale::system();
    const QDateTime now = QDateTime::currentDateTime();
    QString dateFormat = locale.dateFormat(QLocale::ShortFormat);
    if (!dateFormat.contains(QLatin1String("yyyy")))
        dateFormat.replace(QLatin1String("yy"), QLatin1String("yyyy"));
    QString stamp = locale.toString(now.time(), QLocale::ShortFormat) + QLatin1Char(' ')
                    + locale.toString(now.date(), dateFormat);
    stamp.replace(QChar(0x202F), u' ').replace(QChar(0x00A0), u' ');
    e->insertPlainText(stamp);
}

// ---------------------------------------------------------------------------
// View
// ---------------------------------------------------------------------------

void MainWindow::zoomBy(int steps)
{
    const int zoom = steps == 0 ? 100 : qBound(kZoomMin, m_zoom + steps * 10, kZoomMax);
    if (zoom == m_zoom)
        return;
    m_zoom = zoom;
    for (Document *doc : std::as_const(m_docs))
        applyEditorSettings(doc->editor);
    updateStatus();
}

void MainWindow::setWordWrap(bool wrap)
{
    m_settings.wordWrap = wrap;
    for (Document *doc : std::as_const(m_docs))
        doc->editor->setWordWrap(wrap);
    updateEditActions();
    m_settings.save();
}

void MainWindow::setStatusBarVisible(bool visible)
{
    m_settings.statusBar = visible;
    m_statusBar->setVisible(visible);
    m_settings.save();
}

void MainWindow::showSettings(bool show)
{
    if (show == (m_pages->currentIndex() == 1))
        return;
    if (show) {
        m_findBar->closeBar();
        m_settingsPage->setValues(m_settings);
        m_pages->setCurrentIndex(1);
        m_settingsPage->setFocus();
    } else {
        m_pages->setCurrentIndex(0);
        if (Editor *e = editor())
            e->setFocus();
    }
}

QString MainWindow::describeTabs() const
{
    QStringList parts;
    for (int i = 0; i < m_docs.size(); ++i) {
        const Document *doc = m_docs.at(i);
        parts.append(QStringLiteral("%1%2%3").arg(i == m_tabBar->currentIndex() ? QStringLiteral(">") : QString(),
                                                  doc->editor->document()->isModified() && !isPristine(doc)
                                                      ? QStringLiteral("*") : QString(),
                                                  m_tabBar->tabTitle(i)));
    }
    return parts.join(QStringLiteral(" | "));
}
