#include "mainwindow.h"
#include "aleavuetab.h"
#include "dirdatabasepanel.h"
#include "dirbar.h"
#include "madolodostab.h"
#include "shortcutstab.h"
#include "shujukopanel.h"
#include "sattumapictab.h"

#include <QLabel>
#include <QKeySequence>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#ifndef TV_APPVERSION_DISPLAY
#  define TV_APPVERSION_DISPLAY "v0.05.32"
#endif
#ifndef TV_SEMVER
#  define TV_SEMVER "0.5.32"
#endif

static const char* kSecretKeySeq = "Ctrl+Alt+F8";

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(
        QString("TilVista  \u2013  %1").arg(TV_APPVERSION_DISPLAY));
    setMinimumSize(880, 620);

    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mv = new QVBoxLayout(central);
    mv->setContentsMargins(8,8,8,8); mv->setSpacing(4);

    // ── Global dir bar ────────────────────────────────────────────────────────
    m_dirBar = new DirBar;
    connect(m_dirBar, &DirBar::directoryChanged,
            this, &MainWindow::onDirChanged);
    mv->addWidget(m_dirBar);

    // ── Shared ShujukoPanel ───────────────────────────────────────────────────
    // Owned by MainWindow, displayed inside SattumaPicTab's left column.
    // Deactivated until a kaivo entry is loaded.
    m_shujuko = new ShujukoPanel(this);

    // ── Tabs ──────────────────────────────────────────────────────────────────
    m_tabs = new QTabWidget;

    m_aleaVueTab = new AleaVueTab(
        [this]{ return m_dirBar->directory(); }, m_shujuko);
    m_sattumaPicTab = new SattumaPicTab(
        [this]{ return m_dirBar->directory(); }, m_shujuko);
    m_shortcutsTab  = new ShortcutsTab;
    m_madolodosTab  = new MadolodosTab;

    // DirBar Load button → AleaVue scan
    connect(m_dirBar, &DirBar::loadRequested,
            m_aleaVueTab, &AleaVueTab::loadFromDirectory);

    // kaivo DB1 panel
    DirDatabasePanel* db1 = m_aleaVueTab->dbPanel();
    connect(m_aleaVueTab, &AleaVueTab::requestDirChange,
            this, &MainWindow::onDirFromDb);
    connect(db1, &DirDatabasePanel::dirLoaded,
            this, &MainWindow::onDb1FilesLoaded);
    connect(db1, &DirDatabasePanel::activeEntryChanged,
            this, &MainWindow::onActiveEntryChanged);
    connect(db1, &DirDatabasePanel::requestShujukoValidation,
            m_shujuko, &ShujukoPanel::validateFiles);

    // Tab labels
    m_tabs->addTab(m_aleaVueTab,    "\U0001f5bc  AleaVue");
    m_tabs->addTab(m_sattumaPicTab, "\U0001f3b2  SattumaPic");
    m_tabs->addTab(m_shortcutsTab,  "\u2328  Shortcuts");
    m_tabs->addTab(m_madolodosTab,  "\U0001f39e  Madoludus");
    mv->addWidget(m_tabs);

    // ── Status bar lock indicator ─────────────────────────────────────────────
    m_secretIndicator = new QLabel("\U0001f512");
    m_secretIndicator->setToolTip(
        QString("Secret Mode OFF\nShortcut: %1").arg(kSecretKeySeq));
    m_secretIndicator->setStyleSheet("font-size:16px; padding:2px 6px;");
    statusBar()->addPermanentWidget(m_secretIndicator);
    statusBar()->setSizeGripEnabled(false);

    // ── Secret mode shortcut (ApplicationShortcut = fires even with focus
    //    inside LineEdit or other child widgets) ────────────────────────────
    m_secretShortcut = new QShortcut(
        QKeySequence(QLatin1String(kSecretKeySeq)), this);
    m_secretShortcut->setContext(Qt::ApplicationShortcut);
    connect(m_secretShortcut, &QShortcut::activated,
            this, &MainWindow::toggleSecretMode);
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void MainWindow::onDirChanged(const QString& path)
{
    m_aleaVueTab->onDirectoryChanged(path);
    m_sattumaPicTab->onDirectoryChanged(path);
}

void MainWindow::onDirFromDb(const QString& path)
{
    m_dirBar->setDirectory(path);
}

void MainWindow::onDb1FilesLoaded(const QString& path,
                                   const QStringList&,
                                   const QStringList& allFiles)
{
    m_sattumaPicTab->onDirectoryChanged(path, allFiles);
}

void MainWindow::onActiveEntryChanged(const QString& entryName,
                                       const QString& sourceDir)
{
    m_shujuko->setActiveKaivoEntry(entryName, sourceDir);
}

void MainWindow::toggleSecretMode()
{
    m_secretMode = !m_secretMode;
    m_aleaVueTab->dbPanel()->setSecretMode(m_secretMode);
    m_shujuko->setSecretMode(m_secretMode);
    updateSecretIndicator();
}

void MainWindow::updateSecretIndicator()
{
    if (m_secretMode) {
        m_secretIndicator->setText("\U0001f513");
        m_secretIndicator->setToolTip(
            QString("Secret Mode ON\nShortcut: %1\n"
                    "Hidden entries visible. Extra controls enabled.")
                .arg(kSecretKeySeq));
        m_secretIndicator->setStyleSheet(
            "font-size:16px; padding:2px 6px; color:#e8a000;");
    } else {
        m_secretIndicator->setText("\U0001f512");
        m_secretIndicator->setToolTip(
            QString("Secret Mode OFF\nShortcut: %1").arg(kSecretKeySeq));
        m_secretIndicator->setStyleSheet("font-size:16px; padding:2px 6px;");
    }
    statusBar()->showMessage(
        m_secretMode ? "Secret mode activated"
                     : "Secret mode deactivated", 2500);
}
