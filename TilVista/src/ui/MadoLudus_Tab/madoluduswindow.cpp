#include "../madoluduswindow.h"
#include "../madooverlay.h"
#include "core/pathutils.h"
#include "workers/videoframesworker.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QResizeEvent>
#include <QThread>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>
#include <random>
#include <QCloseEvent>
#include <QUrl>
#include <QDir>

#ifndef TV_NO_MULTIMEDIA
#  include <QtMultimedia/QMediaPlayer>
#  include <QtMultimedia/QAudioOutput>
#  include <QtMultimediaWidgets/QVideoWidget>
#endif

// ── Constructor ───────────────────────────────────────────────────────────────

MadoludusWindow::MadoludusWindow(const QStringList&    mediaFiles,
                                  const MadoludusConfig& cfg,
                                  QWidget*              parent)
    : QMainWindow(parent)
    , m_mediaFiles(mediaFiles)
    , m_cfg(cfg)
{
    // Frameless, always-on-top during playback, no taskbar icon
    setWindowFlags(Qt::Window
                 | Qt::FramelessWindowHint
                 | Qt::WindowStaysOnTopHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setStyleSheet("background: black;");
    setMinimumSize(640, 400);

    buildUi();

    if (!m_mediaFiles.isEmpty())
        loadFile(0);
}

MadoludusWindow::~MadoludusWindow()
{
    stopAutoAdvance();
    stopFFExtraction();
}

// ── UI ────────────────────────────────────────────────────────────────────────

void MadoludusWindow::buildUi()
{
    m_central = new QWidget(this);
    setCentralWidget(m_central);

    // Outer: content row (overlay | display) stacked above path label
    auto* outerV = new QVBoxLayout(m_central);
    outerV->setContentsMargins(0, 0, 0, 0);
    outerV->setSpacing(0);

    auto* contentRow = new QHBoxLayout;
    contentRow->setContentsMargins(0, 0, 0, 0);
    contentRow->setSpacing(0);

    // ── Left: vertical control strip ─────────────────────────────────────────
    // MadoOverlay was originally designed as a horizontal bottom bar.
    // For the vertical left-side strip the same widget is re-used but
    // rotated via a fixed-width container; the overlay's own paintEvent
    // still draws the rounded pill, which now reads as a left-side panel.
    // A future refactor could add a proper "orientation" flag to MadoOverlay.
    m_overlay = new MadoOverlay(m_central);
    m_overlay->setFixedWidth(56);
    m_overlay->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Expanding);
    m_overlay->setPaused(m_paused);
    m_overlay->setMuted(m_cfg.muted);
    m_overlay->setFFMode(m_cfg.ffMode);
    m_overlay->setRandomMode(m_cfg.randomMode);
    m_overlay->setSecretMode(m_cfg.secretMode);
    connect(m_overlay, &MadoOverlay::playPauseClicked, this, &MadoludusWindow::onPlayPause);
    connect(m_overlay, &MadoOverlay::prevClicked,      this, &MadoludusWindow::onPrevFile);
    connect(m_overlay, &MadoOverlay::nextClicked,      this, &MadoludusWindow::onNextFile);
    connect(m_overlay, &MadoOverlay::muteClicked,      this, &MadoludusWindow::onMuteToggle);
    connect(m_overlay, &MadoOverlay::ffClicked,        this, &MadoludusWindow::onFFToggle);
    connect(m_overlay, &MadoOverlay::shuffleClicked,   this, &MadoludusWindow::onShuffleToggle);

    contentRow->addWidget(m_overlay);

    // ── Right: display area ───────────────────────────────────────────────────
    auto* displayStack = new QWidget;
    displayStack->setStyleSheet("background: black;");
    displayStack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto* displayLayout = new QVBoxLayout(displayStack);
    displayLayout->setContentsMargins(0,0,0,0);

    m_display = new QLabel;
    m_display->setAlignment(Qt::AlignCenter);
    m_display->setStyleSheet("background: black; color: gray;");
    m_display->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    displayLayout->addWidget(m_display);

#ifndef TV_NO_MULTIMEDIA
    m_videoContainer = new QWidget;
    m_videoContainer->setStyleSheet("background: black;");
    m_videoContainer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_videoContainer->setVisible(false);
    auto* vl = new QVBoxLayout(m_videoContainer);
    vl->setContentsMargins(0,0,0,0);
    m_videoW = new QVideoWidget;
    m_videoW->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vl->addWidget(m_videoW);
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_audio->setVolume(m_cfg.muted ? 0.0f : 0.5f);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoW);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState s){
                if (s == QMediaPlayer::StoppedState) onVideoEnded();
            });
    connect(m_player, &QMediaPlayer::errorOccurred,
            this, [this](QMediaPlayer::Error, const QString& msg){
                m_lblPath->setText(QString("⚠  %1").arg(msg));
            });
    displayLayout->addWidget(m_videoContainer);
