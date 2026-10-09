#include "pagesetup.h"
#include "theme.h"

#include <QComboBox>
#include <QGridLayout>
#include <QLabel>
#include <QLineEdit>
#include <QLocale>
#include <QPushButton>
#include <QRadioButton>
#include <QVBoxLayout>

namespace {

struct Paper { QPageSize::PageSizeId id; const char *name; };
const Paper kPapers[] = {
    { QPageSize::Letter, "Letter" }, { QPageSize::Legal, "Legal" }, { QPageSize::Executive, "Executive" },
    { QPageSize::Tabloid, "Tabloid" }, { QPageSize::Statement, "Statement" }, { QPageSize::A3, "A3" },
    { QPageSize::A4, "A4" }, { QPageSize::A5, "A5" }, { QPageSize::JisB4, "B4 (JIS)" },
    { QPageSize::JisB5, "B5 (JIS)" }, { QPageSize::Envelope10, "Envelope #10" },
    { QPageSize::EnvelopeDL, "Envelope DL" }, { QPageSize::EnvelopeC5, "Envelope C5" },
};

QString formatMargin(double mm, bool metric)
{
    QString text = QString::number(metric ? mm : mm / 25.4, 'f', metric ? 2 : 3);
    while (text.contains(u'.') && (text.endsWith(u'0') || text.endsWith(u'.')))
        text.chop(1);
    return text;
}

double parseMargin(const QString &text, bool metric, double fallback)
{
    bool ok = false;
    const double value = QLocale::c().toDouble(text.trimmed().replace(u',', u'.'), &ok);
    if (!ok || value < 0)
        return fallback;
    return metric ? value : value * 25.4;
}

} // namespace

PageSetupDialog::PageSetupDialog(QWidget *parent, const PageSetup &setup)
    : QDialog(parent), m_setup(setup), m_metric(QLocale::system().measurementSystem() == QLocale::MetricSystem)
{
    setWindowTitle(tr("Page setup"));
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(24, 20, 24, 24);
    outer->setSpacing(16);

    auto *title = new QLabel(tr("Page setup"), this);
    title->setFont(Theme::uiFont(20, QFont::DemiBold));
    outer->addWidget(title);

    auto *grid = new QGridLayout;
    grid->setHorizontalSpacing(12);
    grid->setVerticalSpacing(10);
    int row = 0;
    auto addRow = [&](const QString &label, QWidget *field, int span = 3) {
        grid->addWidget(new QLabel(label, this), row, 0);
        grid->addWidget(field, row, 1, 1, span);
        ++row;
    };

    m_paper = new QComboBox(this);
    for (const Paper &paper : kPapers)
        m_paper->addItem(tr(paper.name), int(paper.id));
    m_paper->setCurrentIndex(qMax(0, m_paper->findData(int(setup.paper))));
    addRow(tr("Paper size"), m_paper);

    auto *orientation = new QWidget(this);
    auto *orientationLayout = new QHBoxLayout(orientation);
    orientationLayout->setContentsMargins(0, 0, 0, 0);
    orientationLayout->setSpacing(20);
    m_portrait = new QRadioButton(tr("Portrait"), orientation);
    m_landscape = new QRadioButton(tr("Landscape"), orientation);
    (setup.landscape ? m_landscape : m_portrait)->setChecked(true);
    orientationLayout->addWidget(m_portrait);
    orientationLayout->addWidget(m_landscape);
    orientationLayout->addStretch(1);
    addRow(tr("Orientation"), orientation);

    auto marginEdit = [this](double value) {
        auto *edit = new QLineEdit(formatMargin(value, m_metric), this);
        edit->setTextMargins(6, 0, 6, 0);
        edit->setFixedWidth(84);
        return edit;
    };
    m_left = marginEdit(setup.left);
    m_right = marginEdit(setup.right);
    m_top = marginEdit(setup.top);
    m_bottom = marginEdit(setup.bottom);
    const QString unit = m_metric ? tr("mm") : tr("in");
    grid->addWidget(new QLabel(tr("Margins (%1)").arg(unit), this), row, 0);
    auto *margins = new QGridLayout;
    margins->setHorizontalSpacing(8);
    margins->setVerticalSpacing(8);
    margins->addWidget(new QLabel(tr("Left"), this), 0, 0);
    margins->addWidget(m_left, 0, 1);
    margins->addWidget(new QLabel(tr("Right"), this), 0, 2);
    margins->addWidget(m_right, 0, 3);
    margins->addWidget(new QLabel(tr("Top"), this), 1, 0);
    margins->addWidget(m_top, 1, 1);
    margins->addWidget(new QLabel(tr("Bottom"), this), 1, 2);
    margins->addWidget(m_bottom, 1, 3);
    margins->setColumnStretch(4, 1);
    grid->addLayout(margins, row, 1, 1, 3);
    ++row;

    m_header = new QLineEdit(setup.header, this);
    m_header->setTextMargins(6, 0, 6, 0);
    addRow(tr("Header"), m_header);
    m_footer = new QLineEdit(setup.footer, this);
    m_footer->setTextMargins(6, 0, 6, 0);
    addRow(tr("Footer"), m_footer);

    auto *hint = new QLabel(tr("Codes: &f file name, &p page number, &d date, &t time, &l &c &r alignment"), this);
    hint->setFont(Theme::uiFont(12));
    hint->setTextFormat(Qt::PlainText);
    QPalette hintPalette = hint->palette();
    hintPalette.setColor(QPalette::WindowText, Theme::colors().textSecondary);
    hint->setPalette(hintPalette);
    grid->addWidget(hint, row, 1, 1, 3);
    outer->addLayout(grid);

    auto *buttons = new QHBoxLayout;
    buttons->setSpacing(8);
    buttons->addStretch(1);
    auto *ok = new QPushButton(tr("OK"), this);
    ok->setProperty("accent", true);
    ok->setDefault(true);
    auto *cancel = new QPushButton(tr("Cancel"), this);
    ok->setMinimumWidth(120);
    cancel->setMinimumWidth(120);
    buttons->addWidget(ok);
    buttons->addWidget(cancel);
    outer->addLayout(buttons);

    connect(ok, &QPushButton::clicked, this, [this] {
        readControls();
        accept();
    });
    connect(cancel, &QPushButton::clicked, this, &QDialog::reject);
    setFixedSize(sizeHint().expandedTo(QSize(460, 0)));
}

void PageSetupDialog::readControls()
{
    m_setup.paper = QPageSize::PageSizeId(m_paper->currentData().toInt());
    m_setup.landscape = m_landscape->isChecked();
    m_setup.left = parseMargin(m_left->text(), m_metric, m_setup.left);
    m_setup.right = parseMargin(m_right->text(), m_metric, m_setup.right);
    m_setup.top = parseMargin(m_top->text(), m_metric, m_setup.top);
    m_setup.bottom = parseMargin(m_bottom->text(), m_metric, m_setup.bottom);
    m_setup.header = m_header->text();
    m_setup.footer = m_footer->text();
}
