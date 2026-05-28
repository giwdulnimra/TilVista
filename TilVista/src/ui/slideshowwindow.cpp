#include "slideshowwindow.h"
#include "shujukopanel.h"
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
                                  ShujukoPanel*      shujuko,
                                  QWidget*           parent)
    : QMainWindow(parent)
    , m_directory(directory), m_imagePaths(imagePaths)
    , m_logPath(logPath), m_logErrors(logErrors)
    , m_shujuko(shujuko)
{
    setWindowTitle("TilVista · AleaVue");
    setStyleSheet("background-color: black;");
    setCursor(Qt::BlankCursor);
    TV::preventSleep();

    const QRect scr = QGuiApplication::primaryScreen()->availableGeometry();
    m_showW = scr.width(); m_showH = scr.height();
    m_ratio = double(m_showW) / double(m_showH);

    m_label = new QLabel;
    m_label->setAlignment(Qt::AlignCenter);
    m_label->setStyleSheet("background-color: black;");
    auto* c = new QWidget; auto* l = new QVBoxLayout(c);
    l->setContentsMargins(0,0,0,0); l->addWidget(m_label);
    setCentralWidget(c);

    m_timer = new QTimer(this);
    m_timer->setInterval(kIntervalMs);
    connect(m_timer, &QTimer::timeout, this, &SlideshowWindow::changeImage);

    // Connect bookmark signal directly to ShujukoPanel if available
    if (m_shujuko)
        connect(this, &SlideshowWindow::bookmarkAdded,
                m_shujuko, &ShujukoPanel::addBookmark);

    m_timer->start();
    changeImage();
}

void SlideshowWindow::resizeEvent(QResizeEvent* e)
{
    QMainWindow::resizeEvent(e);
    m_showW = e->size().width(); m_showH = e->size().height();
    m_ratio = double(m_showW) / double(m_showH);
}

void SlideshowWindow::keyPressEvent(QKeyEvent* event)
{
    switch (event->key()) {
    case Qt::Key_Escape:
        setCursor(Qt::ArrowCursor); TV::restoreSleep(); close(); break;
    case Qt::Key_Right:
        changeImage(); break;
    case Qt::Key_Left:
        goBack(); break;
    case Qt::Key_Space:
        m_timer->isActive() ? m_timer->stop() : m_timer->start(kIntervalMs); break;
    case Qt::Key_S:
        // v0.5.00+: S = bookmark; signal goes to ShujukoPanel::addBookmark()
        if (!m_current.isEmpty()) emit bookmarkAdded(m_current);
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        // Reserved – no action
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
        std::uniform_int_distribution<int> d(0, m_imagePaths.size()-1);
        const QString img = m_imagePaths.at(d(rng));
        m_imageOrder.append(img);
        displayImage(img);
    } else {
        ++m_backtrack;
        const int idx = m_imageOrder.size() + m_backtrack;
        if (idx >= 0 && idx < m_imageOrder.size())
            displayImage(m_imageOrder.at(idx));
        else { m_backtrack = 0; changeImage(); }
    }
}

void SlideshowWindow::displayImage(const QString& path)
{
    m_current = path;
    QImageReader reader(path);
    const QSize orig = reader.size();
    if (orig.isValid() && (orig.width() > m_showW || orig.height() > m_showH)) {
        const double s = qMin(double(m_showW)/orig.width(),
                              double(m_showH)/orig.height());
        reader.setScaledSize(QSize(int(orig.width()*s), int(orig.height()*s)));
    }
    const QImage img = reader.read();
    if (img.isNull()) { logError(path, reader.errorString()); changeImage(); return; }
    QPixmap pm = QPixmap::fromImage(img);
    if (double(pm.width())/double(pm.height()) > m_ratio)
        pm = pm.scaledToWidth(m_showW, Qt::SmoothTransformation);
    else
        pm = pm.scaledToHeight(m_showH-18, Qt::SmoothTransformation);
    m_label->setPixmap(pm);
    m_timer->stop(); m_timer->start(kIntervalMs);
    TV::preventSleep();
}

void SlideshowWindow::goBack()
{
    if (m_imagePaths.isEmpty()) return;
    --m_backtrack;
    const int idx = m_imageOrder.size() + m_backtrack;
    if (idx >= 0 && idx < m_imageOrder.size())
        displayImage(m_imageOrder.at(idx));
    else ++m_backtrack;
}

void SlideshowWindow::logError(const QString& path, const QString& err)
{
    if (!m_logErrors) return;
    QDir().mkpath(m_logPath);
    QFile f(m_logPath + "/loadingerrors.log");
    if (!f.open(QIODevice::Append|QIODevice::Text)) return;
    QTextStream(&f) << "Failed: " << path << "  |  " << err << '\n';
}
