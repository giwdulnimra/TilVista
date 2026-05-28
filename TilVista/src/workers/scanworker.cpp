#include "scanworker.h"
#include "core/pathutils.h"
#include <QDirIterator>
#include <QFileInfo>

ScanWorker::ScanWorker(const QString& directory, QObject* parent)
    : QObject(parent), m_directory(directory) {}

void ScanWorker::run() {
    QStringList all, img;
    try {
        QDirIterator it(m_directory, QDir::Files|QDir::NoDotAndDotDot,
                        QDirIterator::Subdirectories);
        while (it.hasNext()) { it.next(); all << it.filePath(); }
        const int total = all.isEmpty() ? 1 : all.size();
        const auto& suf = TV::imageSuffixes();
        for (int i = 0; i < all.size(); ++i) {
            if (suf.contains('.' + QFileInfo(all[i]).suffix().toLower()))
                img << all[i];
            emit progressChanged((i+1)*100/total);
        }
        emit resultReady(true, img, all);
    } catch (...) { emit resultReady(false, {}, {}); }
}
