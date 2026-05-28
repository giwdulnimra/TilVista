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

// ── Constructor ───────────────────────────────────────────────────────────────

ShujukoPanel::ShujukoPanel(QWidget* parent)
    : QWidget(parent)
    , m_data(QJsonObject{{"kaivo_entry",""},{"source_dir",""},
                          {"files",QJsonArray{}}})
{
    buildUi();
    setEnabled_(false);   // disabled until a kaivo entry is active
}

// ── Public ────────────────────────────────────────────────────────────────────

void ShujukoPanel::setActiveKaivoEntry(const QString& kaivoEntryName,
                                        const QString& sourceDir)
{
    m_entryName = kaivoEntryName;
    m_sourceDir = sourceDir;

    if (kaivoEntryName.isEmpty()) {
        m_jsonPath.clear();
        m_data = QJsonObject{{"kaivo_entry",""},{"source_dir",""},
                              {"files",QJsonArray{}}};
        m_fileList->clear();
        m_lblHeader->setText("shujuko  –  no entry active");
        setEnabled_(false);
        return;
    }

    m_jsonPath = TV::shujukoPath(kaivoEntryName);
    m_lblHeader->setText(QString("shujuko  –  %1").arg(kaivoEntryName));
    setEnabled_(true);
    loadFromDisk();
}

void ShujukoPanel::addBookmark(const QString& filePath)
{
    if (m_entryName.isEmpty() || filePath.isEmpty()) return;

    QJsonArray files = m_data.value("files").toArray();
    // Check duplicate
    for (const auto& v : files)
        if (v.toObject().value("path").toString() == filePath) return;

    QJsonObject entry;
    entry["path"]     = filePath;
    entry["added_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    entry["missing"]  = !QFileInfo::exists(filePath);
    files.append(entry);

    m_data["files"]      = files;
    m_data["kaivo_entry"] = m_entryName;
    m_data["source_dir"] = m_sourceDir;

    saveToDisk();
    refreshFileList();
    m_lblStatus->setText(
        QString("✓  Bookmarked: %1").arg(QFileInfo(filePath).fileName()));
}

void ShujukoPanel::validateFiles()
{
    QJsonArray files = m_data.value("files").toArray();
    bool changed = false;
    for (int i = 0; i < files.size(); ++i) {
        QJsonObject f = files[i].toObject();
        const bool exists = QFileInfo::exists(f.value("path").toString());
        if (f.value("missing").toBool() != !exists) {
            f["missing"] = !exists;
            files[i]     = f;
            changed      = true;
        }
    }
    if (changed) {
        m_data["files"] = files;
        saveToDisk();
        refreshFileList();
    }
    m_lblStatus->setText("✓  File validation done.");
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
    std::uniform_int_distribution<int> dist(0, valid.size() - 1);
    emit fileSelected(valid.at(dist(rng)));
}

void ShujukoPanel::onPickSelected()
{
    auto* item = m_fileList->currentItem();
    if (item) emit fileSelected(item->data(Qt::UserRole).toString());
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
        // Ensure metadata is correct
        m_data["kaivo_entry"] = m_entryName;
        m_data["source_dir"]  = m_sourceDir;
    } else {
        // Fresh entry
        m_data = QJsonObject{
            {"kaivo_entry", m_entryName},
            {"source_dir",  m_sourceDir},
            {"files",       QJsonArray{}}
        };
    }
    refreshFileList();
    if (m_thread) { m_thread->quit(); m_thread->deleteLater(); m_thread = nullptr; }
}

// ── Private ───────────────────────────────────────────────────────────────────

void ShujukoPanel::buildUi()
{
    auto* lv = new QVBoxLayout(this);
    lv->setContentsMargins(6, 6, 6, 6);
    lv->setSpacing(4);

    m_lblHeader = new QLabel("shujuko  –  no entry active");
    m_lblHeader->setStyleSheet("font-weight: bold; font-size: 11px;");
    lv->addWidget(m_lblHeader);

    m_fileList = new QListWidget;
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    m_fileList->setToolTip("Double-click to preview");
    connect(m_fileList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item){
                emit fileSelected(item->data(Qt::UserRole).toString());
            });
    lv->addWidget(m_fileList);

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7); m_pb->setTextVisible(false); m_pb->setVisible(false);
    lv->addWidget(m_pb);

    auto* btnRow = new QHBoxLayout;
    m_btnRandom = new QPushButton("Random from shujuko");
    m_btnSelect = new QPushButton("Select from shujuko");
    btnRow->addWidget(m_btnRandom);
    btnRow->addWidget(m_btnSelect);
    connect(m_btnRandom, &QPushButton::clicked, this, &ShujukoPanel::onPickRandom);
    connect(m_btnSelect, &QPushButton::clicked, this, &ShujukoPanel::onPickSelected);
    lv->addLayout(btnRow);

    m_lblStatus = new QLabel;
    m_lblStatus->setStyleSheet("font-size: 10px; color: gray;");
    lv->addWidget(m_lblStatus);
}

void ShujukoPanel::loadFromDisk()
{
    if (m_jsonPath.isEmpty()) return;
    pbStart(m_pb);

    auto* worker = new DBLoadWorker(m_jsonPath);
    auto* thread = new QThread;
    m_thread = thread;
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
    auto* thread = new QThread;
    m_thread = thread;
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
                      ? m_fileList->currentItem()->data(Qt::UserRole).toString()
                      : QString();
    m_fileList->clear();

    for (const auto& v : m_data.value("files").toArray()) {
        const QJsonObject f   = v.toObject();
        const QString    path = f.value("path").toString();
        const bool       miss = f.value("missing").toBool();
        const QString    name = QFileInfo(path).fileName();

        auto* item = new QListWidgetItem(miss ? QString("⚠ %1").arg(name) : name);
        item->setData(Qt::UserRole, path);
        item->setToolTip(path);
        if (miss) item->setForeground(Qt::gray);
        m_fileList->addItem(item);
    }

    // Restore selection
    const auto hits = m_fileList->findItems(QString(), Qt::MatchContains);
    for (auto* it : hits) {
        if (it->data(Qt::UserRole).toString() == cur) {
            m_fileList->setCurrentItem(it); break;
        }
    }

    const int total = m_fileList->count();
    int missing = 0;
    for (const auto& v : m_data.value("files").toArray())
        if (v.toObject().value("missing").toBool()) ++missing;

    m_lblStatus->setText(
        QString("%1 file(s)%2")
            .arg(total)
            .arg(missing > 0 ? QString("  ⚠ %1 missing").arg(missing) : QString()));
}

void ShujukoPanel::setEnabled_(bool on)
{
    m_fileList->setEnabled(on);
    m_btnRandom->setEnabled(on);
    m_btnSelect->setEnabled(on);
}
