#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QWidget>

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QThread;
class DBSaveWorker;
class DBLoadWorker;
class CatalogueWriteWorker;
class CatalogueLoadWorker;

/// Persistent directory store.
///
/// File layout (all under optegnelser/Kaivo/):
///   tilvista_dirs.json               – index
///   <safe_name>_img.dshow            – image paths, one per line
///   <safe_name>_all.catalogue        – all paths, one per line
///
/// Signals:
///   dirLoaded(path, imageFiles, allFiles)
///       imageFiles / allFiles are empty when no catalogue exists yet.
class DirDatabasePanel : public QWidget
{
    Q_OBJECT
public:
    /// *getCurrentDir* is a callable (or lambda) that returns the currently
    /// active directory from the global DirBar. Pass it as a std::function.
    explicit DirDatabasePanel(std::function<QString()> getCurrentDir,
                              QWidget* parent = nullptr);

    /// Called by AleaVueTab after a directory scan completes.
    /// Pre-fills the name field and writes/updates catalogue files.
    void updateCache(const QString& path,
                     const QStringList& imageFiles,
                     const QStringList& allFiles);

signals:
    void dirLoaded(const QString& absPath,
                   const QStringList& imageFiles,
                   const QStringList& allFiles);

private slots:
    void onSaveClicked();
    void onLoadClicked();
    void onDeleteClicked();
    void onItemDoubleClicked(class QListWidgetItem*);
    void onCurrentNameChanged(const QString& name);

    // Worker callbacks
    void onSaveDone(bool ok);
    void onLoadDone(bool ok, QJsonObject data);
    void onCatalogueLoaded(bool ok, QStringList imageFiles, QStringList allFiles);

private:
    void buildUi();
    void refreshList();
    void emitFromName(const QString& name);
    void writeCataloguesAndJson(const QString& entryName,
                                const QStringList& imageFiles,
                                const QStringList& allFiles);
    void saveJsonOnly();           ///< For deletions
    void loadDb();
    void updateEntryInfo(const QString& name);

    std::function<QString()> m_getCurrentDir;
    QString     m_base;            ///< script directory
    QString     m_kaivoDir;
    QString     m_dbPath;
    QJsonObject m_db;              ///< full JSON document in memory

    // Pending scan data (held until user clicks Save)
    struct PendingCache {
        QString     path;
        QStringList imageFiles;
        QStringList allFiles;
        bool        valid = false;
    } m_pending;

    QString m_pendingLoadPath;     ///< set while catalogue load is in flight

    // UI
    QLineEdit*   m_lineName      = nullptr;
    QListWidget* m_listWidget    = nullptr;
    QLabel*      m_lblEntryInfo  = nullptr;
    QLabel*      m_lblStatus     = nullptr;
    QProgressBar* m_pb           = nullptr;
    QPushButton* m_btnSave       = nullptr;
    QPushButton* m_btnLoad       = nullptr;
    QPushButton* m_btnDelete     = nullptr;

    // Active threads (only one at a time per slot)
    QThread* m_thread    = nullptr;
    QThread* m_catThread = nullptr;
};
