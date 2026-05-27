#include "sattumapictab.h"
#include "reviewdbpanel.h"
#include "videopreviewwidget.h"
#include "core/config.h"
#include "core/pathutils.h"
#include "workers/scanworker.h"

#include <QCheckBox>
#include <QDir>
#include <QFileIconProvider>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QImageReader>
#include <QLabel>
#include <QMessageBox>
#include <QPixmap>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>

#include <random>

static void pbStart(QProgressBar* pb)
{ pb->setRange(0,100); pb->setValue(0); pb->setVisible(true); }
static void pbDone(QProgressBar* pb)
{ pb->setRange(0,100); pb->setValue(100); pb->setVisible(false); }

// ── Constructor ───────────────────────────────────────────────────────────────

SattumaPicTab::SattumaPicTab(std::function<QString()> getGlobalDir,
                              ReviewDBPanel*           reviewDb,
                              QWidget*                 parent)
    : QWidget(parent)
    , m_getGlobalDir(std::move(getGlobalDir))
    , m_reviewDb(reviewDb)
{
    buildUi();
}

void SattumaPicTab::buildUi()
{
    auto* outer = new QHBoxLayout(this);

    // ── Left: controls + DB2 ─────────────────────────────────────────────────
    auto* left = new QWidget;
    left->setMaximumWidth(380);
    auto* lv = new QVBoxLayout(left);
    lv->setSpacing(6);

    auto* row1 = new QHBoxLayout;
    m_btnRandom   = new QPushButton("🎲  Random File");
    m_chkAutoPick = new QCheckBox("Auto-pick on dir select");
    // v0.5.10: default from config (true by default)
    m_chkAutoPick->setChecked(Config::getBool("auto_pick_on_dir_select"));
    connect(m_btnRandom, &QPushButton::clicked,
            this, &SattumaPicTab::onPickRandom);
    row1->addWidget(m_btnRandom);
    row1->addWidget(m_chkAutoPick);
    lv->addLayout(row1);

    m_lblFile = new QLabel("No file selected.");
    m_lblFile->setWordWrap(true);
    m_lblFile->setStyleSheet("font-size: 10px; color: gray;");
    lv->addWidget(m_lblFile);

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7); m_pb->setTextVisible(false); m_pb->setVisible(false);
    lv->addWidget(m_pb);

    auto* row2 = new QHBoxLayout;
    m_btnOpenFile = new QPushButton("📂  Open File");
    m_chkAutoOpen = new QCheckBox("Auto-open on random");
    m_chkAutoOpen->setChecked(Config::getBool("auto_open_on_random"));
    m_btnOpenFile->setEnabled(false);
    connect(m_btnOpenFile, &QPushButton::clicked,
            this, &SattumaPicTab::onOpenFile);
    row2->addWidget(m_btnOpenFile);
    row2->addWidget(m_chkAutoOpen);
    lv->addLayout(row2);

    // v0.5.10: renamed + now highlights file in explorer
    m_btnOpenDir = new QPushButton("📁  Select in File's Directory");
    m_btnOpenDir->setEnabled(false);
    connect(m_btnOpenDir, &QPushButton::clicked,
            this, &SattumaPicTab::onSelectInDir);
    lv->addWidget(m_btnOpenDir);

    // ── DB2 (ReviewDB) below file controls ────────────────────────────────────
    lv->addWidget(m_reviewDb);
    connect(m_reviewDb, &ReviewDBPanel::fileSelected,
            this, &SattumaPicTab::onPreviewFromDb2);

    lv->addStretch();
    outer->addWidget(left);

    // ── Right: preview ────────────────────────────────────────────────────────
    auto* right = new QWidget;
    auto* rv    = new QVBoxLayout(right);
    rv->setContentsMargins(4, 4, 4, 4);
    rv->setSpacing(4);

    auto* hdr = new QLabel("Preview");
    hdr->setAlignment(Qt::AlignCenter);
    hdr->setStyleSheet("font-weight: bold;");
    rv->addWidget(hdr);

    m_preview = new VideoPreviewWidget;
    rv->addWidget(m_preview, 1);

    m_lblType = new QLabel;
    m_lblType->setAlignment(Qt::AlignCenter);
    m_lblType->setStyleSheet("font-size: 10px; color: gray;");
    connect(m_preview, &VideoPreviewWidget::statusText,
            m_lblType, &QLabel::setText);
    rv->addWidget(m_lblType);

    outer->addWidget(right, 1);
}

