#pragma once
#include <QWidget>

class QPushButton;
class QLabel;

/// Semi-transparent floating control bar for Madoludus.
/// Positioned at the bottom of the display area by the parent.
/// Parent calls show()/hide() and updateButtons() as needed.
class MadoOverlay : public QWidget
{
    Q_OBJECT
public:
    explicit MadoOverlay(QWidget* parent = nullptr);

    void setPaused(bool paused);
    void setMuted(bool muted);
    void setFFMode(bool ff);
    void setFileInfo(int index, int total, const QString& name);
    void setSecretMode(bool on);   ///< show/hide FF button

signals:
    void playPauseClicked();
    void prevClicked();
    void nextClicked();
    void muteClicked();
    void ffClicked();

protected:
    void paintEvent(QPaintEvent* event) override;

private:
    QPushButton* m_btnPrev    = nullptr;
    QPushButton* m_btnPlay    = nullptr;
    QPushButton* m_btnNext    = nullptr;
    QPushButton* m_btnMute    = nullptr;
    QPushButton* m_btnFF      = nullptr;
    QLabel*      m_lblInfo    = nullptr;
};
