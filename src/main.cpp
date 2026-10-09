#include "mainwindow.h"
#include "session.h"
#include "theme.h"

#include <QApplication>
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QLocalServer>
#include <QLocalSocket>
#include <QLockFile>
#include <QSettings>

#ifdef NUXPAD_DEVHOOKS
void installDevScript(MainWindow *window, const QString &script);
#endif

namespace {

// One name per session directory, so test runs with their own XDG_STATE_HOME
// never talk to the user's running Nuxpad.
QString serverName()
{
    const QByteArray hash = QCryptographicHash::hash(Session::directory().toUtf8(), QCryptographicHash::Sha1).toHex();
    return QStringLiteral("nuxpad-") + QString::fromLatin1(hash.left(12));
}

// Hands the files to the Nuxpad that is already running. False if nobody answers.
bool sendToRunningInstance(const QStringList &paths)
{
    QLocalSocket socket;
    socket.connectToServer(serverName());
    if (!socket.waitForConnected(400))
        return false;
    socket.write(paths.join(u'\n').toUtf8());
    socket.flush();
    socket.waitForBytesWritten(1000);
    socket.disconnectFromServer();
    if (socket.state() != QLocalSocket::UnconnectedState)
        socket.waitForDisconnected(500);
    return true;
}

} // namespace

int main(int argc, char *argv[])
{
    // Nuxpad always wears the Windows look, whatever Qt theme the desktop selects.
    qunsetenv("QT_QPA_PLATFORMTHEME");
    qunsetenv("QT_STYLE_OVERRIDE");
    qputenv("QT_FONT_DPI", "96");

    QApplication app(argc, argv);
    QApplication::setApplicationName(QStringLiteral("nuxpad"));
    QApplication::setOrganizationName(QStringLiteral("nuxpad"));
    QApplication::setApplicationVersion(QStringLiteral(NUXPAD_VERSION));
    QGuiApplication::setDesktopFileName(QStringLiteral("nuxpad"));

    QStringList files;
    bool newWindow = false;
    const QStringList arguments = QApplication::arguments().mid(1);
    for (const QString &argument : arguments) {
        if (argument == QLatin1String("--new-window"))
            newWindow = true;
        else
            files.append(QFileInfo(argument).absoluteFilePath());
    }

    // The first Nuxpad owns the session; it also receives files opened later.
    QDir().mkpath(Session::directory());
    QLockFile lock(Session::directory() + QStringLiteral("/instance.lock"));
    lock.setStaleLockTime(0);
    const bool primary = lock.tryLock(0);
    if (!primary && !newWindow && !files.isEmpty()) {
        const QSettings settings(QSettings::IniFormat, QSettings::UserScope, QStringLiteral("nuxpad"),
                                 QStringLiteral("nuxpad"));
        if (settings.value("OpenInNewTab", true).toBool() && sendToRunningInstance(files))
            return 0;
    }

    Theme::init();

    MainWindow window(primary);
    window.show();

    QLocalServer server;
    if (primary) {
        QLocalServer::removeServer(serverName());
        server.listen(serverName());
        QObject::connect(&server, &QLocalServer::newConnection, &window, [&server, &window] {
            while (QLocalSocket *socket = server.nextPendingConnection()) {
                QObject::connect(socket, &QLocalSocket::disconnected, &window, [socket, &window] {
                    const QStringList paths = QString::fromUtf8(socket->readAll()).split(u'\n', Qt::SkipEmptyParts);
                    socket->deleteLater();
                    window.bringToFront();
                    window.openPaths(paths);
                });
            }
        });
    }

#ifdef NUXPAD_DEVHOOKS
    installDevScript(&window, qEnvironmentVariable("NUXPAD_DEV_SCRIPT"));
#endif

    if (!files.isEmpty())
        window.openPaths(files);

    return QApplication::exec();
}
