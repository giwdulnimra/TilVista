#include "catalogueworkers.h"
#include "core/pathutils.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTextStream>

// ── CatalogueWriteWorker ─────────────────────────────────────────────────────

CatalogueWriteWorker::CatalogueWriteWorker(const QString&     kaivoDir,
                                           const QString&     dbPath,
                                           const QJsonObject& dbData,
                                           const QString&     entryName,
                                           const QStringList& imageFiles,
                                           const QStringList& allFiles,
                                           QObject* parent)
    : QObject(parent)
    , m_kaivoDir(kaivoDir)
    , m_dbPath(dbPath)
    , m_dbData(dbData)
    , m_entryName(entryName)
    , m_imageFiles(imageFiles)
    , m_allFiles(allFiles)
{}

void CatalogueWriteWorker::run()
{
    QDir().mkpath(m_kaivoDir);

    const QString safe     = TV::safeFilename(m_entryName);
    const QString imgFname = safe + "_img.dshow";
    const QString allFname = safe + "_all.catalogue";

    // ── Write .dshow (image paths) ───────────────────────────────────────────
    {
        QFile f(m_kaivoDir + '/' + imgFname);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit resultReady(false); return;
        }
        QTextStream out(&f);
        for (const QString& p : m_imageFiles) out << p << '\n';
    }

    // ── Write .catalogue (all paths) ─────────────────────────────────────────
    {
        QFile f(m_kaivoDir + '/' + allFname);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit resultReady(false); return;
        }
        QTextStream out(&f);
        for (const QString& p : m_allFiles) out << p << '\n';
    }

    // ── Patch JSON entry → reference filenames, not inline lists ────────────
    QJsonArray entries = m_dbData.value("entries").toArray();
    for (int i = 0; i < entries.size(); ++i) {
        QJsonObject entry = entries[i].toObject();
        if (entry.value("name").toString() == m_entryName) {
            entry["image_files"] = imgFname;
            entry["all_files"]   = allFname;
            entries[i] = entry;
            break;
        }
    }
    m_dbData["entries"] = entries;

    // ── Write updated JSON ───────────────────────────────────────────────────
    {
        QFile f(m_dbPath);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            emit resultReady(false); return;
        }
        f.write(QJsonDocument(m_dbData).toJson(QJsonDocument::Indented));
    }

    emit resultReady(true);
}

// ── CatalogueLoadWorker ──────────────────────────────────────────────────────

CatalogueLoadWorker::CatalogueLoadWorker(const QString& kaivoDir,
                                         const QString& imgFname,
                                         const QString& allFname,
                                         QObject* parent)
    : QObject(parent)
    , m_kaivoDir(kaivoDir)
    , m_imgFname(imgFname)
    , m_allFname(allFname)
{}

QStringList CatalogueLoadWorker::readLines(const QString& path)
{
    QStringList result;
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return result;
    QTextStream in(&f);
    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (!line.isEmpty())
            result << line;
    }
    return result;
}

void CatalogueLoadWorker::run()
{
    const QStringList img = readLines(m_kaivoDir + '/' + m_imgFname);
    const QStringList all = readLines(m_kaivoDir + '/' + m_allFname);
    emit resultReady(!img.isEmpty() || !all.isEmpty(), img, all);
}
