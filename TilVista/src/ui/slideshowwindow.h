#pragma once
#include <QMainWindow>
#include <QStringList>

class QLabel;
class QTimer;
class ReviewDBPanel;

/// Full-screen (or windowed) random image slideshow.
///
/// Keyboard controls (v0.5.00):
///   Esc    – close
///   →      – next image
///   ←      – previous image (history)
///   Space  – pause / resume timer
///   S      – bookmark current image in ReviewDB (was Enter in v0.4.x)
///   Enter  – reserved (no action)
class SlideshowWindow : public QMainWindow
{
    Q_OBJECT
public:
    SlideshowWindow(const QString&     directory,
                    const QStringList& imagePaths,
                    const QString&     logPath,
                    bool               logErrors,
                    ReviewDBPanel*     reviewDb,
                    QWidget*           parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent (QResizeEvent* event) override;

private slots:
    void changeImage();

private:
    void displayImage(const QString& path);
    void goBack();
    void saveBookmark(const QString& path);   ///< → ReviewDBPanel::addBookmark
    void logError(const QString& path, const QString& error);

    QString     m_directory;
    QStringList m_imagePaths;
    QString     m_logPath;
    bool        m_logErrors;
    ReviewDBPanel* m_reviewDb;   ///< shared, not owned

    QLabel*  m_label  = nullptr;
    QTimer*  m_timer  = nullptr;

    QStringList m_imageOrder;
    int         m_backtrack = 0;
    QString     m_current;

    int    m_showW  = 1920;
    int    m_showH  = 1080;
    double m_ratio  = 16.0 / 9.0;

    static constexpr int kIntervalMs = 4500;
};
