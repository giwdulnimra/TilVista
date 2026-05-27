#pragma once
#include <QStringList>
#include <QWidget>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;
class ReviewDBPanel;
class VideoPreviewWidget;

/// Tab for the SattumaPic random file opener.
///
/// v0.5.10:
///   – ReviewDBPanel (DB2) embedded below file controls
///   – "Select in File's Directory": opens folder + highlights file
///   – auto_pick_on_dir_select default from config (true)
class SattumaPicTab : public QWidget
{
    Q_OBJECT
public:
    explicit SattumaPicTab(std::function<QString()> getGlobalDir,
                            ReviewDBPanel*           reviewDb,
                            QWidget* parent = nullptr);

    void onDirectoryChanged(const QString& path,
                            const QStringList& allFiles = {});

private slots:
    void onPickRandom();
    void onScanDone(bool ok, QStringList imageFiles, QStringList allFiles);
    void onOpenFile();
    void onSelectInDir();                 ///< v0.5.10: highlight in explorer
    void onPreviewFromDb2(const QString& path);

private:
    void buildUi();
    void scanAndPick(const QString& path);
    void displayFile(const QString& path);
    void updatePreview(const QString& path);

    std::function<QString()> m_getGlobalDir;
    ReviewDBPanel*           m_reviewDb;    ///< shared, not owned

    QString     m_randomFile;
    QStringList m_allFiles;

    QLabel*           m_lblFile     = nullptr;
    QProgressBar*     m_pb          = nullptr;
    QPushButton*      m_btnRandom   = nullptr;
    QPushButton*      m_btnOpenFile = nullptr;
    QPushButton*      m_btnOpenDir  = nullptr;   ///< renamed v0.5.10
    QCheckBox*        m_chkAutoPick = nullptr;
    QCheckBox*        m_chkAutoOpen = nullptr;
    VideoPreviewWidget* m_preview   = nullptr;
    QLabel*           m_lblType     = nullptr;

    QThread* m_scanThread = nullptr;
};
