#pragma once

#include "appsettings.h"

#include <QList>
#include <QTimer>
#include <QWidget>

class Editor;
class FindBar;
class IconButton;
class QAction;
class QMenuBar;
class QStackedWidget;
class SettingsPage;
class StatusBar;
class TabBar;

class MainWindow : public QWidget
{
    Q_OBJECT
public:
    // ownsSession: this instance restores and saves the session (the first one running).
    explicit MainWindow(bool ownsSession);

    void openPaths(const QStringList &paths);   // each in its own tab; offers to create missing files
    void bringToFront();

    Editor *editor() const;                     // of the current tab
    QMenuBar *menuBar() const { return m_menuBar; }
    TabBar *tabBar() const { return m_tabBar; }
    QString describeTabs() const;               // for the test build

protected:
    void closeEvent(QCloseEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    struct Document {
        QString id;
        Editor *editor = nullptr;
        QString path;
        TextFile::Encoding encoding = TextFile::Utf8;
        TextFile::LineEnding lineEnding = TextFile::Lf;
        bool bufferDirty = true;   // the session buffer file is out of date
    };

    void buildMenus();
    Document *current() const;
    int indexOf(const Document *doc) const;
    Document *createTab(bool select);
    bool isPristine(const Document *doc) const;   // untitled, empty, untouched
    QString titleOf(const Document *doc) const;
    void refreshTab(Document *doc);
    void updateWindowTitle();
    void updateStatus();
    void updateEditActions();
    void applyEditorSettings(Editor *editor);
    void applyTheme();

    bool loadInto(Document *doc, const QString &path, TextFile::Encoding encoding);
    bool writeFile(Document *doc, const QString &path, TextFile::Encoding encoding);
    bool save(Document *doc);
    bool saveAs(Document *doc);
    bool confirmClose(Document *doc);             // false = cancelled
    bool closeTab(int index, bool confirm = true);
    bool runFileDialog(bool save, const Document *doc, QStringList *paths, TextFile::Encoding *encoding);

    void restoreSession();
    void saveSession();
    void scheduleSessionSave();

    // File
    void fileNewTab();
    void fileNewWindow();
    void fileOpen();
    void fileSaveAll();
    void filePageSetup();
    void filePrint();
    // Edit
    void searchWithBing();
    void showFind();
    void showReplace();
    bool find(bool backward);
    void replaceOne();
    void replaceAll();
    void goTo();
    void insertTimeDate();
    // View
    void zoomBy(int steps);
    void setWordWrap(bool wrap);
    void setStatusBarVisible(bool visible);
    void showSettings(bool show);

    AppSettings m_settings;
    bool m_ownsSession;
    int m_zoom = 100;
    bool m_closing = false;

    QStackedWidget *m_pages = nullptr;
    TabBar *m_tabBar = nullptr;
    QMenuBar *m_menuBar = nullptr;
    IconButton *m_gear = nullptr;
    QWidget *m_menuRow = nullptr;
    QWidget *m_divider = nullptr;
    QStackedWidget *m_editors = nullptr;
    FindBar *m_findBar = nullptr;
    StatusBar *m_statusBar = nullptr;
    SettingsPage *m_settingsPage = nullptr;
    QList<Document *> m_docs;
    QTimer m_sessionTimer;

    QAction *m_save = nullptr, *m_saveAs = nullptr, *m_saveAll = nullptr;
    QAction *m_undo = nullptr, *m_cut = nullptr, *m_copy = nullptr, *m_paste = nullptr, *m_delete = nullptr;
    QAction *m_bing = nullptr, *m_find = nullptr, *m_findNext = nullptr, *m_findPrevious = nullptr;
    QAction *m_goTo = nullptr, *m_wordWrap = nullptr, *m_statusBarAction = nullptr;
};
