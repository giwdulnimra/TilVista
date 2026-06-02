#include "shujukopanel.h"
#include "core/pathutils.h"
#include "workers/dbworkers.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QDateTime>
#include <QDir>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QScrollArea>
#include <QThread>
#include <QVBoxLayout>
#include <random>

static void pbStart(QProgressBar* p){p->setRange(0,0);p->setVisible(true);}
static void pbDone (QProgressBar* p){p->setRange(0,100);p->setValue(100);p->setVisible(false);}

ShujukoPanel::ShujukoPanel(QWidget* parent)
    : QWidget(parent)
    , m_data(QJsonObject{{"kaivo_entry",""},{"source_dir",""},{"files",QJsonArray{}}})
{
    buildUi();
    setControlsEnabled(false);
}

// ── Public ────────────────────────────────────────────────────────────────────

void ShujukoPanel::setActiveKaivoEntry(const QString& name, const QString& src)
{
    // Don't do anything if already active with same entry (avoids re-loading on tab switch)
    if (m_entryName == name && !name.isEmpty()) return;

    m_entryName = name; m_sourceDir = src;
    if (name.isEmpty()) {
        m_jsonPath.clear();
        m_data = QJsonObject{{"kaivo_entry",""},{"source_dir",""},{"files",QJsonArray{}}};
        m_fileList->clear();
        m_lblHeader->setText("shujuko  –  no entry active");
        setControlsEnabled(false);
        return;
    }
    m_jsonPath = TV::shujukoPath(name);
    m_lblHeader->setText(QString("shujuko  –  %1").arg(name));
    setControlsEnabled(true);
    loadFromDisk();
}

void ShujukoPanel::addBookmark(const QString& filePath)
{
    if (m_entryName.isEmpty() || filePath.isEmpty()) return;

    QJsonArray files = m_data.value("files").toArray();
    for (const auto& v : files)
        if (v.toObject().value("path").toString() == filePath) return;  // duplicate

    QJsonObject e;
    e["path"]     = filePath;
    e["added_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    e["missing"]  = false;   // just added – assume it exists
    files.append(e);
    m_data["files"]       = files;
    m_data["kaivo_entry"] = m_entryName;
    m_data["source_dir"]  = m_sourceDir;

    refreshFileList();                     // update UI immediately
    saveToDisk();                          // persist in background
    m_lblStatus->setText(
        QString("✓ Saved: %1").arg(QFileInfo(filePath).fileName()));
}

void ShujukoPanel::validateFiles()
{
    if (m_entryName.isEmpty()) return;
    QJsonArray files = m_data.value("files").toArray();
    bool changed = false;
    for (int i = 0; i < files.size(); ++i) {
        QJsonObject f = files[i].toObject();
        const bool exists = QFileInfo::exists(f.value("path").toString());
        if (f.value("missing").toBool() != !exists) {
            f["missing"] = !exists; files[i] = f; changed = true;
        }
    }
    if (changed) { m_data["files"] = files; refreshFileList(); saveToDisk(); }
    m_lblStatus->setText("✓  Validation done.");
}

void ShujukoPanel::setSecretMode(bool on)
{
    m_btnDelete->setVisible(on);
}

QString ShujukoPanel::currentJsonPath() const { return m_jsonPath; }

// ── Slots ─────────────────────────────────────────────────────────────────────

void ShujukoPanel::onPickRandom()
{
    QStringList valid;
    for (const auto& v : m_data.value("files").toArray()) {
        const QJsonObject f = v.toObject();
        if (!f.value("missing").toBool(false))
            valid << f.value("path").toString();
    }
    if (valid.isEmpty()) { m_lblStatus->setText("⚠  No files available."); return; }
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, valid.size()-1);
    emit fileSelected(valid.at(dist(rng)));
}

void ShujukoPanel::onPickSelected()
{
    auto* item = m_fileList->currentItem();
    if (item) emit fileSelected(item->data(Qt::UserRole).toString());
}

void ShujukoPanel::onDeleteEntry()
{
    auto* item = m_fileList->currentItem();
    if (!item) return;
    const QString path = item->data(Qt::UserRole).toString();
    QJsonArray files = m_data.value("files").toArray();
    for (int i = 0; i < files.size(); ++i) {
        if (files[i].toObject().value("path").toString() == path) {
            files.removeAt(i); break;
        }
    }
    m_data["files"] = files;
    refreshFileList();
    saveToDisk();
    m_lblStatus->setText(
        QString("\U0001f5d1  Removed: %1").arg(QFileInfo(path).fileName()));
}

void ShujukoPanel::onSaveDone(bool ok)
{
    m_saving = false;
    if (!ok) m_lblStatus->setText("✗  Save failed.");
    // Thread cleans itself up via deleteLater connections
    m_thread = nullptr;
}

void ShujukoPanel::onLoadDone(bool ok, QJsonObject data)
{
    pbDone(m_pb);
    m_thread = nullptr;
    if (ok && data.contains("files")) {
        m_data = data;
        m_data["kaivo_entry"] = m_entryName;
        m_data["source_dir"]  = m_sourceDir;
    } else {
        m_data = QJsonObject{
            {"kaivo_entry", m_entryName},
            {"source_dir",  m_sourceDir},
            {"files",       QJsonArray{}}};
    }
    refreshFileList();
}

