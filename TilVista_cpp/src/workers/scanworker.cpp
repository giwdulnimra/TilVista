#include "scanworker.h"
#include "core/pathutils.h"

#include <QDirIterator>
#include <QFileInfo>

ScanWorker::ScanWorker(const QString& directory, QObject* parent)
    : QObject(parent), m_directory(directory)
{}

void ScanWorker::run()
{
    QStringList imageFiles;
    QStringList allFiles;

    try {
        // Pass 1 – collect every file (for accurate progress)
        {
            QDirIterator it(m_directory,
                            QDir::Files | QDir::NoDotAndDotDot,
                            QDirIterator::Subdirectories);
            while (it.hasNext()) {
                it.next();
                allFiles << it.filePath();
            }
        }

        const int total = allFiles.isEmpty() ? 1 : allFiles.size();
        const auto& imgSuf = TV::imageSuffixes();

        // Pass 2 – filter images, emit progress
        for (int i = 0; i < allFiles.size(); ++i) {
            const QString& fp  = allFiles.at(i);
            const QString  ext = QFileInfo(fp).suffix().toLower().prepend('.');
            if (imgSuf.contains(ext))
                imageFiles << fp;
            emit progressChanged(static_cast<int>((i + 1) * 100 / total));
        }

        emit resultReady(true, imageFiles, allFiles);
    }
    catch (...) {
        emit resultReady(false, {}, {});
    }
}
