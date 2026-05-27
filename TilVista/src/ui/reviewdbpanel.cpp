#include "reviewdbpanel.h"
#include "core/pathutils.h"
#include "workers/dbworkers.h"

#include <QAbstractItemView>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QJsonArray>
#include <QJsonObject>
#include <QLabel>
#include <QListWidget>
#include <QListWidgetItem>
#include <QProgressBar>
#include <QPushButton>
#include <QTextStream>
#include <QThread>
#include <QVBoxLayout>
#include <random>

// ── Helpers ───────────────────────────────────────────────────────────────────

static void pbStart(QProgressBar* pb)
{ pb->setRange(0,0); pb->setVisible(true); }

static void pbDone(QProgressBar* pb)
{ pb->setRange(0,100); pb->setValue(100); pb->setVisible(false); }

// ── Constructor ───────────────────────────────────────────────────────────────

ReviewDBPanel::ReviewDBPanel(QWidget* parent)
    : QWidget(parent)
    , m_dbPath(TV::reviewDbPath())
    , m_db(QJsonObject{ {"sessions", QJsonArray{}} })
{
    buildUi();
    loadDb();
}

// ── Public ────────────────────────────────────────────────────────────────────

void ReviewDBPanel::addBookmark(const QString& filePath,
                                 const QString& sourceDir)
{
    const QString name =
        QDir(sourceDir).dirName() + '_' +
        QDateTime::currentDateTime().toString("yyMMdd");

    QJsonArray sessions = m_db.value("sessions").toArray();

    // Find existing session for today + this directory
    for (int i = 0; i < sessions.size(); ++i) {
        QJsonObject s = sessions[i].toObject();
        if (s.value("name").toString() == name) {
            QJsonArray files = s.value("files").toArray();
            if (!files.contains(filePath)) {
                files.append(filePath);
                s["files"] = files;
                sessions[i] = s;
                m_db["sessions"] = sessions;
                saveDb();
                refreshSessions();
            }
            return;
        }
    }

    // New session
    QJsonObject s;
    s["name"]       = name;
    s["source_dir"] = sourceDir;
    s["files"]      = QJsonArray{ filePath };
    s["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    sessions.append(s);
    m_db["sessions"] = sessions;
    saveDb();
    refreshSessions();
}

// ── Slots ─────────────────────────────────────────────────────────────────────

void ReviewDBPanel::onSessionChanged(const QString& name)
{
    m_fileList->clear();
    const QJsonArray sessions = m_db.value("sessions").toArray();
    for (const auto& v : sessions) {
        const QJsonObject s = v.toObject();
        if (s.value("name").toString() != name) continue;
        for (const auto& fv : s.value("files").toArray()) {
            const QString fp = fv.toString();
            auto* item = new QListWidgetItem(QFileInfo(fp).fileName());
            item->setData(Qt::UserRole, fp);
            m_fileList->addItem(item);
        }
        break;
    }
}

void ReviewDBPanel::onPickRandom()
{
    const QStringList files = currentFiles();
    if (files.isEmpty()) {
        m_lblStatus->setText("⚠  No session selected.");
        return;
    }
    std::mt19937 rng(std::random_device{}());
    std::uniform_int_distribution<int> dist(0, files.size() - 1);
    emit fileSelected(files.at(dist(rng)));
}

void ReviewDBPanel::onPickSelected()
{
    auto* item = m_fileList->currentItem();
    if (item)
        emit fileSelected(item->data(Qt::UserRole).toString());
}

void ReviewDBPanel::onSaveDone(bool ok)
{
    m_lblStatus->setText(ok ? "✓  Saved." : "✗  Save failed.");
    if (m_thread) {
        m_thread->quit();
        m_thread->deleteLater();
        m_thread = nullptr;
    }
}

void ReviewDBPanel::onLoadDone(bool ok, QJsonObject data)
{
    pbDone(m_pb);
    if (ok && data.contains("sessions"))
        m_db = data;
    else
        migrateLegacyLog();
    refreshSessions();
    if (m_thread) {
        m_thread->quit();
        m_thread->deleteLater();
        m_thread = nullptr;
    }
}

// ── Private ───────────────────────────────────────────────────────────────────

void ReviewDBPanel::buildUi()
{
    auto* lv = new QVBoxLayout(this);
    lv->setContentsMargins(6, 6, 6, 6);
    lv->setSpacing(4);

    auto* hdr = new QLabel("kaivo  –  review");
    hdr->setStyleSheet("font-weight: bold; font-size: 11px;");
    lv->addWidget(hdr);

    m_sessionList = new QListWidget;
    m_sessionList->setMaximumHeight(80);
    m_sessionList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_sessionList, &QListWidget::currentTextChanged,
            this, &ReviewDBPanel::onSessionChanged);
    lv->addWidget(m_sessionList);

    m_fileList = new QListWidget;
    m_fileList->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(m_fileList, &QListWidget::itemDoubleClicked,
            this, [this](QListWidgetItem* item){
                emit fileSelected(item->data(Qt::UserRole).toString());
            });
    lv->addWidget(m_fileList);

    m_pb = new QProgressBar;
    m_pb->setFixedHeight(7);
    m_pb->setTextVisible(false);
    m_pb->setVisible(false);
    lv->addWidget(m_pb);

    auto* btnRow = new QHBoxLayout;
    m_btnRandom = new QPushButton("Random from DB");
    m_btnSelect = new QPushButton("Select from DB");
    btnRow->addWidget(m_btnRandom);
    btnRow->addWidget(m_btnSelect);
    connect(m_btnRandom, &QPushButton::clicked,
            this, &ReviewDBPanel::onPickRandom);
    connect(m_btnSelect, &QPushButton::clicked,
            this, &ReviewDBPanel::onPickSelected);
    lv->addLayout(btnRow);

    m_lblStatus = new QLabel;
    m_lblStatus->setStyleSheet("font-size: 10px; color: gray;");
    lv->addWidget(m_lblStatus);
}