// ── Private ───────────────────────────────────────────────────────────────────

void ShujukoPanel::buildUi()
{
    auto* lv = new QVBoxLayout(this);
    lv->setContentsMargins(6,6,6,6); lv->setSpacing(4);

    m_lblHeader = new QLabel("shujuko  –  no entry active");
    m_lblHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    lv->addWidget(m_lblHeader);

    // ── Scrollable file list ──────────────────────────────────────────────────
    // QListWidget already has its own scrollbar, but we also put it in a
    // QScrollArea so the entire panel scrolls if the window is very small.
    m_fileList = new QListWidget;
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileList->setVerticalScrollBarPolicy(Qt::ScrollBarAsNeeded);
    m_fileList->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    m_fileList->setMinimumHeight(80);
    m_fileList->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    m_fileList->setToolTip("Files bookmarked with S during AleaVue slideshow");
    connect(m_fileList, &QListWidget::itemDoubleClicked, this,
            [this](QListWidgetItem* item){
                emit fileSelected(item->data(Qt::UserRole).toString()); });
    lv->addWidget(m_fileList, 1);

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7); m_pb->setTextVisible(false); m_pb->setVisible(false);
    lv->addWidget(m_pb);

    auto* row1 = new QHBoxLayout;
    m_btnRandom = new QPushButton("Random");
    m_btnSelect = new QPushButton("Select");
    row1->addWidget(m_btnRandom); row1->addWidget(m_btnSelect);
    connect(m_btnRandom, &QPushButton::clicked, this, &ShujukoPanel::onPickRandom);
    connect(m_btnSelect, &QPushButton::clicked, this, &ShujukoPanel::onPickSelected);
    lv->addLayout(row1);

    // v0.5.31/32: remove-from-db button, secret mode only
    m_btnDelete = new QPushButton("\U0001f5d1  Remove from DB");
    m_btnDelete->setToolTip("Remove selected entry from shujuko (secret mode only)");
    m_btnDelete->setVisible(false);
    connect(m_btnDelete, &QPushButton::clicked, this, &ShujukoPanel::onDeleteEntry);
    lv->addWidget(m_btnDelete);

    m_lblStatus = new QLabel;
    m_lblStatus->setStyleSheet("font-size: 10px; color: gray;");
    m_lblStatus->setWordWrap(true);
    lv->addWidget(m_lblStatus);
}

void ShujukoPanel::safeStopThread()
{
    // If a thread is already running, disconnect it so its signals don't fire
    // back into us with stale data, then let it finish on its own.
    if (m_thread) {
        m_thread->disconnect(this);
        m_thread = nullptr;
    }
}

void ShujukoPanel::loadFromDisk()
{
    if (m_jsonPath.isEmpty()) return;
    safeStopThread();
    pbStart(m_pb);
    auto* worker = new DBLoadWorker(m_jsonPath);
    auto* thread = new QThread; m_thread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &DBLoadWorker::run);
    connect(worker, &DBLoadWorker::resultReady, this, &ShujukoPanel::onLoadDone);
    connect(worker, &DBLoadWorker::resultReady, thread, &QThread::quit);
    connect(worker, &DBLoadWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,         thread, &QObject::deleteLater);
    thread->start();
}

void ShujukoPanel::saveToDisk()
{
    if (m_jsonPath.isEmpty() || m_saving) return;   // prevent stacking saves
    m_saving = true;
    QDir().mkpath(TV::kaivoDir());
    safeStopThread();
    auto* worker = new DBSaveWorker(m_jsonPath, m_data);
    auto* thread = new QThread; m_thread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &DBSaveWorker::run);
    connect(worker, &DBSaveWorker::resultReady, this, &ShujukoPanel::onSaveDone);
    connect(worker, &DBSaveWorker::resultReady, thread, &QThread::quit);
    connect(worker, &DBSaveWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,         thread, &QObject::deleteLater);
    thread->start();
}

void ShujukoPanel::refreshFileList()
{
    // Preserve current selection
    const QString cur = m_fileList->currentItem()
        ? m_fileList->currentItem()->data(Qt::UserRole).toString() : QString();
    m_fileList->clear();
    int missing = 0;
    for (const auto& v : m_data.value("files").toArray()) {
        const QJsonObject f   = v.toObject();
        const QString    path = f.value("path").toString();
        if (path.isEmpty()) continue;
        const bool       miss = f.value("missing").toBool(false);
        if (miss) ++missing;
        const QString    name = QFileInfo(path).fileName();
        auto* item = new QListWidgetItem(miss ? QString("⚠ %1").arg(name) : name);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        if (miss) item->setForeground(Qt::gray);
        m_fileList->addItem(item);
    }
    // Restore selection
    for (int i = 0; i < m_fileList->count(); ++i) {
        if (m_fileList->item(i)->data(Qt::UserRole).toString() == cur) {
            m_fileList->setCurrentRow(i); break;
        }
    }
    const int total = m_fileList->count();
    if (!m_saving) {
        m_lblStatus->setText(
            QString("%1 file(s)%2").arg(total)
                .arg(missing > 0 ? QString("  ⚠ %1 missing").arg(missing) : QString()));
    }
}

void ShujukoPanel::setControlsEnabled(bool on)
{
    m_fileList->setEnabled(on);
    m_btnRandom->setEnabled(on);
    m_btnSelect->setEnabled(on);
}
