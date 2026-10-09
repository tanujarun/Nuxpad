#include "textfile.h"

#include <QCoreApplication>
#include <QFile>
#include <QStringConverter>
#include <QStringDecoder>
#include <QStringEncoder>

namespace TextFile {

namespace {

// "ANSI" is the Western Windows code page.
QStringDecoder ansiDecoder()
{
    QStringDecoder d("windows-1252");
    return d.isValid() ? std::move(d) : QStringDecoder(QStringConverter::Latin1);
}

QStringEncoder ansiEncoder()
{
    QStringEncoder e("windows-1252");
    return e.isValid() ? std::move(e) : QStringEncoder(QStringConverter::Latin1);
}

bool isValidUtf8(const QByteArray &data)
{
    QStringDecoder d(QStringConverter::Utf8, QStringConverter::Flag::Stateless);
    const QString s = d.decode(data);
    Q_UNUSED(s);
    return !d.hasError();
}

// Notepad's IsTextUnicode() heuristic, reduced to the part that matters:
// UTF-16 text without a BOM is mostly ASCII with every other byte zero.
Encoding guessUtf16(const QByteArray &data)
{
    const qsizetype n = qMin<qsizetype>(data.size() & ~1, 4096);
    if (n < 2)
        return AutoDetect;
    qsizetype evenZero = 0, oddZero = 0;
    for (qsizetype i = 0; i < n; i += 2) {
        if (data[i] == 0) ++evenZero;
        if (data[i + 1] == 0) ++oddZero;
    }
    const qsizetype pairs = n / 2;
    if (oddZero * 10 >= pairs * 9 && evenZero * 10 <= pairs)
        return Utf16LE;
    if (evenZero * 10 >= pairs * 9 && oddZero * 10 <= pairs)
        return Utf16BE;
    return AutoDetect;
}

Encoding detect(const QByteArray &data, int *bomLength)
{
    *bomLength = 0;
    if (data.startsWith("\xEF\xBB\xBF")) {
        *bomLength = 3;
        return Utf8Bom;
    }
    if (data.startsWith("\xFF\xFE")) {
        *bomLength = 2;
        return Utf16LE;
    }
    if (data.startsWith("\xFE\xFF")) {
        *bomLength = 2;
        return Utf16BE;
    }
    const Encoding utf16 = guessUtf16(data);
    if (utf16 != AutoDetect)
        return utf16;
    return isValidUtf8(data) ? Utf8 : Ansi;
}

LineEnding detectLineEnding(const QString &text, LineEnding fallback)
{
    for (qsizetype i = 0; i < text.size(); ++i) {
        const QChar c = text.at(i);
        if (c == u'\n')
            return Lf;
        if (c == u'\r')
            return (i + 1 < text.size() && text.at(i + 1) == u'\n') ? Crlf : Cr;
    }
    return fallback;
}

} // namespace

Document load(const QString &path, Encoding forced)
{
    Document doc;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        doc.error = file.errorString();
        return doc;
    }
    const QByteArray data = file.readAll();
    if (file.error() != QFileDevice::NoError) {
        doc.error = file.errorString();
        return doc;
    }

    int bom = 0;
    const Encoding detected = detect(data, &bom);
    doc.encoding = forced == AutoDetect ? detected : forced;
    // A forced encoding still skips the byte-order mark that belongs to it.
    if (forced != AutoDetect) {
        bom = 0;
        if ((forced == Utf8 || forced == Utf8Bom) && data.startsWith("\xEF\xBB\xBF")) bom = 3;
        if (forced == Utf16LE && data.startsWith("\xFF\xFE")) bom = 2;
        if (forced == Utf16BE && data.startsWith("\xFE\xFF")) bom = 2;
    }
    const QByteArrayView body = QByteArrayView(data).sliced(bom);

    switch (doc.encoding) {
    case Utf16LE: doc.text = QStringDecoder(QStringConverter::Utf16LE).decode(body); break;
    case Utf16BE: doc.text = QStringDecoder(QStringConverter::Utf16BE).decode(body); break;
    case Ansi: doc.text = ansiDecoder().decode(body); break;
    default: doc.text = QStringDecoder(QStringConverter::Utf8).decode(body); break;
    }

    doc.lineEnding = detectLineEnding(doc.text, Lf);
    doc.text.replace(QLatin1String("\r\n"), QLatin1String("\n"));
    doc.text.replace(u'\r', u'\n');
    doc.ok = true;
    return doc;
}

bool save(const QString &path, const QString &text, Encoding encoding, LineEnding lineEnding, QString *error)
{
    QString out = text;
    if (lineEnding == Crlf)
        out.replace(u'\n', QLatin1String("\r\n"));
    else if (lineEnding == Cr)
        out.replace(u'\n', u'\r');

    QByteArray bytes;
    switch (encoding) {
    case Ansi:
        bytes = ansiEncoder().encode(out);
        break;
    case Utf16LE:
        bytes = QByteArray("\xFF\xFE", 2) + QStringEncoder(QStringConverter::Utf16LE).encode(out);
        break;
    case Utf16BE:
        bytes = QByteArray("\xFE\xFF", 2) + QStringEncoder(QStringConverter::Utf16BE).encode(out);
        break;
    case Utf8Bom:
        bytes = QByteArray("\xEF\xBB\xBF", 3) + out.toUtf8();
        break;
    default:
        bytes = out.toUtf8();
        break;
    }

    // Written in place so that ownership, permissions and symlinks survive.
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error)
            *error = file.errorString();
        return false;
    }
    if (file.write(bytes) != bytes.size() || !file.flush()) {
        if (error)
            *error = file.errorString();
        return false;
    }
    file.close();
    return true;
}

bool fitsAnsi(const QString &text)
{
    QStringEncoder e = ansiEncoder();
    const QByteArray bytes = e.encode(text);
    if (e.hasError())
        return false;
    return ansiDecoder().decode(bytes) == text;
}

QString encodingName(Encoding encoding)
{
    switch (encoding) {
    case Ansi: return QStringLiteral("ANSI");
    case Utf16LE: return QStringLiteral("UTF-16 LE");
    case Utf16BE: return QStringLiteral("UTF-16 BE");
    case Utf8Bom: return QStringLiteral("UTF-8 with BOM");
    default: return QStringLiteral("UTF-8");
    }
}

QString lineEndingName(LineEnding lineEnding)
{
    switch (lineEnding) {
    case Crlf: return QStringLiteral("Windows (CRLF)");
    case Cr: return QStringLiteral("Macintosh (CR)");
    default: return QStringLiteral("Unix (LF)");
    }
}

} // namespace TextFile
