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
/// Design rules (v0.5.20+):
///   – Always linked to exactly one kaivo (DB1) entry by name.
///   – Only visible / active when a kaivo entry is loaded.
///   – Stored in kaivo dir as  <safe_name>_shujuko.json
///   – Deleted when the linked kaivo entry is deleted.
///   – v0.5.20 "Update Entry" validates every listed file still exists.
///   – v0.5.30 secret mode: hidden entries visible only in secret mode.
///
/// JSON schema:
///   {
///     "kaivo_entry":  "Brasilien_250521",
///     "source_dir":   "E:/Fotos/Brasilien",
///     "files": [
///       { "path": "E:/.../img.jpg", "added_at": "2025-05-21T14:00:00",
///         "missing": false }
///     ]
///   }
///
/// Signals:
///   fileSelected(path)  – user chose a file via Random/Select buttons
class ShujukoPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ShujukoPanel(QWidget* parent = nullptr);

    /// Load (or create fresh) the shujuko for *kaivoEntryName*.
    /// Pass empty string to deactivate the panel.
    void setActiveKaivoEntry(const QString& kaivoEntryName,
                              const QString& sourceDir);

    /// Called by SlideshowWindow when S is pressed.
    /// Saves immediately and refreshes the file list.
    void addBookmark(const QString& filePath);

    /// Called by DirDatabasePanel::updateEntry() (v0.5.20).
    /// Checks each stored path and marks missing ones.
    void validateFiles();

    /// Returns the path of the current shujuko JSON (may not exist yet).
    QString currentJsonPath() const;

signals:
    void fileSelected(const QString& path);

private slots:
    void onPickRandom();
    void onPickSelected();
    void onSaveDone(bool ok);
    void onLoadDone(bool ok, QJsonObject data);

private:
    void buildUi();
    void loadFromDisk();
    void saveToDisk();
    void refreshFileList();
    void setEnabled_(bool on);   // enable/disable all controls

    QString     m_entryName;
    QString     m_sourceDir;
    QString     m_jsonPath;
    QJsonObject m_data;          ///< { kaivo_entry, source_dir, files:[...] }

    QLabel*       m_lblHeader  = nullptr;
    QLabel*       m_lblStatus  = nullptr;
    QListWidget*  m_fileList   = nullptr;
    QProgressBar* m_pb         = nullptr;
    QPushButton*  m_btnRandom  = nullptr;
    QPushButton*  m_btnSelect  = nullptr;

    QThread* m_thread = nullptr;
};
