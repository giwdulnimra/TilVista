#pragma once
#include <QList>
#include <QObject>
#include <QString>
#include <QStringList>

/// Extracts a handful of still frames from a video via ffmpeg.
///
/// Used by both VideoPreviewWidget (SattumaPic/AleaVue-adjacent quick
/// preview) and MadoludusWindow's FF/Impression flip-book.
///
/// v0.5.42: frame selection is now duration-aware instead of the old fixed
/// {0,10,20,30,40,50}s list (which produced too few/zero frames for short
/// clips, and always the same handful of moments for long ones regardless
/// of actual length):
///   – IntervalSeconds: one frame every `value` seconds across the whole
///     clip (Madoludus config: "1/sec", "1/2sec", "1/5sec", "1/10sec").
///   – FixedCount: exactly `value` frames, evenly spaced across the whole
///     clip regardless of its length (Madoludus config: "1/N frames").
/// Duration is probed via `ffmpeg -i <path>` (parsing the "Duration:"
/// line from stderr) – no ffprobe dependency. If probing fails, falls back
/// to a small fixed list so there is still *some* preview. Output is
/// capped at 60 frames either way, so e.g. "1/sec" on a long video doesn't
/// spawn hundreds of ffmpeg calls.
class VideoFramesWorker : public QObject {
    Q_OBJECT
public:
    enum class Mode { IntervalSeconds, FixedCount };

    explicit VideoFramesWorker(const QString& videoPath,
                                Mode mode = Mode::IntervalSeconds,
                                double value = 10.0,
                                QObject* parent = nullptr);
    const QString& tmpDir() const { return m_tmpDir; }
public slots: void run();
signals: void resultReady(bool ok, QStringList framePaths);
private:
    static double probeDurationSeconds(const QString& path);
    QList<double> computeTimestamps(double durationSeconds) const;

    QString m_videoPath, m_tmpDir;
    Mode    m_mode;
    double  m_value;
};
