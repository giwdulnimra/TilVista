#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QThread;

/// Shujuko (DB2) – stores files bookmarked with S during AleaVue.
///
/// Always linked to one kaivo entry. Disabled until setActiveKaivoEntry().
/// JSON: optegnelser/kaivo/<safe_name>_shujuko.json
///
/// v0.5.31:
///   – Delete selected entry from shujuko list (visible only in secret mode).
///   – setSecretMode(bool) shows/hides the delete button.
///
/// Signals:
///   fileSelected(path) – user chose a file via Random/Select/DoubleClick
class ShujukoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ShujukoPanel(QWidget* parent = nullptr);

    void setActiveKaivoEntry(const QString& kaivoEntryName,
                              const QString& sourceDir);
    void addBookmark(const QString& filePath);
    void validateFiles();
    void setSecretMode(bool on);

    QString currentJsonPath() const;

signals:
    void fileSelected(const QString& path);

private slots:
    void onPickRandom();
    void onPickSelected();
    void onDeleteEntry();     ///< v0.5.31: delete selected file from shujuko
    void onSaveDone(bool ok);
    void onLoadDone(bool ok, QJsonObject data);

private:
    void buildUi();
    void loadFromDisk();
    void saveToDisk();
    void refreshFileList();
    void setControlsEnabled(bool on);

    QString     m_entryName;
    QString     m_sourceDir;
    QString     m_jsonPath;
    QJsonObject m_data;

    QLabel*       m_lblHeader  = nullptr;
    QLabel*       m_lblStatus  = nullptr;
    QListWidget*  m_fileList   = nullptr;
    QProgressBar* m_pb         = nullptr;
    QPushButton*  m_btnRandom  = nullptr;
    QPushButton*  m_btnSelect  = nullptr;
    QPushButton*  m_btnDelete  = nullptr;   ///< v0.5.31, secret mode only

    QThread* m_thread = nullptr;
};
