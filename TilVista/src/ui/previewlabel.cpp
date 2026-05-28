#include "previewlabel.h"
#include <QResizeEvent>
#include <QSizePolicy>

PreviewLabel::PreviewLabel(QWidget* p) : QLabel(p) {
    setAlignment(Qt::AlignCenter);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setMinimumSize(60,60);
    setStyleSheet("border:1px solid #555;background:#1a1a1a;border-radius:4px;");
}
void PreviewLabel::setSourcePixmap(const QPixmap& pm){ m_source=pm; repaintScaled(); }
void PreviewLabel::clearPreview(){ m_source=QPixmap(); clear(); }
void PreviewLabel::resizeEvent(QResizeEvent* e){ QLabel::resizeEvent(e); repaintScaled(); }
void PreviewLabel::repaintScaled(){
    if(m_source.isNull()) return;
    setPixmap(m_source.scaled(size()-QSize(8,8),
        Qt::KeepAspectRatio, Qt::SmoothTransformation));
}
