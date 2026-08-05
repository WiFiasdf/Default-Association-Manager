#include "platform/win/IconProvider.h"

#include "core/Logging.h"
#include "platform/win/RegKey.h"

#include <QImage>
#include <QMutexLocker>
#include <QPainter>
#include <QPixmap>
#include <QRunnable>
#include <QThread>

#include <objbase.h>
#include <shellapi.h>

namespace das::win {

namespace {

constexpr int kCacheSize   = 512;
constexpr int kIconPixels  = 32;

QImage extractIcon(const QString &key)
{
    SHFILEINFOW info{};
    UINT        flags = SHGFI_ICON | SHGFI_LARGEICON;
    QString     path  = key;

    if (key.startsWith(QLatin1Char('.'))) {
        // 按扩展名取类型图标，不要求文件真实存在
        path = QStringLiteral("dummy") + key;
        flags |= SHGFI_USEFILEATTRIBUTES;
    }

    const DWORD_PTR ok = ::SHGetFileInfoW(RegKey::wstr(path), FILE_ATTRIBUTE_NORMAL, &info,
                                          sizeof(info), flags);
    if (!ok || !info.hIcon)
        return {};

    QImage image = QImage::fromHICON(info.hIcon);
    ::DestroyIcon(info.hIcon);

    if (!image.isNull() && (image.width() != kIconPixels || image.height() != kIconPixels)) {
        image = image.scaled(kIconPixels, kIconPixels, Qt::KeepAspectRatio, Qt::SmoothTransformation);
    }
    return image;
}

QIcon buildPlaceholder()
{
    QPixmap pixmap(kIconPixels, kIconPixels);
    pixmap.fill(Qt::transparent);

    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);
    painter.setBrush(QColor(0, 103, 192, 28));
    painter.drawRoundedRect(QRectF(3, 3, kIconPixels - 6, kIconPixels - 6), 6, 6);
    painter.setBrush(QColor(0, 103, 192, 70));
    painter.drawRoundedRect(QRectF(8, 12, kIconPixels - 16, 3), 1.5, 1.5);
    painter.drawRoundedRect(QRectF(8, 18, kIconPixels - 20, 3), 1.5, 1.5);
    painter.end();

    return QIcon(pixmap);
}

class IconTask : public QRunnable
{
public:
    IconTask(IconProvider *owner, QString key) : m_owner(owner), m_key(std::move(key))
    {
        setAutoDelete(true);
    }

    void run() override
    {
        const HRESULT hr = ::CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
        const QImage  image = extractIcon(m_key);
        if (SUCCEEDED(hr))
            ::CoUninitialize();

        QMetaObject::invokeMethod(m_owner, "onIconLoaded", Qt::QueuedConnection,
                                  Q_ARG(QString, m_key), Q_ARG(QImage, image));
    }

private:
    IconProvider *m_owner;
    QString       m_key;
};

} // namespace

// ---------------------------------------------------------------------------

IconProvider::IconProvider(QObject *parent)
    : QObject(parent), m_cache(kCacheSize), m_placeholder(buildPlaceholder())
{
    m_pool = new QThreadPool(this);
    m_pool->setMaxThreadCount(qBound(2, QThread::idealThreadCount() / 2, 4));
    m_pool->setExpiryTimeout(30000);
}

IconProvider &IconProvider::instance()
{
    static IconProvider provider;
    return provider;
}

QIcon IconProvider::iconFor(const QString &key)
{
    if (key.isEmpty())
        return m_placeholder;

    {
        QMutexLocker locker(&m_mutex);
        if (QIcon *cached = m_cache.object(key))
            return *cached;
        if (m_pending.contains(key))
            return m_placeholder;
        m_pending.insert(key);
    }

    request(key);
    return m_placeholder;
}

QIcon IconProvider::iconForBlocking(const QString &key)
{
    if (key.isEmpty())
        return m_placeholder;

    {
        QMutexLocker locker(&m_mutex);
        if (QIcon *cached = m_cache.object(key))
            return *cached;
    }

    const QImage image = extractIcon(key);
    if (image.isNull())
        return m_placeholder;

    QIcon icon(QPixmap::fromImage(image));
    {
        QMutexLocker locker(&m_mutex);
        m_cache.insert(key, new QIcon(icon));
        m_pending.remove(key);
    }
    return icon;
}

void IconProvider::request(const QString &key)
{
    m_pool->start(new IconTask(this, key));
}

void IconProvider::onIconLoaded(const QString &key, const QImage &image)
{
    {
        QMutexLocker locker(&m_mutex);
        m_pending.remove(key);
        QIcon icon = image.isNull() ? m_placeholder : QIcon(QPixmap::fromImage(image));
        m_cache.insert(key, new QIcon(icon));
    }
    emit iconReady(key);
}

void IconProvider::clear()
{
    QMutexLocker locker(&m_mutex);
    m_cache.clear();
    m_pending.clear();
}

} // namespace das::win
