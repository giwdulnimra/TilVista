#pragma once
#include <QString>
#include <QStringList>

namespace TV {

/// Directory where the executable lives.
QString scriptDir();

/// Directory that holds all kaivo files: <scriptDir>/optegnelser/kaivo/
QString kaivoDir();

/// Full path to the JSON index file (DB1).
QString kaivoDbPath();

/// Full path to the review DB JSON (DB2).
QString reviewDbPath();

/// Log/bookmark directory: <scriptDir>/optegnelser/
QString logDir();

/// Sanitise *name* so it can be used as a cross-platform filename.
QString safeFilename(const QString& name);

/// "<lowest_folder>_<yymmdd>"  e.g. "Fotos_250508"
QString autoEntryName(const QString& dirPath);

/// image file extensions (lower-case, with dot)
const QStringList& imageSuffixes();

/// video file extensions (lower-case, with dot)
const QStringList& videoSuffixes();

/// Open a file or directory with the OS default handler (cross-platform).
void openPath(const QString& path);

/// Open the parent directory of *path* and highlight the file.
/// Windows: explorer /select,  macOS: open -R  Linux: nautilus/dolphin/thunar
void selectInExplorer(const QString& path);

/// Windows only: prevent the system from going to sleep.
void preventSleep();
void restoreSleep();

} // namespace TV
