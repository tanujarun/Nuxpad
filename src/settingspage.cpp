#include "settingspage.h"
#include "widgets.h"

#include <QComboBox>
#include <QFontDatabase>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QRadioButton>
#include <QScrollArea>
#include <QSysInfo>
#include <QVBoxLayout>

namespace {

const Theme::Colors &C() { return Theme::colors(); }

const int kSizes[] = { 8, 9, 10, 11, 12, 14, 16, 18, 20, 22, 24, 26, 28, 36, 48, 72 };

// Secondary (grey) text that follows the theme.
class DimLabel : public QLabel
{
public:
    DimLabel(const QString &text, QWidget *parent, int pixelSize = 12) : QLabel(text, parent)
    {
        setFont(Theme::uiFont(pixelSize));
        setWordWrap(true);
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QPalette pal = palette();
        pal.setColor(QPalette::WindowText, C().textSecondary);
        setPalette(pal);
        QLabel::paintEvent(event);
    }
};

// A settings card: title and description on the left, a control on the right,
// and optionally a body that expands below when the header is clicked.
class SettingCard : public QWidget
{
public:
    SettingCard(const QString &title, const QString &description, QWidget *parent)
        : QWidget(parent)
    {
        m_layout = new QVBoxLayout(this);
        m_layout->setContentsMargins(1, 1, 1, 1);
        m_layout->setSpacing(0);
        m_header = new QWidget(this);
        m_header->setMinimumHeight(66);
        m_headerLayout = new QHBoxLayout(m_header);
        m_headerLayout->setContentsMargins(16, 12, 16, 12);
        m_headerLayout->setSpacing(12);
        auto *texts = new QVBoxLayout;
        texts->setSpacing(2);
        auto *titleLabel = new QLabel(title, m_header);
        titleLabel->setFont(Theme::uiFont(14));
        texts->addStretch(1);
        texts->addWidget(titleLabel);
        if (!description.isEmpty())
            texts->addWidget(new DimLabel(description, m_header));
        texts->addStretch(1);
        m_headerLayout->addLayout(texts, 1);
        m_layout->addWidget(m_header);
    }

    void addHeaderWidget(QWidget *widget) { m_headerLayout->addWidget(widget, 0, Qt::AlignVCenter); }

    // Makes the card an expander with the given body.
    QVBoxLayout *makeExpandable(bool expanded)
    {
        m_chevron = new IconButton(Theme::Icon::ChevronDown, m_header, QSize(32, 32), 8);
        m_headerLayout->addWidget(m_chevron, 0, Qt::AlignVCenter);
        m_body = new QWidget(this);
        auto *bodyLayout = new QVBoxLayout(m_body);
        bodyLayout->setContentsMargins(56, 14, 16, 16);
        bodyLayout->setSpacing(12);
        m_layout->addWidget(m_body);
        m_header->installEventFilter(this);
        connect(m_chevron, &QAbstractButton::clicked, this, [this] { setExpanded(!m_body->isVisibleTo(this)); });
        setExpanded(expanded);
        return bodyLayout;
    }

    void setExpanded(bool expanded)
    {
        m_body->setVisible(expanded);
        m_chevron->setIconKind(expanded ? Theme::Icon::ChevronUp : Theme::Icon::ChevronDown);
        update();
    }

protected:
    bool eventFilter(QObject *watched, QEvent *event) override
    {
        if (watched == m_header && m_body && event->type() == QEvent::MouseButtonRelease) {
            setExpanded(!m_body->isVisibleTo(this));
            return true;
        }
        return QWidget::eventFilter(watched, event);
    }

    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing, true);
        p.setPen(QPen(C().cardBorder, 1));
        p.setBrush(C().card);
        p.drawRoundedRect(QRectF(rect()).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
        if (m_body && m_body->isVisibleTo(this))
            p.fillRect(QRect(1, m_header->height() + 1, width() - 2, 1), C().cardBorder);
    }

private:
    QVBoxLayout *m_layout;
    QHBoxLayout *m_headerLayout;
    QWidget *m_header;
    QWidget *m_body = nullptr;
    IconButton *m_chevron = nullptr;
};

