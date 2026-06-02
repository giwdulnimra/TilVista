#include "madolodostab.h"
#include "madooverlay.h"
#include "core/pathutils.h"
#include "workers/scanworker.h"

#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMouseEvent>
#include <QPixmap>
#include <QProgressBar>
#include <QResizeEvent>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QThread>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>

#ifndef TV_NO_MULTIMEDIA
#  include <QtMultimedia/QMediaPlayer>
#  include <QtMultimedia/QAudioOutput>
#  include <QtMultimediaWidgets/QVideoWidget>
#endif

static constexpr int kImageDelay   = 5000;   // ms normal image display
static constexpr int kImageDelayFF = 2000;   // ms FF mode image display
static constexpr int kOverlayHide  = 3000;   // ms before overlay fades
static constexpr int kSeekStep     = 5000;   // ms for Shift+arrow seek

// ── Constructor / Destructor ──────────────────────────────────────────────────

MadolodosTab::MadolodosTab(std::function<QString()> getGlobalDir,
                            QWidget* parent)
    : QWidget(parent)
    , m_getGlobalDir(std::move(getGlobalDir))
{
    setMouseTracking(true);
    buildUi();
}

MadolodosTab::~MadolodosTab()
{
    stopAutoAdvance();
}

// ── Public ────────────────────────────────────────────────────────────────────

void MadolodosTab::onDirectoryChanged(const QString& path,
                                       const QStringList& allFiles)
{
    stopAutoAdvance();
    m_mediaFiles.clear();
    m_index = -1;

    if (!allFiles.isEmpty()) {
        // Filter to media files only
        const QStringList& imgSuf = TV::imageSuffixes();
        const QStringList& vidSuf = TV::videoSuffixes();
        for (const QString& fp : allFiles) {
            const QString ext = '.' + QFileInfo(fp).suffix().toLower();
            if (imgSuf.contains(ext) || vidSuf.contains(ext))
                m_mediaFiles << fp;
        }
        if (!m_mediaFiles.isEmpty()) {
            loadFile(0);
        } else {
            m_lblStatus->setText("No media files in directory.");
        }
    } else if (!path.isEmpty()) {
        scanDir(path);
    }
}

void MadolodosTab::setSecretMode(bool on)
{
    m_secretMode = on;
    m_overlay->setSecretMode(on);
    // If secret mode is turned OFF, re-enable FF mode
    if (!on && !m_ffMode) {
        m_ffMode = true;
        applyFFMode();
    }
}

// ── UI Builder ────────────────────────────────────────────────────────────────

void MadolodosTab::buildUi()
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    // ── Display area ──────────────────────────────────────────────────────────
    m_stack = new QStackedWidget;
    m_stack->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stack->setStyleSheet("background: black;");

    // Page 0: image
    m_imgLabel = new QLabel;
    m_imgLabel->setAlignment(Qt::AlignCenter);
    m_imgLabel->setStyleSheet("background: black;");
    m_imgLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_stack->addWidget(m_imgLabel);

    // Page 1: video
    m_videoPage = new QWidget;
    m_videoPage->setStyleSheet("background: black;");
    auto* vl = new QVBoxLayout(m_videoPage);
    vl->setContentsMargins(0,0,0,0);
#ifndef TV_NO_MULTIMEDIA
    m_videoW = new QVideoWidget;
    m_videoW->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    vl->addWidget(m_videoW);
    m_player = new QMediaPlayer(this);
    m_audio  = new QAudioOutput(this);
    m_audio->setVolume(0.5f);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoW);
    connect(m_player, &QMediaPlayer::playbackStateChanged,
            this, [this](QMediaPlayer::PlaybackState s){
                if (s == QMediaPlayer::StoppedState) onVideoEnded();
            });
#else
    auto* noVid = new QLabel("Qt Multimedia not available.\nVideos shown as icon.");
    noVid->setAlignment(Qt::AlignCenter);
    noVid->setStyleSheet("color: gray;");
    vl->addWidget(noVid);
