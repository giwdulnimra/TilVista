#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

class CatalogueWriteWorker : public QObject {
    Q_OBJECT
public:
    CatalogueWriteWorker(const QString& kaivoDir, const QString& dbPath,
        const QJsonObject& dbData, const QString& entryName,
        const QStringList& imageFiles, const QStringList& allFiles,
        QObject* p=nullptr);
public slots: void run();
signals: void resultReady(bool ok);
private:
    QString m_kaivoDir, m_dbPath, m_entryName;
    QJsonObject m_dbData;
    QStringList m_imageFiles, m_allFiles;
};

class CatalogueLoadWorker : public QObject {
    Q_OBJECT
public:
    CatalogueLoadWorker(const QString& kaivoDir,
        const QString& imgFname, const QString& allFname, QObject* p=nullptr);
public slots: void run();
signals: void resultReady(bool ok, QStringList imageFiles, QStringList allFiles);
private:
    QString m_kaivoDir, m_imgFname, m_allFname;
    static QStringList readLines(const QString& path);
};
