#pragma once

#include <QWidget>

class IconButton;
class QLineEdit;
class QPushButton;

// Search settings shared by the find bar and F3 / Shift+F3.
struct SearchState {
    QString findText;
    QString replaceText;
    bool matchCase = false;
    bool wrapAround = false;
};

// The floating find / replace bar at the top of the text area.
class FindBar : public QWidget
{
    Q_OBJECT
public:
    FindBar(QWidget *parent, SearchState *state);

    void openFind(const QString &seed);
    void openReplace(const QString &seed);
    void closeBar();
    void reposition();

signals:
    void findNext();
    void findPrevious();
    void replaceOne();
    void replaceAll();
    void closed();

protected:
    void paintEvent(QPaintEvent *event) override;
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void present(bool withReplace, const QString &seed);
    void setReplaceVisible(bool visible);
    void syncButtons();
    void showOptions();

    SearchState *m_state;
    QLineEdit *m_find;
    QLineEdit *m_replace;
    IconButton *m_toggle;
    IconButton *m_next;
    IconButton *m_previous;
    IconButton *m_options;
    QPushButton *m_replaceButton;
    QPushButton *m_replaceAllButton;
    QWidget *m_replaceRow;
};