#endif
    m_stack->addWidget(m_videoPage);

    outer->addWidget(m_stack, 1);

    // ── Bottom status bar ─────────────────────────────────────────────────────
    m_lblStatus = new QLabel("Select a directory to start Madoludus.");
    m_lblStatus->setStyleSheet(
        "font-size: 10px; color: gray; padding: 2px 6px;");
    m_pb = new QProgressBar;
    m_pb->setFixedHeight(4); m_pb->setTextVisible(false); m_pb->setVisible(false);
    outer->addWidget(m_pb);
    outer->addWidget(m_lblStatus);

    // ── Overlay (floating, child of this widget) ──────────────────────────────
    m_overlay = new MadoOverlay(this);
    m_overlay->setPaused(false);
    m_overlay->setMuted(m_muted);
    m_overlay->setFFMode(m_ffMode);
    m_overlay->setFixedHeight(56);
    m_overlay->hide();
    connect(m_overlay, &MadoOverlay::playPauseClicked, this, &MadolodosTab::onPlayPause);
    connect(m_overlay, &MadoOverlay::prevClicked,      this, &MadolodosTab::onPrevFile);
    connect(m_overlay, &MadoOverlay::nextClicked,      this, &MadolodosTab::onNextFile);
    connect(m_overlay, &MadoOverlay::muteClicked,      this, &MadolodosTab::onMuteToggle);
    connect(m_overlay, &MadoOverlay::ffClicked,        this, &MadolodosTab::onFFToggle);

    // ── Timers ─────────────────────────────────────────────────────────────────
    m_advTimer = new QTimer(this);
    m_advTimer->setSingleShot(true);
    connect(m_advTimer, &QTimer::timeout, this, &MadolodosTab::onAutoAdvance);

    m_overlayTimer = new QTimer(this);
    m_overlayTimer->setSingleShot(true);
    m_overlayTimer->setInterval(kOverlayHide);
    connect(m_overlayTimer, &QTimer::timeout, this, &MadolodosTab::onOverlayFade);
}

// ── File loading ──────────────────────────────────────────────────────────────

void MadolodosTab::loadFile(int index)
{
    if (m_mediaFiles.isEmpty()) return;
    index = qBound(0, index, m_mediaFiles.size() - 1);
    m_index = index;
    stopAutoAdvance();

    const QString path = m_mediaFiles.at(index);
    const QString ext  = '.' + QFileInfo(path).suffix().toLower();
    m_isVideo = TV::videoSuffixes().contains(ext);

    m_overlay->setFileInfo(index, m_mediaFiles.size(), QFileInfo(path).fileName());
    m_lblStatus->setText(
        QString("[%1/%2]  %3")
            .arg(index + 1).arg(m_mediaFiles.size()).arg(path));

    if (m_isVideo)
        loadVideo(path);
    else
        loadImage(path);
}

void MadolodosTab::loadImage(const QString& path)
{
    m_stack->setCurrentIndex(0);
#ifndef TV_NO_MULTIMEDIA
    m_player->stop();
#endif
    QImageReader reader(path);
    const QSize  orig = reader.size();
    const QSize  win  = m_stack->size();
    if (orig.isValid() && win.isValid()) {
        const double s = qMin(double(win.width())  / orig.width(),
                               double(win.height()) / orig.height());
        if (s < 1.0)
            reader.setScaledSize(
                QSize(int(orig.width() * s), int(orig.height() * s)));
    }
    const QImage img = reader.read();
    if (img.isNull()) {
        m_imgLabel->setText(QString("Cannot load:\n%1").arg(path));
    } else {
        m_imgLabel->setPixmap(QPixmap::fromImage(img).scaled(
            m_stack->size(),
            Qt::KeepAspectRatio,
            Qt::SmoothTransformation));
    }
    startAutoAdvance();
    m_overlay->setPaused(m_paused);
    updateButtonStates();
}