QLabel *sectionLabel(const QString &text, QWidget *parent)
{
    auto *label = new QLabel(text, parent);
    label->setFont(Theme::uiFont(14, QFont::DemiBold));
    label->setContentsMargins(0, 18, 0, 2);
    return label;
}

QComboBox *makeCombo(QWidget *parent, int width)
{
    auto *combo = new QComboBox(parent);
    combo->setFixedSize(width, 32);
    combo->setFont(Theme::uiFont(14));
    combo->setMaxVisibleItems(12);
    return combo;
}

} // namespace

SettingsPage::SettingsPage(QWidget *parent)
    : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *top = new QHBoxLayout;
    top->setContentsMargins(6, 6, 6, 0);
    auto *back = new IconButton(Theme::Icon::Back, this, QSize(40, 32), 12);
    back->setToolTip(tr("Back"));
    connect(back, &QAbstractButton::clicked, this, &SettingsPage::backRequested);
    top->addWidget(back);
    top->addStretch(1);
    outer->addLayout(top);

    auto *scroll = new QScrollArea(this);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setWidgetResizable(true);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->viewport()->setAutoFillBackground(false);
    auto *content = new QWidget(scroll);
    content->setAutoFillBackground(false);
    auto *centering = new QHBoxLayout(content);
    centering->setContentsMargins(36, 0, 36, 24);
    auto *column = new QWidget(content);
    column->setMaximumWidth(1000);
    centering->addWidget(column, 1);
    auto *list = new QVBoxLayout(column);
    list->setContentsMargins(0, 0, 0, 0);
    list->setSpacing(4);
    scroll->setWidget(content);
    content->setAutoFillBackground(false);   // setWidget() turns it on; the page shows through instead
    outer->addWidget(scroll, 1);

    auto *title = new QLabel(tr("Settings"), column);
    title->setFont(Theme::uiFont(28, QFont::DemiBold));
    title->setContentsMargins(0, 0, 0, 14);
    list->addWidget(title);

    // App theme
    auto *themeCard = new SettingCard(tr("App theme"), tr("Select which app theme to display"), column);
    m_themeValue = new DimLabel(QString(), themeCard, 14);
    m_themeValue->setWordWrap(false);
    themeCard->addHeaderWidget(m_themeValue);
    QVBoxLayout *themeBody = themeCard->makeExpandable(false);
    m_light = new QRadioButton(tr("Light"), themeCard);
    m_dark = new QRadioButton(tr("Dark"), themeCard);
    m_system = new QRadioButton(tr("Use system setting"), themeCard);
    for (QRadioButton *r : { m_light, m_dark, m_system }) {
        r->setFont(Theme::uiFont(14));
        themeBody->addWidget(r);
    }
    list->addWidget(themeCard);
    auto themeSlot = [this](Theme::Mode mode, const QString &name) {
        return [this, mode, name](bool on) {
            if (on) {
                m_themeValue->setText(name);
                if (!m_loading)
                    emit themeChanged(mode);
            }
        };
    };
    connect(m_light, &QRadioButton::toggled, this, themeSlot(Theme::Mode::Light, tr("Light")));
    connect(m_dark, &QRadioButton::toggled, this, themeSlot(Theme::Mode::Dark, tr("Dark")));
    connect(m_system, &QRadioButton::toggled, this, themeSlot(Theme::Mode::System, tr("Use system setting")));

    // Text formatting
    list->addWidget(sectionLabel(tr("Text formatting"), column));
    auto *fontCard = new SettingCard(tr("Font"), QString(), column);
    QVBoxLayout *fontBody = fontCard->makeExpandable(true);
    auto fontRow = [&](const QString &label, QComboBox *combo) {
        auto *row = new QHBoxLayout;
        auto *text = new QLabel(label, fontCard);
        text->setFont(Theme::uiFont(14));
        row->addWidget(text, 1);
        row->addWidget(combo);
        fontBody->addLayout(row);
    };
    m_family = makeCombo(fontCard, 240);
    m_style = makeCombo(fontCard, 240);
    m_size = makeCombo(fontCard, 240);
    const QStringList families = QFontDatabase::families();
    for (const QString &family : families) {
        if (!family.startsWith(u'.') && !QFontDatabase::isPrivateFamily(family))
            m_family->addItem(family);
    }
    for (int size : kSizes)
        m_size->addItem(QString::number(size), size);
    fontRow(tr("Family"), m_family);
    fontRow(tr("Style"), m_style);
    fontRow(tr("Size"), m_size);
    m_preview = new QLabel(tr("The sound of ocean waves calms my soul."), fontCard);
    m_preview->setMinimumHeight(64);
    m_preview->setAlignment(Qt::AlignCenter);
    fontBody->addWidget(m_preview);
    list->addWidget(fontCard);
    connect(m_family, &QComboBox::currentTextChanged, this, [this](const QString &family) {
        if (m_loading)
            return;
        populateStyles(family, m_style->currentText());
        emitFont();
    });
    connect(m_style, &QComboBox::currentTextChanged, this, [this] { if (!m_loading) emitFont(); });
    connect(m_size, &QComboBox::currentTextChanged, this, [this] { if (!m_loading) emitFont(); });

    auto *wrapCard = new SettingCard(tr("Word wrap"), tr("Fit text within window by default"), column);
    m_wrapState = new QLabel(wrapCard);
    m_wrapState->setFont(Theme::uiFont(14));
    m_wrap = new ToggleSwitch(wrapCard);
    wrapCard->addHeaderWidget(m_wrapState);
    wrapCard->addHeaderWidget(m_wrap);
    list->addWidget(wrapCard);
    connect(m_wrap, &QAbstractButton::toggled, this, [this](bool on) {
        m_wrapState->setText(on ? tr("On") : tr("Off"));
        if (!m_loading)
            emit wordWrapChanged(on);
    });

    // Opening Notepad
    list->addWidget(sectionLabel(tr("Opening Notepad"), column));
    auto *openingCard = new SettingCard(tr("Opening files"), tr("Choose where your files are opened"), column);
    m_opening = makeCombo(openingCard, 240);
    m_opening->addItem(tr("Open in a new tab"));
    m_opening->addItem(tr("Open in a new window"));
    openingCard->addHeaderWidget(m_opening);
    list->addWidget(openingCard);
    connect(m_opening, &QComboBox::currentIndexChanged, this, [this](int index) {
        if (!m_loading)
            emit openInNewTabChanged(index == 0);
    });

    auto *startCard = new SettingCard(tr("When Notepad starts"), QString(), column);
    m_startValue = new DimLabel(QString(), startCard, 14);
    m_startValue->setWordWrap(false);
    startCard->addHeaderWidget(m_startValue);
    QVBoxLayout *startBody = startCard->makeExpandable(false);
    m_continue = new QRadioButton(tr("Continue previous session"), startCard);
    m_fresh = new QRadioButton(tr("Start new session and discard unsaved changes"), startCard);
    for (QRadioButton *r : { m_continue, m_fresh }) {
        r->setFont(Theme::uiFont(14));
        startBody->addWidget(r);
    }
    list->addWidget(startCard);
    connect(m_continue, &QRadioButton::toggled, this, [this](bool on) {
        m_startValue->setText(on ? tr("Continue previous session") : tr("Start new session"));
        if (!m_loading)
            emit continueSessionChanged(on);
    });

    // About
    list->addWidget(sectionLabel(tr("About this app"), column));
    auto *about = new QLabel(tr("Notepad for Linux %1").arg(QStringLiteral(NOTEPAD_VERSION)), column);
    about->setFont(Theme::uiFont(14));
    list->addWidget(about);
    list->addWidget(new DimLabel(
        tr("A native clone of the Windows 11 Notepad, built with Qt %1 and running on %2. "
           "It is an independent work-alike and is not affiliated with or endorsed by Microsoft.")
            .arg(QString::fromLatin1(qVersion()), QSysInfo::prettyProductName()), column));
    list->addStretch(1);
}

