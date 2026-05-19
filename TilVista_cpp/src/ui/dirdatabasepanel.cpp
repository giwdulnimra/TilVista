#include "dirdatabasepanel.h"
#include "core/pathutils.h"
#include "workers/catalogueworkers.h"
#include "workers/dbworkers.h"

#include <QAbstractItemView>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QListWidgetItem>
#include <QMessageBox>
#include <QProgressBar>
#include <QPushButton>
#include <QThread>
#include <QVBoxLayout>
#include <QDateTime>
// added
#include <QApplication>

// ── Helpers ──────────────────────────────────────────────────────────────────

static void progressStart(QProgressBar* pb, bool indeterminate)
{
    if (indeterminate)
        pb->setRange(0, 0);
    else {
        pb->setRange(0, 100);
        pb->setValue(0);
    }
    pb->setVisible(true);
}

static void progressDone(QProgressBar* pb)
{
    pb->setRange(0, 100);
    pb->setValue(100);
    pb->setVisible(false);
}

static void startThread(QThread*& threadRef, QObject* worker)
{
    auto* t = new QThread;
    worker->moveToThread(t);
    // self-cleanup wiring
    QObject::connect(t, &QThread::finished, worker, &QObject::deleteLater);
    QObject::connect(t, &QThread::finished, t,      &QObject::deleteLater);
    threadRef = t;
    t->start();
}

// ── Constructor ───────────────────────────────────────────────────────────────

DirDatabasePanel::DirDatabasePanel(std::function<QString()> getCurrentDir,
                                   QWidget* parent)
    : QWidget(parent)
    , m_getCurrentDir(std::move(getCurrentDir))
    , m_base(TV::kaivoDir() + "/../..")   // script dir = two levels above Kaivo
    , m_kaivoDir(TV::kaivoDir())
    , m_dbPath(TV::kaivoDbPath())
{
    // Fix m_base to the actual script dir
    //m_base = QCoreApplication::applicationDirPath();
    m_base = QApplication::applicationDirPath();

    buildUi();
    loadDb();
}

void DirDatabasePanel::buildUi()
{
    auto* lv = new QVBoxLayout(this);
    lv->setContentsMargins(6, 6, 6, 6);
    lv->setSpacing(4);

    auto* hdr = new QLabel("📁  Kaivo  –  Directory Store");
    hdr->setStyleSheet("font-weight: bold; font-size: 11px;");
    lv->addWidget(hdr);

    m_lineName = new QLineEdit;
    m_lineName->setPlaceholderText("Entry name (auto-filled after scan)…");
    lv->addWidget(m_lineName);

    m_listWidget = new QListWidget;
    m_listWidget->setSelectionMode(QAbstractItemView::SingleSelection);
    m_listWidget->setToolTip("Double-click → Load from DB");
    connect(m_listWidget, &QListWidget::itemDoubleClicked,
            this, &DirDatabasePanel::onItemDoubleClicked);
    connect(m_listWidget, &QListWidget::currentTextChanged,
            this, &DirDatabasePanel::onCurrentNameChanged);
    lv->addWidget(m_listWidget);

    m_lblEntryInfo = new QLabel;
    m_lblEntryInfo->setStyleSheet("font-size: 9px; color: #888;");
    m_lblEntryInfo->setWordWrap(true);
    lv->addWidget(m_lblEntryInfo);

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7);
    m_pb->setTextVisible(false);
    m_pb->setVisible(false);
    lv->addWidget(m_pb);

    auto* btnRow = new QHBoxLayout;
    m_btnSave   = new QPushButton("Save to DB");
    m_btnLoad   = new QPushButton("Load from DB");
    m_btnDelete = new QPushButton("Delete from DB");
    for (auto* b : {m_btnSave, m_btnLoad, m_btnDelete})
        btnRow->addWidget(b);
    connect(m_btnSave,   &QPushButton::clicked, this, &DirDatabasePanel::onSaveClicked);
    connect(m_btnLoad,   &QPushButton::clicked, this, &DirDatabasePanel::onLoadClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &DirDatabasePanel::onDeleteClicked);
    lv->addLayout(btnRow);

    m_lblStatus = new QLabel;
    m_lblStatus->setStyleSheet("font-size: 10px; color: gray;");
    lv->addWidget(m_lblStatus);
}

// ── Public ────────────────────────────────────────────────────────────────────