void MadolodosTab::loadVideo(const QString& path)
{
#ifndef TV_NO_MULTIMEDIA
    if (m_ffMode) {
        // FF mode: play muted at full speed (gives impression), no seek
        m_stack->setCurrentIndex(1);
        m_player->setSource(QUrl::fromLocalFile(path));
        m_audio->setVolume(0.0f);
        if (!m_paused) m_player->play();
    } else {
        m_stack->setCurrentIndex(1);
        m_player->setSource(QUrl::fromLocalFile(path));
        m_audio->setVolume(m_muted ? 0.0f : 0.5f);
        if (!m_paused) m_player->play();
    }
#else
    // Fallback: show file icon via image loader which will fail gracefully
    m_stack->setCurrentIndex(0);
    m_imgLabel->setText(QString("Video (no multimedia):\n%1").arg(path));
    startAutoAdvance();
#endif
    m_overlay->setPaused(m_paused);
    updateButtonStates();
}

// ── Auto-advance ──────────────────────────────────────────────────────────────

void MadolodosTab::startAutoAdvance()
{
    if (!m_isVideo) {
        const int delay = m_ffMode ? kImageDelayFF : kImageDelay;
        if (!m_paused) m_advTimer->start(delay);
    }
    // Video: handled by onVideoEnded()
}

void MadolodosTab::stopAutoAdvance()
{
    m_advTimer->stop();
}

void MadolodosTab::applyFFMode()
{
    m_overlay->setFFMode(m_ffMode);
    if (m_index >= 0) loadFile(m_index);   // reload with new mode
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MadolodosTab::onNextFile()
{
    if (m_mediaFiles.isEmpty()) return;
    loadFile((m_index + 1) % m_mediaFiles.size());
}

void MadolodosTab::onPrevFile()
{
    if (m_mediaFiles.isEmpty()) return;
    int prev = m_index - 1;
    if (prev < 0) prev = m_mediaFiles.size() - 1;
    loadFile(prev);
}

void MadolodosTab::onPlayPause()
{
    m_paused = !m_paused;
    m_overlay->setPaused(m_paused);
    if (m_paused) {
        stopAutoAdvance();
#ifndef TV_NO_MULTIMEDIA
        if (m_isVideo) m_player->pause();
#endif
    } else {
        if (m_isVideo) {
#ifndef TV_NO_MULTIMEDIA
            m_player->play();
#endif
        } else {
            startAutoAdvance();
        }
    }
}

void MadolodosTab::onMuteToggle()
{
    m_muted = !m_muted;
    m_overlay->setMuted(m_muted);
#ifndef TV_NO_MULTIMEDIA
    if (!m_ffMode)
        m_audio->setVolume(m_muted ? 0.0f : 0.5f);
#endif
}

void MadolodosTab::onFFToggle()
{
    // Can only turn OFF in secret mode
    if (!m_ffMode || m_secretMode) {
        m_ffMode = !m_ffMode;
        applyFFMode();
    } else {
        m_lblStatus->setText(
            "FF mode can only be disabled in Secret Mode (Ctrl+Alt+F8).");
    }
}

void MadolodosTab::onAutoAdvance()
{
    onNextFile();
}

void MadolodosTab::onOverlayFade()
{
    m_overlay->hide();
}

void MadolodosTab::onVideoEnded()
{
    if (!m_paused) onNextFile();
}

void MadolodosTab::onScanDone(bool ok,
                               QStringList /*imageFiles*/,
                               QStringList allFiles)
{
    m_pb->setVisible(false);
    m_scanThread = nullptr;
    if (!ok) { m_lblStatus->setText("Scan failed."); return; }

    const QStringList& imgSuf = TV::imageSuffixes();
    const QStringList& vidSuf = TV::videoSuffixes();
    m_mediaFiles.clear();
    for (const QString& fp : allFiles) {
        const QString ext = '.' + QFileInfo(fp).suffix().toLower();
        if (imgSuf.contains(ext) || vidSuf.contains(ext))
            m_mediaFiles << fp;
    }
    if (m_mediaFiles.isEmpty()) {
        m_lblStatus->setText("No media files found.");
    } else {
        m_lblStatus->setText(
            QString("Madoludus ready: %1 media files.").arg(m_mediaFiles.size()));
        loadFile(0);
    }
}

// ── Input / Overlay ───────────────────────────────────────────────────────────

void MadolodosTab::keyPressEvent(QKeyEvent* event)
{
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    switch (event->key()) {
    case Qt::Key_Space:    onPlayPause();  break;
    case Qt::Key_Right:
        if (shift) {
#ifndef TV_NO_MULTIMEDIA
            if (m_isVideo && m_player)
                m_player->setPosition(
                    m_player->position() + kSeekStep);
#endif
        } else { onNextFile(); }
        break;
    case Qt::Key_Left:
        if (shift) {
#ifndef TV_NO_MULTIMEDIA
            if (m_isVideo && m_player)
                m_player->setPosition(
                    qMax(0LL, m_player->position() - kSeekStep));
#endif
        } else { onPrevFile(); }
        break;
    case Qt::Key_M:  onMuteToggle(); break;
    case Qt::Key_F:  onFFToggle();   break;
    default: QWidget::keyPressEvent(event);
    }
    showOverlay();
}

void MadolodosTab::mouseMoveEvent(QMouseEvent* event)
{
    QWidget::mouseMoveEvent(event);
    showOverlay();
}

void MadolodosTab::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    positionOverlay();
    // Re-scale current image if displaying one
    if (!m_isVideo && m_index >= 0 && m_index < m_mediaFiles.size()) {
        const QString path = m_mediaFiles.at(m_index);
        QImageReader reader(path);
        const QSize win = m_stack->size();
        if (!win.isEmpty()) {
            reader.setScaledSize(win);
            const QImage img = reader.read();
            if (!img.isNull())
                m_imgLabel->setPixmap(
                    QPixmap::fromImage(img).scaled(
                        win, Qt::KeepAspectRatio, Qt::SmoothTransformation));
        }
    }
}

