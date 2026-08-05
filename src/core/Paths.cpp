#include "core/Paths.h"

#include <QDir>
#include <QStandardPaths>

namespace das {
namespace {

QString ensureDir(const QString &path)
{
    QDir dir(path);
    if (!dir.exists())
        dir.mkpath(QStringLiteral("."));
    return QDir::toNativeSeparators(dir.absolutePath());
}

} // namespace

QString appDataDir()
{
    QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (base.isEmpty())
        base = QDir::homePath() + QStringLiteral("/DefaultAppSetter");
    return ensureDir(base);
}

QString backupDir()
{
    return ensureDir(appDataDir() + QStringLiteral("/backups"));
}

QString logDir()
{
    return ensureDir(appDataDir() + QStringLiteral("/logs"));
}

QString logFilePath()
{
    return logDir() + QStringLiteral("\\app.log");
}

} // namespace das
