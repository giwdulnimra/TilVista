#include "catalogueworkers.h"
#include "core/pathutils.h"
#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QTextStream>

CatalogueWriteWorker::CatalogueWriteWorker(
    const QString& kd, const QString& db, const QJsonObject& data,
    const QString& name, const QStringList& img, const QStringList& all, QObject* p)
    : QObject(p),m_kaivoDir(kd),m_dbPath(db),m_dbData(data),
      m_entryName(name),m_imageFiles(img),m_allFiles(all) {}

void CatalogueWriteWorker::run() {
    QDir().mkpath(m_kaivoDir);
    const QString safe=TV::safeFilename(m_entryName);
    const QString imgF=safe+"_img.dshow", allF=safe+"_all.catalogue";
    auto write=[&](const QString& fn, const QStringList& lst)->bool{
        QFile f(m_kaivoDir+'/'+fn);
        if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate)) return false;
        QTextStream out(&f);
        for(const QString& p:lst) out<<p<<'\n';
        return true;
    };
    if(!write(imgF,m_imageFiles)||!write(allF,m_allFiles)){emit resultReady(false);return;}
    QJsonArray entries=m_dbData.value("entries").toArray();
    for(int i=0;i<entries.size();++i){
        QJsonObject e=entries[i].toObject();
        if(e.value("name").toString()==m_entryName){
            e["image_files"]=imgF; e["all_files"]=allF; entries[i]=e; break;
        }
    }
    QJsonObject db=m_dbData; db["entries"]=entries;
    QFile f(m_dbPath);
    if(!f.open(QIODevice::WriteOnly|QIODevice::Truncate)){emit resultReady(false);return;}
    f.write(QJsonDocument(db).toJson(QJsonDocument::Indented));
    emit resultReady(true);
}

CatalogueLoadWorker::CatalogueLoadWorker(const QString& kd,
    const QString& i, const QString& a, QObject* p)
    : QObject(p),m_kaivoDir(kd),m_imgFname(i),m_allFname(a) {}

QStringList CatalogueLoadWorker::readLines(const QString& path) {
    QStringList r; QFile f(path);
    if(!f.open(QIODevice::ReadOnly|QIODevice::Text)) return r;
    QTextStream in(&f);
    while(!in.atEnd()){ const QString l=in.readLine().trimmed(); if(!l.isEmpty()) r<<l; }
    return r;
}

void CatalogueLoadWorker::run() {
    const QStringList img=readLines(m_kaivoDir+'/'+m_imgFname);
    const QStringList all=readLines(m_kaivoDir+'/'+m_allFname);
    emit resultReady(!img.isEmpty()||!all.isEmpty(), img, all);
}
