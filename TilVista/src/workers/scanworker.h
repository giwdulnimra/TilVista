#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

class ScanWorker : public QObject {
    Q_OBJECT
public:
    explicit ScanWorker(const QString& directory, QObject* parent = nullptr);
public slots:
    void run();
signals:
    void progressChanged(int percent);
    void resultReady(bool ok, QStringList imageFiles, QStringList allFiles);
private:
    QString m_directory;
};
