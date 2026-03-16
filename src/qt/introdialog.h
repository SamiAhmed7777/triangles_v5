#ifndef INTRODIALOG_H
#define INTRODIALOG_H

#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QRadioButton>
#include <QPushButton>

/** Data directory selection dialog shown on first run. */
class IntroDialog : public QDialog
{
    Q_OBJECT

public:
    explicit IntroDialog(QWidget *parent = 0);

    QString getDataDirectory() const;
    void setDataDirectory(const QString &dir);

    /**
     * Check settings or show the dialog to choose data directory.
     * Returns true if a directory was selected, false if the user cancelled.
     * Sets mapArgs["-datadir"] if a non-default directory was chosen.
     */
    static bool pickDataDirectory();

private slots:
    void on_browseButton_clicked();
    void on_defaultRadio_toggled(bool checked);
    void updateFreeSpace();

private:
    QRadioButton *defaultRadio;
    QRadioButton *customRadio;
    QLineEdit *pathEdit;
    QPushButton *browseButton;
    QLabel *freeSpaceLabel;
    QString defaultDataDir;
};

#endif // INTRODIALOG_H
