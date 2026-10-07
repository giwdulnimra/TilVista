#include "mediaviewer.h"
#include "core/pathutils.h"

#include <QFileInfo>
#include <QWidget>
#include <QLabel>
#include <QMediaPlayer>
#include <QVideoWidget>
#include <QAudioOutput>
#include <QImageReader>
#include <QPixmap>
//#include <QResizeEvent>
#include <QGuiApplication>
#include <QScreen>
#include <QTimer>
#include <QUrl>


MediaViewer::MediaViewer(QWidget* p) : QWidget(p)
{
    setWindowTitle("TilVista · MadoLudus");
    setStyleSheet("background-color: black;");

    m_imageLabel = new QLabel(this);
#ifndef TV_NO_MULTIMEDIA
    m_player = new QMediaPlayer(this);
    m_audio = new QAudioOutput(this);
    m_videoW = new QVideoWidget(this);

    m_player->setAudioOutput(m_audio);
    m_player->setVideoOutput(m_videoW);

    connect(m_player, &QMediaPlayer::mediaStatusChanged, this, [this](QMediaPlayer::MediaStatus status) {
        if (status == QMediaPlayer::EndOfMedia) { emit finished(); }
    });
#endif

    TV::preventSleep();

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

    m_imageTimer = new QTimer(this);
    m_imageTimer->setInterval(m_imageDurationMS);
    connect(m_imageTimer, &QTimer::timeout, this, &MediaViewer::imageTimeout);
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
#ifndef TV_NO_MULTIMEDIA
        if (m_videoW) m_videoW->hide();
        if (m_imageLabel) m_imageLabel->show();
#endif //TV_NO_MULTIMEDIA
    }
#ifndef TV_NO_MULTIMEDIA
    else if (TV::videoSuffixes().contains(fileExt))
    {
        loadVideo(path);
        if (m_imageLabel) m_imageLabel->hide();
        if (m_videoW) m_videoW->show();
    }
#endif //TV_NO_MULTIMEDIA
    else
    {
        emit finished(); //finished(path);
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

bool MediaViewer::isPaused() const
{
    return m_paused;
}

void MediaViewer::setImageDurationMS(int newMS)
{
    m_imageDurationMS = newMS;
}

void MediaViewer::loadImage(const QString& path)
{
    // why variables with:  m_<> ??
    QImageReader reader(path);
    const QSize orig = reader.size();
    if (orig.isValid() && (orig.width() > m_showW || orig.height() > m_showH)) {
        const double s = qMin(static_cast<double>(m_showW)/orig.width(),
                              static_cast<double>(m_showH)/orig.height());
        reader.setScaledSize(QSize(static_cast<int>(orig.width() * s), static_cast<int>(orig.height() * s)));
    }
    const QImage img = reader.read();
    if (img.isNull()) { emit finished(); return; }
    QPixmap pm = QPixmap::fromImage(img);
    if (static_cast<double>(pm.width())/static_cast<double>(pm.height()) > m_ratio)
        pm = pm.scaledToWidth(m_showW, Qt::SmoothTransformation);
    else
        pm = pm.scaledToHeight(m_showH-18, Qt::SmoothTransformation);
    m_imageLabel->setPixmap(pm);
    m_imageTimer->stop(); m_imageTimer->start(m_imageDurationMS);
    //jiggleMouse(); // to prevent Sleep-mode
}

void MediaViewer::loadVideo(const QString& path) const
{
#ifndef TV_NO_MULTIMEDIA
    if (m_player)
    {
        m_player->stop();
        m_player->setSource(QUrl::fromLocalFile(path));
        m_player->play();
    }
#endif // TV_NO_MULTIMEDIA
}
