#include "core/Category.h"

#include <QHash>

namespace das {
namespace {

struct CategoryTable
{
    QHash<QString, FileCategory> map;

    CategoryTable()
    {
        const auto add = [this](FileCategory category, std::initializer_list<const char *> exts) {
            for (const char *e : exts)
                map.insert(QString::fromLatin1(e), category);
        };

        add(FileCategory::Image,
            {".jpg", ".jpeg", ".jpe", ".jfif", ".png", ".gif", ".bmp", ".dib", ".tif", ".tiff",
             ".webp", ".heic", ".heif", ".avif", ".ico", ".cur", ".svg", ".svgz", ".raw", ".cr2",
             ".cr3", ".nef", ".arw", ".dng", ".orf", ".rw2", ".psd", ".ai", ".eps", ".tga",
             ".exr", ".hdr", ".jxl", ".jp2", ".pcx", ".wdp", ".jxr"});

        add(FileCategory::Video,
            {".mp4", ".m4v", ".mkv", ".avi", ".mov", ".wmv", ".flv", ".f4v", ".webm", ".mpg",
             ".mpeg", ".mpe", ".m2v", ".ts", ".m2ts", ".mts", ".vob", ".rmvb", ".rm", ".3gp",
             ".3g2", ".asf", ".ogv", ".divx", ".mxf", ".swf"});

        add(FileCategory::Audio,
            {".mp3", ".wav", ".flac", ".aac", ".m4a", ".m4b", ".ogg", ".oga", ".opus", ".wma",
             ".ape", ".alac", ".aiff", ".aif", ".dsf", ".dff", ".mid", ".midi", ".amr", ".ac3",
             ".dts", ".cda", ".mka", ".ra"});

        add(FileCategory::Document,
            {".pdf", ".doc", ".docx", ".docm", ".dot", ".dotx", ".rtf", ".odt", ".xls", ".xlsx",
             ".xlsm", ".xlsb", ".csv", ".ods", ".ppt", ".pptx", ".pptm", ".pps", ".ppsx", ".odp",
             ".txt", ".log", ".md", ".markdown", ".epub", ".mobi", ".azw3", ".djvu", ".chm",
             ".one", ".vsd", ".vsdx", ".xps", ".oxps", ".wps", ".et", ".dps", ".pub", ".tex"});

        add(FileCategory::Code,
            {".c", ".h", ".cc", ".cpp", ".cxx", ".hpp", ".hxx", ".hh", ".inl", ".ipp", ".cs",
             ".java", ".kt", ".kts", ".scala", ".go", ".rs", ".swift", ".m", ".mm", ".py", ".pyw",
             ".pyi", ".rb", ".php", ".pl", ".pm", ".lua", ".r", ".jl", ".dart", ".js", ".mjs",
             ".cjs", ".jsx", ".ts", ".tsx", ".vue", ".svelte", ".html", ".htm", ".xhtml", ".css",
             ".scss", ".sass", ".less", ".json", ".jsonc", ".json5", ".xml", ".xsd", ".xsl",
             ".yaml", ".yml", ".toml", ".ini", ".cfg", ".conf", ".properties", ".env", ".sql",
             ".sh", ".bash", ".zsh", ".fish", ".ps1", ".psm1", ".psd1", ".bat", ".cmd", ".vbs",
             ".asm", ".s", ".f90", ".pas", ".vb", ".gradle", ".cmake", ".pro", ".pri", ".qml",
             ".ui", ".qrc", ".mk", ".makefile", ".dockerfile", ".gitignore", ".diff", ".patch",
             ".ipynb", ".sln", ".vcxproj", ".csproj", ".proto", ".graphql"});

        add(FileCategory::Archive,
            {".zip", ".rar", ".7z", ".tar", ".gz", ".tgz", ".bz2", ".tbz", ".xz", ".txz", ".zst",
             ".lz", ".lzma", ".lzh", ".arj", ".cab", ".iso", ".img", ".wim", ".esd", ".dmg",
             ".pkg", ".jar", ".war", ".apk", ".ipa", ".xapk", ".appx", ".msix", ".z", ".ace",
             ".uue", ".001"});

        add(FileCategory::Executable,
            {".exe", ".msi", ".msu", ".msp", ".com", ".scr", ".dll", ".sys", ".ocx", ".cpl",
             ".ax", ".efi", ".appref-ms", ".lnk", ".url", ".gadget", ".hta", ".jse", ".wsf",
             ".wsh", ".msc", ".reg", ".inf"});
    }
};

const CategoryTable &table()
{
    static const CategoryTable instance;
    return instance;
}

} // namespace

FileCategory categoryForExtension(const QString &extension)
{
    if (extension.isEmpty())
        return FileCategory::Other;

    QString key = extension.toLower();
    if (!key.startsWith(QLatin1Char('.')))
        key.prepend(QLatin1Char('.'));

    return table().map.value(key, FileCategory::Other);
}

const QList<FileCategory> &orderedCategories()
{
    static const QList<FileCategory> list{
        FileCategory::All,      FileCategory::Image,    FileCategory::Video,
        FileCategory::Audio,    FileCategory::Document, FileCategory::Code,
        FileCategory::Archive,  FileCategory::Executable, FileCategory::Other};
    return list;
}

QString fallbackTypeDescription(const QString &extension)
{
    QString bare = extension;
    if (bare.startsWith(QLatin1Char('.')))
        bare.remove(0, 1);
    if (bare.isEmpty())
        return QStringLiteral("未知类型");
    return bare.toUpper() + QStringLiteral(" 文件");
}

QString categoryDisplayName(FileCategory category)
{
    switch (category) {
    case FileCategory::All:        return QStringLiteral("全部");
    case FileCategory::Image:      return QStringLiteral("图片");
    case FileCategory::Video:      return QStringLiteral("视频");
    case FileCategory::Audio:      return QStringLiteral("音频");
    case FileCategory::Document:   return QStringLiteral("文档");
    case FileCategory::Code:       return QStringLiteral("代码");
    case FileCategory::Archive:    return QStringLiteral("压缩包");
    case FileCategory::Executable: return QStringLiteral("可执行");
    case FileCategory::Other:      return QStringLiteral("其他");
    }
    return QStringLiteral("其他");
}

QString setStatusDisplayName(SetStatus status)
{
    switch (status) {
    case SetStatus::Success:          return QStringLiteral("已生效");
    case SetStatus::NeedsUserConfirm: return QStringLiteral("需手动确认");
    case SetStatus::Failed:           return QStringLiteral("失败");
    case SetStatus::Skipped:          return QStringLiteral("无需更改");
    }
    return QStringLiteral("未知");
}

QString setMethodDisplayName(SetMethod method)
{
    switch (method) {
    case SetMethod::Silent: return QStringLiteral("静默设置");
    case SetMethod::Guided: return QStringLiteral("系统引导");
    case SetMethod::None:   return QStringLiteral("未执行");
    }
    return QStringLiteral("未执行");
}

} // namespace das
