#include "previewlabel.h"

#include <QResizeEvent>
#include <QSizePolicy>

PreviewLabel::PreviewLabel(QWidget* parent)
    : QLabel(parent)
{
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(60, 60);
    setStyleSheet(
        "border: 1px solid #555;"
        "background-color: #1a1a1a;"
        "border-radius: 4px;");
}

void PreviewLabel::setSourcePixmap(const QPixmap& pm)
{
    m_source = pm;
    repaintScaled();
}

void PreviewLabel::clearPreview()
{
    m_source = QPixmap();
    clear();
}

void PreviewLabel::resizeEvent(QResizeEvent* event)
{
    QLabel::resizeEvent(event);
    repaintScaled();
}

void PreviewLabel::repaintScaled()
{
    if (m_source.isNull()) return;
    const QSize target = size() - QSize(8, 8);
    QPixmap scaled = m_source.scaled(
        target,
        Qt::KeepAspectRatio,
        Qt::SmoothTransformation);
    setPixmap(scaled);
}
