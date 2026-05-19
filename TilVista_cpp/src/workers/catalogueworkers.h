#pragma once
#include <QJsonObject>
#include <QObject>
#include <QString>
#include <QStringList>

// ── CatalogueWriteWorker ─────────────────────────────────────────────────────
/// Writes <name>_img.dshow, <name>_all.catalogue, then updates
/// tilvista_dirs.json so that the entry references the two filenames.
class CatalogueWriteWorker : public QObject
{
    Q_OBJECT
public:
    CatalogueWriteWorker(const QString&     kaivoDir,
                         const QString&     dbPath,
                         const QJsonObject& dbData,
                         const QString&     entryName,
                         const QStringList& imageFiles,
                         const QStringList& allFiles,
                         QObject* parent = nullptr);
public slots:
    void run();
signals:
    void resultReady(bool ok);

private:
    QString     m_kaivoDir;
    QString     m_dbPath;
    QJsonObject m_dbData;     ///< full DB, will be mutated in run()
    QString     m_entryName;
    QStringList m_imageFiles;
    QStringList m_allFiles;
};

// ── CatalogueLoadWorker ──────────────────────────────────────────────────────
/// Reads <name>_img.dshow and <name>_all.catalogue from kaivoDir.
class CatalogueLoadWorker : public QObject
{
    Q_OBJECT
public:
    CatalogueLoadWorker(const QString& kaivoDir,
                        const QString& imgFname,
                        const QString& allFname,
                        QObject* parent = nullptr);
public slots:
    void run();
signals:
    void resultReady(bool ok, QStringList imageFiles, QStringList allFiles);

private:
    QString m_kaivoDir;
    QString m_imgFname;
    QString m_allFname;

    static QStringList readLines(const QString& path);
};
