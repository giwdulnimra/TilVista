#pragma once
#include <QStringList>
#include <QWidget>
#include <functional>

class QLabel;
class QProgressBar;
class QPushButton;
class QStackedWidget;
class QThread;
class QTimer;
class MadoOverlay;

/// Madoludus – in-window slideshow for images and videos.
///
/// Keyboard (tab must have focus, or use ApplicationShortcut in MainWindow):
///   Space          – Play / Pause
///   →  ←           – Next / Previous file
///   Shift+→ Shift+← – Video: seek +5 / −5 s
///   M              – Mute toggle
///   F              – Toggle FF mode (secret mode required to turn OFF)
///
/// FF / Impression mode (default ON):
///   Images: 2 s per image (instead of 5 s)
///   Videos: extract 1 frame/s via ffmpeg and show them as a flipbook
///   Can only be disabled via F key when Secret Mode is active.
///
/// Auto-advance:
///   Video: on playback end signal
///   Image: after 5 s (normal) or 2 s (FF mode)
class MadolodosTab : public QWidget
{
    Q_OBJECT
public:
    explicit MadolodosTab(std::function<QString()> getGlobalDir,
                          QWidget* parent = nullptr);
    ~MadolodosTab() override;

    void onDirectoryChanged(const QString& path,
                            const QStringList& allFiles = {});
    void setSecretMode(bool on);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;

private slots:
    void onScanDone(bool ok, QStringList imageFiles, QStringList allFiles);
    void onNextFile();
    void onPrevFile();
    void onPlayPause();
    void onMuteToggle();
    void onFFToggle();
    void onAutoAdvance();
    void onOverlayFade();
    void onVideoEnded();

private:
    void buildUi();
    void buildOverlay();
    void loadFile(int index);
    void loadImage(const QString& path);
    void loadVideo(const QString& path);
    void startAutoAdvance();
    void stopAutoAdvance();
    void showOverlay();
    void positionOverlay();
    void updateButtonStates();
    void scanDir(const QString& path);
    void applyFFMode();
    bool isMediaFile(const QString& path) const;

    std::function<QString()> m_getGlobalDir;
    QStringList m_mediaFiles;
    int         m_index     = -1;

    bool m_paused     = false;
    bool m_muted      = false;
    bool m_ffMode     = true;
    bool m_secretMode = false;
    bool m_isVideo    = false;

    // ── Display stack: page 0 = image, page 1 = video ──────────────────────
    QStackedWidget* m_stack      = nullptr;
    QLabel*         m_imgLabel   = nullptr;
    QWidget*        m_videoPage  = nullptr;

    // ── Overlay ────────────────────────────────────────────────────────────
    MadoOverlay*    m_overlay      = nullptr;
    QTimer*         m_overlayTimer = nullptr;

    // ── Auto-advance timer ─────────────────────────────────────────────────
    QTimer*         m_advTimer   = nullptr;

    // ── Status ─────────────────────────────────────────────────────────────
    QLabel*         m_lblStatus  = nullptr;
    QProgressBar*   m_pb         = nullptr;

    // ── Scan ───────────────────────────────────────────────────────────────
    QThread*        m_scanThread = nullptr;

#ifndef TV_NO_MULTIMEDIA
    class QMediaPlayer* m_player = nullptr;
    class QAudioOutput* m_audio  = nullptr;
    class QVideoWidget* m_videoW = nullptr;
#endif
};
