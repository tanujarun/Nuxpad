#pragma once

#include <QPlainTextEdit>

// The text area of one tab: plain text, a one pixel caret, overlay scroll
// bars and the Windows 11 context menu.
class Editor : public QPlainTextEdit
{
    Q_OBJECT
public:
    explicit Editor(QWidget *parent = nullptr);

    void setWordWrap(bool wrap);
    bool wordWrap() const { return lineWrapMode() != NoWrap; }
    void applyFont(const QFont &font);
    void applyTheme();

    int currentLine() const;     // 1-based
    int currentColumn() const;   // 1-based
    void deleteSelection();
    void goToLine(int line);     // 1-based

signals:
    void zoomRequested(int steps);
    void fileDropped(const QString &path);
    void searchWithBingRequested();

protected:
    void keyPressEvent(QKeyEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;
    bool canInsertFromMimeData(const QMimeData *source) const override;
    void insertFromMimeData(const QMimeData *source) override;
};
