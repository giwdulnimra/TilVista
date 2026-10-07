#ifndef TV_MADOLUDUSWINDOW_H
#define TV_MADOLUDUSWINDOW_H

#include "mediaviewer.h"
#include <QMainWindow>
#include <QStringList>

class QLabel;
class MadoOverlay;
class QCloseEvent;

struct MadoludusConfig {
    bool showImages = true;
    bool showVideos = true;
    //bool ffMode = true;
    //double ffValue = 10.0;
    //bool ffFixedCount = false;
    bool muted = false;
    bool randomMode = false;
    bool secretMode = false;
    bool fullscreen = false;
};

class MadoludusWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MadoludusWindow(const QStringList& mediaFiles,
                              const MadoludusConfig& cfg,
                              QWidget* parent = nullptr);
    ~MadoludusWindow() override;

    void shufflePlaylist();

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
    void advance();
    void onPlayPause() const;
    void onMuteToggle();
    void onFFToggle();
    void onShuffleToggle();
private:
    void buildUi();
    void updateButtonStates();

    QStringList     m_mediaFiles;
    MadoludusConfig m_cfg;
    QStringList m_playlist;
    int64_t m_playlistIndex = 0;

    // ── UI ────────────────────────────────────────────────────────────────
    QWidget*        m_central = nullptr;
    MediaViewer*    m_viewer = nullptr;
    QLabel*         m_lblPath = nullptr;
    MadoOverlay*    m_overlay = nullptr;

    // Window drag (frameless)
    QPoint m_dragStart;
    bool   m_dragging = false;
};

#endif // TV_MADOLUDUSWINDOW_H
