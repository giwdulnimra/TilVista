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
class QRegularExpression;
class QThread;

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

    std::function<QString()> m_getCurrentDir;
    QString     m_base, m_kaivoDir, m_dbPath;
    QJsonObject m_db;
    bool        m_secretMode = false;

    struct PendingCache {
        QString path; QStringList imageFiles, allFiles; bool valid = false;
    } m_pending;

    QString m_pendingLoadPath;
    QString m_activeEntryName;
    QString m_updatingEntryName;   ///< stored at scan-start, used in callback

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

    QThread* m_thread    = nullptr;
    QThread* m_catThread = nullptr;
    QThread* m_updThread = nullptr;
};
