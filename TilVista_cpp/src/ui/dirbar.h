#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;

/// Global directory selector: LineEdit | Browse button.
/// Emits directoryChanged(path) whenever a valid directory is committed.
class DirBar : public QWidget
{
    Q_OBJECT
public:
    explicit DirBar(QWidget* parent = nullptr);

    QString directory() const;
    void    setDirectory(const QString& path);

signals:
    void directoryChanged(const QString& path);

private slots:
    void onBrowse();
    void onTextCommitted();

private:
    QLineEdit*   m_lineEdit;
    QPushButton* m_btnBrowse;
};
