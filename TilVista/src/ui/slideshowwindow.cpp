#include "slideshowwindow.h"
#include "reviewdbpanel.h"
#include "core/pathutils.h"

#include <QDir>
#include <QFile>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QPixmap>
#include <QResizeEvent>
#include <QScreen>
#include <QTextStream>
#include <QTimer>
#include <QVBoxLayout>

#include <random>

SlideshowWindow::SlideshowWindow(const QString&     directory,
                                  const QStringList& imagePaths,
                                  const QString&     logPath,
                                  bool               logErrors,
                                  ReviewDBPanel*     reviewDb,
                                  QWidget*           parent)
    : QMainWindow(parent)
    , m_directory(directory)
    , m_imagePaths(imagePaths)
    , m_logPath(logPath)
    , m_logErrors(logErrors)
    , m_reviewDb(reviewDb)
{
    setWindowTitle("TilVista · AleaVue");
    setStyleSheet("background-color: black;");
    setCursor(Qt::BlankCursor);
    TV::preventSleep();

    const QRect screen = QGuiApplication::primaryScreen()->availableGeometry();
    m_showW = screen.width();
    m_showH = screen.height();
    m_ratio = double(m_showW) / double(m_showH);

    m_label = new QLabel;
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("background-color: black;");

    auto* container = new QWidget;
    auto* layout    = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_label);
    setCentralWidget(container);

    m_timer = new QTimer(this);
    m_timer->setInterval(kIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SlideshowWindow::changeImage);
    m_timer->start();

    changeImage();
}

void SlideshowWindow::resizeEvent(QResizeEvent* event)
{
    QMainWindow::resizeEvent(event);
    m_showW = event->size().width();
    m_showH = event->size().height();
    m_ratio = double(m_showW) / double(m_showH);
}

void SlideshowWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        setCursor(Qt::ArrowCursor);
        TV::restoreSleep();
        close();
        break;
    case Qt::Key_Right:
        changeImage();
        break;
    case Qt::Key_Left:
        goBack();
        break;
    case Qt::Key_Space:
        m_timer->isActive() ? m_timer->stop() : m_timer->start(kIntervalMs);
        break;
    case Qt::Key_S:
        // v0.5.00: S = bookmark (was Enter in v0.4.x)
        saveBookmark(m_current);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // Reserved – no action in v0.5.x
        break;
    default:
        QMainWindow::keyPressEvent(event);
    }
}

void SlideshowWindow::changeImage()
{
    if (m_imagePaths.isEmpty()) return;

    if (m_backtrack >= -1) {
        m_backtrack = 0;
        std::mt19937 rng(std::random_device{}());
        std::uniform_int_distribution<int> dist(0, m_imagePaths.size() - 1);
        const QString img = m_imagePaths.at(dist(rng));
        m_imageOrder.append(img);
        displayImage(img);
    } else {
        ++m_backtrack;
        const int idx = m_imageOrder.size() + m_backtrack;
        if (idx >= 0 && idx < m_imageOrder.size())
            displayImage(m_imageOrder.at(idx));
        else {
            m_backtrack = 0;
            changeImage();
        }
    }
}

void SlideshowWindow::displayImage(const QString& path)
{
    m_current = path;
    QImageReader reader(path);
    const QSize orig = reader.size();
    if (orig.isValid() && (orig.width() > m_showW || orig.height() > m_showH)) {
        const double scale = qMin(double(m_showW) / orig.width(),
                                  double(m_showH) / orig.height());
        reader.setScaledSize(QSize(int(orig.width()  * scale),
                                   int(orig.height() * scale)));
    }
    const QImage image = reader.read();
    if (image.isNull()) {
        logError(path, reader.errorString());
        changeImage();
        return;
    }
    QPixmap pm = QPixmap::fromImage(image);
    if (double(pm.width()) / double(pm.height()) > m_ratio)
        pm = pm.scaledToWidth(m_showW, Qt::SmoothTransformation);
    else
        pm = pm.scaledToHeight(m_showH - 18, Qt::SmoothTransformation);

    m_label->setPixmap(pm);
    m_timer->stop();
    m_timer->start(kIntervalMs);
    TV::preventSleep();
}

void SlideshowWindow::goBack()
{
    if (m_imagePaths.isEmpty()) return;
    --m_backtrack;
    const int idx = m_imageOrder.size() + m_backtrack;
    if (idx >= 0 && idx < m_imageOrder.size())
        displayImage(m_imageOrder.at(idx));
    else
        ++m_backtrack;  // clamp at start
}

void SlideshowWindow::saveBookmark(const QString& path)
{
    // v0.5.10: write to ReviewDB (DB2) instead of flat .txt
    if (!path.isEmpty() && m_reviewDb)
        m_reviewDb->addBookmark(path, m_directory);
}

void SlideshowWindow::logError(const QString& path, const QString& error)
{
    if (!m_logErrors) return;
    QDir().mkpath(m_logPath);
    QFile f(m_logPath + "/loadingerrors.log");
    if (!f.open(QIODevice::Append | QIODevice::Text)) return;
    QTextStream out(&f);
    out << "Failed: " << path << "  |  " << error << '\n';
}
