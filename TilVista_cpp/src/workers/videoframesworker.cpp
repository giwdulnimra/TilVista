#include "videoframesworker.h"

#include <QDir>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>

const QList<int> VideoFramesWorker::frameTimes = {0, 10, 20, 30, 40, 50};

VideoFramesWorker::VideoFramesWorker(const QString& videoPath, QObject* parent)
    : QObject(parent), m_videoPath(videoPath)
{
    // Create a persistent temp dir (not auto-deleted – caller cleans up)
    const QString base = QDir::tempPath() + "/tilvista_vid";
    QDir().mkpath(base);
    m_tmpDir = base;
}

void VideoFramesWorker::run()
{
    // Requires ffmpeg in PATH
    if (QStandardPaths::findExecutable("ffmpeg").isEmpty()) {
        emit resultReady(false, {});
        return;
    }

    QStringList framePaths;
    for (int t : frameTimes) {
        const QString outPath = QString("%1/f%2.jpg").arg(m_tmpDir).arg(t, 3, 10, QChar('0'));

        QProcess proc;
        proc.start("ffmpeg", {
            "-i", m_videoPath,
            "-ss", QString::number(t),
            "-vframes", "1",
            "-f", "image2",
            "-y", outPath
        });
        proc.waitForFinished(12000);   // 12 s timeout per frame

        if (proc.exitCode() == 0 && QFile::exists(outPath))
            framePaths << outPath;
    }

    emit resultReady(!framePaths.isEmpty(), framePaths);
}
