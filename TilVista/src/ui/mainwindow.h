#pragma once
#include <QMainWindow>
#include <QStringList>

class DirBar;
class AleaVueTab;
class DirDatabasePanel;
class ShujukoPanel;
class SattumaPicTab;
class QLabel;
class QShortcut;
class QTabWidget;

/// MainWindow – root widget.
///
/// v0.5.31:
///   – Secret mode via QShortcut (Ctrl+Alt+F8), ApplicationShortcut context
///     → fires regardless of which widget has keyboard focus.
///   – Lock indicator: small 🔒/🔓 label in the status bar bottom-right.
///   – Window title shows TV_APPVERSION_DISPLAY (e.g. "v0.05.31").
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
    void toggleSecretMode();   ///< triggered by Ctrl+Alt+F8 shortcut

private:
    void updateSecretIndicator();

    DirBar*        m_dirBar        = nullptr;
    QTabWidget*    m_tabs          = nullptr;
    ShujukoPanel*  m_shujuko       = nullptr;
    AleaVueTab*    m_aleaVueTab    = nullptr;
    SattumaPicTab* m_sattumaPicTab = nullptr;

    QLabel*    m_secretIndicator = nullptr;   ///< lock icon in status bar
    QShortcut* m_secretShortcut  = nullptr;
    bool       m_secretMode      = false;
};
