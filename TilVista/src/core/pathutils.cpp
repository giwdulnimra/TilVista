#include "pathutils.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDesktopServices>
#include <QDir>
#include <QFileInfo>
#include <QProcess>
#include <QStandardPaths>
#include <QUrl>

#ifdef Q_OS_WIN
#  include <windows.h>
#endif

namespace TV {

// ── Paths ─────────────────────────────────────────────────────────────────────

QString scriptDir()
{
    return QCoreApplication::applicationDirPath();
}

QString kaivoDir()   // v0.5.00: lowercase "kaivo"
{
    return QDir::cleanPath(scriptDir() + "/optegnelser/kaivo");
}

QString kaivoDbPath()
{
    return kaivoDir() + "/tilvista_dirs.json";
}

QString reviewDbPath()
{
    return kaivoDir() + "/tilvista_review.json";
}

QString logDir()
{
    return QDir::cleanPath(scriptDir() + "/optegnelser");
}

// ── Filename sanitisation ─────────────────────────────────────────────────────

QString safeFilename(const QString& name)
{
    static const QString forbidden = R"(\/:*?"<>|)";
    QString out;
    out.reserve(name.size());
    for (QChar c : name)
        out += forbidden.contains(c) ? QChar('_') : c;
    while (out.startsWith('.'))
        out.remove(0, 1);
    return out.isEmpty() ? QStringLiteral("entry") : out;
}

// ── Auto entry name ───────────────────────────────────────────────────────────

QString autoEntryName(const QString& dirPath)
{
    const QString folder = QDir(dirPath).dirName();
    const QString date   = QDateTime::currentDateTime().toString("yyMMdd");
    return folder + '_' + date;
}

// ── File-type sets ────────────────────────────────────────────────────────────

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
        ".flv", ".webm", ".m4v", ".ts", ".mts"
    };
    return s;
}

// ── Cross-platform open ───────────────────────────────────────────────────────

void openPath(const QString& path)
{
    QDesktopServices::openUrl(QUrl::fromLocalFile(path));
}

void selectInExplorer(const QString& path)
{
    if (!QFileInfo::exists(path)) {
        openPath(QFileInfo(path).absolutePath());
        return;
    }

#if defined(Q_OS_WIN)
    QProcess::startDetached("explorer",
        { "/select,", QDir::toNativeSeparators(path) });

#elif defined(Q_OS_MAC)
    QProcess::startDetached("open", { "-R", path });

#else
    // Linux: try common file managers in order of preference
    struct FM { const char* exe; QStringList args; };
    const QList<FM> managers = {
        { "nautilus", { "--select", path } },
        { "dolphin",  { "--select", path } },
        { "nemo",     { path              } },
        { "thunar",   { QFileInfo(path).absolutePath() } },
    };
    for (const auto& fm : managers) {
        if (!QStandardPaths::findExecutable(QLatin1String(fm.exe)).isEmpty()) {
            QProcess::startDetached(QLatin1String(fm.exe), fm.args);
            return;
        }
    }
    // Last resort: just open the directory
    openPath(QFileInfo(path).absolutePath());
#endif
}

// ── Sleep prevention ──────────────────────────────────────────────────────────

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
