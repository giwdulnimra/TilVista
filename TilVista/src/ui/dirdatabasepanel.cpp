#include "dirdatabasepanel.h"
#include "core/pathutils.h"
#include "workers/catalogueworkers.h"
#include "workers/dbworkers.h"
#include "workers/scanworker.h"

#include <QAbstractItemView>
#include <QCoreApplication>
#include <QDateTime>
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

static void pbStart(QProgressBar* p, bool ind=true)
{ p->setRange(ind?0:0, ind?0:100); if(!ind) p->setValue(0); p->setVisible(true); }
static void pbDone(QProgressBar* p)
{ p->setRange(0,100); p->setValue(100); p->setVisible(false); }

// ── Constructor ───────────────────────────────────────────────────────────────

DirDatabasePanel::DirDatabasePanel(std::function<QString()> getCurrentDir,
                                    QWidget* parent)
    : QWidget(parent)
    , m_getCurrentDir(std::move(getCurrentDir))
    , m_base(TV::scriptDir())
    , m_kaivoDir(TV::kaivoDir())
    , m_dbPath(TV::kaivoDbPath())
    , m_db(QJsonObject{{"entries", QJsonArray{}}})
{
    buildUi();
    loadDb();
}

// ── Public ────────────────────────────────────────────────────────────────────

void DirDatabasePanel::updateCache(const QString& path,
                                    const QStringList& imageFiles,
                                    const QStringList& allFiles)
{
    m_lineName->setText(TV::autoEntryName(path));
    const QString stored = QDir(m_base).relativeFilePath(path);
    QJsonArray entries = m_db.value("entries").toArray();
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject e = entries[i].toObject();
        const QString resolved =
            QDir::cleanPath(m_base+'/'+e.value("path").toString());
        if (resolved == QDir::cleanPath(path)) {
            e["scanned_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
            entries[i] = e; m_db["entries"] = entries;
            writeCataloguesAndJson(e.value("name").toString(), imageFiles, allFiles);
            updateEntryInfo(e.value("name").toString());
            return;
        }
    }
    m_pending = { stored, imageFiles, allFiles, true };
}

void DirDatabasePanel::setSecretMode(bool on)
{
    if (m_secretMode == on) return;
    m_secretMode = on;
    m_btnSecret->setVisible(on);
    refreshList();
    emit secretModeChanged(on);
    m_lblStatus->setText(on ? "\U0001f513  Secret mode active"
                            : "\U0001f512  Secret mode off");
}

// ── Button slots ──────────────────────────────────────────────────────────────

void DirDatabasePanel::onSaveClicked()
{
    const QString path = m_getCurrentDir();
    if (path.isEmpty() || !QDir(path).exists()) {
        m_lblStatus->setText("⚠  No valid directory."); return;
    }
    QString name = m_lineName->text().trimmed();
    if (name.isEmpty()) name = TV::autoEntryName(path);
    m_lineName->setText(name);
    const QString stored = QDir(m_base).relativeFilePath(path);
    const QString safe = TV::safeFilename(name);
    const QString imgF = safe+"_img.dshow", allF = safe+"_all.catalogue";
    const QString ts   = QDateTime::currentDateTime().toString(Qt::ISODate);
    QJsonArray entries = m_db.value("entries").toArray();
    bool found = false;
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject e = entries[i].toObject();
        if (e.value("name").toString() == name) {
            e["path"]=stored; e["image_files"]=imgF; e["all_files"]=allF;
            e["scanned_at"]=ts; entries[i]=e; found=true; break;
        }
    }
    if (!found) {
        QJsonObject e; e["name"]=name; e["path"]=stored;
        e["image_files"]=imgF; e["all_files"]=allF;
        e["scanned_at"]=ts; e["hidden"]=false; entries.append(e);
    }
    m_db["entries"] = entries; m_pending.valid = false;
    refreshList();
    writeCataloguesAndJson(name, m_pending.imageFiles, m_pending.allFiles);
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
    const QString name = item->text().remove(QRegularExpression("^◌ "));
    QJsonArray entries = m_db.value("entries").toArray();
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject e = entries[i].toObject();
        if (e.value("name").toString() != name) continue;
        for (const QString& key : {"image_files","all_files"}) {
            const QString fn = e.value(key).toString();
            if (!fn.isEmpty()) QFile::remove(m_kaivoDir+'/'+fn);
        }
        QFile::remove(TV::shujukoPath(name));
        entries.removeAt(i); break;
    }
    m_db["entries"] = entries;
    if (m_activeEntryName == name) {
        m_activeEntryName.clear(); emit activeEntryChanged({},{});
    }
    refreshList(); m_lblEntryInfo->clear();
    saveJsonOnly();
}

