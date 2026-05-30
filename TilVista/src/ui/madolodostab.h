#pragma once
#include <QWidget>

/// Tab 4 – Madoludus (in-window slideshow, v0.5.40 placeholder).
/// "Madoludus" – invented portmanteau from Esperanto "madon" (window/frame)
/// + Latin "ludus" (play/game). Displays a placeholder until v0.5.40.
class MadolodosTab : public QWidget
{
    Q_OBJECT
public:
    explicit MadolodosTab(QWidget* parent = nullptr);
};
