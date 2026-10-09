#pragma once

#include <QList>
#include <QWidget>

// The Windows 11 tab strip: rounded tabs on the mica band, a close button or
// an "unsaved" dot per tab, and the add button after the last tab.
class TabBar : public QWidget
{
    Q_OBJECT
public:
    static constexpr int Height = 41;

    explicit TabBar(QWidget *parent = nullptr);

    int count() const { return m_tabs.size(); }
    int currentIndex() const { return m_current; }
    int addTab(const QString &title);
    void removeTab(int index);
    void setCurrentIndex(int index);
    void setTabTitle(int index, const QString &title);
    void setTabToolTip(int index, const QString &toolTip);
    void setTabModified(int index, bool modified);
    QString tabTitle(int index) const { return m_tabs.value(index).title; }
    QRect tabRect(int index) const;

signals:
    void currentChanged(int index);
    void tabCloseRequested(int index);
    void tabMoved(int from, int to);
    void newTabRequested();
    void contextMenuRequested(int index, const QPoint &globalPos);

protected:
    void paintEvent(QPaintEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void contextMenuEvent(QContextMenuEvent *event) override;
    bool event(QEvent *event) override;

private:
    struct Tab {
        QString title;
        QString toolTip;
        bool modified = false;
    };
    enum Part { Nothing, Body, CloseButton, AddButton };

    int tabWidth() const;
    QRect closeRect(int index) const;
    QRect addRect() const;
    Part hitTest(const QPoint &pos, int *index) const;
    void setHover(Part part, int index);

    QList<Tab> m_tabs;
    int m_current = -1;
    Part m_hoverPart = Nothing;
    int m_hoverIndex = -1;
    Part m_pressPart = Nothing;
    int m_pressIndex = -1;
    QPoint m_pressPos;
    bool m_dragging = false;
};
