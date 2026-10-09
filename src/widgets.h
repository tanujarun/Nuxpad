#pragma once

#include "theme.h"

#include <QAbstractButton>
#include <QList>
#include <QPointer>
#include <QWidget>

class QEventLoop;
class QLabel;
class QPushButton;
class QVBoxLayout;
class QHBoxLayout;

// A borderless button showing one line icon (tab close, add tab, settings...).
class IconButton : public QAbstractButton
{
    Q_OBJECT
public:
    IconButton(Theme::Icon icon, QWidget *parent, const QSize &size = QSize(32, 28), qreal iconSize = 12);
    void setIconKind(Theme::Icon icon);
    QSize sizeHint() const override { return m_size; }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    Theme::Icon m_icon;
    QSize m_size;
    qreal m_iconSize;
    bool m_hover = false;
};

// WinUI toggle switch.
class ToggleSwitch : public QAbstractButton
{
    Q_OBJECT
public:
    explicit ToggleSwitch(QWidget *parent = nullptr);
    QSize sizeHint() const override { return QSize(40, 20); }

protected:
    void paintEvent(QPaintEvent *event) override;
    void enterEvent(QEnterEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    bool m_hover = false;
};

// A WinUI ContentDialog: a card over a dimmed copy of its host window.
// Modal for that window only; exec() returns the result of the chosen button.
class ContentDialog : public QWidget
{
    Q_OBJECT
public:
    ContentDialog(QWidget *host, const QString &title);

    void setText(const QString &text);
    void setContent(QWidget *content);           // placed under the text
    QPushButton *addButton(const QString &text, int result, bool accent = false);
    void setCancelResult(int result) { m_cancelResult = result; }
    void setInitialFocus(QWidget *widget) { m_initialFocus = widget; }
    void done(int result);
    int exec();

    // Convenience forms.
    static void message(QWidget *host, const QString &title, const QString &text);
    static int ask(QWidget *host, const QString &title, const QString &text, const QStringList &buttons,
                   int accentIndex = 0, int cancelIndex = -1);

protected:
    void paintEvent(QPaintEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;
    bool focusNextPrevChild(bool next) override;

private:
    void layoutCard();

    QWidget *m_host;
    QWidget *m_card;
    QWidget *m_footer;
    QVBoxLayout *m_bodyLayout;
    QHBoxLayout *m_buttonLayout;
    QLabel *m_text = nullptr;
    QList<QPushButton *> m_buttons;
    QPointer<QWidget> m_initialFocus;
    QEventLoop *m_loop = nullptr;
    int m_result = -1;
    int m_cancelResult = -1;
};
