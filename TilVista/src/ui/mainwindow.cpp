#include "mainwindow.h"
#include "aleavuetab.h"
#include "dirdatabasepanel.h"
#include "dirbar.h"
#include "shujukopanel.h"
#include "sattumapictab.h"

#include <QKeyEvent>
#include <QTabWidget>
#include <QVBoxLayout>
#include <QWidget>

// TV_APPVERSION defined via CMake target_compile_definitions
#ifndef TV_APPVERSION
#  define TV_APPVERSION "v0_0530"
#endif

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle(QString("TilVista  –  %1").arg(TV_APPVERSION));
    setMinimumSize(860, 600);

    auto* central = new QWidget;
    setCentralWidget(central);
    auto* mv = new QVBoxLayout(central);
    mv->setContentsMargins(8,8,8,8); mv->setSpacing(4);

    // ── Global directory bar ──────────────────────────────────────────────────
    m_dirBar = new DirBar;
    connect(m_dirBar, &DirBar::directoryChanged, this, &MainWindow::onDirChanged);
    mv->addWidget(m_dirBar);

    // ── ShujukoPanel – shared between AleaVue (slideshow) and SattumaPic ─────
    // Created here so it persists; placed visually inside SattumaPicTab.
    // Deactivated (disabled) until a kaivo entry is explicitly loaded.
    m_shujuko = new ShujukoPanel(this);

    // ── Tabs ──────────────────────────────────────────────────────────────────
    m_tabs = new QTabWidget;

    m_aleaVueTab    = new AleaVueTab(
        [this]{ return m_dirBar->directory(); }, m_shujuko);
    m_sattumaPicTab = new SattumaPicTab(
        [this]{ return m_dirBar->directory(); }, m_shujuko);

    // DirBar Load → AleaVue scan
    connect(m_dirBar, &DirBar::loadRequested,
            m_aleaVueTab, &AleaVueTab::loadFromDirectory);

    // kaivo DB1 panel signals
    DirDatabasePanel* db1 = m_aleaVueTab->dbPanel();
    connect(m_aleaVueTab, &AleaVueTab::requestDirChange,
            this, &MainWindow::onDirFromDb);
    connect(db1, &DirDatabasePanel::dirLoaded,
            this, &MainWindow::onDb1FilesLoaded);

    // ── ShujukoPanel activation ───────────────────────────────────────────────
    // When kaivo entry is loaded → activate shujuko with that entry
    connect(db1, &DirDatabasePanel::activeEntryChanged,
            this, &MainWindow::onActiveEntryChanged);

    // When "Update Entry" fires → ask shujuko to validate its file list
    connect(db1, &DirDatabasePanel::requestShujukoValidation,
            m_shujuko, &ShujukoPanel::validateFiles);

    // v0.5.30: propagate secret mode to DB1 panel
    // (mainwindow handles the keysequence; calls db1->setSecretMode())

    m_tabs->addTab(m_aleaVueTab,    "🖼  AleaVue");
    m_tabs->addTab(m_sattumaPicTab, "🎲  SattumaPic");
    mv->addWidget(m_tabs);
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

void MainWindow::onDb1FilesLoaded(const QString&     path,
                                   const QStringList& /*imageFiles*/,
                                   const QStringList& allFiles)
{
    m_sattumaPicTab->onDirectoryChanged(path, allFiles);
}

void MainWindow::onActiveEntryChanged(const QString& entryName,
                                       const QString& sourceDir)
{
    // Activate or deactivate the ShujukoPanel
    m_shujuko->setActiveKaivoEntry(entryName, sourceDir);
}

// ── v0.5.30 Secret Key State Machine ─────────────────────────────────────────
// Sequence: Alt(press) → S → AltGr → L
// AltGr on Windows/Linux = Key_AltGr; also arrives as Ctrl+Alt on some layouts.
// We accept either Key_AltGr or (Ctrl+Alt modifier set without a real key).

void MainWindow::keyPressEvent(QKeyEvent* event)
{
    if (advanceSecretKey(event->key(), event->modifiers())) {
        event->accept();
        return;
    }
    QMainWindow::keyPressEvent(event);
}

void MainWindow::keyReleaseEvent(QKeyEvent* event)
{
    // Reset on Escape so a half-typed sequence doesn't lock things
    if (event->key() == Qt::Key_Escape)
        m_skState = SKState::Idle;
    QMainWindow::keyReleaseEvent(event);
}

bool MainWindow::advanceSecretKey(int key, Qt::KeyboardModifiers mods)
{
    switch (m_skState) {
    case SKState::Idle:
        if (key == Qt::Key_Alt || mods.testFlag(Qt::AltModifier)) {
            m_skState = SKState::GotAlt; return true;
        }
        break;
    case SKState::GotAlt:
        if (key == Qt::Key_S) { m_skState = SKState::GotS; return true; }
        m_skState = SKState::Idle; break;
    case SKState::GotS:
        if (key == Qt::Key_AltGr ||
            (mods.testFlag(Qt::ControlModifier) && mods.testFlag(Qt::AltModifier)))
        { m_skState = SKState::GotAltGr; return true; }
        m_skState = SKState::Idle; break;
    case SKState::GotAltGr:
        if (key == Qt::Key_L) {
            m_skState    = SKState::Idle;
            m_secretMode = !m_secretMode;
            m_aleaVueTab->dbPanel()->setSecretMode(m_secretMode);
            return true;
        }
        m_skState = SKState::Idle; break;
    }
    return false;
}
