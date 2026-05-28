#include "dbworkers.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>

DBSaveWorker::DBSaveWorker(const QString& p, const QJsonObject& d, QObject* par)
    : QObject(par), m_path(p), m_data(d) {}
void DBSaveWorker::run() {
    QDir().mkpath(QFileInfo(m_path).absolutePath());
    QFile f(m_path);
    if (!f.open(QIODevice::WriteOnly|QIODevice::Truncate)) { emit resultReady(false); return; }
    f.write(QJsonDocument(m_data).toJson(QJsonDocument::Indented));
    emit resultReady(true);
}

DBLoadWorker::DBLoadWorker(const QString& p, QObject* par) : QObject(par), m_path(p) {}
void DBLoadWorker::run() {
    QFile f(m_path);
    if (!f.open(QIODevice::ReadOnly)) { emit resultReady(false,{}); return; }
    QJsonParseError e;
    auto doc = QJsonDocument::fromJson(f.readAll(), &e);
    if (e.error != QJsonParseError::NoError || !doc.isObject()) { emit resultReady(false,{}); return; }
    emit resultReady(true, doc.object());
}
