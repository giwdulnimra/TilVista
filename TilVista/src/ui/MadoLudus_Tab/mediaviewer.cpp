#include "mediaviewer.h"
#include "core/pathutils.h"

#include <QFileInfo>
#include <QGuiApplication>
#include <QImageReader>
#include <QLabel>
#include <QResizeEvent>
#include <QScreen>
#include <QStackedLayout>
#include <QTimer>
#include <QUrl>
#ifndef TV_NO_MULTIMEDIA
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#endif // TV_NO_MULTIMEDIA
#include <QPixmap>
#include <QWidget>


MediaViewer::MediaViewer(QWidget* p) : QWidget(p)
{
    setWindowTitle("TilVista · MadoLudus");
    setStyleSheet("background-color: black;");

    m_stack = new QStackedLayout(this);
    m_stack->setContentsMargins(0, 0, 0, 0);

    m_imageLabel = new QLabel(this);
    m_imageLabel->setAlignment(Qt::AlignCenter);
    m_imageLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Ignored);
    // Ignored -> enables shrinking the window
    m_stack->addWidget(m_imageLabel);

    m_imageTimer = new QTimer(this);
    //m_imageTimer->setInterval(m_imageDurationMS);
    m_imageTimer->setSingleShot(true);
    connect(m_imageTimer, &QTimer::timeout, this, &MediaViewer::imageTimeout);

#ifndef TV_NO_MULTIMEDIA
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_videoW = new QVideoWidget(this);
    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoW);
    m_stack->addWidget(m_videoW);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) { emit finished(); }
    });
#endif

    m_stack->setCurrentWidget((m_imageLabel));
    TV::preventSleep();
    /*
    const QRect scr = QGuiApplication::primaryScreen()->availableGeometry();
    if (m_fullscreen)
    {
        m_showW = scr.width();
        m_showH = scr.height();
    }
    else
    {
        m_showW = scr.width() / 2;
        m_showH = scr.height() / 2;
        setGeometry(0, 0, m_showW, m_showH);
    }
    */
}
MediaViewer::~MediaViewer(){ stop(); TV::restoreSleep(); }

void MediaViewer::imageTimeout() { emit finished(); }

void MediaViewer::showFile(const QString& path)
{
    stop();
    QString fileExt = "." + QFileInfo(path).suffix().toLower();
    if (TV::imageSuffixes().contains(fileExt))
    {
        loadImage(path);
    }
#ifndef TV_NO_MULTIMEDIA
    else if (TV::videoSuffixes().contains(fileExt))
    {
        loadVideo(path);
    }
#endif //TV_NO_MULTIMEDIA
    else
    {
        QTimer::singleShot(10, this, [this] { emit finished(); });
    }
}

void MediaViewer::stop()
{
    m_paused = false;
    if (m_imageTimer) { m_imageTimer->stop(); }
#ifndef TV_NO_MULTIMEDIA
    if (m_player) { m_player->stop(); }
#endif //TV_NO_MULTIMEDIA
}

void MediaViewer::togglePause()
{
    m_paused = !m_paused;
}
bool MediaViewer::isPaused() const { return m_paused; }
void MediaViewer::setImageDurationMS(int newMS) { m_imageDurationMS = newMS; }

void MediaViewer::loadImage(const QString& path)
{
    const QSize maxSize = QGuiApplication::primaryScreen()->availableGeometry().size();

    QImageReader reader(path);
    const QSize orig = reader.size();
    if (orig.isValid() && (orig.width() > maxSize.width() || orig.height() > maxSize.height())) {
        reader.setScaledSize(orig.scaled(maxSize, Qt::KeepAspectRatio));
    }

    const QImage img = reader.read();
    if (img.isNull())
    {
        QTimer::singleShot(10, this, [this] { emit finished(); });
        return;
    }
    m_pixmap = QPixmap::fromImage(img);
    m_stack->setCurrentWidget(m_imageLabel);
    updateImageLabel();
    m_imageTimer->start(m_imageDurationMS); // oneshot
    //jiggleMouse(); // to prevent Sleep-mode
}

void MediaViewer::loadVideo(const QString& path) const
{
#ifndef TV_NO_MULTIMEDIA
    m_stack->setCurrentWidget(m_videoW);
    if (m_player)
    {
        m_player->stop();
        m_player->setSource(QUrl::fromLocalFile(path));
        m_player->play();
    }
#else
    Q_UNUSED(path)
#endif // TV_NO_MULTIMEDIA
}

void MediaViewer::resizeEvent(QResizeEvent* event)
{
    QWidget::resizeEvent(event);
    updateImageLabel();
}

void MediaViewer::updateImageLabel()
{
    if (m_pixmap.isNull()) { return; }
    m_imageLabel->setPixmap(m_pixmap.scaled(size(), Qt::KeepAspectRatio, Qt::SmoothTransformation));
}