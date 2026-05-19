#pragma once
#include <QString>
#include <QStringList>

namespace TV {

/// Directory that holds all Kaivo files: <scriptDir>/optegnelser/Kaivo/
QString kaivoDir();

/// Full path to the JSON index file.
QString kaivoDbPath();

/// Log/bookmark directory: <scriptDir>/optegnelser/
QString logDir();

/// Sanitise *name* so it can be used as a cross-platform filename.
/// Replaces  \ / : * ? " < > |  with underscores; strips leading dots.
QString safeFilename(const QString& name);

/// "<lowest_folder>_<yymmdd>"  e.g. "Fotos_250508"
QString autoEntryName(const QString& dirPath);

/// image file extensions (lower-case, with dot)
const QStringList& imageSuffixes();

/// video file extensions (lower-case, with dot)
const QStringList& videoSuffixes();

/// Open a file or directory with the OS default handler (cross-platform).
void openPath(const QString& path);

/// Windows only: prevent the system from going to sleep.
void preventSleep();
void restoreSleep();

} // namespace TV
