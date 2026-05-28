#pragma once
#include <QMainWindow>
#include <QStringList>

class QLabel;
class QTimer;
class ShujukoPanel;

/// Full-screen (or windowed) random image slideshow.
///
/// S key emits bookmarkAdded(path) which ShujukoPanel::addBookmark() handles.
/// Enter key is reserved (no action).
class SlideshowWindow : public QMainWindow
{
    Q_OBJECT
public:
    SlideshowWindow(const QString&     directory,
                    const QStringList& imagePaths,
                    const QString&     logPath,
                    bool               logErrors,
                    ShujukoPanel*      shujuko,
                    QWidget*           parent = nullptr);

signals:
    void bookmarkAdded(const QString& path);

protected:
    void keyPressEvent(QKeyEvent* event) override;
    void resizeEvent (QResizeEvent* event) override;

private slots:
    void changeImage();

private:
    void displayImage(const QString& path);
    void goBack();
    void logError(const QString& path, const QString& error);

    QString     m_directory;
    QStringList m_imagePaths;
    QString     m_logPath;
    bool        m_logErrors;
    ShujukoPanel* m_shujuko;

    QLabel* m_label = nullptr;
    QTimer* m_timer = nullptr;

    QStringList m_imageOrder;
    int         m_backtrack = 0;
    QString     m_current;

    int    m_showW = 1920, m_showH = 1080;
    double m_ratio = 16.0 / 9.0;
    static constexpr int kIntervalMs = 4500;
};