void SettingsPage::setValues(const AppSettings &settings)
{
    m_loading = true;
    (settings.theme == Theme::Mode::Light ? m_light : settings.theme == Theme::Mode::Dark ? m_dark : m_system)
        ->setChecked(true);
    const int familyIndex = m_family->findText(settings.font.family, Qt::MatchFixedString);
    m_family->setCurrentIndex(qMax(0, familyIndex));
    QString style = QStringLiteral("Regular");
    const QString family = m_family->currentText();
    const QStringList styles = QFontDatabase::styles(family);
    for (const QString &s : styles) {
        if (QFontDatabase::weight(family, s) == settings.font.weight
            && QFontDatabase::italic(family, s) == settings.font.italic) {
            style = s;
            break;
        }
    }
    populateStyles(family, style);
    int sizeIndex = m_size->findData(settings.font.pointSize);
    if (sizeIndex < 0) {
        m_size->addItem(QString::number(settings.font.pointSize), settings.font.pointSize);
        sizeIndex = m_size->count() - 1;
    }
    m_size->setCurrentIndex(sizeIndex);
    m_wrap->setChecked(settings.wordWrap);
    m_wrapState->setText(settings.wordWrap ? tr("On") : tr("Off"));
    m_opening->setCurrentIndex(settings.openInNewTab ? 0 : 1);
    (settings.continueSession ? m_continue : m_fresh)->setChecked(true);
    m_startValue->setText(settings.continueSession ? tr("Continue previous session") : tr("Start new session"));
    m_loading = false;
    updatePreview();
}