void DirDatabasePanel::updateCache(const QString&     path,
                                   const QStringList& imageFiles,
                                   const QStringList& allFiles)
{
    m_lineName->setText(TV::autoEntryName(path));

    const QString storedPath =
        QDir(m_base).relativeFilePath(path);

    // Find existing entry by resolved path
    QJsonArray entries = m_db.value("entries").toArray();
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject e = entries[i].toObject();
        const QString resolved =
            QDir::cleanPath(m_base + '/' + e.value("path").toString());
        if (resolved == QDir::cleanPath(path)) {
            e["scanned_at"] = QDateTime::currentDateTime()
                              .toString(Qt::ISODate);
            entries[i] = e;
            m_db["entries"] = entries;
            writeCataloguesAndJson(e.value("name").toString(),
                                   imageFiles, allFiles);
            updateEntryInfo(e.value("name").toString());
            return;
        }
    }

    // No existing entry – stash for later Save
    m_pending = { storedPath, imageFiles, allFiles, true };
}

// ── Slots: buttons ────────────────────────────────────────────────────────────

void DirDatabasePanel::onSaveClicked()
{
    const QString path = m_getCurrentDir();
    if (path.isEmpty() || !QDir(path).exists()) {
        m_lblStatus->setText("⚠  No valid directory active.");
        return;
    }
    QString name = m_lineName->text().trimmed();
    if (name.isEmpty())
        name = TV::autoEntryName(path);
    m_lineName->setText(name);

    const QString stored = QDir(m_base).relativeFilePath(path);
    const QString safe   = TV::safeFilename(name);
    const QString imgF   = safe + "_img.dshow";
    const QString allF   = safe + "_all.catalogue";
    const QString ts     = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray entries = m_db.value("entries").toArray();
    bool found = false;
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject e = entries[i].toObject();
        if (e.value("name").toString() == name) {
            e["path"]        = stored;
            e["image_files"] = imgF;
            e["all_files"]   = allF;
            e["scanned_at"]  = ts;
            entries[i] = e;
            found = true;
            break;
        }
    }
    if (!found) {
        QJsonObject e;
        e["name"]        = name;
        e["path"]        = stored;
        e["image_files"] = imgF;
        e["all_files"]   = allF;
        e["scanned_at"]  = ts;
        entries.append(e);
    }
    m_db["entries"] = entries;
    m_pending.valid = false;

    refreshList();
    writeCataloguesAndJson(name,
                           m_pending.imageFiles,
                           m_pending.allFiles);
}

void DirDatabasePanel::onLoadClicked()
{
    auto* item = m_listWidget->currentItem();
    if (item) emitFromName(item->text());
}

void DirDatabasePanel::onDeleteClicked()
{
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    const QString name = item->text();

    QJsonArray entries = m_db.value("entries").toArray();
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject e = entries[i].toObject();
        if (e.value("name").toString() == name) {
            // Remove catalogue files
            for (const QString& key : {"image_files", "all_files"}) {
                const QString fname = e.value(key).toString();
                if (!fname.isEmpty())
                    QFile::remove(m_kaivoDir + '/' + fname);
            }
            entries.removeAt(i);
            break;
        }
    }
    m_db["entries"] = entries;
    refreshList();
    m_lblEntryInfo->clear();
    saveJsonOnly();
}

void DirDatabasePanel::onItemDoubleClicked(QListWidgetItem* item)
{
    if (item) emitFromName(item->text());
}

void DirDatabasePanel::onCurrentNameChanged(const QString& name)
{
    updateEntryInfo(name);
}

// ── Slots: worker callbacks ───────────────────────────────────────────────────

void DirDatabasePanel::onSaveDone(bool ok)
{
    progressDone(m_pb);
    m_btnSave->setEnabled(true);
    m_lblStatus->setText(ok ? "✓  Saved." : "✗  Save failed.");
    m_thread = nullptr;
}

void DirDatabasePanel::onLoadDone(bool ok, QJsonObject data)
{
    progressDone(m_pb);
    if (ok && data.contains("entries"))
        m_db = data;
    refreshList();
    m_thread = nullptr;
}

void DirDatabasePanel::onCatalogueLoaded(bool ok,
                                          QStringList imageFiles,
                                          QStringList allFiles)
{
    progressDone(m_pb);
    m_catThread = nullptr;
    const int n = imageFiles.size();
    m_lblStatus->setText(ok
        ? QString("✓  %1 images loaded from catalogue.").arg(n)
        : "⚠  Catalogue read error – will rescan.");
    emit dirLoaded(m_pendingLoadPath, imageFiles, allFiles);
}

// ── Private helpers ───────────────────────────────────────────────────────────

