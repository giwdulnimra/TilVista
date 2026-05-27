#include "mainwindow.h"
#include "aleavuetab.h"
#include "dirdatabasepanel.h"
#include "dirbar.h"
#include "reviewdbpanel.h"
#include "sattumapictab.h"

#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("TilVista  –  v0.5.10");
    setMinimumSize(840, 580);

    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mv = new QVBoxLayout(central);
    mv->setContentsMargins(8, 8, 8, 8);
    mv->setSpacing(4);

    // ── Global directory bar (v0.5.00: Browse | Load from Directory) ─────────
    m_dirBar = new DirBar;
    connect(m_dirBar, &DirBar::directoryChanged,
            this, &MainWindow::onDirChanged);
    mv->addWidget(m_dirBar);

    // ── Shared ReviewDB (DB2) – lives here, displayed inside SattumaPicTab ───
    m_reviewDb = new ReviewDBPanel(this);

    // ── Tabs ──────────────────────────────────────────────────────────────────
    m_tabs = new QTabWidget;

    m_aleaVueTab = new AleaVueTab(
        [this]{ return m_dirBar->directory(); },
        m_reviewDb);

    m_sattumaPicTab = new SattumaPicTab(
        [this]{ return m_dirBar->directory(); },
        m_reviewDb);

    // DirBar Load button → AleaVue scan
    connect(m_dirBar, &DirBar::loadRequested,
            m_aleaVueTab, &AleaVueTab::loadFromDirectory);

    // AleaVue DB1 panel → sync DirBar + pass cached files to SattumaPic
    connect(m_aleaVueTab, &AleaVueTab::requestDirChange,
            this, &MainWindow::onDirFromDb);
    connect(m_aleaVueTab->dbPanel(), &DirDatabasePanel::dirLoaded,
            this, &MainWindow::onDb1FilesLoaded);

    // v0.5.00: tab labels without parenthetical descriptions
    m_tabs->addTab(m_aleaVueTab,    "🖼  AleaVue");
    m_tabs->addTab(m_sattumaPicTab, "🎲  SattumaPic");
    mv->addWidget(m_tabs);
}

void MainWindow::onDirChanged(const QString& path)
{
    m_aleaVueTab->onDirectoryChanged(path);
    m_sattumaPicTab->onDirectoryChanged(path);
}

void MainWindow::onDirFromDb(const QString& path)
{
    m_dirBar->setDirectory(path);
    // SattumaPic notified via onDb1FilesLoaded with cached all_files
}

void MainWindow::onDb1FilesLoaded(const QString&     path,
                                   const QStringList& /*imageFiles*/,
                                   const QStringList& allFiles)
{
    m_sattumaPicTab->onDirectoryChanged(path, allFiles);
}
