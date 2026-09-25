#include "../madoludustab.h"
#include "../madoluduswindow.h"
#include "../videopreviewwidget.h"
#include "core/pathutils.h"
#include "workers/scanworker.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDir>
#include <QFileInfo>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QFileIconProvider>
#include <QImageReader>
#include <QGuiApplication>
#include <QScreen>
#include <QDir>

static void pbStart(QProgressBar* p)
{ p->setRange(0,100); p->setValue(0); p->setVisible(true); }
static void pbDone(QProgressBar* p)
{ p->setRange(0,100); p->setValue(100); p->setVisible(false); }

// ── FF-Rate combo items ────────────────────────────────────────────────────
// Each entry: { display label, Mode::IntervalSeconds/FixedCount, value }
struct FFRateEntry {
    const char* label;
    bool fixedCount;   // false = IntervalSeconds, true = FixedCount
    double value;
};
static const FFRateEntry kFFRates[] = {
    { "1/sec",        false, 1.0  },
    { "1/2 sec",      false, 2.0  },
    { "1/5 sec",      false, 5.0  },
    { "1/10 sec",     false, 10.0 },
    { "1/2 frames",   true,  2.0  },
    { "1/3 frames",   true,  3.0  },
    { "1/10 frames",  true,  10.0 },
    { "1/30 frames",  true,  30.0 },
    { "1/60 frames",  true,  60.0 },
};
static constexpr int kFFRateCount =
    static_cast<int>(sizeof(kFFRates) / sizeof(kFFRates[0]));

// ── Constructor ───────────────────────────────────────────────────────────────

MadoludusTab::MadoludusTab(std::function<QString()> getGlobalDir, QWidget* parent)
    : QWidget(parent)
    , m_getGlobalDir(std::move(getGlobalDir))
{
    buildUi();
}

MadoludusTab::~MadoludusTab() = default;

// ── Public ────────────────────────────────────────────────────────────────────

void MadoludusTab::onDirectoryChanged(const QString& path,
                                       const QStringList& allFiles)
{
    m_allFiles = allFiles;
    const QStringList files = filteredFiles();
    if (!files.isEmpty()) {
        m_lblStatus->setText(QString("%1 media file(s) available.").arg(files.size()));
        // NEU: erste Datei im Preview anzeigen
        showPreview(files.first());
    } else if (!path.isEmpty()) {
        scanDir(path);
    }
}

void MadoludusTab::setSecretMode(bool on)
{
    m_secretMode = on;
    // Secret mode is forwarded to any open window immediately
    if (m_window) {
        // Re-open would be disruptive; the window picks it up via its own
        // FFToggle guard. No action needed here beyond storing the flag.
    }
}

// ── Build UI ──────────────────────────────────────────────────────────────────

