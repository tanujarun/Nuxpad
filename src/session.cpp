#include "session.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>
#include <QStandardPaths>

namespace Session {

namespace {

QString bufferPath(const QString &id)
{
    return directory() + QStringLiteral("/buffers/") + id + QStringLiteral(".txt");
}

} // namespace

QString directory()
{
    return QStandardPaths::writableLocation(QStandardPaths::GenericStateLocation) + QStringLiteral("/nuxpad");
}

QList<SessionTab> load(int *activeIndex)
{
    QList<SessionTab> tabs;
    *activeIndex = 0;
    QFile file(directory() + QStringLiteral("/session.json"));
    if (!file.open(QIODevice::ReadOnly))
        return tabs;
    const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
    *activeIndex = root.value("active").toInt();
    const QJsonArray array = root.value("tabs").toArray();
    for (const QJsonValue &value : array) {
        const QJsonObject o = value.toObject();
        SessionTab tab;
        tab.id = o.value("id").toString();
        tab.path = o.value("path").toString();
        tab.encoding = o.value("encoding").toInt();
        tab.lineEnding = o.value("lineEnding").toInt();
        tab.modified = o.value("modified").toBool();
        tab.hasBuffer = o.value("buffer").toBool();
        tab.cursor = o.value("cursor").toInt();
        tab.anchor = o.value("anchor").toInt();
        if (tab.id.isEmpty())
            continue;
        if (tab.hasBuffer) {
            QFile buffer(bufferPath(tab.id));
            if (buffer.open(QIODevice::ReadOnly))
                tab.text = QString::fromUtf8(buffer.readAll());
            else
                tab.hasBuffer = false;
            if (tab.text.isNull())
                tab.text = QStringLiteral("");
        }
        tabs.append(tab);
    }
    return tabs;
}

void save(const QList<SessionTab> &tabs, int activeIndex)
{
    QDir().mkpath(directory() + QStringLiteral("/buffers"));

    QJsonArray array;
    QStringList keep;
    for (const SessionTab &tab : tabs) {
        QJsonObject o;
        o.insert("id", tab.id);
        o.insert("path", tab.path);
        o.insert("encoding", tab.encoding);
        o.insert("lineEnding", tab.lineEnding);
        o.insert("modified", tab.modified);
        o.insert("buffer", tab.hasBuffer);
        o.insert("cursor", tab.cursor);
        o.insert("anchor", tab.anchor);
        array.append(o);
        if (tab.hasBuffer) {
            keep.append(tab.id + QStringLiteral(".txt"));
            if (!tab.text.isNull() || !QFile::exists(bufferPath(tab.id))) {
                QSaveFile buffer(bufferPath(tab.id));
                if (buffer.open(QIODevice::WriteOnly)) {
                    buffer.write(tab.text.toUtf8());
                    buffer.commit();
                }
            }
        }
    }

    QJsonObject root;
    root.insert("version", 1);
    root.insert("active", activeIndex);
    root.insert("tabs", array);
    QSaveFile file(directory() + QStringLiteral("/session.json"));
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(root).toJson(QJsonDocument::Compact));
        file.commit();
    }

    // drop the buffers of tabs that no longer exist
    QDir buffers(directory() + QStringLiteral("/buffers"));
    const QStringList files = buffers.entryList({ QStringLiteral("*.txt") }, QDir::Files);
    for (const QString &name : files) {
        if (!keep.contains(name))
            buffers.remove(name);
    }
}

} // namespace Session
