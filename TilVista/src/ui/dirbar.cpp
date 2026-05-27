#include "dirbar.h"

#include <QDir>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

DirBar::DirBar(QWidget* parent) : QWidget(parent)
{
    auto* layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 4, 0, 4);

    m_lineEdit = new QLineEdit;
    m_lineEdit->setPlaceholderText("Select or type a directory path…");
    // v0.5.00: Enter commits the path for BOTH tabs, same as Browse
    connect(m_lineEdit, &QLineEdit::returnPressed,
            this, &DirBar::onEnter);

    m_btnBrowse = new QPushButton("Browse");
    m_btnBrowse->setFixedWidth(70);
    connect(m_btnBrowse, &QPushButton::clicked, this, &DirBar::onBrowse);

    // v0.5.00: Load button moved from AleaVueTab into DirBar
    m_btnLoad = new QPushButton("Load from Directory");
    connect(m_btnLoad, &QPushButton::clicked,
            this, &DirBar::loadRequested);

    layout->addWidget(m_lineEdit);
    layout->addWidget(m_btnBrowse);
    layout->addWidget(m_btnLoad);
}

QString DirBar::directory() const
{
    return m_lineEdit->text().trimmed();
}

void DirBar::setDirectory(const QString& path)
{
    m_lineEdit->setText(path);
}

void DirBar::onBrowse()
{
    const QString path = QFileDialog::getExistingDirectory(
        this, "Select Directory", directory());
    if (!path.isEmpty()) {
        setDirectory(path);
        emit directoryChanged(path);
    }
}

void DirBar::onEnter()
{
    const QString path = directory();
    if (QDir(path).exists())
        emit directoryChanged(path);
    // Note: loadRequested is NOT emitted here – Enter just sets the active
    // directory for both tabs. The user must explicitly click "Load from
    // Directory" to trigger a scan.
}
