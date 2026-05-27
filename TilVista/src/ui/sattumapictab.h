#pragma once
#include <QStringList>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QCheckBox;
class QThread;
class VideoPreviewWidget;

/// Tab for the SattumaPic random file opener with scalable preview.
class SattumaPicTab : public QWidget
{
    Q_OBJECT
public:
    explicit SattumaPicTab(std::function<QString()> getGlobalDir,
                           QWidget* parent = nullptr);

    /// Called by MainWindow when global dir changes.
    /// Pass cached allFiles to avoid re-scanning (may be empty).
    void onDirectoryChanged(const QString& path,
                            const QStringList& allFiles = {});

private slots:
    void onPickRandom();
    void onScanDone(bool ok, QStringList imageFiles, QStringList allFiles);
    void onOpenFile();
    void onOpenFileDir();

private:
    void buildUi();
    void scanAndPick(const QString& path);
    void updatePreview(const QString& path);

    std::function<QString()> m_getGlobalDir;

    QString     m_randomFile;
    QStringList m_allFiles;

    QLabel*           m_lblFile      = nullptr;
    QProgressBar*     m_pb           = nullptr;
    QPushButton*      m_btnRandom    = nullptr;
    QPushButton*      m_btnOpenFile  = nullptr;
    QPushButton*      m_btnOpenDir   = nullptr;
    QCheckBox*        m_chkAutoPick  = nullptr;
    QCheckBox*        m_chkAutoOpen  = nullptr;
    VideoPreviewWidget* m_preview    = nullptr;
    QLabel*           m_lblType      = nullptr;

    QThread* m_scanThread = nullptr;
};
