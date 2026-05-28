#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>

class DBSaveWorker : public QObject {
    Q_OBJECT
public:
    DBSaveWorker(const QString& path, const QJsonObject& data, QObject* p=nullptr);
public slots: void run();
signals: void resultReady(bool ok);
private: QString m_path; QJsonObject m_data;
};

class DBLoadWorker : public QObject {
    Q_OBJECT
public:
    explicit DBLoadWorker(const QString& path, QObject* p=nullptr);
public slots: void run();
signals: void resultReady(bool ok, QJsonObject data);
private: QString m_path;
};
