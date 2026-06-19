#include "madooverlay.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QPainter>
#include <QPushButton>

static QPushButton* makeBtn(const QString& text, QWidget* parent)
{
    auto* b = new QPushButton(text, parent);
    b->setFixedSize(40, 40);
    b->setFlat(true);
    b->setStyleSheet(
        "QPushButton {"
        "  font-size: 18px;"
        "  color: white;"
        "  background: transparent;"
        "  border: none;"
        "  border-radius: 6px;"
        "}"
        "QPushButton:hover {"
        "  background: rgba(255,255,255,40);"
        "}"
        "QPushButton:pressed {"
        "  background: rgba(255,255,255,70);"
        "}");
    return b;
}

MadoOverlay::MadoOverlay(QWidget* parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, false);
    setMouseTracking(true);

    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(12, 6, 12, 6);
    layout->setSpacing(4);

    m_btnPrev = makeBtn("\u23ee", this);   // ⏮
    m_btnPlay = makeBtn("\u23f8", this);   // ⏸ (updated dynamically)
    m_btnNext = makeBtn("\u23ed", this);   // ⏭
    m_btnMute = makeBtn("\U0001f507", this); // 🔇 (updated dynamically)
    m_btnFF   = makeBtn("FF", this);
    m_btnFF->setStyleSheet(
        m_btnFF->styleSheet() +
        "QPushButton { font-size: 12px; font-weight: bold; color: #e8a000; }");

    // v0.5.42: shuffle / random-order toggle (Madoludus "Random selection")
    m_btnShuffle = makeBtn("\U0001f500", this);   // 🔀
    m_btnShuffle->setToolTip("Random order OFF (R) – sequential order");

    m_lblInfo = new QLabel(this);
    m_lblInfo->setStyleSheet("color: rgba(255,255,255,200); font-size: 11px;");
    m_lblInfo->setAlignment(Qt::AlignCenter);

    layout->addWidget(m_btnPrev);
    layout->addWidget(m_btnPlay);
    layout->addWidget(m_btnNext);
    layout->addSpacing(8);
    layout->addWidget(m_btnMute);
    layout->addWidget(m_btnFF);
    layout->addWidget(m_btnShuffle);
    layout->addStretch();
    layout->addWidget(m_lblInfo);

    connect(m_btnPlay,    &QPushButton::clicked, this, &MadoOverlay::playPauseClicked);
    connect(m_btnPrev,    &QPushButton::clicked, this, &MadoOverlay::prevClicked);
    connect(m_btnNext,    &QPushButton::clicked, this, &MadoOverlay::nextClicked);
    connect(m_btnMute,    &QPushButton::clicked, this, &MadoOverlay::muteClicked);
    connect(m_btnFF,      &QPushButton::clicked, this, &MadoOverlay::ffClicked);
    connect(m_btnShuffle, &QPushButton::clicked, this, &MadoOverlay::shuffleClicked);
}

void MadoOverlay::setPaused(bool paused)
{
    m_btnPlay->setText(paused ? "\u23f5" : "\u23f8");  // ▶ or ⏸
}

void MadoOverlay::setMuted(bool muted)
{
    m_btnMute->setText(muted
        ? "\U0001f507"    // 🔇
        : "\U0001f50a");  // 🔊
}

void MadoOverlay::setFFMode(bool ff)
{
    m_btnFF->setStyleSheet(
        "QPushButton {"
        "  font-size: 12px; font-weight: bold;"
        "  color: " + QString(ff ? "#e8a000" : "rgba(255,255,255,120)") + ";"
        "  background: transparent; border: none; border-radius: 6px;"
        "}"
        "QPushButton:hover { background: rgba(255,255,255,40); }");
}

void MadoOverlay::setRandomMode(bool on)
{
    m_btnShuffle->setStyleSheet(
        "QPushButton {"
        "  font-size: 16px;"
        "  color: " + QString(on ? "#4fc3f7" : "rgba(255,255,255,120)") + ";"
        "  background: transparent; border: none; border-radius: 6px;"
        "}"
        "QPushButton:hover { background: rgba(255,255,255,40); }");
    m_btnShuffle->setToolTip(on
        ? "Random order ON (R) – Next picks a random file"
        : "Random order OFF (R) – sequential order");
}

void MadoOverlay::setFileInfo(int index, int total, const QString& name)
{
    if (total > 0)
        m_lblInfo->setText(
            QString("%1 / %2  ·  %3").arg(index + 1).arg(total).arg(name));
    else
        m_lblInfo->clear();
}

void MadoOverlay::setSecretMode(bool on)
{
    // FF button is always shown in Madoludus (unlike shujuko delete button)
    // but in secret mode it gets a tooltip explaining it can be toggled
    m_btnFF->setToolTip(on
        ? "FF/Impression mode (F key) – Secret Mode: can toggle OFF"
        : "FF/Impression mode (F key) – disable requires Secret Mode");
}

void MadoOverlay::paintEvent(QPaintEvent*)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    // Semi-transparent dark pill at the bottom
    p.setBrush(QColor(0, 0, 0, 160));
    p.setPen(Qt::NoPen);
    p.drawRoundedRect(rect().adjusted(4, 4, -4, -4), 10, 10);
}