void DirDatabasePanel::onUpdateClicked()
{
    // Guard: don't start if already scanning
    if (m_updThread) { m_lblStatus->setText("⚠  Already scanning…"); return; }

    auto* item = m_listWidget->currentItem();
    if (!item) { m_lblStatus->setText("⚠  Select an entry first."); return; }

    // Strip decoration prefix added in refreshList
    const QString name = item->text().remove(QRegularExpression("^◌ "));
    m_updatingEntryName = name;   // store for use in callback

    QJsonArray entries = m_db.value("entries").toArray();
    for (const auto& v : entries) {
        const QJsonObject e = v.toObject();
        if (e.value("name").toString() != name) continue;
        const QString absPath =
            QDir::cleanPath(m_base+'/'+e.value("path").toString());
        if (!QDir(absPath).exists()) {
            m_lblStatus->setText("⚠  Directory not found."); return;
        }
        pbStart(m_pb, false); m_btnUpdate->setEnabled(false);
        m_lblStatus->setText(QString("Updating: %1 …").arg(name));
        auto* worker = new ScanWorker(absPath);
        auto* thread = new QThread; m_updThread = thread;
        worker->moveToThread(thread);
        connect(thread, &QThread::started,  worker, &ScanWorker::run);
        connect(worker, &ScanWorker::progressChanged, m_pb, &QProgressBar::setValue);
        connect(worker, &ScanWorker::resultReady,
                this,   &DirDatabasePanel::onUpdateScanDone);
        connect(worker, &ScanWorker::resultReady, thread, &QThread::quit);
        connect(worker, &ScanWorker::resultReady, worker, &QObject::deleteLater);
        connect(thread, &QThread::finished,       thread, &QObject::deleteLater);
        thread->start();
        return;
    }
    m_lblStatus->setText("⚠  Entry not found in DB.");
}

void DirDatabasePanel::onToggleHiddenClicked()
{
    auto* item = m_listWidget->currentItem();
    if (!item) return;
    const QString name = item->text().remove(QRegularExpression("^◌ "));
    QJsonArray entries = m_db.value("entries").toArray();
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject e = entries[i].toObject();
        if (e.value("name").toString() != name) continue;
        const bool nowHidden = !e.value("hidden").toBool(false);
        e["hidden"] = nowHidden; entries[i] = e; m_db["entries"] = entries;
        saveJsonOnly(); refreshList();
        m_lblStatus->setText(nowHidden
            ? QString("\U0001f441  %1 hidden").arg(name)
            : QString("\U0001f441  %1 visible").arg(name));
        return;
    }
}

void DirDatabasePanel::onItemDoubleClicked(QListWidgetItem* item)
{ if (item) emitFromName(item->text().remove(QRegularExpression("^◌ "))); }

void DirDatabasePanel::onCurrentNameChanged(const QString& rawName)
{ updateEntryInfo(rawName.trimmed().remove(QRegularExpression("^◌ "))); }

// ── Worker callbacks ──────────────────────────────────────────────────────────

void DirDatabasePanel::onSaveDone(bool ok)
{
    pbDone(m_pb); m_btnSave->setEnabled(true);
    m_lblStatus->setText(ok ? "✓  Saved." : "✗  Save failed.");
    if (m_thread) { m_thread->quit(); m_thread->deleteLater(); m_thread = nullptr; }
}

void DirDatabasePanel::onIndexLoadDone(bool ok, QJsonObject data)
{
    pbDone(m_pb);
    if (ok && data.contains("entries")) m_db = data;
    refreshList();
    if (m_thread) { m_thread->quit(); m_thread->deleteLater(); m_thread = nullptr; }
}

void DirDatabasePanel::onCatalogueLoaded(bool ok,
                                          QStringList imageFiles,
                                          QStringList allFiles)
{
    pbDone(m_pb);
    if (m_catThread) { m_catThread->quit(); m_catThread->deleteLater(); m_catThread = nullptr; }
    m_lblStatus->setText(ok
        ? QString("✓  %1 images loaded.").arg(imageFiles.size())
        : "⚠  Catalogue error – will rescan.");
    QString sourceDir;
    for (const auto& v : m_db.value("entries").toArray()) {
        const QJsonObject e = v.toObject();
        if (e.value("name").toString() == m_activeEntryName) {
            sourceDir = QDir::cleanPath(m_base+'/'+e.value("path").toString()); break;
        }
    }
    emit dirLoaded(m_pendingLoadPath, imageFiles, allFiles);
    emit activeEntryChanged(m_activeEntryName, sourceDir);
}