// ── Public ────────────────────────────────────────────────────────────────────

void SattumaPicTab::onDirectoryChanged(const QString& path,
                                        const QStringList& allFiles)
{
    m_allFiles = allFiles;
    if (m_chkAutoPick->isChecked()) {
        if (!m_allFiles.isEmpty())
            onPickRandom();
        else
            scanAndPick(path);
    }
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void SattumaPicTab::onPickRandom()
{
    const QString dir = m_getGlobalDir();
    if (dir.isEmpty()) {
        QMessageBox::warning(this, "No Directory",
                             "Please select a directory first.");
        return;
    }
    if (m_allFiles.isEmpty()) { scanAndPick(dir); return; }

    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, m_allFiles.size() - 1);
    const QString picked = m_allFiles.at(dist(rng));
    m_randomFile = picked;
    displayFile(picked);

    if (m_chkAutoOpen->isChecked())
        onOpenFile();
}

void SattumaPicTab::onScanDone(bool ok,
                                QStringList /*imageFiles*/,
                                QStringList allFiles)
{
    pbDone(m_pb);
    m_scanThread = nullptr;
    if (ok) {
        m_allFiles = allFiles;
        onPickRandom();
    }
}

void SattumaPicTab::onOpenFile()
{
    if (!m_randomFile.isEmpty())
        TV::openPath(m_randomFile);
}

void SattumaPicTab::onSelectInDir()
{
    // v0.5.10: opens folder AND highlights the file
    if (!m_randomFile.isEmpty())
        TV::selectInExplorer(m_randomFile);
}

void SattumaPicTab::onPreviewFromDb2(const QString& path)
{
    m_randomFile = path;
    displayFile(path);
}

// ── Private ───────────────────────────────────────────────────────────────────

void SattumaPicTab::scanAndPick(const QString& path)
{
    pbStart(m_pb);
    auto* worker = new ScanWorker(path);
    auto* thread = new QThread;
    m_scanThread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &ScanWorker::run);
    connect(worker, &ScanWorker::progressChanged, m_pb, &QProgressBar::setValue);
    connect(worker, &ScanWorker::resultReady,
            this,   &SattumaPicTab::onScanDone);
    connect(worker, &ScanWorker::resultReady, thread, &QThread::quit);
    connect(worker, &ScanWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,       thread, &QObject::deleteLater);
    thread->start();
}

void SattumaPicTab::displayFile(const QString& path)
{
    m_lblFile->setText(path);
    m_btnOpenFile->setEnabled(true);
    m_btnOpenDir->setEnabled(true);
    updatePreview(path);
}

void SattumaPicTab::updatePreview(const QString& path)
{
    const QString ext = '.' + QFileInfo(path).suffix().toLower();

    if (TV::imageSuffixes().contains(ext)) {
        QImageReader reader(path);
        const QSize orig = reader.size();
        if (orig.isValid() && (orig.width() > 1024 || orig.height() > 1024)) {
            const double scale = qMin(1024.0 / orig.width(),
                                      1024.0 / orig.height());
            reader.setScaledSize(QSize(int(orig.width()  * scale),
                                       int(orig.height() * scale)));
        }
        const QImage img = reader.read();
        if (!img.isNull()) {
            m_preview->showPixmap(QPixmap::fromImage(img),
                                  QString("Image  %1").arg(ext));
            return;
        }
    }

    if (TV::videoSuffixes().contains(ext)) {
        m_preview->play(path);
        return;
    }

    QFileIconProvider fip;
    const QIcon icon = fip.icon(QFileInfo(path));
    if (!icon.isNull())
        m_preview->showPixmap(icon.pixmap(QSize(128, 128)),
                              QString("File  %1")
                                  .arg(ext.isEmpty() ? "(no ext)" : ext));
    else
        m_preview->clearPreview();
}