#endif

    contentRow->addWidget(displayStack, 1);
    outerV->addLayout(contentRow, 1);

    // ── Bottom: path label ────────────────────────────────────────────────────
    m_lblPath = new QLabel;
    m_lblPath->setStyleSheet("color: #888; font-size: 10px; padding: 2px 6px;"
                              "background: #111;");
    m_lblPath->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_lblPath->setFixedHeight(20);
    outerV->addWidget(m_lblPath);

    // ── Timers ─────────────────────────────────────────────────────────────
    m_advTimer = new QTimer(this);
    m_advTimer->setSingleShot(true);
    connect(m_advTimer, &QTimer::timeout, this, &MadoludusWindow::onAutoAdvance);

    m_ffCycleTimer = new QTimer(this);
    connect(m_ffCycleTimer, &QTimer::timeout, this, &MadoludusWindow::cycleFFFrame);
}

// ── File loading ──────────────────────────────────────────────────────────────

void MadoludusWindow::loadFile(int index)
{
    if (m_mediaFiles.isEmpty()) return;
    index = qBound(0, index, m_mediaFiles.size() - 1);
    m_index = index;
    stopAutoAdvance();
    stopFFExtraction();

    // Track in history
    if (m_orderPos < 0 || m_orderPos >= m_order.size() - 1) {
        m_order.append(index);
        m_orderPos = m_order.size() - 1;
    }

    const QString path = m_mediaFiles.at(index);
    const QString ext  = '.' + QFileInfo(path).suffix().toLower();
    m_isVideo = TV::videoSuffixes().contains(ext);

    m_overlay->setFileInfo(index, m_mediaFiles.size(), QFileInfo(path).fileName());
    m_lblPath->setText(path);

    emit currentFileChanged(m_mediaFiles.at(index));

    if (m_isVideo)
        loadVideo(path);
    else
        loadImage(path);
}

void MadoludusWindow::loadImage(const QString& path)
{
    m_usingFlipbook = false;
#ifndef TV_NO_MULTIMEDIA
    if (m_player) m_player->stop();
    if (m_videoContainer) m_videoContainer->setVisible(false);
#endif
    m_display->setVisible(true);

    QImageReader reader(path);
    const QSize orig = reader.size();
    const QSize win  = m_display->size().isEmpty() ? QSize(800,600) : m_display->size();
    if (orig.isValid()) {
        const double s = qMin(double(win.width())  / orig.width(),
                               double(win.height()) / orig.height());
        if (s < 1.0) reader.setScaledSize(
            QSize(int(orig.width()*s), int(orig.height()*s)));
    }
    const QImage img = reader.read();
    if (img.isNull())
        m_display->setText(QString("Cannot load:\n%1").arg(path));
    else
        m_display->setPixmap(QPixmap::fromImage(img).scaled(
            win, Qt::KeepAspectRatio, Qt::SmoothTransformation));

    startAutoAdvance();
    updateButtonStates();
}

void MadoludusWindow::loadVideo(const QString& path)
{
#ifndef TV_NO_MULTIMEDIA
    if (m_player) m_player->stop();
#endif
    if (m_cfg.ffMode) {
        m_usingFlipbook = true;
#ifndef TV_NO_MULTIMEDIA
        if (m_videoContainer) m_videoContainer->setVisible(false);
#endif
        m_display->setVisible(true);
        m_display->setText("Extracting preview frames…");

        const VideoFramesWorker::Mode mode = m_cfg.ffFixedCount
            ? VideoFramesWorker::Mode::FixedCount
            : VideoFramesWorker::Mode::IntervalSeconds;
        startFFExtraction(path);
        Q_UNUSED(mode)
    } else {
#ifndef TV_NO_MULTIMEDIA
        m_usingFlipbook = false;
        m_display->setVisible(false);
        m_videoContainer->setVisible(true);
        m_player->setSource(QUrl::fromLocalFile(path));
        m_audio->setVolume(m_cfg.muted ? 0.0f : 0.5f);
        if (!m_paused) m_player->play();
#else
        // No multimedia – fall back to flip-book regardless of FF setting
        m_usingFlipbook = true;
        m_display->setVisible(true);
        m_display->setText("Extracting preview frames…");
        startFFExtraction(path);
#endif
    }
    updateButtonStates();
}

// ── Navigation ────────────────────────────────────────────────────────────────

int MadoludusWindow::nextIndex() const
{
    if (m_mediaFiles.size() <= 1) return qMax(0, m_index);
    if (!m_cfg.randomMode) return (m_index + 1) % m_mediaFiles.size();
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, m_mediaFiles.size() - 1);
    int n; do { n = dist(rng); } while (n == m_index);
    return n;
}

