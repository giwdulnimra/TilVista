#pragma once
#include <QJsonObject>
#include <QString>
#include <QStringList>
#include <QWidget>

class QLabel;
class QListWidget;
class QProgressBar;
class QPushButton;
class QScrollArea;
class QThread;

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
    void onDeleteEntry();
    void onSaveDone(bool ok);
    void onLoadDone(bool ok, QJsonObject data);

private:
    void buildUi();
    void loadFromDisk();
    void saveToDisk();
    void refreshFileList();
    void setControlsEnabled(bool on);
    void safeStopThread();          ///< wait-free cleanup before starting new thread

    QString     m_entryName;
    QString     m_sourceDir;
    QString     m_jsonPath;
    QJsonObject m_data;

    QLabel*       m_lblHeader = nullptr;
    QLabel*       m_lblStatus = nullptr;
    QListWidget*  m_fileList  = nullptr;
    QProgressBar* m_pb        = nullptr;
    QPushButton*  m_btnRandom = nullptr;
    QPushButton*  m_btnSelect = nullptr;
    QPushButton*  m_btnDelete = nullptr;

    QThread* m_thread = nullptr;
    bool     m_saving = false;   ///< guard: don't stack multiple saves
};
