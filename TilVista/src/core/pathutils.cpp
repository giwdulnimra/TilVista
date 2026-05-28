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
QString kaivoDir()    { return QDir::cleanPath(scriptDir() + "/optegnelser/kaivo"); }
QString kaivoDbPath() { return kaivoDir() + "/tilvista_dirs.json"; }
QString logDir()      { return QDir::cleanPath(scriptDir() + "/optegnelser"); }

QString shujukoPath(const QString& kaivoEntryName)
{
    return kaivoDir() + '/' + safeFilename(kaivoEntryName) + "_shujuko.json";
}

QString safeFilename(const QString& name)
{
    static const QString bad = R"(\/:*?"<>|)";
    QString out; out.reserve(name.size());
    for (QChar c : name) out += bad.contains(c) ? QChar('_') : c;
    while (out.startsWith('.')) out.remove(0, 1);
    return out.isEmpty() ? QStringLiteral("entry") : out;
}

QString autoEntryName(const QString& dirPath)
{
    return QDir(dirPath).dirName() + '_' +
           QDateTime::currentDateTime().toString("yyMMdd");
}

const QStringList& imageSuffixes()
{
    static const QStringList s = {
        ".jpg",".jpeg",".png",".bmp",".tif",".tiff",".gif",".webp"};
    return s;
}
const QStringList& videoSuffixes()
{
    static const QStringList s = {
        ".mp4",".mkv",".avi",".mov",".wmv",".flv",".webm",".m4v",".ts",".mts"};
    return s;
}

void openPath(const QString& path)
{ QDesktopServices::openUrl(QUrl::fromLocalFile(path)); }

void selectInExplorer(const QString& path)
{
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
