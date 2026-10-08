#ifndef TILVISTA_MEDIAVIEWER_H
#define TILVISTA_MEDIAVIEWER_H
#include <QWidget>
#include <QPixmap>
class QLabel;
class QStackedLayout;
class QTimer;
class QResizeEvent;
#ifndef TV_NO_MULTIMEDIA
class QMediaPlayer;
class QVideoWidget;
class QAudioOutput;
#endif // TV_NO_MULTIMEDIA

class MediaViewer : public QWidget
{
    Q_OBJECT
public:
    explicit MediaViewer(QWidget *parent = nullptr);
    ~MediaViewer() override; //??

    void showFile(const QString &path);   // räumt intern zuerst per stop() auf, entscheidet dann Bild vs. Video
    void stop();                           // Timer/Video stoppen, KEIN finished()
    void togglePause();                    // Pause/Resume (Bild-Timer bzw. Video)
    bool isPaused() const;

    void setImageDurationMS(int newMS);
    // Default 5000ms, überschreibbar (Vorgriff auf nächsten Schritt,
    // wenn's aus der Config kommt) - Aufgabe: sinnvoller Setter/Member

    signals:
        void finished();   // NUR natürliches Ende (Bild-Timer abgelaufen ODER Video zu Ende)

protected slots:
protected:
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void imageTimeout();
private:
    void loadImage(const QString &path);
    void loadVideo(const QString &path) const;
    void updateImageLabel();

    bool m_paused = false;
    //bool m_fullscreen = false;
    int  m_imageDurationMS = 5000;
    QPixmap m_pixmap;

    QStackedLayout* m_stack = nullptr;
    QLabel* m_imageLabel = nullptr;
    QTimer* m_imageTimer = nullptr;
#ifndef TV_NO_MULTIMEDIA
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio  = nullptr;
    QVideoWidget* m_videoW = nullptr;
#endif
};

#endif //TILVISTA_MEDIAVIEWER_H
