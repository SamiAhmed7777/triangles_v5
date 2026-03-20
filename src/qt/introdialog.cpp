#include "introdialog.h"
#include "util.h"
#include "bootstrap.h"

#include <QSettings>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QFileDialog>
#include <QDir>
#include <QMessageBox>
#include <QDialogButtonBox>
#include <QProgressDialog>
#include <QCheckBox>
#include <QApplication>

#include <boost/filesystem.hpp>

IntroDialog::IntroDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle("Triangles");
    setMinimumWidth(520);

    // Match existing Triangles dark theme
    setStyleSheet(
        "QDialog { background-color: #000; color: #f26522; }"
        "QLabel { color: #f26522; }"
        "QRadioButton { color: #f26522; }"
        "QRadioButton::indicator { border: 1px solid #f26522; background-color: #000; width: 12px; height: 12px; border-radius: 7px; }"
        "QRadioButton::indicator:checked { background-color: #f26522; }"
        "QLineEdit { background-color: #1c1c1c; border: 1px solid #f26522; color: #f26522; padding: 4px; }"
        "QPushButton { background-color: #000; color: #f26522; border: 1px solid #f26522; padding: 4px 16px; min-height: 20px; }"
        "QPushButton:hover { background-color: #61280E; }"
    );

    defaultDataDir = QString::fromStdString(GetDefaultDataDir().string());

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    // Welcome header
    QLabel *welcomeLabel = new QLabel(tr("Welcome to Triangles!"));
    welcomeLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #f26522;");
    mainLayout->addWidget(welcomeLabel);

    // Description
    QLabel *descLabel = new QLabel(tr(
        "Triangles will store its blockchain data, wallet, and configuration in a data directory. "
        "You can use the default directory or choose a custom location. "
        "The data directory requires several hundred MB of free space."
    ));
    descLabel->setWordWrap(true);
    mainLayout->addWidget(descLabel);

    mainLayout->addSpacing(8);

    // Default directory radio
    defaultRadio = new QRadioButton(tr("Use the default data directory"));
    defaultRadio->setChecked(true);
    mainLayout->addWidget(defaultRadio);

    // Show default path
    QLabel *defaultPathLabel = new QLabel(defaultDataDir);
    defaultPathLabel->setStyleSheet("color: #999; margin-left: 24px; font-size: 11px;");
    mainLayout->addWidget(defaultPathLabel);

    mainLayout->addSpacing(4);

    // Custom directory radio
    customRadio = new QRadioButton(tr("Use a custom data directory:"));
    mainLayout->addWidget(customRadio);

    // Path input + browse button
    QHBoxLayout *pathLayout = new QHBoxLayout();
    pathLayout->setContentsMargins(24, 0, 0, 0);

    pathEdit = new QLineEdit(defaultDataDir);
    pathEdit->setEnabled(false);
    pathLayout->addWidget(pathEdit);

    browseButton = new QPushButton(tr("Browse..."));
    browseButton->setEnabled(false);
    pathLayout->addWidget(browseButton);

    mainLayout->addLayout(pathLayout);

    // Free space label
    freeSpaceLabel = new QLabel();
    freeSpaceLabel->setStyleSheet("color: #999; margin-left: 24px; font-size: 11px;");
    mainLayout->addWidget(freeSpaceLabel);

    mainLayout->addStretch(1);

    // OK / Cancel buttons
    QHBoxLayout *buttonLayout = new QHBoxLayout();
    buttonLayout->addStretch(1);

    QPushButton *okButton = new QPushButton(tr("OK"));
    QPushButton *cancelButton = new QPushButton(tr("Cancel"));

    buttonLayout->addWidget(okButton);
    buttonLayout->addWidget(cancelButton);
    mainLayout->addLayout(buttonLayout);

    // Connections
    connect(defaultRadio, SIGNAL(toggled(bool)), this, SLOT(on_defaultRadio_toggled(bool)));
    connect(browseButton, SIGNAL(clicked()), this, SLOT(on_browseButton_clicked()));
    connect(pathEdit, SIGNAL(textChanged(QString)), this, SLOT(updateFreeSpace()));
    connect(okButton, SIGNAL(clicked()), this, SLOT(accept()));
    connect(cancelButton, SIGNAL(clicked()), this, SLOT(reject()));

    updateFreeSpace();
}

QString IntroDialog::getDataDirectory() const
{
    if (defaultRadio->isChecked())
        return defaultDataDir;
    return pathEdit->text();
}

void IntroDialog::setDataDirectory(const QString &dir)
{
    pathEdit->setText(dir);
    if (dir == defaultDataDir) {
        defaultRadio->setChecked(true);
    } else {
        customRadio->setChecked(true);
    }
}

void IntroDialog::on_browseButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this, tr("Choose data directory"), pathEdit->text());
    if (!dir.isEmpty())
        pathEdit->setText(dir);
}

void IntroDialog::on_defaultRadio_toggled(bool checked)
{
    pathEdit->setEnabled(!checked);
    browseButton->setEnabled(!checked);
    if (checked)
        pathEdit->setText(defaultDataDir);
    updateFreeSpace();
}