void MadoludusTab::buildUi()
{
    auto* outer = new QHBoxLayout(this);
    outer->setSpacing(8);

    // ── Left: config panel ────────────────────────────────────────────────────
    auto* left = new QWidget;
    left->setMaximumWidth(380);
    auto* lv = new QVBoxLayout(left);
    lv->setSpacing(6);

    auto* hdr = new QLabel("Configure MadoLudus before start");
    hdr->setStyleSheet("font-weight: bold; font-size: 11px;");
    lv->addWidget(hdr);

    m_chkImages = new QCheckBox("Images");
    m_chkImages->setChecked(true);
    m_chkVideos = new QCheckBox("Videos");
    m_chkVideos->setChecked(true);
    lv->addWidget(m_chkImages);
    lv->addWidget(m_chkVideos);

    // Video sub-options indented
    auto* videoSub = new QWidget;
    auto* vsLay = new QVBoxLayout(videoSub);
    vsLay->setContentsMargins(20, 0, 0, 0);
    vsLay->setSpacing(4);

    m_chkFF = new QCheckBox("FF-Mode");
    m_chkFF->setChecked(true);
    m_chkFF->setToolTip(
        "Impression mode: extract frames via ffmpeg and cycle them.\n"
        "Disabling requires Secret Mode.");
    vsLay->addWidget(m_chkFF);

    auto* rateRow = new QHBoxLayout;
    m_cmbFFRate = new QComboBox;
    for (int i = 0; i < kFFRateCount; ++i)
        m_cmbFFRate->addItem(kFFRates[i].label);
    m_cmbFFRate->setCurrentIndex(0);   // "1/sec" default
    m_cmbFFRate->setToolTip(
        "Frame extraction rate for FF/Impression mode.\n"
        "'1/sec' = one frame per second of video.\n"
        "'1/N frames' = N frames evenly across the whole clip.");
    rateRow->addSpacing(20);
    rateRow->addWidget(m_cmbFFRate);
    rateRow->addStretch();
    vsLay->addLayout(rateRow);

    m_chkMuted = new QCheckBox("Muted");
    m_chkMuted->setChecked(false);
    vsLay->addWidget(m_chkMuted);
    lv->addWidget(videoSub);

    // Enable/disable video sub-options when Videos checkbox changes
    auto syncVideoSub = [this, videoSub]() {
        videoSub->setEnabled(m_chkVideos->isChecked());
    };
    connect(m_chkVideos, &QCheckBox::toggled, this, syncVideoSub);
    syncVideoSub();

    m_chkRandom = new QCheckBox("random sorted");
    m_chkRandom->setChecked(false);
    lv->addWidget(m_chkRandom);

    lv->addStretch();

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7); m_pb->setTextVisible(false); m_pb->setVisible(false);
    lv->addWidget(m_pb);

    m_lblStatus = new QLabel("Select a directory first.");
    m_lblStatus->setWordWrap(true);
    m_lblStatus->setStyleSheet("font-size: 10px; color: gray;");
    lv->addWidget(m_lblStatus);

    m_btnPreview = new QPushButton("🔍  Preview file");
    m_btnPreview->setFixedHeight(44);
    m_btnPreview->setStyleSheet("font-size: 13px; font-weight: bold;");
    connect(m_btnPreview, &QPushButton::clicked, this, [this]() {
        const QStringList files = filteredFiles();
        if (!files.isEmpty()) showPreview(files.first());});
    lv->addWidget(m_btnPreview);

    m_btnStart = new QPushButton("\u25b6  Start MadoLudus");
    m_btnStart->setFixedHeight(44);
    m_btnStart->setStyleSheet("font-size: 13px; font-weight: bold;");
    connect(m_btnStart, &QPushButton::clicked, this, &MadoludusTab::onStartClicked);
    lv->addWidget(m_btnStart);



    outer->addWidget(left);

    // ── Right: preview area ───────────────────────────────────────────────────
    auto* right = new QWidget;
    auto* rv = new QVBoxLayout(right);
    rv->setContentsMargins(4, 4, 4, 4);
    rv->setSpacing(4);

    auto* pvHdr = new QLabel("Preview");
    pvHdr->setAlignment(Qt::AlignCenter);
    pvHdr->setStyleSheet("font-weight: bold;");
    rv->addWidget(pvHdr);

    m_preview = new VideoPreviewWidget;
    rv->addWidget(m_preview, 1);

    m_lblType = new QLabel;
    m_lblType->setAlignment(Qt::AlignCenter);
    m_lblType->setStyleSheet("font-size: 10px; color: gray;");
    connect(m_preview, &VideoPreviewWidget::statusText, m_lblType, &QLabel::setText);
    rv->addWidget(m_lblType);

    outer->addWidget(right, 1);
}

// ── Preview ───────────────────────────────────────────────────────────────────

