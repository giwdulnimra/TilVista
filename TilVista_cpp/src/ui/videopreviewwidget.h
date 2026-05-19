#pragma once
#include "previewlabel.h"

#include <QStackedWidget>
#include <QStringList>
#include <QTimer>

class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class QThread;
class VideoFramesWorker;

/// Three-tier video preview:
///   Page 0  –  QVideoWidget (real playback, muted loop)
///   Page 1  –  PreviewLabel with ffmpeg frame cycling (10 s interval)
///              or static icon when ffmpeg is unavailable
class VideoPreviewWidget : public QStackedWidget
{
    Q_OBJECT
public:
    explicit VideoPreviewWidget(QWidget* parent = nullptr);
    ~VideoPreviewWidget() override;

    /// Start video preview for *path*.
    void play(const QString& path);

    /// Display a static pixmap instead (images, icons).
    void showPixmap(const QPixmap& pm, const QString& label = {});

    /// Stop all playback and clear the widget.
    void clearPreview();

signals:
    void statusText(const QString& text);

private slots:
    void onFramesReady(bool ok, const QStringList& framePaths);
    void nextFrame();

private:
    void stop();
    void cleanupTmpDir();

    // Page 0: real player
    QMediaPlayer*  m_player   = nullptr;
    QAudioOutput*  m_audio    = nullptr;
    QVideoWidget*  m_videoW   = nullptr;

    // Page 1: frame label
    PreviewLabel*  m_frameLabel;
    QTimer*        m_cycleTimer;
    QStringList    m_framePaths;
    int            m_frameIdx  = 0;
    QString        m_tmpDir;

    // Frame-extraction thread
    QThread*           m_vidThread  = nullptr;
    VideoFramesWorker* m_vidWorker  = nullptr;

    static constexpr int kFrameCycleMs = 10'000;
};
