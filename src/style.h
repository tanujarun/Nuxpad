#pragma once

#include <QMenu>
#include <QProxyStyle>

class QStyleOptionSlider;
class QStyleOptionMenuItem;

// WinUI rendering of the controls Notepad uses: menu bar, flyout menus,
// overlay scroll bars, buttons, text boxes, combo boxes, radio buttons.
class FluentStyle : public QProxyStyle
{
    Q_OBJECT
public:
    FluentStyle();

    int styleHint(StyleHint hint, const QStyleOption *option = nullptr, const QWidget *widget = nullptr,
                  QStyleHintReturn *returnData = nullptr) const override;
    int pixelMetric(PixelMetric metric, const QStyleOption *option = nullptr,
                    const QWidget *widget = nullptr) const override;
    QSize sizeFromContents(ContentsType type, const QStyleOption *option, const QSize &size,
                           const QWidget *widget) const override;
    QRect subControlRect(ComplexControl control, const QStyleOptionComplex *option, SubControl subControl,
                         const QWidget *widget) const override;
    void drawPrimitive(PrimitiveElement element, const QStyleOption *option, QPainter *painter,
                       const QWidget *widget) const override;
    void drawControl(ControlElement element, const QStyleOption *option, QPainter *painter,
                     const QWidget *widget) const override;
    void drawComplexControl(ComplexControl control, const QStyleOptionComplex *option, QPainter *painter,
                            const QWidget *widget) const override;
    void polish(QWidget *widget) override;
    using QProxyStyle::polish;

private:
    void drawScrollBar(const QStyleOptionSlider *option, QPainter *painter, const QWidget *widget) const;
    void drawMenuItem(const QStyleOptionMenuItem *option, QPainter *painter, const QWidget *widget) const;
};

// A flyout menu: rounded panel with a drop shadow to its right and below.
class FluentMenu : public QMenu
{
    Q_OBJECT
public:
    static constexpr int Shadow = 12;

    explicit FluentMenu(const QString &title = QString(), QWidget *parent = nullptr);
    FluentMenu *addFluentMenu(const QString &title);
};