void ReviewDBPanel::refreshSessions()
{
    const QString cur = m_sessionList->currentItem()
                      ? m_sessionList->currentItem()->text() : QString();
    m_sessionList->clear();
    for (const auto& v : m_db.value("sessions").toArray())
        m_sessionList->addItem(v.toObject().value("name").toString());
    const auto hits = m_sessionList->findItems(cur, Qt::MatchExactly);
    if (!hits.isEmpty())
        m_sessionList->setCurrentItem(hits.first());
}

QStringList ReviewDBPanel::currentFiles() const
{
    auto* item = m_sessionList->currentItem();
    if (!item) return {};
    const QString name = item->text();
    for (const auto& v : m_db.value("sessions").toArray()) {
        const QJsonObject s = v.toObject();
        if (s.value("name").toString() == name) {
            QStringList out;
            for (const auto& fv : s.value("files").toArray())
                out << fv.toString();
            return out;
        }
    }
    return {};
}

void ReviewDBPanel::saveDb()
{
    auto* worker = new DBSaveWorker(m_dbPath, m_db);
    auto* thread = new QThread;
    m_thread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &DBSaveWorker::run);
    connect(worker, &DBSaveWorker::resultReady,
            this,   &ReviewDBPanel::onSaveDone);
    connect(worker, &DBSaveWorker::resultReady,
            thread, &QThread::quit);
    connect(worker, &DBSaveWorker::resultReady,
            worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,
            thread, &QObject::deleteLater);
    thread->start();
}

void ReviewDBPanel::loadDb()
{
    pbStart(m_pb);
    auto* worker = new DBLoadWorker(m_dbPath);
    auto* thread = new QThread;
    m_thread = thread;
    worker->moveToThread(thread);
    connect(thread, &QThread::started,  worker, &DBLoadWorker::run);
    connect(worker, &DBLoadWorker::resultReady,
            this,   &ReviewDBPanel::onLoadDone);
    connect(worker, &DBLoadWorker::resultReady,
            thread, &QThread::quit);
    connect(worker, &DBLoadWorker::resultReady,
            worker, &QObject::deleteLater);
    connect(thread, &QThread::finished,
            thread, &QObject::deleteLater);
    thread->start();
}

void ReviewDBPanel::migrateLegacyLog()
{
    const QString oldPath = TV::logDir() + "/reviewpics.txt";
    QFile f(oldPath);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QStringList paths;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty()) paths << line;
    }
    f.close();
    if (paths.isEmpty()) return;

    QJsonObject session;
    session["name"]       = QString("legacy_%1")
        .arg(QDateTime::currentDateTime().toString("yyMMdd"));
    session["source_dir"] = QString();
    QJsonArray arr;
    for (const QString& p : paths) arr.append(p);
    session["files"]      = arr;
    session["created_at"] = QDateTime::currentDateTime().toString(Qt::ISODate);

    QJsonArray sessions = m_db.value("sessions").toArray();
    sessions.append(session);
    m_db["sessions"] = sessions;
    saveDb();

    // Rename so it is not imported again
    QFile::rename(oldPath, oldPath + ".migrated");
}
