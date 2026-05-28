#include "shujukopanel.h"
#include "core/pathutils.h"
#include "workers/dbworkers.h"

#include <QAbstractItemView>
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
#include <QThread>
#include <QVBoxLayout>
#include <random>

static void pbStart(QProgressBar* p){ p->setRange(0,0); p->setVisible(true); }
static void pbDone (QProgressBar* p){ p->setRange(0,100); p->setValue(100); p->setVisible(false); }

ShujukoPanel::ShujukoPanel(QWidget* parent)
    : QWidget(parent)
    , m_data(QJsonObject{{"kaivo_entry",""},{"source_dir",""},
                          {"files",QJsonArray{}}})
{
    buildUi();
    setControlsEnabled(false);
}

// ── Public ────────────────────────────────────────────────────────────────────

void ShujukoPanel::setActiveKaivoEntry(const QString& name, const QString& src)
{
    m_entryName = name; m_sourceDir = src;
    if (name.isEmpty()) {
        m_jsonPath.clear();
        m_data = QJsonObject{{"kaivo_entry",""},{"source_dir",""},
                              {"files",QJsonArray{}}};
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
        if (v.toObject().value("path").toString() == filePath) return;
    QJsonObject e;
    e["path"]     = filePath;
    e["added_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    e["missing"]  = !QFileInfo::exists(filePath);
    files.append(e);
    m_data["files"]       = files;
    m_data["kaivo_entry"] = m_entryName;
    m_data["source_dir"]  = m_sourceDir;
    saveToDisk();
    refreshFileList();
    m_lblStatus->setText(
        QString("✓  %1").arg(QFileInfo(filePath).fileName()));
}

void ShujukoPanel::validateFiles()
{
    QJsonArray files = m_data.value("files").toArray();
    bool changed = false;
    for (int i = 0; i < files.size(); ++i) {
        QJsonObject f = files[i].toObject();
        const bool exists = QFileInfo::exists(f.value("path").toString());
        if (f.value("missing").toBool() != !exists) {
            f["missing"] = !exists; files[i] = f; changed = true;
        }
    }
    if (changed) { m_data["files"] = files; saveToDisk(); refreshFileList(); }
    m_lblStatus->setText("✓  Validation done.");
}

void ShujukoPanel::setSecretMode(bool on)
{
    // Delete button only visible in secret mode
    m_btnDelete->setVisible(on);
}

QString ShujukoPanel::currentJsonPath() const { return m_jsonPath; }

// ── Slots ─────────────────────────────────────────────────────────────────────

void ShujukoPanel::onPickRandom()
{
    QStringList valid;
    for (const auto& v : m_data.value("files").toArray()) {
        const QJsonObject f = v.toObject();
        if (!f.value("missing").toBool())
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
    // v0.5.31: remove selected file from shujuko list (secret mode only)
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
    saveToDisk();
    refreshFileList();
    m_lblStatus->setText(QString("🗑  Removed: %1")
        .arg(QFileInfo(path).fileName()));
}

void ShujukoPanel::onSaveDone(bool ok)
{
    if (!ok) m_lblStatus->setText("✗  Save failed.");
    if (m_thread) { m_thread->quit(); m_thread->deleteLater(); m_thread = nullptr; }
}

void ShujukoPanel::onLoadDone(bool ok, QJsonObject data)
{
    pbDone(m_pb);
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
    if (m_thread) { m_thread->quit(); m_thread->deleteLater(); m_thread = nullptr; }
}

// ── Private ───────────────────────────────────────────────────────────────────

void ShujukoPanel::buildUi()
{
    auto* lv = new QVBoxLayout(this);
    lv->setContentsMargins(6,6,6,6); lv->setSpacing(4);

    m_lblHeader = new QLabel("shujuko  –  no entry active");
    m_lblHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    lv->addWidget(m_lblHeader);

    m_fileList = new QListWidget;
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileList->setToolTip("Files bookmarked with S during AleaVue slideshow");
    connect(m_fileList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item){
                emit fileSelected(item->data(Qt::UserRole).toString());
            });
    lv->addWidget(m_fileList);

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7); m_pb->setTextVisible(false); m_pb->setVisible(false);
    lv->addWidget(m_pb);

    auto* row1 = new QHBoxLayout;
    m_btnRandom = new QPushButton("Random from shujuko");
    m_btnSelect = new QPushButton("Select from shujuko");
    row1->addWidget(m_btnRandom); row1->addWidget(m_btnSelect);
    connect(m_btnRandom, &QPushButton::clicked, this, &ShujukoPanel::onPickRandom);
    connect(m_btnSelect, &QPushButton::clicked, this, &ShujukoPanel::onPickSelected);
    lv->addLayout(row1);

    // v0.5.31: delete button – only visible in secret mode
    m_btnDelete = new QPushButton("🗑  Delete from shujuko");
    m_btnDelete->setToolTip("Remove selected file from this shujuko entry\n"
                             "(only available in secret mode)");
    m_btnDelete->setVisible(false);
    connect(m_btnDelete, &QPushButton::clicked, this, &ShujukoPanel::onDeleteEntry);
    lv->addWidget(m_btnDelete);

    m_lblStatus = new QLabel;
    m_lblStatus->setStyleSheet("font-size: 10px; color: gray;");
    lv->addWidget(m_lblStatus);
}

void ShujukoPanel::loadFromDisk()
{
    if (m_jsonPath.isEmpty()) return;
    pbStart(m_pb);
    auto* worker = new DBLoadWorker(m_jsonPath);
    auto* thread = new QThread; m_thread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &DBLoadWorker::run);
    connect(worker, &DBLoadWorker::resultReady,
            this,   &ShujukoPanel::onLoadDone);
    connect(worker, &DBLoadWorker::resultReady, thread, &QThread::quit);
    connect(worker, &DBLoadWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,         thread, &QObject::deleteLater);
    thread->start();
}

void ShujukoPanel::saveToDisk()
{
    if (m_jsonPath.isEmpty()) return;
    QDir().mkpath(TV::kaivoDir());
    auto* worker = new DBSaveWorker(m_jsonPath, m_data);
    auto* thread = new QThread; m_thread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &DBSaveWorker::run);
    connect(worker, &DBSaveWorker::resultReady,
            this,   &ShujukoPanel::onSaveDone);
    connect(worker, &DBSaveWorker::resultReady, thread, &QThread::quit);
    connect(worker, &DBSaveWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,         thread, &QObject::deleteLater);
    thread->start();
}

void ShujukoPanel::refreshFileList()
{
    const QString cur = m_fileList->currentItem()
        ? m_fileList->currentItem()->data(Qt::UserRole).toString() : QString();
    m_fileList->clear();
    int missing = 0;
    for (const auto& v : m_data.value("files").toArray()) {
        const QJsonObject f    = v.toObject();
        const QString    path  = f.value("path").toString();
        const bool       miss  = f.value("missing").toBool();
        if (miss) ++missing;
        const QString    name  = QFileInfo(path).fileName();
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
    m_lblStatus->setText(
        QString("%1 file(s)%2").arg(total)
            .arg(missing > 0 ? QString("  ⚠ %1 missing").arg(missing) : QString()));
}

void ShujukoPanel::setControlsEnabled(bool on)
{
    m_fileList->setEnabled(on);
    m_btnRandom->setEnabled(on);
    m_btnSelect->setEnabled(on);
    // delete button respects secret mode independently – handled by setSecretMode
}
