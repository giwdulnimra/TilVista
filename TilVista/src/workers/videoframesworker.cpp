#include "videoframesworker.h"
#include <QDir>
#include <QFile>
#include <QProcess>
#include <QStandardPaths>

const QList<int> VideoFramesWorker::frameTimes = {0,10,20,30,40,50};

VideoFramesWorker::VideoFramesWorker(const QString& p, QObject* par)
    : QObject(par), m_videoPath(p) {
    m_tmpDir = QDir::tempPath()+"/tilvista_vid";
    QDir().mkpath(m_tmpDir);
}

void VideoFramesWorker::run() {
    if(QStandardPaths::findExecutable("ffmpeg").isEmpty()){
        emit resultReady(false,{}); return;
    }
    QStringList frames;
    for(int t:frameTimes){
        const QString out=QString("%1/f%2.jpg").arg(m_tmpDir).arg(t,3,10,QChar('0'));
        QProcess proc;
        proc.start("ffmpeg",{"-i",m_videoPath,"-ss",QString::number(t),
                             "-vframes","1","-f","image2","-y",out});
        proc.waitForFinished(12000);
        if(proc.exitCode()==0&&QFile::exists(out)) frames<<out;
    }
    emit resultReady(!frames.isEmpty(),frames);
}
