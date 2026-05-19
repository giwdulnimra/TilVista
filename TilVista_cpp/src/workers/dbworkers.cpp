#include "dbworkers.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>

// ── DBSaveWorker ─────────────────────────────────────────────────────────────

DBSaveWorker::DBSaveWorker(const QString& dbPath,
                           const QJsonObject& data,
                           QObject* parent)
    : QObject(parent), m_dbPath(dbPath), m_data(data)
{}

void DBSaveWorker::run()
{
    QFileInfo fi(m_dbPath);
    QDir().mkpath(fi.absolutePath());

    QFile f(m_dbPath);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        emit resultReady(false);
        return;
    }
    f.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    emit resultReady(true);
}

// ── DBLoadWorker ─────────────────────────────────────────────────────────────

DBLoadWorker::DBLoadWorker(const QString& dbPath, QObject* parent)
    : QObject(parent), m_dbPath(dbPath)
{}

void DBLoadWorker::run()
{
    QFile f(m_dbPath);
    if (!f.open(QIODevice::ReadOnly)) {
        emit resultReady(false, {});
        return;
    }
    QJsonParseError err;
    auto doc = QJsonDocument::fromJson(f.readAll(), &err);
    if (err.error != QJsonParseError::NoError || !doc.isObject()) {
        emit resultReady(false, {});
        return;
    }
    emit resultReady(true, doc.object());
}
