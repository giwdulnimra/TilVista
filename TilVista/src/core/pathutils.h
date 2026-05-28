#pragma once
#include <QString>
#include <QStringList>

namespace TV {

QString scriptDir();
QString kaivoDir();
QString kaivoDbPath();
QString shujukoPath(const QString& kaivoEntryName);
QString logDir();
QString safeFilename(const QString& name);
QString autoEntryName(const QString& dirPath);

/// Image suffixes – includes HEIC/HEIF, AVIF, WebP.
/// HEIC requires OS codec on Windows (HEVC Video Extensions from MS Store).
/// AVIF requires Qt 6.5+ with AVIF plugin or libavif.
const QStringList& imageSuffixes();

/// Video suffixes – includes MOV (used by iPhone alongside HEIC).
const QStringList& videoSuffixes();

/// All media suffixes combined (image + video) for convenience.
QStringList allMediaSuffixes();

void openPath(const QString& path);
void selectInExplorer(const QString& path);
void preventSleep();
void restoreSleep();

} // namespace TV
