#pragma once
#include <QMainWindow>
#include <QStringList>

class QLabel;
class QTimer;

/// Full-screen (or windowed) random image slideshow.
///
/// Keyboard controls:
///   Esc    – close
///   →      – next image
///   ←      – previous image
///   Space  – pause / resume timer
///   Enter  – bookmark current image to optegnelser/reviewpics.txt
class SlideshowWindow : public QMainWindow
{
    Q_OBJECT
public:
    SlideshowWindow(const QString&     directory,
                    const QStringList& imagePaths,
                    const QString&     logPath,
                    bool               logErrors,
                    QWidget*           parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent (QResizeEvent* event) override;

private slots:
    void changeImage();

private:
    void displayImage(const QString& path);
    void goBack();
    void saveBookmark(const QString& path);
    void logError(const QString& path, const QString& error);

    QString     m_directory;
    QStringList m_imagePaths;
    QString     m_logPath;
    bool        m_logErrors;

    QLabel*  m_label  = nullptr;
    QTimer*  m_timer  = nullptr;

    QStringList m_imageOrder;   ///< history of displayed images
    int         m_backtrack = 0; ///< negative = navigating back in history
    QString     m_current;

    int    m_showW  = 1920;
    int    m_showH  = 1080;
    double m_ratio  = 16.0 / 9.0;

    static constexpr int kIntervalMs = 4500;
};
