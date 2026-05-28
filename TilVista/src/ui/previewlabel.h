#pragma once
#include <QLabel>
#include <QPixmap>

class PreviewLabel : public QLabel {
    Q_OBJECT
public:
    explicit PreviewLabel(QWidget* parent=nullptr);
    void setSourcePixmap(const QPixmap& pm);
    void clearPreview();
protected:
    void resizeEvent(QResizeEvent* e) override;
private:
    void repaintScaled();
    QPixmap m_source;
};
