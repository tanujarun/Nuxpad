// Test build only. Runs the semicolon-separated steps in NOTEPAD_DEV_SCRIPT:
// real key and mouse events through Qt's input pipeline, window grabs to PNG
// and state dumps, so behaviour and looks can be checked without touching the
// user's own pointer or keyboard.
#include "editor.h"
#include "mainwindow.h"
#include "tabbar.h"

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QMenu>
#include <QMenuBar>
#include <QPushButton>
#include <QTest>
#include <QTextStream>
#include <QTimer>
#include <QWindow>

namespace {

class DevScript : public QObject
{
public:
    DevScript(MainWindow *window, const QString &script)
        : QObject(window), m_window(window), m_steps(script.split(u';', Qt::SkipEmptyParts))
    {
        QTimer::singleShot(400, this, &DevScript::next);
    }

private:
    static QString plain(QString text)
    {
        const qsizetype tab = text.indexOf(u'\t');
        if (tab >= 0)
            text.truncate(tab);
        return text.remove(u'&');
    }

    QWidget *front() const
    {
        if (QWidget *w = QApplication::activePopupWidget())
            return w;
        if (QWidget *w = QApplication::activeModalWidget())
            return w;
        if (QWidget *w = QApplication::activeWindow())
            return w;
        return m_window;
    }

    void next()
    {
        if (m_steps.isEmpty())
            return;
        const QString step = m_steps.takeFirst().trimmed();
        const QString command = step.section(u' ', 0, 0);
        const QString argument = step.section(u' ', 1);
        int delay = 150;
        // the offscreen platform does not hand activation around by itself
        if (!QApplication::activePopupWidget()) {
            QWidget *modal = QApplication::activeModalWidget();
            QWidget *wanted = modal ? modal : QApplication::activeWindow() ? nullptr : m_window;
            if (wanted && !wanted->isActiveWindow()) {
                wanted->activateWindow();
                (void)QTest::qWaitForWindowActive(wanted, 1000);
            }
        }

        if (command == "wait") {
            delay = argument.toInt();
        } else if (command == "grab") {
            front()->grab().save(argument);
        } else if (command == "grabmain") {
            m_window->grab().save(argument);
        } else if (command == "menu") {
            const auto actions = m_window->menuBar()->actions();
            for (QAction *a : actions) {
                if (plain(a->text()) == argument)
                    m_window->menuBar()->setActiveAction(a);
            }
        } else if (command == "trigger") {
            const auto actions = m_window->findChildren<QAction *>();
            for (QAction *a : actions) {
                if (plain(a->text()) == argument) {
                    QTimer::singleShot(0, a, &QAction::trigger);
                    break;
                }
            }
        } else if (command == "click") {
            const auto buttons = front()->findChildren<QAbstractButton *>();
            for (QAbstractButton *b : buttons) {
                if (b->isVisible() && (plain(b->text()) == argument || b->toolTip() == argument)) {
                    QTimer::singleShot(0, b, [b] { b->click(); });
                    break;
                }
            }
        } else if (command == "key") {
            QWidget *target = front();
            QTimer::singleShot(0, target, [target, argument] {
                QTest::keySequence(target->window()->windowHandle(), QKeySequence(argument));
            });
        } else if (command == "type") {
            QString text = argument;
            text.replace(QLatin1String("\\n"), QLatin1String("\n")).replace(QLatin1String("\\t"), QLatin1String("\t"));
            if (QWidget *target = QApplication::focusWidget()) {
                for (const QChar c : std::as_const(text)) {
                    if (c == u'\n')
                        QTest::keyClick(target, Qt::Key_Return);
                    else if (c == u'\t')
                        QTest::keyClick(target, Qt::Key_Tab);
                    else
                        QTest::keyClicks(target, QString(c));
                }
            }
        } else if (command == "mouse") {
            // mouse <x> <y> [right|double|move|middle] on the front window
            const QStringList parts = argument.split(u' ');
            QWidget *top = front();
            const QPoint pos(parts.value(0).toInt(), parts.value(1).toInt());
            QWidget *child = top->childAt(pos);
            QWidget *target = child ? child : top;
            const QPoint local = target->mapFrom(top, pos);
            const QString mode = parts.value(2);
            QTimer::singleShot(0, target, [target, local, mode] {
                if (mode == "right") {
                    QContextMenuEvent event(QContextMenuEvent::Mouse, local, target->mapToGlobal(local));
                    QApplication::sendEvent(target, &event);
                } else if (mode == "double") {
                    QTest::mouseDClick(target, Qt::LeftButton, {}, local);
                } else if (mode == "move") {
                    QTest::mouseMove(target, local);
                } else if (mode == "middle") {
                    QTest::mouseClick(target, Qt::MiddleButton, {}, local);
                } else {
                    QTest::mouseClick(target, Qt::LeftButton, {}, local);
                }
            });
        } else if (command == "tab") {
            m_window->tabBar()->setCurrentIndex(argument.toInt());
        } else if (command == "clip") {
            QApplication::clipboard()->setText(argument);
        } else if (command == "dump") {
            QFile file(argument);
            if (file.open(QIODevice::WriteOnly | QIODevice::Append)) {
                QTextStream out(&file);
                Editor *e = m_window->editor();
                QString selection = e ? e->textCursor().selectedText() : QString();
                selection.replace(QChar::ParagraphSeparator, QLatin1String("\\n"));
                QString text = e ? e->toPlainText() : QString();
                text.replace(u'\n', QLatin1String("\\n"));
                out << "title=" << m_window->windowTitle() << " | tabs=[" << m_window->describeTabs() << "]"
                    << " | ln=" << (e ? e->currentLine() : 0) << " col=" << (e ? e->currentColumn() : 0)
                    << " | sel=[" << selection << "] | text=[" << text << "]"
                    << " | font=" << (e ? e->font().family() : QString()) << ' ' << (e ? e->font().pixelSize() : 0)
                    << "px | wrap=" << (e ? e->wordWrap() : false) << "\n";
            }
        } else if (command == "quit") {
            QTimer::singleShot(0, m_window, [this] { m_window->close(); });
            return;
        }
        QTimer::singleShot(delay, this, &DevScript::next);
    }

    MainWindow *m_window;
    QStringList m_steps;
};

} // namespace

void installDevScript(MainWindow *window, const QString &script)
{
    if (!script.isEmpty())
        new DevScript(window, script);
}
