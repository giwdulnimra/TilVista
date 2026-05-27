#pragma once
#include <QMainWindow>
#include <QStringList>

class DirBar;
class AleaVueTab;
class SattumaPicTab;
class QTabWidget;

class MainWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit MainWindow(QWidget* parent = nullptr);

private slots:
    void onDirChanged(const QString& path);
    void onDirFromDb(const QString& path);
    void onDbFilesLoaded(const QString& path,
                         const QStringList& imageFiles,
                         const QStringList& allFiles);

private:
    DirBar*        m_dirBar       = nullptr;
    QTabWidget*    m_tabs         = nullptr;
    AleaVueTab*    m_aleaVueTab   = nullptr;
    SattumaPicTab* m_sattumaPicTab= nullptr;
};
