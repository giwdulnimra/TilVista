#pragma once
#include <QMainWindow>
#include <QStringList>

class DirBar;
class AleaVueTab;
class DirDatabasePanel;
class ShujukoPanel;
class SattumaPicTab;
class QTabWidget;

/// MainWindow – root widget.
///
/// v0.5.30: Secret mode triggered by Alt+S+AltGr+L key sequence.
///          State machine tracks: Alt → S → AltGr → L.
class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

private slots:
    void onDirChanged(const QString& path);
    void onDirFromDb(const QString& path);
    void onDb1FilesLoaded(const QString& path,
                          const QStringList& imageFiles,
                          const QStringList& allFiles);
    void onActiveEntryChanged(const QString& entryName,
                               const QString& sourceDir);

private:
    void initSecretKeyState();
    bool advanceSecretKey(int key, Qt::KeyboardModifiers mods);

    DirBar*        m_dirBar        = nullptr;
    QTabWidget*    m_tabs          = nullptr;
    ShujukoPanel*  m_shujuko       = nullptr;   ///< shared between tabs
    AleaVueTab*    m_aleaVueTab    = nullptr;
    SattumaPicTab* m_sattumaPicTab = nullptr;

    // v0.5.30 secret key state machine: Alt → S → AltGr → L
    enum class SKState { Idle, GotAlt, GotS, GotAltGr };
    SKState m_skState = SKState::Idle;
    bool    m_secretMode = false;
};
