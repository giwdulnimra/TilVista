#pragma once
#include <QString>
#include <QStringList>

namespace TV {

QString scriptDir();

/// optegnelser/kaivo/
QString kaivoDir();

/// tilvista_dirs.json  (DB1 index)
QString kaivoDbPath();

/// <safe_name>_shujuko.json for a given kaivo entry name
QString shujukoPath(const QString& kaivoEntryName);

/// optegnelser/
QString logDir();

QString safeFilename(const QString& name);
QString autoEntryName(const QString& dirPath);

const QStringList& imageSuffixes();
const QStringList& videoSuffixes();

void openPath(const QString& path);
void selectInExplorer(const QString& path);

void preventSleep();
void restoreSleep();

} // namespace TV
