#include "core/Logging.h"

#include "core/Paths.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QTextStream>

Q_LOGGING_CATEGORY(logReg, "app.reg")
Q_LOGGING_CATEGORY(logAssoc, "app.assoc")
Q_LOGGING_CATEGORY(logUi, "app.ui")

namespace das {
namespace {

constexpr qint64 kMaxLogBytes  = 2 * 1024 * 1024;
constexpr int    kRotateKeep   = 3;

QMutex            g_mutex;
QFile            *g_file    = nullptr;
QTextStream      *g_stream  = nullptr;
QtMessageHandler  g_previous = nullptr;

const char *levelName(QtMsgType type)
{
    switch (type) {
    case QtDebugMsg:    return "DEBUG";
    case QtInfoMsg:     return "INFO ";
    case QtWarningMsg:  return "WARN ";
    case QtCriticalMsg: return "ERROR";
    case QtFatalMsg:    return "FATAL";
    }
    return "?????";
}

void rotateIfNeeded()
{
    if (!g_file)
        return;
    if (g_file->size() < kMaxLogBytes)
        return;

    if (g_stream) {
        g_stream->flush();
        delete g_stream;
        g_stream = nullptr;
    }
    const QString base = g_file->fileName();
    g_file->close();

    // app.log.3 丢弃，app.log.2 -> app.log.3 ...
    QFile::remove(base + QStringLiteral(".%1").arg(kRotateKeep));
    for (int i = kRotateKeep - 1; i >= 1; --i) {
        const QString from = base + QStringLiteral(".%1").arg(i);
        const QString to   = base + QStringLiteral(".%1").arg(i + 1);
        if (QFile::exists(from))
            QFile::rename(from, to);
    }
    QFile::rename(base, base + QStringLiteral(".1"));

    if (g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text))
        g_stream = new QTextStream(g_file);
}

void messageHandler(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    {
        QMutexLocker locker(&g_mutex);
        if (g_stream) {
            rotateIfNeeded();
            if (g_stream) {
                *g_stream << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss.zzz"))
                          << QStringLiteral(" [") << QString::fromLatin1(levelName(type))
                          << QStringLiteral("] [")
                          << QString::fromLatin1(context.category ? context.category : "default")
                          << QStringLiteral("] ") << message << Qt::endl;
            }
        }
    }
    if (g_previous)
        g_previous(type, context, message);
}

} // namespace

void installLogging()
{
    QMutexLocker locker(&g_mutex);
    if (g_file)
        return;

    g_file = new QFile(logFilePath());
    if (!g_file->open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        delete g_file;
        g_file = nullptr;
        return;
    }
    g_stream = new QTextStream(g_file);
    g_previous = qInstallMessageHandler(messageHandler);
}

void shutdownLogging()
{
    QMutexLocker locker(&g_mutex);
    qInstallMessageHandler(g_previous);
    g_previous = nullptr;
    if (g_stream) {
        g_stream->flush();
        delete g_stream;
        g_stream = nullptr;
    }
    if (g_file) {
        g_file->close();
        delete g_file;
        g_file = nullptr;
    }
}

QString redactSid(const QString &sid)
{
    if (sid.size() <= 4)
        return QStringLiteral("****");
    return QStringLiteral("S-...-") + sid.right(4);
}

QString redactHash(const QString &hash)
{
    if (hash.size() <= 4)
        return QStringLiteral("****");
    return hash.left(4) + QStringLiteral("...");
}

} // namespace das