void SettingsPage::populateStyles(const QString &family, const QString &preferred)
{
    const bool wasLoading = m_loading;
    m_loading = true;
    QStringList styles = QFontDatabase::styles(family);
    if (styles.isEmpty())
        styles = { QStringLiteral("Regular"), QStringLiteral("Italic"), QStringLiteral("Bold"),
                   QStringLiteral("Bold Italic") };
    m_style->clear();
    m_style->addItems(styles);
    int index = m_style->findText(preferred, Qt::MatchFixedString);
    if (index < 0)
        index = m_style->findText(QStringLiteral("Regular"), Qt::MatchFixedString);
    m_style->setCurrentIndex(qMax(0, index));
    m_loading = wasLoading;
}

void SettingsPage::emitFont()
{
    FontChoice font;
    font.family = m_family->currentText();
    font.pointSize = m_size->currentData().toInt() > 0 ? m_size->currentData().toInt() : 11;
    const QString style = m_style->currentText();
    if (QFontDatabase::styles(font.family).isEmpty()) {
        font.weight = style.contains(QLatin1String("Bold")) ? 700 : 400;
        font.italic = style.contains(QLatin1String("Italic"));
    } else {
        font.weight = QFontDatabase::weight(font.family, style);
        font.italic = QFontDatabase::italic(font.family, style);
    }
    updatePreview();
    emit fontChanged(font);
}

void SettingsPage::updatePreview()
{
    QFont f(m_family->currentText());
    const QString style = m_style->currentText();
    if (!QFontDatabase::styles(f.family()).isEmpty()) {
        f.setWeight(QFont::Weight(QFontDatabase::weight(f.family(), style)));
        f.setItalic(QFontDatabase::italic(f.family(), style));
    }
    const int points = m_size->currentData().toInt() > 0 ? m_size->currentData().toInt() : 11;
    f.setPixelSize(qRound(points * 96.0 / 72.0));
    m_preview->setFont(f);
}

void SettingsPage::keyPressEvent(QKeyEvent *event)
{
    if (event->key() == Qt::Key_Escape)
        emit backRequested();
    else
        QWidget::keyPressEvent(event);
}

void SettingsPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), C().mica);
}
