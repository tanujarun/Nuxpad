#pragma once

#include "printing.h"

#include <QDialog>

class QComboBox;
class QLineEdit;
class QRadioButton;

// Page setup: paper, orientation, margins, header and footer.
class PageSetupDialog : public QDialog
{
    Q_OBJECT
public:
    PageSetupDialog(QWidget *parent, const PageSetup &setup);
    PageSetup setup() const { return m_setup; }

private:
    void readControls();

    PageSetup m_setup;
    bool m_metric;
    QComboBox *m_paper;
    QRadioButton *m_portrait;
    QRadioButton *m_landscape;
    QLineEdit *m_left, *m_right, *m_top, *m_bottom;
    QLineEdit *m_header, *m_footer;
};
