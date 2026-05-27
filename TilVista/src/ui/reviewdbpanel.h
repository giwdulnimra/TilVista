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

/// DB2 – stores individual files bookmarked with the S key during AleaVue.
///
/// JSON schema (tilvista_review.json):
///   { "sessions": [
///       { "name":       "Brasilien_250521",
///         "source_dir": "E:/...",
///         "files":      ["path1", "path2", ...],
///         "created_at": "2025-05-21T14:00:00" }
///   ]}
///
/// Legacy migration: on first load, reviewpics.txt is imported and renamed.
///
/// Signals:
///   fileSelected(path) – emitted when a file is chosen via Random/Select
class ReviewDBPanel : public QWidget
{
    Q_OBJECT
public:
    explicit ReviewDBPanel(QWidget* parent = nullptr);

    /// Called by SlideshowWindow when S is pressed.
    void addBookmark(const QString& filePath, const QString& sourceDir);

signals:
    void fileSelected(const QString& path);

private slots:
    void onSessionChanged(const QString& name);
    void onPickRandom();
    void onPickSelected();
    void onSaveDone(bool ok);
    void onLoadDone(bool ok, QJsonObject data);

private:
    void buildUi();
    void refreshSessions();
    QStringList currentFiles() const;
    void saveDb();
    void loadDb();
    void migrateLegacyLog();

    QString     m_dbPath;
    QJsonObject m_db;             ///< { "sessions": [...] }

    QListWidget*  m_sessionList = nullptr;
    QListWidget*  m_fileList    = nullptr;
    QProgressBar* m_pb          = nullptr;
    QPushButton*  m_btnRandom   = nullptr;
    QPushButton*  m_btnSelect   = nullptr;
    QLabel*       m_lblStatus   = nullptr;

    QThread* m_thread = nullptr;
};