void DirDatabasePanel::emitFromName(const QString& name)
{
    const QJsonArray entries = m_db.value("entries").toArray();
    for (const auto& v : entries) {
        const QJsonObject e = v.toObject();
        if (e.value("name").toString() != name) continue;

        const QString absPath = QDir::cleanPath(
            m_base + '/' + e.value("path").toString());

        if (!QDir(absPath).exists()) {
            QMessageBox::warning(this, "Path not found",
                QString("Could not resolve:\n%1\n\n(Stored: %2)")
                    .arg(absPath, e.value("path").toString()));
            return;
        }

        const QString imgF = e.value("image_files").toString();
        const QString allF = e.value("all_files").toString();

        if (!imgF.isEmpty() && !allF.isEmpty()) {
            m_pendingLoadPath = absPath;
            progressStart(m_pb, true);

            auto* worker = new CatalogueLoadWorker(m_kaivoDir, imgF, allF);
            auto* thread = new QThread;
            m_catThread  = thread;

            worker->moveToThread(thread);
            connect(thread, &QThread::started,
                    worker, &CatalogueLoadWorker::run);
            connect(worker, &CatalogueLoadWorker::resultReady,
                    this,   &DirDatabasePanel::onCatalogueLoaded);
            connect(worker, &CatalogueLoadWorker::resultReady,
                    thread, &QThread::quit);
            connect(worker, &CatalogueLoadWorker::resultReady,
                    worker, &QObject::deleteLater);
            connect(thread, &QThread::finished,
                    thread, &QObject::deleteLater);
            thread->start();
        } else {
            // No catalogue yet → emit empty lists, caller will rescan
            emit dirLoaded(absPath, {}, {});
        }
        m_lblStatus->setText(QString("Loading: %1 …").arg(absPath));
        return;
    }
}

void DirDatabasePanel::writeCataloguesAndJson(const QString&     entryName,
                                              const QStringList& imageFiles,
                                              const QStringList& allFiles)
{
    progressStart(m_pb, true);
    m_btnSave->setEnabled(false);

    auto* worker = new CatalogueWriteWorker(
        m_kaivoDir, m_dbPath, m_db, entryName, imageFiles, allFiles);
    auto* thread = new QThread;
    m_thread = thread;

    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &CatalogueWriteWorker::run);
    connect(worker, &CatalogueWriteWorker::resultReady,
            this,   &DirDatabasePanel::onSaveDone);
    connect(worker, &CatalogueWriteWorker::resultReady,
            thread, &QThread::quit);
    connect(worker, &CatalogueWriteWorker::resultReady,
            worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DirDatabasePanel::saveJsonOnly()
{
    progressStart(m_pb, true);
    auto* worker = new DBSaveWorker(m_dbPath, m_db);
    auto* thread = new QThread;
    m_thread = thread;

    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &DBSaveWorker::run);
    connect(worker, &DBSaveWorker::resultReady,
            this,   &DirDatabasePanel::onSaveDone);
    connect(worker, &DBSaveWorker::resultReady, thread, &QThread::quit);
    connect(worker, &DBSaveWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DirDatabasePanel::loadDb()
{
    progressStart(m_pb, true);
    auto* worker = new DBLoadWorker(m_dbPath);
    auto* thread = new QThread;
    m_thread = thread;

    worker->moveToThread(thread);
    connect(thread, &QThread::started, worker, &DBLoadWorker::run);
    connect(worker, &DBLoadWorker::resultReady,
            this,   &DirDatabasePanel::onLoadDone);
    connect(worker, &DBLoadWorker::resultReady, thread, &QThread::quit);
    connect(worker, &DBLoadWorker::resultReady, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);
    thread->start();
}

void DirDatabasePanel::refreshList()
{
    const QString cur = m_listWidget->currentItem()
                      ? m_listWidget->currentItem()->text() : QString();
    m_listWidget->clear();
    const QJsonArray entries = m_db.value("entries").toArray();
    for (const auto& v : entries)
        m_listWidget->addItem(v.toObject().value("name").toString());

    const auto items = m_listWidget->findItems(cur, Qt::MatchExactly);
    if (!items.isEmpty())
        m_listWidget->setCurrentItem(items.first());
}

void DirDatabasePanel::updateEntryInfo(const QString& name)
{
    const QJsonArray entries = m_db.value("entries").toArray();
    for (const auto& v : entries) {
        const QJsonObject e = v.toObject();
        if (e.value("name").toString() == name) {
            m_lblEntryInfo->setText(
                QString("🖼 %1\n📄 %2\n⏱ %3")
                    .arg(e.value("image_files").toString("—"))
                    .arg(e.value("all_files").toString("—"))
                    .arg(e.value("scanned_at").toString("—")));
            return;
        }
    }
    m_lblEntryInfo->clear();
}
