#pragma once
#include <QMainWindow>
#include <QStringList>

class QLabel;
class QTimer;
class MadoOverlay;
class VideoFramesWorker;
class QThread;
class QMediaPlayer;
class QAudioOutput;
class QVideoWidget;
class QCloseEvent;

/// Configuration passed from MadoludusTab to MadoludusWindow.
struct MadoludusConfig {
    bool   showImages  = true;
    bool   showVideos  = true;
    bool   ffMode      = true;
    double ffValue     = 10.0;      ///< seconds (IntervalSeconds) or count (FixedCount)
    bool   ffFixedCount = false;    ///< false = IntervalSeconds, true = FixedCount
    bool   muted       = false;
    bool   randomMode  = false;
    bool   secretMode  = false;
};

/// Madoludus playback window (v0.5.42 refactor).
///
/// Frameless, borderless window with a left-side vertical control strip
/// (see ASCII layout in TilVista_GUI.md):
///
///   ┌──────────────────────────────────┐
///   │ [⏮][⏸][⏭]                       │
///   │ [🔊]       ← grey on transparent│
///   │ [FF]                             │
///   │ [🔀]        Display Space        │
///   │                                  │
///   └──────────────────────────────────┘
///   [display file path]
///
/// The window emits windowClosed() when the user presses Escape or clicks
/// the close button (the OS chrome is hidden, so Escape is the main exit).
///
/// Navigation history: Previous (←) always walks backwards through the
/// actual order the files were shown in, even in random mode.
class MadoludusWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MadoludusWindow(const QStringList&   mediaFiles,
                              const MadoludusConfig& cfg,
                              QWidget*             parent = nullptr);
    ~MadoludusWindow() override;

signals:
    void windowClosed();
    void currentFileChanged(const QString& path);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onNextFile();
    void onPrevFile();
    void onPlayPause();
    void onMuteToggle();
    void onFFToggle();
    void onShuffleToggle();
    void onAutoAdvance();
    void onVideoEnded();
    void onFFFramesReady(bool ok, QStringList framePaths);
    void cycleFFFrame();

private:
    void buildUi();
    void loadFile(int index);
    void loadImage(const QString& path);
    void loadVideo(const QString& path);
    int  nextIndex() const;
    void startAutoAdvance();
    void stopAutoAdvance();
    void startFFExtraction(const QString& path);
    void stopFFExtraction();
    void updateButtonStates();
    void scaleCurrentImage();

    QStringList    m_mediaFiles;
    MadoludusConfig m_cfg;

    int  m_index      = -1;
    bool m_paused     = false;
    bool m_isVideo    = false;
    bool m_usingFlipbook = false;

    // Navigation history (Previous works even in random mode)
    QList<int> m_order;
    int        m_orderPos = -1;

    // ── UI ────────────────────────────────────────────────────────────────
    QWidget*        m_central   = nullptr;
    QLabel*         m_display   = nullptr;   // image / flip-book page
    QLabel*         m_lblPath   = nullptr;   // bottom status line

    // ── Overlay (left-side vertical strip, always visible) ────────────────
    MadoOverlay*    m_overlay   = nullptr;

    // ── Timers ────────────────────────────────────────────────────────────
    QTimer*         m_advTimer      = nullptr;
    QTimer*         m_ffCycleTimer  = nullptr;

    // ── FF flip-book ──────────────────────────────────────────────────────
    QThread*    m_ffThread   = nullptr;
    QStringList m_ffFrames;
    int         m_ffFrameIdx = 0;
    QString     m_ffTmpDir;

    // ── Direct video playback (FF mode OFF) ───────────────────────────────
#ifndef TV_NO_MULTIMEDIA
    QWidget*      m_videoContainer = nullptr;
    QMediaPlayer* m_player = nullptr;
    QAudioOutput* m_audio  = nullptr;
    QVideoWidget* m_videoW = nullptr;
#endif

    // Window drag (frameless)
    QPoint m_dragStart;
    bool   m_dragging = false;

    static constexpr int kImageDelay   = 5000;
    static constexpr int kImageDelayFF = 2000;
    static constexpr int kFFCycleMs    = 600;
    static constexpr int kSeekStep     = 5000;
};
