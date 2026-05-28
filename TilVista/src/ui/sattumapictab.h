#pragma once
#include <QStringList>
#include <QWidget>
#include <functional>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;
class ShujukoPanel;
class VideoPreviewWidget;

/// SattumaPic tab – random file opener.
/// v0.5.10+: ShujukoPanel (DB2) embedded in left column.
///           ShujukoPanel is only active when a kaivo entry is loaded.
class SattumaPicTab : public QWidget
{
    Q_OBJECT
public:
    explicit SattumaPicTab(std::function<QString()> getGlobalDir,
                            ShujukoPanel*            shujuko,
                            QWidget* parent = nullptr);

    void onDirectoryChanged(const QString& path,
                            const QStringList& allFiles = {});

private slots:
    void onPickRandom();
    void onScanDone(bool ok, QStringList imageFiles, QStringList allFiles);
    void onOpenFile();
    void onSelectInDir();
    void onPreviewFromShujuko(const QString& path);

private:
    void buildUi();
    void scanAndPick(const QString& path);
    void displayFile(const QString& path);
    void updatePreview(const QString& path);

    std::function<QString()> m_getGlobalDir;
    ShujukoPanel*            m_shujuko;   // shared, not owned

    QString     m_randomFile;
    QStringList m_allFiles;

    QLabel*             m_lblFile     = nullptr;
    QProgressBar*       m_pb          = nullptr;
    QPushButton*        m_btnRandom   = nullptr;
    QPushButton*        m_btnOpenFile = nullptr;
    QPushButton*        m_btnOpenDir  = nullptr;
    QCheckBox*          m_chkAutoPick = nullptr;
    QCheckBox*          m_chkAutoOpen = nullptr;
    VideoPreviewWidget* m_preview     = nullptr;
    QLabel*             m_lblType     = nullptr;
    QThread*            m_scanThread  = nullptr;
};
