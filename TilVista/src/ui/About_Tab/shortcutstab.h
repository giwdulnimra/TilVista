#pragma once
#include <QWidget>

/// Tab 3 – keyboard shortcut reference, grouped by functional area.
/// Includes secret mode shortcuts (always shown – obscurity ≠ security).
class ShortcutsTab : public QWidget
{
    Q_OBJECT
public:
    explicit ShortcutsTab(QWidget* parent = nullptr);
};
