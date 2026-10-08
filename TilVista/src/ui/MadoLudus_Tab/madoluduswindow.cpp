#include "madoluduswindow.h"
#include "madooverlay.h"
#include "core/pathutils.h"

#include <algorithm>
#include <random>
#include <QWidget>
#include <QLabel>
#include <QDir>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QResizeEvent>
#include <QCloseEvent>
#include <QGridLayout>
//#include <QHBoxLayout>
//#include <QVBoxLayout>


MadoludusWindow::MadoludusWindow(const QStringList& mediaFiles,
                                 const MadoludusConfig& cfg,
                                 QWidget* parent)
    : QMainWindow(parent)
    , m_mediaFiles(mediaFiles)
    , m_cfg(cfg)
{
    // how do i seperate Taskbaricons vor the main window and the AleaVue/MadoLudus Subwindows? -> different parenting
    setWindowFlags(Qt::Window
                 | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_DeleteOnClose);
    setStyleSheet("background: black;");
    setMinimumSize(640, 400);

    const QRect scr = QGuiApplication::primaryScreen()->availableGeometry();
    if (m_cfg.fullscreen) {
        setGeometry(scr);
    } else {
        setGeometry(QRect(scr.topLeft(), QSize(scr.width()/2, scr.height()/2)));
    }

    m_playlist = m_mediaFiles;
    if (m_cfg.randomMode) { shufflePlaylist(); }

    buildUi();

    if (!m_playlist.isEmpty()) {
        advance();
    }
}

MadoludusWindow::~MadoludusWindow() {}

// ── UI ────────────────────────────────────────────────────────────────────────
void MadoludusWindow::buildUi()
{
    m_central = new QWidget(this);
    setCentralWidget(m_central);

    auto* grid = new QGridLayout(m_central);
    grid->setContentsMargins(0, 0, 0, 0);
    grid->setSpacing(0);

    // ── Layer 0 -> MediaViewer
    m_viewer = new MediaViewer(m_central);
    m_viewer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(m_viewer, &MediaViewer::finished, this, &MadoludusWindow::advance);
    grid->addWidget(m_viewer, 0, 0, 3, 3);

    // ── Layer 1 -> overlay, progressbar
    m_overlay = new MadoOverlay(m_central);
    m_overlay->setFixedWidth(56);
    m_overlay->setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);

    //m_overlay->setPaused(m_paused);
    //m_overlay->setMuted(m_cfg.muted);
    //m_overlay->setFFMode(m_cfg.ffMode);
    //m_overlay->setRandomMode(m_cfg.randomMode);
    //m_overlay->setSecretMode(m_cfg.secretMode);

    connect(m_overlay, &MadoOverlay::playPauseClicked, this, &MadoludusWindow::onPlayPause);
    connect(m_overlay, &MadoOverlay::muteClicked,      this, &MadoludusWindow::onMuteToggle);
    connect(m_overlay, &MadoOverlay::ffClicked,        this, &MadoludusWindow::onFFToggle);
    connect(m_overlay, &MadoOverlay::shuffleClicked,   this, &MadoludusWindow::onShuffleToggle);

    //<Progressbar4Videos> = new Progressbar4Videos;

    m_lblPath = new QLabel(m_central);
    m_lblPath->setStyleSheet("color: #888; font-size: 10px; padding: 2px 6px;"
                              //"background: #111;");
                             "background: rgba(17, 17, 17, 180);");
    m_lblPath->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    m_lblPath->setFixedHeight(20);

    grid->addWidget(m_overlay, 0, 0, 3, 1);
    // grid->addWidget(<Progressbar4Videos>, 2, 0, 1, 3);
    grid->addWidget(m_lblPath, 3, 0, 1, 3);
}

void MadoludusWindow::shufflePlaylist()
{
    std::mt19937 rng(std::random_device{}());
    std::shuffle(m_playlist.begin(), m_playlist.end(), rng);
}

// ── Playback control ──────────────────────────────────────────────────────────
void MadoludusWindow::advance()
{
    if (m_playlist.isEmpty()) return;
    if (m_playlistIndex >= m_playlist.size()) {shufflePlaylist(); m_playlistIndex = 0;}

    QString currentPath = m_playlist.at(m_playlistIndex);
    m_viewer->showFile(currentPath);
    if (m_lblPath) {
        m_lblPath->setText(currentPath);
    }
    m_playlistIndex++;
}

void MadoludusWindow::onPlayPause() const
{
    m_viewer->togglePause();
    m_overlay->setPaused(m_viewer->isPaused());
}

void MadoludusWindow::onMuteToggle() {}
void MadoludusWindow::onFFToggle() {}
void MadoludusWindow::onShuffleToggle() {}

// ── Keyboard&Events ────────────────────────────────────────────────────────────
void MadoludusWindow::keyPressEvent(QKeyEvent* event)
{
    const bool shift = event->modifiers().testFlag(Qt::ShiftModifier);
    switch (event->key()) {
    case Qt::Key_Escape: close(); break;
    case Qt::Key_Space:  onPlayPause(); break;
    case Qt::Key_Right:
        if (shift) {
#ifndef TV_NO_MULTIMEDIA
            if (true) { ; }
#endif
        } else { advance(); }
        break;
    case Qt::Key_Left:
        if (shift) {
#ifndef TV_NO_MULTIMEDIA
            if (true) { ; }
#endif
        } else { ; }
        break;
    case Qt::Key_M: onMuteToggle();   break;
    case Qt::Key_F: onFFToggle();     break;
    case Qt::Key_R: onShuffleToggle(); break;
    default: QMainWindow::keyPressEvent(event);
    }
}

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

void MadoludusWindow::updateButtonStates() {}

void MadoludusWindow::resizeEvent(QResizeEvent* event) { QMainWindow::resizeEvent(event); }

void MadoludusWindow::closeEvent(QCloseEvent* e) { emit windowClosed(); QMainWindow::closeEvent(e); }
