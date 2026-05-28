#include "videopreviewwidget.h"
#include "workers/videoframesworker.h"
#include <QDir>
#include <QLabel>
#include <QThread>
#include <QUrl>
#ifndef TV_NO_MULTIMEDIA
#include <QtMultimedia/QMediaPlayer>
#include <QtMultimedia/QAudioOutput>
#include <QtMultimediaWidgets/QVideoWidget>
#endif

VideoPreviewWidget::VideoPreviewWidget(QWidget* p) : QStackedWidget(p) {
    setSizePolicy(QSizePolicy::Expanding,QSizePolicy::Expanding);
#ifndef TV_NO_MULTIMEDIA
    m_videoW=new QVideoWidget; m_player=new QMediaPlayer(this);
    m_audio=new QAudioOutput(this); m_audio->setVolume(0.0f);
    m_player->setAudioOutput(m_audio); m_player->setVideoOutput(m_videoW);
    m_player->setLoops(QMediaPlayer::Infinite);
    addWidget(m_videoW);
#else
    addWidget(new QLabel("(no multimedia)"));
#endif
    m_frameLabel=new PreviewLabel; addWidget(m_frameLabel);
    m_cycleTimer=new QTimer(this); m_cycleTimer->setInterval(kCycleMs);
    connect(m_cycleTimer,&QTimer::timeout,this,&VideoPreviewWidget::nextFrame);
    setCurrentIndex(1);
}

VideoPreviewWidget::~VideoPreviewWidget(){ stop_(); cleanupTmp(); }

void VideoPreviewWidget::play(const QString& path) {
    stop_(); cleanupTmp();
#ifndef TV_NO_MULTIMEDIA
    if(m_player){ setCurrentIndex(0);
        m_player->setSource(QUrl::fromLocalFile(path)); m_player->play();
        emit statusText("Video  (QMediaPlayer – muted)"); return; }
#endif
    setCurrentIndex(1); emit statusText("Video – extracting frames…");
    auto* w=new VideoFramesWorker(path); m_tmpDir=w->tmpDir();
    auto* t=new QThread; m_vidThread=t; m_vidWorker=w;
    w->moveToThread(t);
    connect(t,&QThread::started,w,&VideoFramesWorker::run);
    connect(w,&VideoFramesWorker::resultReady,this,&VideoPreviewWidget::onFramesReady);
    connect(w,&VideoFramesWorker::resultReady,t,&QThread::quit);
    connect(w,&VideoFramesWorker::resultReady,w,&QObject::deleteLater);
    connect(t,&QThread::finished,t,&QObject::deleteLater);
    t->start();
}

void VideoPreviewWidget::showPixmap(const QPixmap& pm,const QString& lbl){
    stop_(); cleanupTmp(); setCurrentIndex(1);
    m_frameLabel->setSourcePixmap(pm); emit statusText(lbl);
}

void VideoPreviewWidget::clearPreview(){
    stop_(); cleanupTmp(); setCurrentIndex(1);
    m_frameLabel->clearPreview(); emit statusText({});
}

void VideoPreviewWidget::stop_(){
    m_cycleTimer->stop();
#ifndef TV_NO_MULTIMEDIA
    if(m_player) m_player->stop();
#endif
    if(m_vidThread){ m_vidThread->quit(); m_vidThread=nullptr; m_vidWorker=nullptr; }
}

void VideoPreviewWidget::cleanupTmp(){
    if(!m_tmpDir.isEmpty()){ QDir(m_tmpDir).removeRecursively(); m_tmpDir.clear(); }
    m_framePaths.clear(); m_frameIdx=0;
}

void VideoPreviewWidget::onFramesReady(bool ok,const QStringList& paths){
    m_vidThread=nullptr; m_vidWorker=nullptr;
    if(ok&&!paths.isEmpty()){
        m_framePaths=paths; m_frameIdx=0; nextFrame(); m_cycleTimer->start();
        emit statusText(QString("Video  (ffmpeg, %1 frames)").arg(paths.size()));
    } else { emit statusText("Video  (no preview – install ffmpeg or Qt Multimedia)"); }
}

void VideoPreviewWidget::nextFrame(){
    if(m_framePaths.isEmpty()) return;
    m_frameIdx=(m_frameIdx+1)%m_framePaths.size();
    QPixmap pm(m_framePaths.at(m_frameIdx));
    if(!pm.isNull()) m_frameLabel->setSourcePixmap(pm);
}
