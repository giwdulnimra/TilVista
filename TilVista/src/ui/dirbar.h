#pragma once
#include <QWidget>

class QLineEdit;
class QPushButton;

/// Global directory selector: LineEdit | Browse | Load from Directory
///
/// Signals:
///   directoryChanged(path)  – valid dir committed via Browse or Enter
///   loadRequested()         – Load-button clicked (triggers AleaVue scan)
class DirBar : public QWidget
{
    Q_OBJECT
public:
    explicit DirBar(QWidget* parent = nullptr);

    QString directory() const;
    void    setDirectory(const QString& path);

signals:
    void directoryChanged(const QString& path);
    void loadRequested();

private slots:
    void onBrowse();
    void onEnter();   ///< v0.5.00: Enter in LineEdit = same as Browse result

private:
    QLineEdit*   m_lineEdit;
    QPushButton* m_btnBrowse;
    QPushButton* m_btnLoad;
};
