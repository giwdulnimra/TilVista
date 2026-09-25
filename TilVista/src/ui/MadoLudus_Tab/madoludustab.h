#pragma once
#include <QStringList>
#include <QWidget>
#include <functional>
#include "madoluduswindow.h"

class QCheckBox;
class QComboBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;
class VideoPreviewWidget;

/// Madoludus config tab (v0.5.42 refactor).
///
/// The tab shows the configuration panel and a small preview area.
/// Pressing "Start MadoLudus" opens a separate MadoludusWindow.
///
/// ASCII layout (see TilVista_GUI.md):
///   ┌──────────────────────────┬─────────────────┐
///   │ Configure MadoLudus      │  Preview Window  │
///   │ [x] Images               │                  │
///   │ [x] Videos               │                  │
///   │    [x] FF-Mode           │                  │
///   │    [1/sec ▼]             │                  │
///   │    [x] Muted             │──────────────────│
///   │ [x] random sorted        │ [preview path]   │
///   │                          │                  │
///   │ [ ▶ Start MadoLudus ]   │                  │
///   └──────────────────────────┴─────────────────┘
class MadoludusTab : public QWidget
{
    Q_OBJECT
public:
    explicit MadoludusTab(std::function<QString()> getGlobalDir,
                          QWidget* parent = nullptr);
    ~MadoludusTab() override;

    void onDirectoryChanged(const QString& path,
                            const QStringList& allFiles = {});
    void setSecretMode(bool on);

private slots:
    void showPreview(const QString& path);
    void onStartClicked();
    void onScanDone(bool ok, QStringList imageFiles, QStringList allFiles);
    void onWindowClosed();
    void openWindow(const QStringList& files);

private:
    void buildUi();
    void scanDir(const QString& path);
    MadoludusConfig buildConfig() const;
    QStringList filteredFiles() const;

    std::function<QString()> m_getGlobalDir;
    QStringList m_allFiles;
    bool        m_secretMode = false;

    // ── Config widgets ────────────────────────────────────────────────────
    QCheckBox*   m_chkImages   = nullptr;
    QCheckBox*   m_chkVideos   = nullptr;
    QCheckBox*   m_chkFF       = nullptr;
    QComboBox*   m_cmbFFRate   = nullptr;
    QCheckBox*   m_chkMuted    = nullptr;
    QCheckBox*   m_chkRandom   = nullptr;
    QPushButton* m_btnPreview  = nullptr;
    QPushButton* m_btnStart    = nullptr;
    QLabel*      m_lblStatus   = nullptr;
    QProgressBar* m_pb         = nullptr;

    // ── Preview area (right column) ───────────────────────────────────────
    VideoPreviewWidget* m_preview  = nullptr;
    QLabel*             m_lblType  = nullptr;

    // ── Scan thread ───────────────────────────────────────────────────────
    QThread* m_scanThread = nullptr;

    // ── Running playback window ───────────────────────────────────────────
    MadoludusWindow* m_window = nullptr;
};