void MadoludusWindow::onNextFile()
{
    if (m_mediaFiles.isEmpty()) return;
    int next;
    if (m_orderPos < m_order.size() - 1) {
        // Step forward through existing history
        ++m_orderPos;
        next = m_order.at(m_orderPos);
    } else {
        next = nextIndex();
        m_order.append(next);
        m_orderPos = m_order.size() - 1;
    }
    loadFile(next);
}

void MadoludusWindow::onPrevFile()
{
    if (m_mediaFiles.isEmpty() || m_orderPos <= 0) return;
    --m_orderPos;
    // Don't re-append to history; just re-load the historical index
    const int prev = m_order.at(m_orderPos);
    m_index = prev;
    stopAutoAdvance();
    stopFFExtraction();
    const QString path = m_mediaFiles.at(prev);
    const QString ext  = '.' + QFileInfo(path).suffix().toLower();
    m_isVideo = TV::videoSuffixes().contains(ext);
    m_overlay->setFileInfo(prev, m_mediaFiles.size(), QFileInfo(path).fileName());
    m_lblPath->setText(path);
    if (m_isVideo) loadVideo(path); else loadImage(path);
}

// ── Playback control ──────────────────────────────────────────────────────────

void MadoludusWindow::onPlayPause()
{
    m_paused = !m_paused;
    m_overlay->setPaused(m_paused);
    if (m_paused) {
        stopAutoAdvance();
        if (m_isVideo) {
            if (m_usingFlipbook) m_ffCycleTimer->stop();
#ifndef TV_NO_MULTIMEDIA
            else if (m_player) m_player->pause();
#endif
        }
    } else {
        if (m_isVideo) {
            if (m_usingFlipbook && !m_ffFrames.isEmpty()) {
                m_ffCycleTimer->start(kFFCycleMs);
                m_advTimer->start(qMax(kImageDelayFF, kFFCycleMs * m_ffFrames.size()));
            }
#ifndef TV_NO_MULTIMEDIA
            else if (!m_usingFlipbook && m_player) m_player->play();
#endif
        } else {
            startAutoAdvance();
        }
    }
}

void MadoludusWindow::onMuteToggle()
{
    m_cfg.muted = !m_cfg.muted;
    m_overlay->setMuted(m_cfg.muted);
#ifndef TV_NO_MULTIMEDIA
    if (!m_usingFlipbook && m_audio)
        m_audio->setVolume(m_cfg.muted ? 0.0f : 0.5f);
#endif
}

void MadoludusWindow::onFFToggle()
{
    if (!m_cfg.ffMode || m_cfg.secretMode) {
        m_cfg.ffMode = !m_cfg.ffMode;
        m_overlay->setFFMode(m_cfg.ffMode);
        if (m_index >= 0) loadFile(m_index);
    }
}

void MadoludusWindow::onShuffleToggle()
{
    m_cfg.randomMode = !m_cfg.randomMode;
    m_overlay->setRandomMode(m_cfg.randomMode);
}

void MadoludusWindow::onAutoAdvance()  { onNextFile(); }
void MadoludusWindow::onVideoEnded()   { if (!m_paused && !m_usingFlipbook) onNextFile(); }

// ── Auto-advance ──────────────────────────────────────────────────────────────

void MadoludusWindow::startAutoAdvance()
{
    if (!m_isVideo && !m_paused)
        m_advTimer->start(m_cfg.ffMode ? kImageDelayFF : kImageDelay);
}

void MadoludusWindow::stopAutoAdvance() { m_advTimer->stop(); }

// ── FF flip-book ──────────────────────────────────────────────────────────────

