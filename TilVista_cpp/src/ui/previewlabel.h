#pragma once
#include <QLabel>
#include <QPixmap>

/// A QLabel that scales its pixmap to fill all available space
/// while keeping the aspect ratio. No fixed size needed.
class PreviewLabel : public QLabel
{
    Q_OBJECT
public:
    explicit PreviewLabel(QWidget* parent = nullptr);

    void setSourcePixmap(const QPixmap& pm);
    void clearPreview();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void repaintScaled();

    QPixmap m_source;
};