void IntroDialog::updateFreeSpace()
{
    QString path = getDataDirectory();
    boost::filesystem::path fsPath(path.toStdString());

    // Walk up to find an existing parent
    try {
        while (!fsPath.empty() && !boost::filesystem::exists(fsPath))
            fsPath = fsPath.parent_path();

        if (!fsPath.empty()) {
            boost::filesystem::space_info si = boost::filesystem::space(fsPath);
            double freeGB = (double)si.available / (1024.0 * 1024.0 * 1024.0);
            freeSpaceLabel->setText(tr("Free space: %1 GB").arg(QString::number(freeGB, 'f', 2)));
        } else {
            freeSpaceLabel->setText(tr("Cannot determine free space"));
        }
    } catch (const boost::filesystem::filesystem_error &) {
        freeSpaceLabel->setText(tr("Cannot determine free space"));
    }
}

bool IntroDialog::pickDataDirectory()
{
    namespace fs = boost::filesystem;

    QSettings settings;
    // If -datadir was passed on the command line, skip the dialog entirely
    if (mapArgs.count("-datadir"))
        return true;

    QString dataDir = settings.value("strDataDir", "").toString();

    if (dataDir.isEmpty()) {
        // First run - show the dialog
        IntroDialog dlg;
        if (dlg.exec() != QDialog::Accepted)
            return false;

        dataDir = dlg.getDataDirectory();
        settings.setValue("strDataDir", dataDir);
    }

    // If the saved path is the default, don't set -datadir (let normal defaults work)
    QString defaultDir = QString::fromStdString(GetDefaultDataDir().string());
    if (dataDir != defaultDir) {
        mapArgs["-datadir"] = dataDir.toStdString();
    }

    // Ensure the directory exists
    try {
        fs::create_directories(fs::path(dataDir.toStdString()));
    } catch (const fs::filesystem_error &) {
        QMessageBox::critical(0, "Triangles",
            QString("Error: Could not create data directory \"%1\".").arg(dataDir));
        return false;
    }

    // Offer bootstrap download on each startup (unless user checked "don't ask again")
    fs::path dataDirPath(dataDir.toStdString());
    if (!settings.value("bootstrapDontAsk", false).toBool())
    {
        QMessageBox msgBox;
        msgBox.setWindowTitle("Triangles");
        msgBox.setText(
            "Would you like to download the latest blockchain snapshot?\n\n"
            "This will download the blockchain data from the Triangles network "
            "and replace any existing chain data in your data directory.\n\n"
            "Click Yes to download, or No to sync from the network.");
        msgBox.setIcon(QMessageBox::Question);
        msgBox.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
        msgBox.setDefaultButton(QMessageBox::Yes);
        QCheckBox *dontAskBox = new QCheckBox("Don't show this again");
        msgBox.setCheckBox(dontAskBox);

        int ret = msgBox.exec();

        if (dontAskBox->isChecked())
            settings.setValue("bootstrapDontAsk", true);

        if (ret == QMessageBox::Yes)
        {
            std::string host = Bootstrap::DEFAULT_HOST;
            std::string strError;
            std::vector<std::string> files;

            // Fetch file list (try domain first, then fallback to IP)
            if (!Bootstrap::FetchFileList(host, files, strError)) {
                host = Bootstrap::FALLBACK_HOST;
                if (!Bootstrap::FetchFileList(host, files, strError)) {
                    QMessageBox::warning(0, "Triangles",
                        QString("Could not reach bootstrap server:\n%1\n\n"
                                "The wallet will sync from the network instead.")
                        .arg(QString::fromStdString(strError)));
                    return true;
                }
            }

            // Show progress dialog
            QProgressDialog progress("Downloading blockchain data...", "Cancel",
                                     0, (int)files.size(), 0);
            progress.setWindowTitle("Triangles - Bootstrap");
            progress.setWindowModality(Qt::ApplicationModal);
            progress.setMinimumDuration(0);
            progress.setValue(0);

            bool failed = false;
            for (int i = 0; i < (int)files.size(); i++) {
                if (progress.wasCanceled())
                    break;

                QString filename = QString::fromStdString(files[i]);
                progress.setLabelText(
                    QString("Downloading %1 (%2 of %3)...")
                    .arg(filename).arg(i + 1).arg((int)files.size()));
                progress.setValue(i);
                QApplication::processEvents();

                // Create subdirectories if needed (e.g. txleveldb/)
                fs::path destPath = dataDirPath / files[i];
                fs::create_directories(destPath.parent_path());

                // Download this file
                std::string urlPath = std::string(Bootstrap::BASE_PATH) + files[i];
                if (!Bootstrap::DownloadFile(host, urlPath, destPath,
                    [](int64_t, int64_t) {
                        QApplication::processEvents();
                    }, strError))
                {
                    QMessageBox::warning(0, "Triangles",
                        QString("Download failed for %1:\n%2\n\n"
                                "The wallet will sync remaining data from the network.")
                        .arg(filename).arg(QString::fromStdString(strError)));
                    failed = true;
                    break;
                }
            }

            if (!failed)
                progress.setValue((int)files.size());
        }
    }

    return true;
}
