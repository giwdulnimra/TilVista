#include "shortcutstab.h"

#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

// ── Shortcut data ─────────────────────────────────────────────────────────────

struct Row { const char* key; const char* description; };

struct Section {
    const char*        title;
    QList<Row>         rows;
};

static const QList<Section> kSections = {
    {
        "Global / Main Window",
        {
            {"Ctrl+Alt+F8",        "Toggle Secret Mode"},
            {"Enter (in dir bar)", "Set directory for both tabs (same as Browse result)"},
        }
    },
    {
        "AleaVue  –  Slideshow Fullscreen",
        {
            {"→  (Right)",  "Next image (random)"},
            {"←  (Left)",   "Previous image (history)"},
            {"Space",       "Pause / Resume auto-advance"},
            {"S",           "Bookmark current image → shujuko (DB2)"},
            {"Esc",         "Close slideshow, restore sleep prevention"},
            {"Enter",       "Reserved (no action)"},
        }
    },
    {
        "SattumaPic  –  Random File Picker",
        {
            {"(none)",  "All actions via buttons or shujuko panel"},
        }
    },
    {
        "Secret Mode  (Ctrl+Alt+F8 to toggle)",
        {
            {"—",  "Hidden kaivo entries become visible (shown with ◌ prefix)"},
            {"—",  "DB panel: '👁 Toggle Hidden' button appears"},
            {"—",  "ShujukoPanel: '🗑 Remove from DB' button appears"},
            {"—",  "Lock icon in status bar changes to 🔓 (amber)"},
        }
    },
    {
        "Madoludus  –  In-Window Slideshow  (v0.5.40, not yet active)",
        {
            {"Space",             "Play / Pause video or slideshow"},
            {"→  /  ←",          "Next / previous file"},
            {"Shift+→ / Shift+←","Video only: jump +5 / −5 seconds"},
            {"M",                 "Mute / unmute video"},
            {"F",                 "Fast-Forward mode (1 frame/s, muted) – default ON"},
            {"—",                 "FF mode can only be disabled in Secret Mode"},
            {"—",                 "Auto-advance: next file after video ends or 5 s for images"},
        }
    },
};

// ── Widget ────────────────────────────────────────────────────────────────────

ShortcutsTab::ShortcutsTab(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(8);

    // Title
    auto* title = new QLabel("Keyboard Shortcuts");
    title->setStyleSheet("font-size: 14px; font-weight: bold;");
    outer->addWidget(title);

    // Scrollable content area
    auto* scroll  = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* cv      = new QVBoxLayout(content);
    cv->setSpacing(16);

    for (const Section& sec : kSections) {
        // Section header
        auto* secLabel = new QLabel(sec.title);
        secLabel->setStyleSheet(
            "font-weight: bold; font-size: 11px;"
            "padding: 3px 0; border-bottom: 1px solid #555;");
        cv->addWidget(secLabel);

        // Table for this section
        auto* table = new QTableWidget(
            static_cast<int>(sec.rows.size()), 2);
        table->setHorizontalHeaderLabels({"Shortcut", "Description"});
        table->horizontalHeader()->setStretchLastSection(true);
        table->horizontalHeader()->setSectionResizeMode(
            0, QHeaderView::ResizeToContents);
        table->verticalHeader()->setVisible(false);
        table->setEditTriggers(QAbstractItemView::NoEditTriggers);
        table->setSelectionMode(QAbstractItemView::NoSelection);
        table->setShowGrid(false);
        table->setAlternatingRowColors(true);
        table->setFocusPolicy(Qt::NoFocus);
        table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
        // Don't show scroll bars – table expands to full height
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        QFont mono("Courier New", 10);

        for (int r = 0; r < sec.rows.size(); ++r) {
            auto* keyItem  = new QTableWidgetItem(sec.rows[r].key);
            auto* descItem = new QTableWidgetItem(sec.rows[r].description);
            keyItem->setFont(mono);
            table->setItem(r, 0, keyItem);
            table->setItem(r, 1, descItem);
        }
        table->resizeRowsToContents();

        // Fix height so the outer scroll area handles scrolling, not the table
        int h = table->horizontalHeader()->height();
        for (int r = 0; r < table->rowCount(); ++r)
            h += table->rowHeight(r);
        table->setFixedHeight(h + 4);

        cv->addWidget(table);
    }

    cv->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
}
