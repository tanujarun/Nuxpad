#pragma once

#include <QWidget>

// Windows 11 Notepad status bar: caret position and character count on the
// left; zoom, line ending and encoding on the right.
class StatusBar : public QWidget
{
    Q_OBJECT
public:
    static constexpr int Height = 32;

    explicit StatusBar(QWidget *parent = nullptr);

    void setPosition(int line, int column);
    void setCharacters(qsizetype total, qsizetype selected);
    void setZoom(int percent);
    void setLineEnding(const QString &text);
    void setEncoding(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;

private:
    QString m_position, m_characters, m_zoom, m_lineEnding, m_encoding;
};
