#include "videoframesworker.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>
#include <QStandardPaths>

VideoFramesWorker::VideoFramesWorker(const QString& p, Mode mode, double value, QObject* par)
    : QObject(par), m_videoPath(p), m_mode(mode), m_value(value) {
    m_tmpDir = QDir::tempPath()+"/tilvista_vid";
    QDir().mkpath(m_tmpDir);
}

double VideoFramesWorker::probeDurationSeconds(const QString& path)
{
    // No ffprobe dependency: `ffmpeg -i <path>` prints container metadata
    // (incl. "Duration: HH:MM:SS.xx") to stderr and then exits non-zero
    // for lacking an output file – we only care about that stderr text.
    QProcess proc;
    proc.start("ffmpeg", {"-i", path});
    proc.waitForFinished(8000);
    const QString out = QString::fromUtf8(proc.readAllStandardError());
    static const QRegularExpression re(R"(Duration:\s*(\d+):(\d+):(\d+(?:\.\d+)?))");
    const auto m = re.match(out);
    if (!m.hasMatch()) return 0.0;
    return m.captured(1).toDouble() * 3600.0
         + m.captured(2).toDouble() * 60.0
         + m.captured(3).toDouble();
}

QList<double> VideoFramesWorker::computeTimestamps(double durationSeconds) const
{
    constexpr int kMaxFrames = 60;
    QList<double> ts;
    if (durationSeconds <= 0.5) { ts << 0.0; return ts; }

    if (m_mode == Mode::IntervalSeconds) {
        const double step = qMax(0.2, m_value);
        for (double t = 0.0; t < durationSeconds; t += step) ts << t;
        if (ts.isEmpty()) ts << 0.0;
    } else {   // FixedCount – evenly spaced across the whole duration
        const int count = qMax(1, int(m_value));
        if (count == 1) { ts << durationSeconds / 2.0; return ts; }
        for (int i = 0; i < count; ++i)
            ts << durationSeconds * double(i) / double(count - 1);
    }

    if (ts.size() > kMaxFrames) {
        QList<double> capped;
        for (int i = 0; i < kMaxFrames; ++i)
            capped << ts.at(i * (ts.size() - 1) / (kMaxFrames - 1));
        return capped;
    }
    return ts;
}

void VideoFramesWorker::run() {
    if(QStandardPaths::findExecutable("ffmpeg").isEmpty()){
        emit resultReady(false,{}); return;
    }

    const double duration = probeDurationSeconds(m_videoPath);
    const QList<double> times = duration > 0.0
        ? computeTimestamps(duration)
        : QList<double>{0.0, 5.0, 10.0, 15.0, 20.0, 25.0};   // best-effort fallback

    QStringList frames;
    int idx = 0;
    for (double t : times) {
        const QString out = QString("%1/f%2.jpg").arg(m_tmpDir).arg(idx++, 3, 10, QChar('0'));
        QProcess proc;
        proc.start("ffmpeg", {"-i", m_videoPath, "-ss", QString::number(t, 'f', 2),
                              "-vframes", "1", "-f", "image2", "-y", out});
        proc.waitForFinished(12000);
        if (proc.exitCode() == 0 && QFile::exists(out)) frames << out;
    }
    emit resultReady(!frames.isEmpty(), frames);
}
