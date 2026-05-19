#pragma once
#include <QStringList>
#include <QWidget>

class QLabel;
class QProgressBar;
class QPushButton;
class QCheckBox;
class QThread;
class ScanWorker;
class DirDatabasePanel;
class SlideshowWindow;

/// Tab for the AleaVue random image slideshow.
class AleaVueTab : public QWidget
{
    Q_OBJECT
public:
    explicit AleaVueTab(std::function<QString()> getGlobalDir,
                        QWidget* parent = nullptr);

    /// Called by MainWindow when the global DirBar changes.
    void onDirectoryChanged(const QString& path);

    /// Exposes the DB panel so MainWindow can connect its dirLoaded signal.
    DirDatabasePanel* dbPanel() const { return m_dbPanel; }

signals:
    /// Emitted when the DB panel loads a dir, so MainWindow can sync DirBar.
    void requestDirChange(const QString& path);

private slots:
    void onLoadFromDirectory();
    void onDbDirLoaded(const QString& path,
                       const QStringList& imageFiles,
                       const QStringList& allFiles);
    void onScanDone(bool ok, QStringList imageFiles, QStringList allFiles);
    void onScanThenOpen(bool ok, QStringList imageFiles, QStringList allFiles);
    void onStartSlideshow();

private:
    void buildUi();
    void scanDir(const QString& path);
    void openWindow(const QString& directory, const QStringList& imagePaths);

    std::function<QString()> m_getGlobalDir;

    QStringList m_imagePaths;
    QStringList m_allFiles;
    QString     m_lastScannedPath;

    QLabel*       m_lblStatus  = nullptr;
    QProgressBar* m_pb         = nullptr;
    QPushButton*  m_btnLoadDir = nullptr;
    QPushButton*  m_btnStart   = nullptr;
    QCheckBox*    m_chkFullscreen = nullptr;

    DirDatabasePanel* m_dbPanel          = nullptr;
    SlideshowWindow*  m_slideshowWindow  = nullptr;

    QThread*     m_scanThread = nullptr;
};
