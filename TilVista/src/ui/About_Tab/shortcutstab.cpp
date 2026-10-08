#include "shortcutstab.h"

#include <QFont>
#include <QFrame>
#include <QHeaderView>
#include <QLabel>
#include <QScrollArea>
#include <QSizePolicy>
#include <QTableWidget>
#include <QVBoxLayout>

#ifndef TV_APPVERSION_DISPLAY
#  define TV_APPVERSION_DISPLAY "—"
#endif
#ifndef TV_SEMVER
#  define TV_SEMVER "—"
#endif

struct Row    { const char* key; const char* description; };
struct Section{ const char* title; QList<Row> rows; };

static const QList<Section> kSections = {
    {
        "Global",
        {
            {"Ctrl+Alt+F8",          "Toggle Secret Mode\n"
                                     "(fires even when a text field has keyboard focus)"},
            {"Enter  (in DirBar)",   "Commit path for both tabs  –  same effect as Browse"},
        }
    },
    {
        "AleaVue  –  Fullscreen Slideshow",
        {
            {"\u2192  (Right Arrow)",  "Next image  (random pick)"},
            {"\u2190  (Left Arrow)",   "Previous image  (navigate history)"},
            {"Space",                  "Pause / Resume auto-advance timer"},
            {"S",                      "Bookmark current image \u2192 shujuko (DB2)"},
            {"Esc",                    "Close slideshow, restore OS sleep prevention"},
            {"Enter",                  "Reserved \u2013 no action in current version"},
        }
    },
    {
        "Madoludus  –  In-Window Slideshow",
        {
            {"Space",                     "Play / Pause"},
            {"\u2192  /  \u2190",         "Next / Previous file (history-aware,\n"
                                          "works in random order too)"},
            {"Shift+\u2192 / Shift+\u2190","Video (direct playback only): seek +5 s / \u22125 s"},
            {"M",                          "Mute / Unmute video"},
            {"F",                          "Toggle FF / Impression mode\n"
                                           "Disabling FF requires Secret Mode"},
            {"R",                          "Toggle random / sequential file order  (v0.5.42)"},
        }
    },
    {
        "SattumaPic  –  Random File Picker",
        {
            {"(none)", "All interactions via mouse / buttons"},
        }
    },
    {
        "Secret Mode  \u2013  activate with Ctrl+Alt+F8",
        {
            {"\u2014", "Hidden kaivo entries become visible  (\u25cc prefix, grey)"},
            {"\u2014", "kaivo panel:  \"\U0001f441 Toggle Hidden\"  button appears"},
            {"\u2014", "shujuko panel:  \"\U0001f5d1 Remove from DB\"  button appears"},
            {"\u2014", "Madoludus: FF mode can be disabled via F key"},
            {"\u2014", "Status bar icon changes to \U0001f513 (amber)"},
            {"\u2014", "Shortcut configurable in mainwindow.cpp  \u2192  kSecretKeySeq"},
        }
    },
};

// ── Build a section table fixed to its content height ─────────────────────────
static QTableWidget* makeTable(const Section& sec)
{
    auto* table = new QTableWidget(static_cast<int>(sec.rows.size()), 2);
    table->setHorizontalHeaderLabels({"Shortcut / Key", "Description"});
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table->verticalHeader()->setVisible(false);
    table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table->setSelectionMode(QAbstractItemView::NoSelection);
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setFocusPolicy(Qt::NoFocus);
    table->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Minimum);
    table->setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    table->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    QFont mono("Courier New", 10);
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
    return table;
}

// ── Widget ─────────────────────────────────────────────────────────────────────
ShortcutsTab::ShortcutsTab(QWidget* parent) : QWidget(parent)
{
    auto* outer = new QVBoxLayout(this);
    outer->setContentsMargins(12, 12, 12, 12);
    outer->setSpacing(6);

    // ── Title block ────────────────────────────────────────────────────────────
    auto* title = new QLabel("About  \u00b7  TilVista");
    title->setStyleSheet("font-size:15px; font-weight:bold;");
    outer->addWidget(title);

    auto* ver = new QLabel(
        QString("Version %1  \u00b7  \u00a9 Ludwig, Armin  \u00b7  2024\u20132025")
            .arg(TV_APPVERSION_DISPLAY));
    ver->setStyleSheet("font-size:11px; color:gray; margin-bottom:4px;");
    outer->addWidget(ver);

    auto* desc = new QLabel(
        "TilVista (norw. tilfeldig + span. vista)\n"
        "Sub-tools:  AleaVue  \u00b7  SattumaPic  \u00b7  Madoludus\n"
        "DB: kaivo (directory store)  \u00b7  shujuko (bookmark store)");
    desc->setStyleSheet("font-size:10px; color:#888; margin-bottom:8px;");
    outer->addWidget(desc);

    auto* sep = new QFrame;
    sep->setFrameShape(QFrame::HLine);
    sep->setStyleSheet("color: #555;");
    outer->addWidget(sep);

    // ── Scrollable shortcut tables ─────────────────────────────────────────────
    auto* scroll  = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto* content = new QWidget;
    auto* cv      = new QVBoxLayout(content);
    cv->setSpacing(14);

    for (const Section& sec : kSections) {
        auto* lbl = new QLabel(sec.title);
        lbl->setStyleSheet(
            "font-weight:bold; font-size:11px;"
            "padding:3px 0; border-bottom:1px solid #555;");
        cv->addWidget(lbl);
        cv->addWidget(makeTable(sec));
    }
    cv->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll);
}