void DirDatabasePanel::onUpdateScanDone(bool ok,
                                         QStringList imageFiles,
                                         QStringList allFiles)
{
    pbDone(m_pb); m_btnUpdate->setEnabled(true);
    m_updThread = nullptr;

    if (!ok) { m_lblStatus->setText("✗  Update scan failed."); return; }

    // Use the stored entry name – don't rely on currentItem() which may have changed
    if (m_updatingEntryName.isEmpty()) return;
    writeCataloguesAndJson(m_updatingEntryName, imageFiles, allFiles);
    emit requestShujukoValidation();
    m_lblStatus->setText(
        QString("✓  Updated: %1 img / %2 total.")
            .arg(imageFiles.size()).arg(allFiles.size()));
    m_updatingEntryName.clear();
}

// ── Private helpers ───────────────────────────────────────────────────────────

void DirDatabasePanel::buildUi()
{
    auto* lv = new QVBoxLayout(this);
    lv->setContentsMargins(6,6,6,6); lv->setSpacing(4);

    auto* hdr = new QLabel("kaivo  –  Directory Store");
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
    m_pb->setFixedHeight(7); m_pb->setTextVisible(false); m_pb->setVisible(false);
    lv->addWidget(m_pb);

    auto* row1 = new QHBoxLayout;
    m_btnSave   = new QPushButton("Save to DB");
    m_btnLoad   = new QPushButton("Load from DB");
    m_btnDelete = new QPushButton("Delete from DB");
    for (auto* b : {m_btnSave,m_btnLoad,m_btnDelete}) row1->addWidget(b);
    connect(m_btnSave,   &QPushButton::clicked, this, &DirDatabasePanel::onSaveClicked);
    connect(m_btnLoad,   &QPushButton::clicked, this, &DirDatabasePanel::onLoadClicked);
    connect(m_btnDelete, &QPushButton::clicked, this, &DirDatabasePanel::onDeleteClicked);
    lv->addLayout(row1);

    auto* row2 = new QHBoxLayout;
    m_btnUpdate = new QPushButton("\u21ba  Update Entry");
    m_btnUpdate->setToolTip("Rescan directory, refresh catalogues. Keeps shujuko intact.");
    connect(m_btnUpdate, &QPushButton::clicked, this, &DirDatabasePanel::onUpdateClicked);
    row2->addWidget(m_btnUpdate);

    m_btnSecret = new QPushButton("\U0001f441  Toggle Hidden");
    m_btnSecret->setVisible(false);
    connect(m_btnSecret, &QPushButton::clicked, this, &DirDatabasePanel::onToggleHiddenClicked);
    row2->addWidget(m_btnSecret);
    lv->addLayout(row2);

    m_lblStatus = new QLabel;
    m_lblStatus->setStyleSheet("font-size: 10px; color: gray;");
    lv->addWidget(m_lblStatus);
}

void DirDatabasePanel::emitFromName(const QString& name)
{
    if (name.isEmpty()) return;
    for (const auto& v : m_db.value("entries").toArray()) {
        const QJsonObject e = v.toObject();
        if (e.value("name").toString() != name) continue;
        const QString absPath =
            QDir::cleanPath(m_base+'/'+e.value("path").toString());
        if (!QDir(absPath).exists()) {
            QMessageBox::warning(this,"Path not found",
                QString("Could not resolve:\n%1").arg(absPath)); return;
        }
        m_activeEntryName = name; m_pendingLoadPath = absPath;
        const QString imgF = e.value("image_files").toString();
        const QString allF = e.value("all_files").toString();
        if (!imgF.isEmpty() && !allF.isEmpty()) {
            pbStart(m_pb);
            auto* worker = new CatalogueLoadWorker(m_kaivoDir, imgF, allF);
            auto* thread = new QThread; m_catThread = thread;
            worker->moveToThread(thread);
            connect(thread,&QThread::started, worker,&CatalogueLoadWorker::run);
            connect(worker,&CatalogueLoadWorker::resultReady,
                    this,  &DirDatabasePanel::onCatalogueLoaded);
            connect(worker,&CatalogueLoadWorker::resultReady,thread,&QThread::quit);
            connect(worker,&CatalogueLoadWorker::resultReady,worker,&QObject::deleteLater);
            connect(thread,&QThread::finished, thread,&QObject::deleteLater);
            thread->start();
        } else {
            const QString src = QDir::cleanPath(m_base+'/'+e.value("path").toString());
            emit dirLoaded(absPath,{},{});
            emit activeEntryChanged(name, src);
        }
        m_lblStatus->setText(QString("Loading: %1 …").arg(absPath));
        return;
    }
}

