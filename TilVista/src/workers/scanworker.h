#ifndef TILVISTA_SCANWORKER_H
#define TILVISTA_SCANWORKER_H
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

#endif //TILLVISTA_SCANWORKER_H