void MadoludusTab::showPreview(const QString& path)
{
    const QString ext = '.' + QFileInfo(path).suffix().toLower();
    if (TV::imageSuffixes().contains(ext)) {
        QImageReader reader(path);
        const QSize orig = reader.size();
        if (orig.isValid() && (orig.width() > 1024 || orig.height() > 1024)) {
            const double s = qMin(1024.0/orig.width(), 1024.0/orig.height());
            reader.setScaledSize(QSize(int(orig.width()*s), int(orig.height()*s)));
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
    // Fallback: Datei-Icon
    QFileIconProvider fip;
    const QIcon icon = fip.icon(QFileInfo(path));
    if (!icon.isNull())
        m_preview->showPixmap(icon.pixmap(QSize(128,128)),
                              QString("File  %1").arg(ext));
    else
        m_preview->clearPreview();
}

// ── Start ─────────────────────────────────────────────────────────────────────

void MadoludusTab::onStartClicked()
{
    // If a window is already running, just bring it to the front
    if (m_window) {
        m_window->raise();
        m_window->activateWindow();
        return;
    }

    const QStringList files = filteredFiles();
    if (files.isEmpty()) {
        const QString dir = m_getGlobalDir();
        if (dir.isEmpty()) {
            QMessageBox::warning(this, "No Directory",
                "Please select a directory first.");
            return;
        }
        // Trigger scan then open
        scanDir(dir);
        return;
    }
    openWindow(files);
}

void MadoludusTab::openWindow(const QStringList& files)
{
    MadoludusConfig cfg = buildConfig();
    cfg.secretMode = m_secretMode;
    m_window = new MadoludusWindow(files, cfg);

    // Start at half screen size, top-left corner
    const QRect scr = QGuiApplication::primaryScreen()->availableGeometry();
    m_window->setGeometry(0, 0, scr.width() / 2, scr.height() / 2);

    connect(m_window, &MadoludusWindow::windowClosed,
            this, &MadoludusTab::onWindowClosed);

    connect(m_window, &MadoludusWindow::currentFileChanged,
            this, [this](const QString& path) { showPreview(path); });

    m_window->show();
    m_lblStatus->setText(QString("MadoLudus running – %1 file(s).").arg(files.size()));
}

void MadoludusTab::onWindowClosed()
{
    m_window = nullptr;
    m_lblStatus->setText("MadoLudus closed.");
}

// ── Scan ─────────────────────────────────────────────────────────────────────

void MadoludusTab::scanDir(const QString& path)
{
    if (m_scanThread) { m_scanThread->disconnect(this); m_scanThread = nullptr; }
    pbStart(m_pb);
    m_btnStart->setEnabled(false);
    m_lblStatus->setText(QString("Scanning: %1 …").arg(path));

    auto* worker = new ScanWorker(path);
    auto* thread = new QThread; m_scanThread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &ScanWorker::run);
    connect(worker, &ScanWorker::progressChanged, m_pb, &QProgressBar::setValue);
    connect(worker, &ScanWorker::resultReady,
            this, &MadoludusTab::onScanDone);
    connect(worker, &ScanWorker::resultReady, thread, &QThread::quit);
    connect(worker, &ScanWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
    const QStringList files = filteredFiles();
    if (!files.isEmpty()) {
        m_lblStatus->setText(QString("%1 media file(s) ready.").arg(files.size()));
        showPreview(files.first());   // NEU
        // openWindow(files);  ← nur wenn du Auto-Start willst
    }
}

void MadoludusTab::onScanDone(bool ok, QStringList /*img*/, QStringList allFiles)
{
    pbDone(m_pb);
    m_btnStart->setEnabled(true);
    m_scanThread = nullptr;
    if (!ok) { m_lblStatus->setText("Scan failed."); return; }
    m_allFiles = allFiles;
    const QStringList files = filteredFiles();
    if (files.isEmpty()) {
        m_lblStatus->setText("No matching media files found.");
        return;
    }
    m_lblStatus->setText(QString("%1 media file(s) ready.").arg(files.size()));
    openWindow(files);
}

// ── Helpers ───────────────────────────────────────────────────────────────────

MadoludusConfig MadoludusTab::buildConfig() const
{
    MadoludusConfig cfg;
    cfg.showImages  = m_chkImages->isChecked();
    cfg.showVideos  = m_chkVideos->isChecked();
    cfg.ffMode      = m_chkFF->isChecked();
    cfg.muted       = m_chkMuted->isChecked();
    cfg.randomMode  = m_chkRandom->isChecked();

    const int idx = m_cmbFFRate->currentIndex();
    if (idx >= 0 && idx < kFFRateCount) {
        cfg.ffFixedCount = kFFRates[idx].fixedCount;
        cfg.ffValue      = kFFRates[idx].value;
    } else {
        cfg.ffFixedCount = false;
        cfg.ffValue      = 10.0;
    }
    return cfg;
}

QStringList MadoludusTab::filteredFiles() const
{
    const bool wantImg = m_chkImages->isChecked();
    const bool wantVid = m_chkVideos->isChecked();
    const QStringList& imgSuf = TV::imageSuffixes();
    const QStringList& vidSuf = TV::videoSuffixes();
    QStringList out;
    for (const QString& fp : m_allFiles) {
        const QString ext = '.' + QFileInfo(fp).suffix().toLower();
        if (wantImg && imgSuf.contains(ext)) out << fp;
        else if (wantVid && vidSuf.contains(ext)) out << fp;
    }
    return out;
}
