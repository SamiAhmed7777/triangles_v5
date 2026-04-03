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

#include <set>

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

    // Check for pending data directory migration
    if (settings.value("fPendingDataDirMigration", false).toBool()) {
        QString oldDir = settings.value("strDataDirPrevious", "").toString();
        if (!oldDir.isEmpty() && oldDir != dataDir) {
            if (!migrateDataDirectory(oldDir, dataDir)) {
                // Migration failed - revert to old directory
                QMessageBox::warning(0, "Triangles",
                    QString("Data directory migration failed.\nContinuing with the previous directory:\n%1")
                        .arg(oldDir));
                dataDir = oldDir;
                settings.setValue("strDataDir", oldDir);
            }
        }
        // Clear migration state regardless
        settings.remove("strDataDirPrevious");
        settings.setValue("fPendingDataDirMigration", false);
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

    // Auto-bootstrap: if no blockchain data exists, download automatically.
    // If data exists, offer optional re-download (unless user checked "don't ask again").
    fs::path dataDirPath(dataDir.toStdString());
    bool needsBootstrap = Bootstrap::NeedsBootstrap(dataDirPath);
    bool userWantsBootstrap = false;

    if (needsBootstrap)
    {
        // No blockchain data — bootstrap automatically, just inform the user
        QMessageBox::information(0, "Triangles",
            "No blockchain data found.\n\n"
            "Downloading the latest blockchain snapshot automatically.\n"
            "This will only take a few minutes.");
        userWantsBootstrap = true;
    }
    else if (!settings.value("bootstrapDontAsk", false).toBool())
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

        userWantsBootstrap = (ret == QMessageBox::Yes);
    }

        if (userWantsBootstrap)
        {
            std::string host = Bootstrap::DEFAULT_HOST;
            std::string strError;

            QProgressDialog progress("Downloading blockchain snapshot...", "Cancel",
                                     0, 100, 0);
            progress.setWindowTitle("Triangles - Bootstrap");
            progress.setWindowModality(Qt::ApplicationModal);
            progress.setMinimumDuration(0);
            progress.setValue(0);

            auto progressFn = [&progress](int64_t bytesDownloaded, int64_t totalBytes) {
                if (totalBytes > 0) {
                    int pct = (int)((bytesDownloaded * 100) / totalBytes);
                    progress.setValue(pct);
                    progress.setLabelText(
                        QString("Downloading blockchain snapshot... %1 MB / %2 MB")
                        .arg(bytesDownloaded / (1024*1024))
                        .arg(totalBytes / (1024*1024)));
                } else {
                    progress.setLabelText(
                        QString("Downloading blockchain snapshot... %1 MB")
                        .arg(bytesDownloaded / (1024*1024)));
                }
                QApplication::processEvents();
            };

            bool success = Bootstrap::DownloadBootstrap(host, dataDirPath, progressFn, strError);
            if (!success) {
                host = Bootstrap::FALLBACK_HOST;
                progress.setValue(0);
                success = Bootstrap::DownloadBootstrap(host, dataDirPath, progressFn, strError);
            }

            if (!success) {
                QMessageBox::warning(0, "Triangles",
                    QString("Could not download blockchain snapshot:\n%1\n\n"
                            "The wallet will sync from the network instead.")
                    .arg(QString::fromStdString(strError)));
            } else {
                progress.setValue(100);
            }
        }
    }

    return true;
}

static void copyDirectoryRecursive(const boost::filesystem::path& src,
                                   const boost::filesystem::path& dst)
{
    namespace fs = boost::filesystem;
    fs::create_directories(dst);
    for (fs::directory_iterator it(src), end; it != end; ++it) {
        fs::path dstChild = dst / it->path().filename();
        if (fs::is_directory(it->path())) {
            copyDirectoryRecursive(it->path(), dstChild);
        } else {
            fs::copy_file(it->path(), dstChild, fs::copy_options::overwrite_existing);
        }
    }
}