void MadolodosTab::showOverlay()
{
    positionOverlay();
    m_overlay->show();
    m_overlayTimer->start();
}

void MadolodosTab::positionOverlay()
{
    if (!m_overlay) return;
    const int margin = 12;
    const int h = m_overlay->height();
    // Place above status label area
    const int statusH = m_lblStatus->height() + m_pb->height() + 4;
    m_overlay->setGeometry(
        margin,
        height() - statusH - h - margin,
        width() - 2 * margin,
        h);
    m_overlay->raise();
}

void MadolodosTab::updateButtonStates()
{
    m_overlay->setPaused(m_paused);
    m_overlay->setMuted(m_muted);
    m_overlay->setFFMode(m_ffMode);
}

void MadolodosTab::scanDir(const QString& path)
{
    // Stop any previous scan before starting a new one
    if (m_scanThread) { m_scanThread->disconnect(this); m_scanThread = nullptr; }
    m_pb->setRange(0, 100); m_pb->setValue(0); m_pb->setVisible(true);
    m_lblStatus->setText(QString("Scanning: %1 …").arg(path));
    auto* worker = new ScanWorker(path);
    auto* thread = new QThread; m_scanThread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &ScanWorker::run);
    connect(worker, &ScanWorker::progressChanged, m_pb, &QProgressBar::setValue);
    connect(worker, &ScanWorker::resultReady, this, &MadolodosTab::onScanDone);
    connect(worker, &ScanWorker::resultReady, thread, &QThread::quit);
    connect(worker, &ScanWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,       thread, &QObject::deleteLater);
    thread->start();
}

bool MadolodosTab::isMediaFile(const QString& path) const
{
    const QString ext = '.' + QFileInfo(path).suffix().toLower();
    return TV::imageSuffixes().contains(ext) ||
           TV::videoSuffixes().contains(ext);
}
