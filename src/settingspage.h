#pragma once

#include "appsettings.h"

#include <QWidget>

class QComboBox;
class QLabel;
class QRadioButton;
class ToggleSwitch;

// The Settings page that replaces the editor while it is open.
class SettingsPage : public QWidget
{
    Q_OBJECT
public:
    explicit SettingsPage(QWidget *parent = nullptr);

    void setValues(const AppSettings &settings);

signals:
    void backRequested();
    void themeChanged(Theme::Mode mode);
    void fontChanged(const FontChoice &font);
    void wordWrapChanged(bool wrap);
    void openInNewTabChanged(bool newTab);
    void continueSessionChanged(bool continueSession);

protected:
    void paintEvent(QPaintEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void populateStyles(const QString &family, const QString &preferred);
    void emitFont();
    void updatePreview();

    bool m_loading = false;
    QRadioButton *m_light, *m_dark, *m_system;
    QComboBox *m_family, *m_style, *m_size;
    QLabel *m_preview;
    ToggleSwitch *m_wrap;
    QLabel *m_wrapState;
    QComboBox *m_opening;
    QRadioButton *m_continue, *m_fresh;
    QLabel *m_themeValue, *m_startValue;
};