bool IntroDialog::migrateDataDirectory(const QString& oldPath, const QString& newPath)
{
    namespace fs = boost::filesystem;

    fs::path srcDir(oldPath.toStdString());
    fs::path dstDir(newPath.toStdString());

    if (!fs::exists(srcDir) || !fs::is_directory(srcDir))
        return false;

    // Create destination directory
    try {
        fs::create_directories(dstDir);
    } catch (const fs::filesystem_error& e) {
        printf("Migration: Cannot create destination directory: %s\n", e.what());
        return false;
    }

    // Check free space
    try {
        quint64 srcSize = 0;
        for (fs::recursive_directory_iterator it(srcDir), end; it != end; ++it) {
            if (fs::is_regular_file(*it))
                srcSize += fs::file_size(*it);
        }
        fs::space_info si = fs::space(dstDir);
        if (si.available < srcSize + (50 * 1024 * 1024)) { // 50MB headroom
            printf("Migration: Insufficient disk space. Need %llu, have %llu\n",
                   (unsigned long long)srcSize, (unsigned long long)si.available);
            return false;
        }
    } catch (const fs::filesystem_error& e) {
        printf("Migration: Cannot check disk space: %s\n", e.what());
        return false;
    }

    // Files/directories to skip during copy
    static const std::set<std::string> skipFiles = {
        ".lock",
        "debug.log",
        "db.log",
    };

    // Show progress dialog
    QProgressDialog progress("Moving data directory...", QString(), 0, 0, 0);
    progress.setWindowTitle("Triangles - Data Migration");
    progress.setWindowModality(Qt::ApplicationModal);
    progress.setMinimumDuration(0);
    progress.setCancelButton(0);
    progress.show();
    QApplication::processEvents();

    // Phase 1: Copy wallet.dat FIRST (most critical file)
    fs::path walletSrc = srcDir / "wallet.dat";
    fs::path walletDst = dstDir / "wallet.dat";
    if (fs::exists(walletSrc)) {
        progress.setLabelText("Copying wallet.dat...");
        QApplication::processEvents();
        try {
            // Copy to temp name first, then rename for atomicity
            fs::path walletTmp = dstDir / "wallet.dat.migrating";
            fs::copy_file(walletSrc, walletTmp, fs::copy_options::overwrite_existing);

            // Verify copy by checking file size
            if (fs::file_size(walletTmp) != fs::file_size(walletSrc)) {
                fs::remove(walletTmp);
                printf("Migration: wallet.dat copy size mismatch!\n");
                return false;
            }

            // Rename into place
            if (fs::exists(walletDst))
                fs::remove(walletDst);
            fs::rename(walletTmp, walletDst);
        } catch (const fs::filesystem_error& e) {
            printf("Migration: Failed to copy wallet.dat: %s\n", e.what());
            return false; // Abort - wallet is critical
        }
    }

    // Phase 2: Copy everything else
    int filesCopied = 0;
    try {
        for (fs::directory_iterator it(srcDir), end; it != end; ++it) {
            std::string filename = it->path().filename().string();

            // Skip special files
            if (skipFiles.count(filename))
                continue;

            // Skip wallet.dat (already copied)
            if (filename == "wallet.dat")
                continue;

            fs::path dst = dstDir / filename;

            progress.setLabelText(QString("Copying %1...").arg(QString::fromStdString(filename)));
            QApplication::processEvents();

            if (fs::is_directory(it->path())) {
                copyDirectoryRecursive(it->path(), dst);
            } else {
                fs::copy_file(it->path(), dst, fs::copy_options::overwrite_existing);
            }
            filesCopied++;
        }
    } catch (const fs::filesystem_error& e) {
        // Non-wallet copy failure: log but don't abort
        // Chain data can be re-synced; wallet was already safely copied
        printf("Migration: Warning: failed to copy some files: %s\n", e.what());
    }

    // Phase 3: Rename old wallet.dat as safety backup (don't delete old dir)
    try {
        if (fs::exists(walletSrc)) {
            fs::rename(walletSrc, srcDir / "wallet.dat.bak-migrated");
        }
    } catch (...) {
        // Not critical
    }

    progress.close();
    printf("Migration: Successfully copied %d items from %s to %s\n",
           filesCopied, srcDir.string().c_str(), dstDir.string().c_str());
    return true;
}
