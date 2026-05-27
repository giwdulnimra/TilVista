#include "mainwindow.h"
#include "aleavuetab.h"
#include "dirdatabasepanel.h"
#include "dirbar.h"
#include "sattumapictab.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("TilVista  –  v0.4.0");
    setMinimumSize(800, 560);

    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mv = new QVBoxLayout(central);
    mv->setContentsMargins(8, 8, 8, 8);
    mv->setSpacing(4);

    // ── Global directory bar ─────────────────────────────────────────────────
    m_dirBar = new DirBar;
    connect(m_dirBar, &DirBar::directoryChanged,
            this, &MainWindow::onDirChanged);
    mv->addWidget(m_dirBar);

    // ── Tabs ─────────────────────────────────────────────────────────────────
    m_tabs = new QTabWidget;

    m_aleaVueTab    = new AleaVueTab(   [this]{ return m_dirBar->directory(); });
    m_sattumaPicTab = new SattumaPicTab([this]{ return m_dirBar->directory(); });

    // When the DB panel loads a dir: sync DirBar + forward cached files
    connect(m_aleaVueTab, &AleaVueTab::requestDirChange,
            this, &MainWindow::onDirFromDb);
    connect(m_aleaVueTab->dbPanel(), &DirDatabasePanel::dirLoaded,
            this, &MainWindow::onDbFilesLoaded);

    m_tabs->addTab(m_aleaVueTab,    "🖼   AleaVue  (Slideshow)");
    m_tabs->addTab(m_sattumaPicTab, "🎲   SattumaPic  (Random File)");
    mv->addWidget(m_tabs);
}

void MainWindow::onDirChanged(const QString& path)
{
    m_aleaVueTab->onDirectoryChanged(path);
    m_sattumaPicTab->onDirectoryChanged(path);
}

void MainWindow::onDirFromDb(const QString& path)
{
    // AleaVueTab notifies itself; we only need to update the bar
    m_dirBar->setDirectory(path);
}

void MainWindow::onDbFilesLoaded(const QString&     path,
                                  const QStringList& /*imageFiles*/,
                                  const QStringList& allFiles)
{
    // Pass cached allFiles to SattumaPic so it doesn't need to rescan
    m_sattumaPicTab->onDirectoryChanged(path, allFiles);
}
