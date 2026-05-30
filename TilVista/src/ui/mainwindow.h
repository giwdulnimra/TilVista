#pragma once
#include <QMainWindow>
#include <QStringList>

class DirBar;
class AleaVueTab;
class DirDatabasePanel;
class MadolodosTab;
class ShortcutsTab;
class ShujukoPanel;
class SattumaPicTab;
class QLabel;
class QShortcut;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

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
    ShortcutsTab*  m_shortcutsTab  = nullptr;
    MadolodosTab*  m_madolodosTab  = nullptr;

    QLabel*    m_secretIndicator = nullptr;
    QShortcut* m_secretShortcut  = nullptr;
    bool       m_secretMode      = false;
};
