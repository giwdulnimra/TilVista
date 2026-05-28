#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QWidget>
#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QProgressBar;
class QPushButton;
class QThread;

/// DB1 – kaivo directory store.
///
/// v0.5.20: "Update Entry" button rescans the directory and refreshes
///          catalogues without deleting the linked shujuko.
///          After rescan, emits requestShujukoValidation() so ShujukoPanel
///          can check whether its stored files still exist.
///
/// v0.5.30: Secret Mode – entries marked "hidden":true are invisible
///          unless secret mode is active.
///          secretModeChanged(bool) signal lets the rest of the UI adapt.
///
/// Signals:
///   dirLoaded(path, imageFiles, allFiles)
///       – emitted after catalogue is loaded; imageFiles/allFiles empty
///         when no catalogue exists yet (caller should rescan).
///   activeEntryChanged(entryName, sourceDir)
///       – emitted together with dirLoaded so ShujukoPanel can activate.
///   requestShujukoValidation()
///       – emitted after "Update Entry" rescan so ShujukoPanel validates.
///   secretModeChanged(bool active)
///       – emitted when secret mode is toggled (v0.5.30).
class DirDatabasePanel : public QWidget
{
    Q_OBJECT
public:
    explicit DirDatabasePanel(std::function<QString()> getCurrentDir,
                              QWidget* parent = nullptr);

    /// Called by AleaVueTab after a directory scan completes.
    void updateCache(const QString& path,
                     const QStringList& imageFiles,
                     const QStringList& allFiles);

    /// v0.5.30: enable/disable secret mode from outside (MainWindow key handler).
    void setSecretMode(bool on);
    bool secretMode() const { return m_secretMode; }

signals:
    void dirLoaded(const QString& absPath,
                   const QStringList& imageFiles,
                   const QStringList& allFiles);
    void activeEntryChanged(const QString& entryName,
                             const QString& sourceDir);
    void requestShujukoValidation();
    void secretModeChanged(bool active);

private slots:
    void onSaveClicked();
    void onLoadClicked();
    void onDeleteClicked();
    void onUpdateClicked();          // v0.5.20
    void onToggleHiddenClicked();    // v0.5.30
    void onItemDoubleClicked(class QListWidgetItem*);
    void onCurrentNameChanged(const QString& name);

    // Worker callbacks
    void onSaveDone(bool ok);
    void onIndexLoadDone(bool ok, QJsonObject data);
    void onCatalogueLoaded(bool ok, QStringList imageFiles, QStringList allFiles);
    void onUpdateScanDone(bool ok, QStringList imageFiles, QStringList allFiles);

private:
    void buildUi();
    void refreshList();
    void emitFromName(const QString& name);
    void writeCataloguesAndJson(const QString& entryName,
                                const QStringList& imageFiles,
                                const QStringList& allFiles);
    void saveJsonOnly();
    void loadDb();
    void updateEntryInfo(const QString& name);
    bool isEntryVisible(const QJsonObject& entry) const;

    std::function<QString()> m_getCurrentDir;
    QString     m_base;
    QString     m_kaivoDir;
    QString     m_dbPath;
    QJsonObject m_db;              ///< { "entries": [...] }
    bool        m_secretMode = false;

    struct PendingCache {
        QString     path;
        QStringList imageFiles;
        QStringList allFiles;
        bool        valid = false;
    } m_pending;

    QString m_pendingLoadPath;
    QString m_activeEntryName;     ///< currently loaded entry (for shujuko)

    // UI
    QLineEdit*    m_lineName     = nullptr;
    QListWidget*  m_listWidget   = nullptr;
    QLabel*       m_lblEntryInfo = nullptr;
    QLabel*       m_lblStatus    = nullptr;
    QProgressBar* m_pb           = nullptr;
    QPushButton*  m_btnSave      = nullptr;
    QPushButton*  m_btnLoad      = nullptr;
    QPushButton*  m_btnDelete    = nullptr;
    QPushButton*  m_btnUpdate    = nullptr;   // v0.5.20
    QPushButton*  m_btnSecret    = nullptr;   // v0.5.30, hidden by default

    QThread* m_thread    = nullptr;
    QThread* m_catThread = nullptr;
    QThread* m_updThread = nullptr;
};
