#pragma once
#include <QStringList>
#include <QWidget>
#include <functional>

class QCheckBox;
class QLabel;
class QProgressBar;
class QPushButton;
class QThread;
class DirDatabasePanel;
class ShujukoPanel;
class SlideshowWindow;

class AleaVueTab : public QWidget
{
    Q_OBJECT
public:
    explicit AleaVueTab(std::function<QString()> getGlobalDir,
                        ShujukoPanel*            shujuko,
                        QWidget* parent = nullptr);

    void onDirectoryChanged(const QString& path);
    void loadFromDirectory();

    DirDatabasePanel* dbPanel() const { return m_dbPanel; }

signals:
    void requestDirChange(const QString& path);

private slots:
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
    ShujukoPanel*            m_shujuko;   // shared, not owned

    QStringList m_imagePaths;
    QStringList m_allFiles;
    QString     m_lastScannedPath;

    QLabel*           m_lblStatus     = nullptr;
    QProgressBar*     m_pb            = nullptr;
    QPushButton*      m_btnStart      = nullptr;
    QCheckBox*        m_chkFullscreen = nullptr;
    DirDatabasePanel* m_dbPanel       = nullptr;
    SlideshowWindow*  m_slideshowWindow = nullptr;
    QThread*          m_scanThread    = nullptr;
};
