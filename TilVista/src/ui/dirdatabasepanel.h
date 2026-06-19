#pragma once
#include <QJsonObject>
#include <QStringList>
#include <QWidget>
#include <functional>

class QLabel;
class QLineEdit;
class QListWidget;
class QListWidgetItem;
class QProgressBar;
class QPushButton;
class QThread;

/// kaivo Directory Database Panel (DB1).
///
/// Threading rules (v0.5.42):
///   – m_thread   : catalogue write / JSON-only save / index load
///   – m_catThread: catalogue read (on "Load from DB")
///   – m_updThread: directory rescan ("Update Entry")
///   Each slot that starts a thread first calls safeStop(thread) to
///   disconnect and disown any still-running thread before launching a new one.
///   This prevents signal delivery to stale `this` pointers.
///
///   While a catalogue load (m_catThread) is running, setBusy(true) disables
///   Save/Load/Delete/Update and the entry list, so a second click can't
///   start a competing operation on the same QJsonObject (m_db) while it is
///   being read. emitFromName() also ignores re-entrant calls outright.
///
///   flushPendingWrites() is called from MainWindow::closeEvent() so an
///   in-flight "Save to DB" write still completes before the app actually
///   exits, instead of being silently abandoned mid-write.
class DirDatabasePanel : public QWidget
{
    Q_OBJECT
public:
    explicit DirDatabasePanel(std::function<QString()> getCurrentDir,
                              QWidget* parent = nullptr);

    void updateCache(const QString& path,
                     const QStringList& imageFiles,
                     const QStringList& allFiles);
    void setSecretMode(bool on);
    bool secretMode() const { return m_secretMode; }

    /// v0.5.42: block briefly (pumping the event loop) until any in-flight
    /// save/catalogue-write thread has actually finished writing to disk.
    /// Safe to call from MainWindow::closeEvent().
    void flushPendingWrites();

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
    void onUpdateClicked();
    void onToggleHiddenClicked();
    void onItemDoubleClicked(QListWidgetItem*);
    void onCurrentNameChanged(const QString& name);

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

    /// v0.5.42: enable/disable everything that touches m_db while a
    /// catalogue load is running in the background.
    void setBusy(bool busy);

    /// Disconnect all signals from *thread* to this object, then clear the
    /// pointer. The thread and worker clean themselves up via deleteLater.
    void safeStop(QThread*& threadRef);

    /// Strip the "◌ " decoration prefix added by refreshList().
    static QString rawName(const QString& displayName);

    std::function<QString()> m_getCurrentDir;
    QString     m_base, m_kaivoDir, m_dbPath;
    QJsonObject m_db;
    bool        m_secretMode = false;

    struct PendingCache {
        QString path; QStringList imageFiles, allFiles; bool valid = false;
    } m_pending;

    QString m_pendingLoadPath;
    QString m_activeEntryName;
    QString m_updatingEntryName;

    QLineEdit*    m_lineName     = nullptr;
    QListWidget*  m_listWidget   = nullptr;
    QLabel*       m_lblEntryInfo = nullptr;
    QLabel*       m_lblStatus    = nullptr;
    QProgressBar* m_pb           = nullptr;
    QPushButton*  m_btnSave      = nullptr;
    QPushButton*  m_btnLoad      = nullptr;
    QPushButton*  m_btnDelete    = nullptr;
    QPushButton*  m_btnUpdate    = nullptr;
    QPushButton*  m_btnSecret    = nullptr;

    QThread* m_thread    = nullptr;   // save / load index
    QThread* m_catThread = nullptr;   // catalogue load
    QThread* m_updThread = nullptr;   // rescan
};
