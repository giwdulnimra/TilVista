#include "mainwindow.h"
#include "aleavuetab.h"
#include "dirdatabasepanel.h"
#include "dirbar.h"
#include "madolodostab.h"
#include "shortcutstab.h"
#include "shujukopanel.h"
#include "sattumapictab.h"

#include <QIcon>
#include <QLabel>
#include <QKeySequence>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

#ifndef TV_APPVERSION_DISPLAY
#  define TV_APPVERSION_DISPLAY "v0.05.33"
#endif
#ifndef TV_SEMVER
#  define TV_SEMVER "0.5.33"
#endif

static const char* kSecretKeySeq = "Ctrl+Alt+F8";

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(
        QString("TilVista  \u2013  %1").arg(TV_APPVERSION_DISPLAY));
    setMinimumSize(880, 620);

    // ── App icon (tilvista.ico via Qt resource) ───────────────────────────────
    // The icon is embedded in resources.qrc as :/icons/tilvista.ico
    // Falls back gracefully if the file is missing (no crash).
    const QIcon appIcon(":/icons/tilvista.ico");
    if (!appIcon.isNull())
        setWindowIcon(appIcon);

    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mv = new QVBoxLayout(central);
    mv->setContentsMargins(8,8,8,8); mv->setSpacing(4);

    m_dirBar = new DirBar;
    connect(m_dirBar, &DirBar::directoryChanged,
            this, &MainWindow::onDirChanged);
    mv->addWidget(m_dirBar);

    m_shujuko = new ShujukoPanel(this);

    m_tabs = new QTabWidget;

    m_aleaVueTab    = new AleaVueTab(
        [this]{ return m_dirBar->directory(); }, m_shujuko);
    m_sattumaPicTab = new SattumaPicTab(
        [this]{ return m_dirBar->directory(); }, m_shujuko);
    m_madolodosTab  = new MadolodosTab;
    m_shortcutsTab  = new ShortcutsTab;   // renamed to "About"

    connect(m_dirBar, &DirBar::loadRequested,
            m_aleaVueTab, &AleaVueTab::loadFromDirectory);

    DirDatabasePanel* db1 = m_aleaVueTab->dbPanel();
    connect(m_aleaVueTab, &AleaVueTab::requestDirChange,
            this, &MainWindow::onDirFromDb);
    connect(db1, &DirDatabasePanel::dirLoaded,
            this, &MainWindow::onDb1FilesLoaded);
    connect(db1, &DirDatabasePanel::activeEntryChanged,
            this, &MainWindow::onActiveEntryChanged);
    connect(db1, &DirDatabasePanel::requestShujukoValidation,
            m_shujuko, &ShujukoPanel::validateFiles);

    // ── Tab order: AleaVue | SattumaPic | Madoludus | About ──────────────────
    m_tabs->addTab(m_aleaVueTab,    "\U0001f5bc  AleaVue");
    m_tabs->addTab(m_sattumaPicTab, "\U0001f3b2  SattumaPic");
    m_tabs->addTab(m_madolodosTab,  "\U0001f39e  Madoludus");
    m_tabs->addTab(m_shortcutsTab,  "About");   // no symbol, as requested
    mv->addWidget(m_tabs);

    // ── Status bar lock indicator ─────────────────────────────────────────────
    m_secretIndicator = new QLabel("\U0001f512");
    m_secretIndicator->setToolTip(
        QString("Secret Mode OFF\nShortcut: %1").arg(kSecretKeySeq));
    m_secretIndicator->setStyleSheet("font-size:16px; padding:2px 6px;");
    statusBar()->addPermanentWidget(m_secretIndicator);
    statusBar()->setSizeGripEnabled(false);

    m_secretShortcut = new QShortcut(
        QKeySequence(QLatin1String(kSecretKeySeq)), this);
    m_secretShortcut->setContext(Qt::ApplicationShortcut);
    connect(m_secretShortcut, &QShortcut::activated,
            this, &MainWindow::toggleSecretMode);
}

void MainWindow::onDirChanged(const QString& path)
{
    m_aleaVueTab->onDirectoryChanged(path);
    m_sattumaPicTab->onDirectoryChanged(path);
}
void MainWindow::onDirFromDb(const QString& path)
{ m_dirBar->setDirectory(path); }

void MainWindow::onDb1FilesLoaded(const QString& path,
                                   const QStringList&,
                                   const QStringList& allFiles)
{ m_sattumaPicTab->onDirectoryChanged(path, allFiles); }

void MainWindow::onActiveEntryChanged(const QString& entryName,
                                       const QString& sourceDir)
{ m_shujuko->setActiveKaivoEntry(entryName, sourceDir); }

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
        m_secretMode ? "Secret mode activated" : "Secret mode deactivated", 2500);
}
