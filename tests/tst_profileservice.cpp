#include "core/Paths.h"
#include "services/ProfileService.h"

#include <QDir>
#include <QFile>
#include <QTest>
#include <QTemporaryDir>

using namespace das;

class tst_ProfileService : public QObject
{
    Q_OBJECT

private slots:
    void roundTrip_exportImport();
    void entryFields_preserved();
};

static ProfileEntryList sampleEntries()
{
    ProfileEntryList entries;
    entries.append(ProfileEntry{QStringLiteral(".png"), QStringLiteral("PNGFile"), false});
    entries.append(ProfileEntry{QStringLiteral(".jpg"), QStringLiteral("PhotoViewer.File"), false});
    entries.append(ProfileEntry{QStringLiteral("http"), QStringLiteral("ChromeHTML"), true});
    return entries;
}

void tst_ProfileService::roundTrip_exportImport()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("profile.json"));
    ProfileService svc;

    QString err;
    QVERIFY(svc.exportProfile(path, sampleEntries(), &err));
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QVERIFY(QFile::exists(path));

    const ProfileEntryList back = svc.importProfile(path, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(back.size(), sampleEntries().size());
}

void tst_ProfileService::entryFields_preserved()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());

    const QString path = dir.filePath(QStringLiteral("profile.json"));
    ProfileService svc;

    QString err;
    QVERIFY(svc.exportProfile(path, sampleEntries(), &err));

    const ProfileEntryList back = svc.importProfile(path, &err);
    QVERIFY2(err.isEmpty(), qPrintable(err));
    QCOMPARE(back.size(), 3);

    // 校验逐项内容（顺序也应保持一致）
    QCOMPARE(back.at(0).target, QStringLiteral(".png"));
    QCOMPARE(back.at(0).isProtocol, false);
    QCOMPARE(back.at(0).progId, QStringLiteral("PNGFile"));

    QCOMPARE(back.at(1).target, QStringLiteral(".jpg"));
    QCOMPARE(back.at(1).progId, QStringLiteral("PhotoViewer.File"));

    QCOMPARE(back.at(2).target, QStringLiteral("http"));
    QCOMPARE(back.at(2).isProtocol, true);
    QCOMPARE(back.at(2).progId, QStringLiteral("ChromeHTML"));
}

QTEST_MAIN(tst_ProfileService)

#include "tst_profileservice.moc"
