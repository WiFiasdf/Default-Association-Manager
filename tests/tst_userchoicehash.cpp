#include "platform/win/UserChoiceHash.h"

#include <QTest>
#include <QVector>

using namespace das::win::userchoice;

class tst_UserChoiceHash : public QObject
{
    Q_OBJECT

private slots:
    void determinism();
    void length();
    void lowerCaseInvariant();
    void variesWithInput();
    void buildBaseInfo_includesTimestamp();
};

void tst_UserChoiceHash::determinism()
{
    const QString target = QStringLiteral(".png");
    const QString sid    = QStringLiteral("S-1-5-21-1234-5678-9012-1001");
    const QString progId = QStringLiteral("PNGFile");
    const quint64 t      = 0x01DA2B3C4D5E6F70ULL;

    const QString a = compute(target, sid, progId, t);
    const QString b = compute(target, sid, progId, t);
    QCOMPARE(a, b);
    QVERIFY(!a.isEmpty());
}

void tst_UserChoiceHash::length()
{
    // 8 字节输入 → Base64 固定为 12 个字符（含一个 '=' 补齐）
    const QString hash = compute(QStringLiteral(".pdf"),
                                 QStringLiteral("S-1-5-21-1-1"),
                                 QStringLiteral("AcroExch.Document.DC"),
                                 0x0123456789ABCDEFULL);
    QCOMPARE(hash.length(), 12);
    QVERIFY(hash.endsWith(QLatin1Char('=')) || hash.length() == 12);
}

void tst_UserChoiceHash::lowerCaseInvariant()
{
    const QString sid    = QStringLiteral("S-1-5-21-9-9");
    const QString progId = QStringLiteral("Foo.Bar");
    const quint64 t      = 0x0A0B0C0D0E0F1011ULL;

    // 输入串应为小写，故扩展名大小写差异不应影响结果
    const QString upper = compute(QStringLiteral(".PNG"), sid, progId, t);
    const QString lower = compute(QStringLiteral(".png"), sid, progId, t);
    QCOMPARE(upper, lower);

    // 同样验证 baseInfo 层级
    QCOMPARE(buildBaseInfo(QStringLiteral(".PNG"), sid, progId, t),
             buildBaseInfo(QStringLiteral(".png"), sid, progId, t));
    QVERIFY(buildBaseInfo(QStringLiteral(".png"), sid, progId, t).isLower());
}

void tst_UserChoiceHash::variesWithInput()
{
    const QString sid = QStringLiteral("S-1-5-21-2-2");
    const quint64 t   = 0x1122334455667788ULL;

    const QString base = compute(QStringLiteral(".txt"), sid, QStringLiteral("AppA"), t);
    const QString other = compute(QStringLiteral(".txt"), sid, QStringLiteral("AppB"), t);
    QVERIFY(base != other);

    const QString differentTarget = compute(QStringLiteral(".doc"), sid, QStringLiteral("AppA"), t);
    QVERIFY(base != differentTarget);
}

void tst_UserChoiceHash::buildBaseInfo_includesTimestamp()
{
    const QString sid    = QStringLiteral("S-1-5-21-3-3");
    const QString progId = QStringLiteral("Prog");
    const QString t1     = buildBaseInfo(QStringLiteral(".zip"), sid, progId, 0x000000000000FFFFULL);
    const QString t2     = buildBaseInfo(QStringLiteral(".zip"), sid, progId, 0xFFFFFFFFFFFF0000ULL);
    QVERIFY(t1.contains(QStringLiteral("ffff")));
    QVERIFY(t2.contains(QStringLiteral("ffff0000")));
    QVERIFY(t1 != t2);
}

QTEST_MAIN(tst_UserChoiceHash)

#include "tst_userchoicehash.moc"
