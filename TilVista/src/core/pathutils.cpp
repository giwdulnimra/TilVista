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

QString scriptDir()   { return QCoreApplication::applicationDirPath(); }
QString kaivoDir()    { return QDir::cleanPath(scriptDir()+"/optegnelser/kaivo"); }
QString kaivoDbPath() { return kaivoDir()+"/tilvista_dirs.json"; }
QString logDir()      { return QDir::cleanPath(scriptDir()+"/optegnelser"); }

QString shujukoPath(const QString& name)
{ return kaivoDir()+'/'+safeFilename(name)+"_shujuko.json"; }

QString safeFilename(const QString& name) {
    static const QString bad = R"(\/:*?"<>|)";
    QString out; out.reserve(name.size());
    for (QChar c : name) out += bad.contains(c) ? QChar('_') : c;
    while (out.startsWith('.')) out.remove(0,1);
    return out.isEmpty() ? QStringLiteral("entry") : out;
}

QString autoEntryName(const QString& dirPath) {
    return QDir(dirPath).dirName()+'_'+
           QDateTime::currentDateTime().toString("yyMMdd");
}

const QStringList& imageSuffixes() {
    // ── Format notes ─────────────────────────────────────────────────────────
    // .heic/.heif  – Apple format (iPhone photos). Requires:
    //                Windows: HEVC Video Extensions (free, MS Store)
    //                Linux:   libheif + Qt image plugin
    //                macOS:   native, works out of the box
    // .avif        – Requires Qt 6.5+ with qt-avif-image-plugin or libavif.
    //                https://github.com/novomesk/qt-avif-image-plugin
    // .webp        – Supported natively since Qt 5.14.
    // .jxl         – JPEG XL; no official Qt plugin yet (2025).
    // .tga         – Targa; supported by Qt natively.
    // .svg         – Vector; QSvgRenderer – included but will display at
    //                raster quality when scaled via QImageReader.
    static const QStringList s = {
        // Common raster
        ".jpg", ".jpeg", ".png", ".bmp", ".gif",
        // TIFF family
        ".tif", ".tiff",
        // Modern formats
        ".webp",   // Qt 5.14+, widely supported
        ".avif",   // Qt 6.5+ with plugin – graceful fallback if absent
        ".heic",   // Apple HEIC – needs OS/platform codec
        ".heif",   // HEIF container (broader than HEIC)
        // Less common but Qt-supported
        ".tga",    // Targa
        ".pbm", ".pgm", ".ppm",  // PBM family
        ".xbm", ".xpm",          // X11 bitmap/pixmap
        ".svg",                  // Vector (rendered via QSvgRenderer)
        ".ico",                  // Windows icon
    };
    return s;
}

const QStringList& videoSuffixes() {
    // .mov – Apple QuickTime; heavily used alongside HEIC on iPhone.
    // .hevc / .h265 – raw HEVC stream; rare as standalone file.
    static const QStringList s = {
        ".mp4", ".mkv", ".avi", ".mov", ".wmv",
        ".flv", ".webm", ".m4v", ".ts", ".mts",
        ".3gp",   // Mobile video
        ".ogv",   // Ogg video
    };
    return s;
}

QStringList allMediaSuffixes() {
    return imageSuffixes() + videoSuffixes();
}

void openPath(const QString& path)
{ QDesktopServices::openUrl(QUrl::fromLocalFile(path)); }

void selectInExplorer(const QString& path) {
    if (!QFileInfo::exists(path)) { openPath(QFileInfo(path).absolutePath()); return; }
#if defined(Q_OS_WIN)
    QProcess::startDetached("explorer",{"/select,",QDir::toNativeSeparators(path)});
#elif defined(Q_OS_MAC)
    QProcess::startDetached("open",{"-R",path});
#else
    struct FM { const char* exe; QStringList args; };
    for (const FM& fm : {
        FM{"nautilus",{"--select",path}},
        FM{"dolphin", {"--select",path}},
        FM{"nemo",    {path}},
        FM{"thunar",  {QFileInfo(path).absolutePath()}}})
    {
        if (!QStandardPaths::findExecutable(QLatin1String(fm.exe)).isEmpty()) {
            QProcess::startDetached(QLatin1String(fm.exe), fm.args); return;
        }
    }
    openPath(QFileInfo(path).absolutePath());
#endif
}

void preventSleep() {
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS | ES_SYSTEM_REQUIRED);
#endif
}
void restoreSleep() {
#ifdef Q_OS_WIN
    SetThreadExecutionState(ES_CONTINUOUS);
#endif
}

} // namespace TV
