#include "pathutils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QUrl>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace TV {

// ── Paths ────────────────────────────────────────────────────────────────────

static QString scriptDir()
{
    return QCoreApplication::applicationDirPath();
}

QString kaivoDir()
{
    return QDir::cleanPath(scriptDir() + "/optegnelser/Kaivo");
}

QString kaivoDbPath()
{
    return kaivoDir() + "/tilvista_dirs.json";
}

QString logDir()
{
    return QDir::cleanPath(scriptDir() + "/optegnelser");
}

// ── Filename sanitisation ────────────────────────────────────────────────────

QString safeFilename(const QString& name)
{
    static const QString forbidden = R"(\/:*?"<>|)";
    QString out;
    out.reserve(name.size());
    for (QChar c : name)
        out += forbidden.contains(c) ? QChar('_') : c;
    // strip leading dots (hidden on Unix)
    while (out.startsWith('.'))
        out.remove(0, 1);
    return out.isEmpty() ? QStringLiteral("entry") : out;
}

// ── Auto entry name ──────────────────────────────────────────────────────────

QString autoEntryName(const QString& dirPath)
{
    const QString folder = QDir(dirPath).dirName();
    const QString date   = QDateTime::currentDateTime().toString("yyMMdd");
    return folder + '_' + date;
}

// ── File-type sets ───────────────────────────────────────────────────────────

const QStringList& imageSuffixes()
{
    static const QStringList s = {
        ".jpg", ".jpeg", ".png", ".bmp",
        ".tif", ".tiff", ".gif", ".webp"
    };
    return s;
}

const QStringList& videoSuffixes()
{
    static const QStringList s = {
        ".mp4", ".mkv", ".avi", ".mov", ".wmv",
        ".flv", ".webm", ".m4v", ".ts",  ".mts"
    };
    return s;
}

// ── Cross-platform open ──────────────────────────────────────────────────────

void openPath(const QString& path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

// ── Sleep prevention (Windows only) ─────────────────────────────────────────

void preventSleep()
{
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#endif
}

void restoreSleep()
{
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

} // namespace TV
