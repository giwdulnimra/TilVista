#include "shortcutstab.h"

#include <QFont>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QTableWidget>
#include <QVBoxLayout>

struct Row { const char* key; const char* description; };
struct Section { const char* title; QList<Row> rows; };

static const QList<Section> kSections = {
    {
        "Global / Main Window",
        {
            {"Ctrl+Alt+F8",        "Toggle Secret Mode (fires even when a text field has focus)"},
            {"Enter (in dir bar)", "Set directory for both tabs – same effect as Browse result"},
        }
    },
    {
        "AleaVue  –  Fullscreen Slideshow",
        {
            {"→  (Right Arrow)", "Next image (random pick)"},
            {"←  (Left Arrow)",  "Previous image (navigate history)"},
            {"Space",            "Pause / Resume auto-advance timer"},
            {"S",                "Bookmark current image → shujuko (DB2)"},
            {"Esc",              "Close slideshow, restore OS sleep prevention"},
            {"Enter",            "Reserved – no action in v0.5.x"},
        }
    },
    {
        "SattumaPic  –  Random File Picker",
        {
            {"(none)", "All interactions via mouse / buttons"},
        }
    },
    {
        "Secret Mode  (activate: Ctrl+Alt+F8)",
        {
            {"—", "Hidden kaivo entries become visible (shown with ◌ prefix, grey)"},
            {"—", "kaivo panel: '👁 Toggle Hidden' button appears"},
            {"—", "shujuko panel: '🗑 Remove from DB' button appears"},
            {"—", "Status bar icon changes to 🔓 (amber)"},
            {"—", "Shortcut can be changed in mainwindow.cpp → kSecretKeySeq"},
        }
    },
    {
        "Madoludus  –  In-Window Slideshow  (planned: v0.5.40)",
        {
            {"Space",              "Play / Pause video or slideshow"},
            {"→  /  ←",           "Next / previous file"},
            {"Shift+→ / Shift+←", "Video: jump +5 s / −5 s"},
            {"M",                  "Mute / Unmute video"},
            {"F",                  "Fast-Forward / Impression mode (1 frame/s, muted)"},
            {"—",                  "FF mode is ON by default; disable only in Secret Mode"},
            {"—",                  "Auto-advance: after video end, or after 5 s for images"},
        }
    },
};

ShortcutsTab::ShortcutsTab(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(8);

    // ── Page title ────────────────────────────────────────────────────────────
    auto* title = new QLabel("About  ·  TilVista");
    title->setStyleSheet("font-size:15px; font-weight:bold;");
    outer->addWidget(title);

#ifndef TV_APPVERSION_DISPLAY
#  define TV_APPVERSION_DISPLAY "—"
#endif
    auto* ver = new QLabel(
        QString("Version %1  ·  \u00a9 Ludwig, Armin  ·  2024\u20132025")
            .arg(TV_APPVERSION_DISPLAY));
    ver->setStyleSheet("font-size:11px; color:gray; margin-bottom:8px;");
    outer->addWidget(ver);

    // ── Scrollable shortcut tables ────────────────────────────────────────────
    auto* scroll  = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* cv      = new QVBoxLayout(content);
    cv->setSpacing(16);

    QFont mono("Courier New", 10);

    for (const Section& sec : kSections) {
        auto* secLabel = new QLabel(sec.title);
        secLabel->setStyleSheet(
            "font-weight:bold; font-size:11px;"
            "padding:3px 0; border-bottom:1px solid #555;");
        cv->addWidget(secLabel);

        auto* table = new QTableWidget(static_cast<int>(sec.rows.size()), 2);
        table->setHorizontalHeaderLabels({"Shortcut / Key", "Description"});
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
        table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

        for (int r = 0; r < sec.rows.size(); ++r) {
            auto* k = new QTableWidgetItem(sec.rows[r].key);
            auto* d = new QTableWidgetItem(sec.rows[r].description);
            k->setFont(mono);
            table->setItem(r, 0, k);
            table->setItem(r, 1, d);
        }
        table->resizeRowsToContents();
        int h = table->horizontalHeader()->height();
        for (int r = 0; r < table->rowCount(); ++r) h += table->rowHeight(r);
        table->setFixedHeight(h + 4);
        cv->addWidget(table);
    }

    cv->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
}