void DirDatabasePanel::writeCataloguesAndJson(const QString& entryName,
                                               const QStringList& img,
                                               const QStringList& all)
{
    pbStart(m_pb); m_btnSave->setEnabled(false);
    auto* worker = new CatalogueWriteWorker(
        m_kaivoDir, m_dbPath, m_db, entryName, img, all);
    auto* thread = new QThread; m_thread = thread;
    worker->moveToThread(thread);
    connect(thread,&QThread::started, worker,&CatalogueWriteWorker::run);
    connect(worker,&CatalogueWriteWorker::resultReady,this,&DirDatabasePanel::onSaveDone);
    connect(worker,&CatalogueWriteWorker::resultReady,thread,&QThread::quit);
    connect(worker,&CatalogueWriteWorker::resultReady,worker,&QObject::deleteLater);
    connect(thread,&QThread::finished, thread,&QObject::deleteLater);
    thread->start();
}

void DirDatabasePanel::saveJsonOnly()
{
    pbStart(m_pb);
    auto* worker = new DBSaveWorker(m_dbPath, m_db);
    auto* thread = new QThread; m_thread = thread;
    worker->moveToThread(thread);
    connect(thread,&QThread::started, worker,&DBSaveWorker::run);
    connect(worker,&DBSaveWorker::resultReady,this,&DirDatabasePanel::onSaveDone);
    connect(worker,&DBSaveWorker::resultReady,thread,&QThread::quit);
    connect(worker,&DBSaveWorker::resultReady,worker,&QObject::deleteLater);
    connect(thread,&QThread::finished, thread,&QObject::deleteLater);
    thread->start();
}

void DirDatabasePanel::loadDb()
{
    pbStart(m_pb);
    auto* worker = new DBLoadWorker(m_dbPath);
    auto* thread = new QThread; m_thread = thread;
    worker->moveToThread(thread);
    connect(thread,&QThread::started, worker,&DBLoadWorker::run);
    connect(worker,&DBLoadWorker::resultReady,this,&DirDatabasePanel::onIndexLoadDone);
    connect(worker,&DBLoadWorker::resultReady,thread,&QThread::quit);
    connect(worker,&DBLoadWorker::resultReady,worker,&QObject::deleteLater);
    connect(thread,&QThread::finished, thread,&QObject::deleteLater);
    thread->start();
}

void DirDatabasePanel::refreshList()
{
    const QString cur = m_listWidget->currentItem()
        ? m_listWidget->currentItem()->text() : QString();
    m_listWidget->clear();
    for (const auto& v : m_db.value("entries").toArray()) {
        const QJsonObject e = v.toObject();
        if (!isEntryVisible(e)) continue;
        const bool hidden = e.value("hidden").toBool(false);
        const QString name = e.value("name").toString();
        auto* item = new QListWidgetItem(hidden ? QString("◌ %1").arg(name) : name);
        if (hidden) item->setForeground(Qt::gray);
        m_listWidget->addItem(item);
    }
    const auto hits = m_listWidget->findItems(cur, Qt::MatchExactly);
    if (!hits.isEmpty()) m_listWidget->setCurrentItem(hits.first());
}

void DirDatabasePanel::updateEntryInfo(const QString& name)
{
    for (const auto& v : m_db.value("entries").toArray()) {
        const QJsonObject e = v.toObject();
        if (e.value("name").toString() == name) {
            m_lblEntryInfo->setText(
                QString("\U0001f5bc %1\n\U0001f4c4 %2\n\u23f1 %3%4")
                    .arg(e.value("image_files").toString("—"))
                    .arg(e.value("all_files").toString("—"))
                    .arg(e.value("scanned_at").toString("—"))
                    .arg(e.value("hidden").toBool() ? "\n\U0001f441 hidden" : ""));
            return;
        }
    }
    m_lblEntryInfo->clear();
}

bool DirDatabasePanel::isEntryVisible(const QJsonObject& entry) const
{
    if (entry.value("hidden").toBool(false) && !m_secretMode) return false;
    return true;
}
