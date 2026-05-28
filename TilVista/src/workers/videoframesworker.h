#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class VideoFramesWorker : public QObject {
    Q_OBJECT
public:
    explicit VideoFramesWorker(const QString& videoPath, QObject* parent=nullptr);
    const QString& tmpDir() const { return m_tmpDir; }
public slots: void run();
signals: void resultReady(bool ok, QStringList framePaths);
private:
    QString m_videoPath, m_tmpDir;
    static const QList<int> frameTimes;
};
