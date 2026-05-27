#include "videopreviewwidget.h"
#include "workers/videoframesworker.h"

#include <QDir>
#include <QLabel>
#include <QThread>
#include <QUrl>
#include <QVBoxLayout>

#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimediaWidgets/QVideoWidget>

VideoPreviewWidget::VideoPreviewWidget(QWidget* parent)
    : QStackedWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    // ── Page 0: real video playback ──────────────────────────────────────────
    m_videoW  = new QVideoWidget;
    m_player  = new QMediaPlayer(this);
    m_audio   = new QAudioOutput(this);
    m_audio->setVolume(0.0f);                // muted preview
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoW);
    m_player->setLoops(QMediaPlayer::Infinite);
    addWidget(m_videoW);                    // index 0

    // ── Page 1: frame-cycling label ──────────────────────────────────────────
    m_frameLabel = new PreviewLabel;
    addWidget(m_frameLabel);                // index 1

    m_cycleTimer = new QTimer(this);
    m_cycleTimer->setInterval(kFrameCycleMs);
    connect(m_cycleTimer, &QTimer::timeout, this, &VideoPreviewWidget::nextFrame);

    setCurrentIndex(1);
}

VideoPreviewWidget::~VideoPreviewWidget()
{
    stop();
    cleanupTmpDir();
}

// ── Public ────────────────────────────────────────────────────────────────────

void VideoPreviewWidget::play(const QString& path)
{
    stop();
    cleanupTmpDir();

    // Try QMediaPlayer first
    if (m_player) {
        setCurrentIndex(0);
        m_player->setSource(QUrl::fromLocalFile(path));
        m_player->play();
        emit statusText(QStringLiteral("Video  (QMediaPlayer – muted)"));
        return;
    }

    // ffmpeg frame-cycling fallback
    setCurrentIndex(1);
    emit statusText(QStringLiteral("Video  – extracting frames…"));

    auto* worker = new VideoFramesWorker(path);
    m_tmpDir     = worker->tmpDir();
    m_vidThread  = new QThread;
    m_vidWorker  = worker;

    worker->moveToThread(m_vidThread);
    connect(m_vidThread, &QThread::started,  worker, &VideoFramesWorker::run);
    connect(worker, &VideoFramesWorker::resultReady, this, &VideoPreviewWidget::onFramesReady);
    connect(worker, &VideoFramesWorker::resultReady, m_vidThread, &QThread::quit);
    connect(worker, &VideoFramesWorker::resultReady, worker, &QObject::deleteLater);
    connect(m_vidThread, &QThread::finished, m_vidThread, &QObject::deleteLater);
    m_vidThread->start();
}

void VideoPreviewWidget::showPixmap(const QPixmap& pm, const QString& label)
{
    stop();
    cleanupTmpDir();
    setCurrentIndex(1);
    m_frameLabel->setSourcePixmap(pm);
    emit statusText(label);
}

void VideoPreviewWidget::clearPreview()
{
    stop();
    cleanupTmpDir();
    setCurrentIndex(1);
    m_frameLabel->clearPreview();
    emit statusText({});
}

// ── Private ───────────────────────────────────────────────────────────────────

void VideoPreviewWidget::stop()
{
    m_cycleTimer->stop();
    if (m_player)
        m_player->stop();
    if (m_vidThread) {
        m_vidThread->quit();
        m_vidThread->wait(2000);
        m_vidThread  = nullptr;
        m_vidWorker  = nullptr;
    }
}

void VideoPreviewWidget::cleanupTmpDir()
{
    if (!m_tmpDir.isEmpty()) {
        QDir(m_tmpDir).removeRecursively();
        m_tmpDir.clear();
    }
    m_framePaths.clear();
    m_frameIdx = 0;
}

void VideoPreviewWidget::onFramesReady(bool ok, const QStringList& framePaths)
{
    m_vidThread = nullptr;
    m_vidWorker = nullptr;

    if (ok && !framePaths.isEmpty()) {
        m_framePaths = framePaths;
        m_frameIdx   = 0;
        nextFrame();            // show first frame immediately
        m_cycleTimer->start();
        emit statusText(
            QString("Video  (ffmpeg, %1 frames, %2 s cycle)")
                .arg(framePaths.size())
                .arg(kFrameCycleMs / 1000));
    } else {
        emit statusText(
            QStringLiteral("Video  (no preview – install ffmpeg or PyQt6-Qt6 Multimedia)"));
    }
}

void VideoPreviewWidget::nextFrame()
{
    if (m_framePaths.isEmpty()) return;
    m_frameIdx = (m_frameIdx + 1) % m_framePaths.size();
    const QPixmap pm(m_framePaths.at(m_frameIdx));
    if (!pm.isNull())
        m_frameLabel->setSourcePixmap(pm);
}
