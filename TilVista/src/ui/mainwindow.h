#pragma once
#include <QMainWindow>
#include <QStringList>

class DirBar;
class AleaVueTab;
class DirDatabasePanel;
class MadoludusTab;
class ShortcutsTab;
class ShujukoPanel;
class SattumaPicTab;
class QCloseEvent;
class QLabel;
class QShortcut;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    /// v0.5.42: give kaivo (DirDatabasePanel) and shujuko (ShujukoPanel) a
    /// brief moment to finish any in-flight background save before the
    /// app actually exits – see *::flushPendingWrites().
    void closeEvent(QCloseEvent* event) override;

private slots:
    void onDirChanged(const QString& path);
    void onDirFromDb(const QString& path);
    void onDb1FilesLoaded(const QString& path,
                          const QStringList& imageFiles,
                          const QStringList& allFiles);
    void onActiveEntryChanged(const QString& entryName,
                               const QString& sourceDir);
    void toggleSecretMode();

private:
    void updateSecretIndicator();

    DirBar*        m_dirBar        = nullptr;
    QTabWidget*    m_tabs          = nullptr;
    ShujukoPanel*  m_shujuko       = nullptr;
    AleaVueTab*    m_aleaVueTab    = nullptr;
    SattumaPicTab* m_sattumaPicTab = nullptr;
    MadoludusTab*  m_madoludusTab  = nullptr;
    ShortcutsTab*  m_shortcutsTab  = nullptr;

    QLabel*    m_secretIndicator = nullptr;
    QShortcut* m_secretShortcut  = nullptr;
    bool       m_secretMode      = false;
};
