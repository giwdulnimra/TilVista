#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>

// ── DBSaveWorker ─────────────────────────────────────────────────────────────
/// Writes only tilvista_dirs.json (index). Used for deletions.
class DBSaveWorker : public QObject
{
    Q_OBJECT
public:
    DBSaveWorker(const QString& dbPath,
                 const QJsonObject& data,
                 QObject* parent = nullptr);
public slots:
    void run();
signals:
    void resultReady(bool ok);
private:
    QString     m_dbPath;
    QJsonObject m_data;
};

// ── DBLoadWorker ─────────────────────────────────────────────────────────────
/// Reads tilvista_dirs.json.
class DBLoadWorker : public QObject
{
    Q_OBJECT
public:
    explicit DBLoadWorker(const QString& dbPath,
                          QObject* parent = nullptr);
public slots:
    void run();
signals:
    void resultReady(bool ok, QJsonObject data);
private:
    QString m_dbPath;
};
