#pragma once
#include <QObject>
#include <QString>
#include <QStringList>

/// Recursively scans *directory*.
/// Emits both image paths and all-file paths for dual caching.
class ScanWorker : public QObject
{
    Q_OBJECT
public:
    explicit ScanWorker(const QString& directory, QObject* parent = nullptr);

public slots:
    void run();

signals:
    void progressChanged(int percent);           ///< 0–100
    void resultReady(bool ok,
                     QStringList imageFiles,
                     QStringList allFiles);

private:
    QString m_directory;
};
