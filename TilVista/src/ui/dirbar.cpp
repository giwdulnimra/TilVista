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
    connect(m_lineEdit, &QLineEdit::editingFinished,
            this, &DirBar::onTextCommitted);

    m_btnBrowse = new QPushButton("Browse");
    m_btnBrowse->setFixedWidth(80);
    connect(m_btnBrowse, &QPushButton::clicked, this, &DirBar::onBrowse);

    layout->addWidget(m_lineEdit);
    layout->addWidget(m_btnBrowse);
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

void DirBar::onTextCommitted()
{
    const QString path = directory();
    if (QDir(path).exists())
        emit directoryChanged(path);
}
