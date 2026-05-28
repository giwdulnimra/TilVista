#pragma once
#include "previewlabel.h"
#include <QStackedWidget>
#include <QStringList>
#include <QTimer>

class QMediaPlayer; class QAudioOutput; class QVideoWidget;
class QThread; class VideoFramesWorker;

class VideoPreviewWidget : public QStackedWidget {
    Q_OBJECT
public:
    explicit VideoPreviewWidget(QWidget* parent=nullptr);
    ~VideoPreviewWidget() override;
    void play(const QString& path);
    void showPixmap(const QPixmap& pm, const QString& label={});
    void clearPreview();
signals:
    void statusText(const QString& text);
private slots:
    void onFramesReady(bool ok, const QStringList& paths);
    void nextFrame();
private:
    void stop_(); void cleanupTmp();
    QMediaPlayer* m_player=nullptr; QAudioOutput* m_audio=nullptr;
    QVideoWidget* m_videoW=nullptr;
    PreviewLabel* m_frameLabel; QTimer* m_cycleTimer;
    QStringList m_framePaths; int m_frameIdx=0; QString m_tmpDir;
    QThread* m_vidThread=nullptr; VideoFramesWorker* m_vidWorker=nullptr;
    static constexpr int kCycleMs=10000;
};
