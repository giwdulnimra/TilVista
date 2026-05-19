#include "aleavuetab.h"
#include "dirdatabasepanel.h"
#include "slideshowwindow.h"
#include "core/pathutils.h"
#include "workers/scanworker.h"

#include <QCheckBox>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

// ── Helpers ──────────────────────────────────────────────────────────────────

static void pbStart(QProgressBar* pb) { pb->setRange(0,100); pb->setValue(0); pb->setVisible(true); }
static void pbDone (QProgressBar* pb) { pb->setRange(0,100); pb->setValue(100); pb->setVisible(false); }

// ── Constructor ───────────────────────────────────────────────────────────────

AleaVueTab::AleaVueTab(std::function<QString()> getGlobalDir, QWidget* parent)
    : QWidget(parent), m_getGlobalDir(std::move(getGlobalDir))
{
    buildUi();
}

void AleaVueTab::buildUi()
{
    auto* outer = new QHBoxLayout(this);

    // ── Left controls ────────────────────────────────────────────────────────
    auto* left = new QWidget;
    auto* lv   = new QVBoxLayout(left);
    lv->setSpacing(6);

    m_lblStatus = new QLabel("No directory selected.");
    m_lblStatus->setWordWrap(true);
    lv->addWidget(m_lblStatus);

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7);
    m_pb->setTextVisible(false);
    m_pb->setVisible(false);
    lv->addWidget(m_pb);

    m_btnLoadDir = new QPushButton("📂  Load from Directory");
    m_btnLoadDir->setToolTip("Scan selected directory for images");
    connect(m_btnLoadDir, &QPushButton::clicked,
            this, &AleaVueTab::onLoadFromDirectory);
    lv->addWidget(m_btnLoadDir);

    m_chkFullscreen = new QCheckBox("Fullscreen");
    m_chkFullscreen->setChecked(true);
    lv->addWidget(m_chkFullscreen);

    m_btnStart = new QPushButton("▶  Start AleaVue");
    m_btnStart->setFixedHeight(48);
    connect(m_btnStart, &QPushButton::clicked,
            this, &AleaVueTab::onStartSlideshow);
    lv->addWidget(m_btnStart);

    lv->addStretch();
    outer->addWidget(left, 1);

    // ── Right: DB panel ──────────────────────────────────────────────────────
    m_dbPanel = new DirDatabasePanel(m_getGlobalDir);
    connect(m_dbPanel, &DirDatabasePanel::dirLoaded,
            this, &AleaVueTab::onDbDirLoaded);
    outer->addWidget(m_dbPanel, 1);
}

// ── Public ────────────────────────────────────────────────────────────────────

void AleaVueTab::onDirectoryChanged(const QString& path)
{
    m_imagePaths.clear();
    m_allFiles.clear();
    m_lblStatus->setText(QString("Dir set: %1").arg(path));
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void AleaVueTab::onLoadFromDirectory()
{
    const QString path = m_getGlobalDir();
    if (path.isEmpty() || !QDir(path).exists()) {
        QMessageBox::warning(this, "No Directory",
                             "Please select a directory first.");
        return;
    }
    scanDir(path);
}

void AleaVueTab::onDbDirLoaded(const QString& path,
                                const QStringList& imageFiles,
                                const QStringList& allFiles)
{
    emit requestDirChange(path);

    if (!imageFiles.isEmpty()) {
        m_imagePaths = imageFiles;
        m_allFiles   = allFiles;
        m_lastScannedPath = path;
        m_lblStatus->setText(
            QString("✓  %1 images (from cache)  –  %2")
                .arg(imageFiles.size()).arg(path));
    } else {
        m_lblStatus->setText(
            QString("Loaded from DB: %1  –  scanning…").arg(path));
        scanDir(path);
    }
}

void AleaVueTab::onScanDone(bool ok,
                              QStringList imageFiles,
                              QStringList allFiles)
{
    pbDone(m_pb);
    m_btnStart->setEnabled(true);
    m_btnLoadDir->setEnabled(true);
    m_scanThread = nullptr;

    if (ok) {
        m_imagePaths = imageFiles;
        m_allFiles   = allFiles;
        m_lblStatus->setText(
            QString("✓  %1 images found.").arg(imageFiles.size()));
        m_dbPanel->updateCache(m_lastScannedPath, imageFiles, allFiles);
    } else {
        m_lblStatus->setText("✗  Scan failed.");
    }
}

void AleaVueTab::onScanThenOpen(bool ok,
                                  QStringList imageFiles,
                                  QStringList allFiles)
{
    pbDone(m_pb);
    m_btnStart->setEnabled(true);
    m_btnLoadDir->setEnabled(true);
    m_scanThread = nullptr;

    if (!ok || imageFiles.isEmpty()) {
        QMessageBox::warning(this, "No Images",
                             "No image files found in the selected directory.");
        return;
    }
    m_imagePaths = imageFiles;
    m_allFiles   = allFiles;
    m_dbPanel->updateCache(m_getGlobalDir(), imageFiles, allFiles);
    openWindow(m_getGlobalDir(), imageFiles);
}

void AleaVueTab::onStartSlideshow()
{
    const QString dir = m_getGlobalDir();
    if (dir.isEmpty()) {
        QMessageBox::warning(this, "No Directory",
                             "Please select a directory first.");
        return;
    }
    if (!m_imagePaths.isEmpty()) {
        openWindow(dir, m_imagePaths);
        return;
    }
    // Lazy scan then open
    pbStart(m_pb);
    m_btnStart->setEnabled(false);
    m_btnLoadDir->setEnabled(false);

    auto* worker = new ScanWorker(dir);
    auto* thread = new QThread;
    m_scanThread = thread;

    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &ScanWorker::run);
    connect(worker, &ScanWorker::progressChanged, m_pb, &QProgressBar::setValue);
    connect(worker, &ScanWorker::resultReady,
            this,   &AleaVueTab::onScanThenOpen);
    connect(worker, &ScanWorker::resultReady, thread, &QThread::quit);
    connect(worker, &ScanWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,       thread, &QObject::deleteLater);
    thread->start();
}

// ── Private ───────────────────────────────────────────────────────────────────

void AleaVueTab::scanDir(const QString& path)
{
    pbStart(m_pb);
    m_pb->setValue(0);
    m_btnStart->setEnabled(false);
    m_btnLoadDir->setEnabled(false);
    m_lblStatus->setText(QString("Scanning: %1 …").arg(path));
    m_lastScannedPath = path;

    auto* worker = new ScanWorker(path);
    auto* thread = new QThread;
    m_scanThread = thread;

    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &ScanWorker::run);
    connect(worker, &ScanWorker::progressChanged, m_pb, &QProgressBar::setValue);
    connect(worker, &ScanWorker::resultReady,
            this,   &AleaVueTab::onScanDone);
    connect(worker, &ScanWorker::resultReady, thread, &QThread::quit);
    connect(worker, &ScanWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,       thread, &QObject::deleteLater);
    thread->start();
}

void AleaVueTab::openWindow(const QString& directory,
                              const QStringList& imagePaths)
{
    m_slideshowWindow = new SlideshowWindow(
        directory, imagePaths, TV::logDir(), /*log=*/false);

    if (m_chkFullscreen->isChecked())
        m_slideshowWindow->showFullScreen();
    else
        m_slideshowWindow->show();
}
