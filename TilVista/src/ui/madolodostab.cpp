#include "madolodostab.h"

#include <QLabel>
#include <QVBoxLayout>

MadolodosTab::MadolodosTab(QWidget* parent) : QWidget(parent)
{
    auto* lv = new QVBoxLayout(this);
    lv->setAlignment(Qt::AlignCenter);

    auto* icon = new QLabel("🎞");
    icon->setAlignment(Qt::AlignCenter);
    icon->setStyleSheet("font-size: 48px;");
    lv->addWidget(icon);

    auto* title = new QLabel("Madoludus");
    title->setAlignment(Qt::AlignCenter);
    title->setStyleSheet("font-size: 20px; font-weight: bold; margin-top: 8px;");
    lv->addWidget(title);

    auto* sub = new QLabel("In-Window Slideshow  ·  v0.5.40");
    sub->setAlignment(Qt::AlignCenter);
    sub->setStyleSheet("font-size: 12px; color: gray;");
    lv->addWidget(sub);

    auto* desc = new QLabel(
        "Planned features:\n"
        "  • Image + video playback inside the main window\n"
        "  • Semi-transparent overlay controls\n"
        "  • Space = Play/Pause\n"
        "  • ← / → = Previous / Next\n"
        "  • Shift+← / Shift+→ = Video seek ±5 s\n"
        "  • M = Mute toggle\n"
        "  • F = Fast-Forward / Impression mode (default ON)\n"
        "  • FF mode: 1 frame/s, muted – disable only in Secret Mode\n"
        "  • Auto-advance: end of video, or 5 s for images");
    desc->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    desc->setStyleSheet(
        "font-size: 11px; color: #888; margin: 16px 40px;"
        "background: transparent;");
    desc->setWordWrap(true);
    lv->addWidget(desc);
}