void MadoludusWindow::startFFExtraction(const QString& path)
{
    stopFFExtraction();
    const VideoFramesWorker::Mode mode = m_cfg.ffFixedCount
        ? VideoFramesWorker::Mode::FixedCount
        : VideoFramesWorker::Mode::IntervalSeconds;
    auto* worker = new VideoFramesWorker(path, mode, m_cfg.ffValue);
    m_ffTmpDir = worker->tmpDir();
    auto* thread = new QThread;
    m_ffThread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &VideoFramesWorker::run);
    connect(worker, &VideoFramesWorker::resultReady,
            this,   &MadoludusWindow::onFFFramesReady);
    connect(worker, &VideoFramesWorker::resultReady, thread, &QThread::quit);
    connect(worker, &VideoFramesWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void MadoludusWindow::stopFFExtraction()
{
    m_ffCycleTimer->stop();
    if (m_ffThread) { m_ffThread->disconnect(this); m_ffThread = nullptr; }
    if (!m_ffTmpDir.isEmpty()) { QDir(m_ffTmpDir).removeRecursively(); m_ffTmpDir.clear(); }
    m_ffFrames.clear(); m_ffFrameIdx = 0;
}

void MadoludusWindow::onFFFramesReady(bool ok, QStringList framePaths)
{
    m_ffThread = nullptr;
    if (!ok || framePaths.isEmpty()) {
        m_display->setText(
            "(no video preview – install ffmpeg, or disable FF mode in Secret Mode)");
        startAutoAdvance();
        return;
    }
    m_ffFrames = framePaths; m_ffFrameIdx = 0;
    cycleFFFrame();
    if (!m_paused) {
        m_ffCycleTimer->start(kFFCycleMs);
        m_advTimer->start(qMax(kImageDelayFF, kFFCycleMs * m_ffFrames.size()));
    }
}

void MadoludusWindow::cycleFFFrame()
{
    if (m_ffFrames.isEmpty()) return;
    const QPixmap pm(m_ffFrames.at(m_ffFrameIdx));
    if (!pm.isNull())
        m_display->setPixmap(pm.scaled(
            m_display->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    m_ffFrameIdx = (m_ffFrameIdx + 1) % m_ffFrames.size();
}

// ── Keyboard ──────────────────────────────────────────────────────────────────

void MadoludusWindow::keyPressEvent(QKeyEvent* event)
{
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    switch (event->key()) {
    case Qt::Key_Escape: close(); break;
    case Qt::Key_Space:  onPlayPause(); break;
    case Qt::Key_Right:
        if (shift) {
#ifndef TV_NO_MULTIMEDIA
            if (m_isVideo && !m_usingFlipbook && m_player)
                m_player->setPosition(m_player->position() + kSeekStep);
#endif
        } else { onNextFile(); }
        break;
    case Qt::Key_Left:
        if (shift) {
#ifndef TV_NO_MULTIMEDIA
            if (m_isVideo && !m_usingFlipbook && m_player)
                m_player->setPosition(qMax(0LL, m_player->position() - kSeekStep));
#endif
        } else { onPrevFile(); }
        break;
    case Qt::Key_M: onMuteToggle();   break;
    case Qt::Key_F: onFFToggle();     break;
    case Qt::Key_R: onShuffleToggle(); break;
    default: QMainWindow::keyPressEvent(event);
    }
}

// ── Frameless window: drag to move ───────────────────────────────────────────

void MadoludusWindow::mousePressEvent(QMouseEvent* event)
{
    if (event->button() == Qt::LeftButton &&
        !m_overlay->geometry().contains(event->pos())) {
        m_dragging  = true;
        m_dragStart = event->globalPosition().toPoint() - frameGeometry().topLeft();
    }
    QMainWindow::mousePressEvent(event);
}

void MadoludusWindow::mouseMoveEvent(QMouseEvent* event)
{
    if (m_dragging && (event->buttons() & Qt::LeftButton))
        move(event->globalPosition().toPoint() - m_dragStart);
    QMainWindow::mouseMoveEvent(event);
}

// ── Resize: re-scale current content ─────────────────────────────────────────

void MadoludusWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    scaleCurrentImage();
}

void MadoludusWindow::scaleCurrentImage()
{
    if (!m_display->isVisible() || m_display->pixmap().isNull()) return;
    if (m_usingFlipbook && !m_ffFrames.isEmpty()) {
        const int shown = (m_ffFrameIdx + m_ffFrames.size() - 1) % m_ffFrames.size();
        const QPixmap pm(m_ffFrames.at(shown));
        if (!pm.isNull())
            m_display->setPixmap(pm.scaled(
                m_display->size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
    } else if (!m_isVideo && m_index >= 0 && m_index < m_mediaFiles.size()) {
        QImageReader reader(m_mediaFiles.at(m_index));
        const QSize win = m_display->size();
        if (!win.isEmpty()) {
            reader.setScaledSize(win);
            const QImage img = reader.read();
            if (!img.isNull())
                m_display->setPixmap(QPixmap::fromImage(img).scaled(
                    win, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void MadoludusWindow::updateButtonStates()
{
    m_overlay->setPaused(m_paused);
    m_overlay->setMuted(m_cfg.muted);
    m_overlay->setFFMode(m_cfg.ffMode);
    m_overlay->setRandomMode(m_cfg.randomMode);
}

void MadoludusWindow::closeEvent(QCloseEvent* e)
{
    stopAutoAdvance();
    stopFFExtraction();
#ifndef TV_NO_MULTIMEDIA
    if (m_player) m_player->stop();
#endif
    emit windowClosed();
    QMainWindow::closeEvent(e);
}
