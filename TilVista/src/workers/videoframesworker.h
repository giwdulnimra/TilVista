#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

/// Extracts still frames from a video via ffmpeg (if available in PATH).
/// Frames are placed in a temporary directory owned by the caller; the
/// caller is responsible for deleting it when done.
class VideoFramesWorker : public QObject
{
    Q_OBJECT
public:
    explicit VideoFramesWorker(const QString& videoPath,
                               QObject* parent = nullptr);
    ~VideoFramesWorker() override = default;

    /// Temporary directory created in the constructor; valid until deleted.
    const QString& tmpDir() const { return m_tmpDir; }

public slots:
    void run();

signals:
    void resultReady(bool ok, QStringList framePaths);

private:
    QString m_videoPath;
    QString m_tmpDir;

    static const QList<int> frameTimes;   ///< seconds at which to extract
};
