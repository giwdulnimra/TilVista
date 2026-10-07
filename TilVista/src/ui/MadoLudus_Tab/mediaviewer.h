#ifndef TILVISTA_MEDIAVIEWER_H
#define TILVISTA_MEDIAVIEWER_H
#include <QWidget>
class QLabel;
class QTimer;
class QMediaPlayer;
class QVideoWidget;
class QAudioOutput;

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

private slots:
    // Aufgabe: passende Slot-Signatur für Video-Status-Änderungen finden
    // (siehe Hinweis unten zu QMediaPlayer)
private:
    void loadImage(const QString &path);
    void loadVideo(const QString &path) const;

    bool m_paused = false;
    bool m_fullscreen = false;
    int  m_imageDurationMS = 5000;
    int  m_showW = 1920, m_showH = 1080;
    double m_ratio = 16.0 / 9.0;

    QLabel* m_imageLabel = nullptr;
    QTimer* m_imageTimer = nullptr;
#ifndef TV_NO_MULTIMEDIA
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio  = nullptr;
    QVideoWidget* m_videoW = nullptr;
#endif

protected slots:
    void imageTimeout();
protected:
};

#endif //TILVISTA_MEDIAVIEWER_H
