#include "introdialog.h"
#include "util.h"
#include "bootstrap.h"
#include "utxosnapshot.h"
#include "checkpoints.h"
#include "snapshotnet.h"

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

#include <cstdio>
#include <ctime>
#include <set>

#include <filesystem>

namespace fs = std::filesystem;

// Convert QString to fs::path preserving non-ASCII characters on Windows.
// On Windows, QString::toStdString() returns UTF-8 but std::filesystem::path
// constructed from a narrow string then uses the ANSI code page, which
// mangles UTF-8 paths. QString::toStdWString() + fs::path(std::wstring)
// preserves them. On non-Windows platforms the UTF-8 path is correct.
static fs::path qstringToPath(const QString& s)
{
#ifdef WIN32
    return fs::path(std::wstring(s.toStdWString()));
#else
    return fs::path(s.toStdString());
#endif
}

IntroDialog::IntroDialog(QWidget *parent) :
    QDialog(parent)
{
    setWindowTitle("Triangles");
    setMinimumWidth(520);

    // Match existing Triangles dark theme
    setStyleSheet(
        "QDialog { background-color: #000; color: #e32105; }"
        "QLabel { color: #e32105; }"
        "QRadioButton { color: #e32105; }"
        "QRadioButton::indicator { border: 1px solid #e32105; background-color: #000; width: 12px; height: 12px; border-radius: 7px; }"
        "QRadioButton::indicator:checked { background-color: #e32105; }"
        "QLineEdit { background-color: #1c1c1c; border: 1px solid #e32105; color: #e32105; padding: 4px; }"
        "QPushButton { background-color: #000; color: #e32105; border: 1px solid #e32105; padding: 4px 16px; min-height: 20px; }"
        "QPushButton:hover { background-color: #3d0e04; }"
    );

    defaultDataDir = QString::fromStdString(GetDefaultDataDir().string());

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setContentsMargins(20, 20, 20, 20);
    mainLayout->setSpacing(12);

    // Welcome header
    QLabel *welcomeLabel = new QLabel(tr("Welcome to Triangles!"));
    welcomeLabel->setStyleSheet("font-size: 16px; font-weight: bold; color: #e32105;");
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
    std::filesystem::path fsPath = qstringToPath(path);

    // Walk up to find an existing parent
    try {
        while (!fsPath.empty() && !std::filesystem::exists(fsPath))
            fsPath = fsPath.parent_path();

        if (!fsPath.empty()) {
            std::filesystem::space_info si = std::filesystem::space(fsPath);
            double freeGB = (double)si.available / (1024.0 * 1024.0 * 1024.0);
            freeSpaceLabel->setText(tr("Free space: %1 GB").arg(QString::number(freeGB, 'f', 2)));
        } else {
            freeSpaceLabel->setText(tr("Cannot determine free space"));
        }
    } catch (const std::filesystem::filesystem_error &) {
        freeSpaceLabel->setText(tr("Cannot determine free space"));
    }
}

bool IntroDialog::pickDataDirectory()
{
    namespace fs = std::filesystem;

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
    QString defaultDir = QString::fromStdString(
        std::string(GetDefaultDataDir().u8string()));
    if (dataDir != defaultDir) {
        // Pass the data dir to the daemon as UTF-8 bytes so a non-ASCII path
        // on Windows isn't mangled by the ANSI code page (path::string() does
        // that). The daemon side uses fs::u8path() to convert back.
        mapArgs["-datadir"] = std::string(qstringToPath(dataDir).u8string());
    }

    // Ensure the directory exists
    try {
        fs::create_directories(qstringToPath(dataDir));
    } catch (const fs::filesystem_error &) {
        QMessageBox::critical(0, "Triangles",
            QString("Error: Could not create data directory \"%1\".").arg(dataDir));
        return false;
    }

    // Auto-bootstrap: if no blockchain data exists, download automatically.
    // If data exists, offer optional re-download (unless user checked "don't ask again").
    fs::path dataDirPath = qstringToPath(dataDir);
    bool needsBootstrap = Bootstrap::NeedsBootstrap(dataDirPath);
    bool userWantsBootstrap = false;
    // Captured local-load error from the staged-snapshot probe below, surfaced
    // in the HTTPS-failure dialog so users see why their snapshot was rejected.
    std::string lastLocalLoadError;

    if (needsBootstrap)
    {
        // No blockchain data — bootstrap automatically, just inform the user.
        // The actual mechanism (local snapshot vs HTTPS download vs network
        // sync) is decided after probing the data directory; the dialog
        // intentionally doesn't promise "downloading" because we may find a
        // staged utxo-snapshot.bin and skip the network entirely.
        QMessageBox::information(0, "Triangles",
            "No blockchain data found.\n\n"
            "Setting up the wallet now — this only happens once.\n"
            "If a local snapshot is available it will be loaded automatically.");
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

        // First-run local-snapshot probe: if utxo-snapshot.bin is already in the
        // data directory (placed there by the user, an installer, or a
        // previous P2P fetch), load it directly. This avoids the HTTPS
        // bootstrap path entirely on first run.
        fs::path stagedSnap = dataDirPath / "utxo-snapshot.bin";
        std::error_code stagedEc;
        // Use the non-throwing error_code overload and probe symlink
        // status separately. We reject symlinks: an auto-loaded snapshot
        // is supposed to be a file the user placed in the data dir, not a
        // symlink an attacker could redirect to anything; if the user
        // genuinely wants to symlink, they can resolve it and copy the
        // file. A symlink_status probe error (broken perms, ENOENT on a
        // parent component) is treated as "not present" and falls through
        // to HTTPS bootstrap.
        fs::file_status symSt = fs::symlink_status(stagedSnap, stagedEc);
        bool stagedPresent = (!stagedEc &&
                              fs::is_symlink(symSt) == false &&
                              fs::is_regular_file(symSt));
        if (stagedEc) {
            printf("IntroDialog: cannot probe staged snapshot path (%s); skipping local load\n",
                   stagedEc.message().c_str());
            stagedPresent = false;
        }
        if (stagedPresent) {
            printf("IntroDialog: found staged utxo-snapshot.bin in data dir, attempting local load...\n");

            // Validate the staged file against the compiled-in hash before
            // touching the chain DB. This prevents loading a stale or wrong
            // snapshot from a previous install into a fresh data dir.
            int snapHeight = Checkpoints::GetBestSnapshotHeight();
            uint256 expectedHash;
            bool hashOk = (snapHeight > 0) &&
                           Checkpoints::GetSnapshotHash(snapHeight, expectedHash);

            std::string localErr;
            bool loaded = false;
            if (hashOk) {
                uint256 actualHash;
                std::string hashErr;
                if (SnapshotNet::ComputeSnapshotFileHash(stagedSnap, actualHash, hashErr)) {
                    if (actualHash != expectedHash) {
                        // Staged file is for a different release or otherwise
                        // doesn't match this build's compiled-in hash. Do NOT
                        // delete it — the user may have staged it intentionally,
                        // or it may belong to another release. Quarantine with
                        // a unique suffix so a previously quarantined file is
                        // never overwritten. If no free name can be found (or
                        // the rename itself fails for perms/locks), leave the
                        // original in place and report the exact error so the
                        // user can recover manually.
                        std::error_code rmEc;
                        fs::path quarantine;
                        bool foundFreeName = false;
                        // Capture the timestamp once — repeated time() calls
                        // inside the loop would just shift the suffix but add
                        // nothing useful, and could overflow on busy systems.
                        const std::string quarantinePrefix =
                            stagedSnap.string() + ".rejected." +
                            std::to_string(::time(nullptr)) + ".";
                        // Collision-safe suffix: timestamp + a small loop
                        // counter. Two rejections within the same second
                        // (e.g. double-clicked bootstrap dialog) still get
                        // distinct destinations. The loop caps at 1000 attempts;
                        // if every candidate is occupied we refuse to rename
                        // (overwriting an earlier quarantined file would lose
                        // the user's data and is worse than just reporting the
                        // conflict).
                        for (int attempt = 0; attempt < 1000; ++attempt) {
                            std::string name = quarantinePrefix +
                                               std::to_string(attempt);
                            quarantine = name;
                            std::error_code probeEc;
                            if (!fs::exists(quarantine, probeEc) && !probeEc) {
                                foundFreeName = true;
                                break;
                            }
                        }
                        if (!foundFreeName) {
                            // All 1000 candidate names were already taken.
                            // This is extremely unlikely in normal operation
                            // but if it happens, surface an honest error —
                            // a "no space on device" message would be
                            // misleading here.
                            rmEc = std::make_error_code(std::errc::file_exists);
                        } else {
                            fs::rename(stagedSnap, quarantine, rmEc);
                        }
                        if (rmEc) {
                            localErr = "staged utxo-snapshot.bin hash does not match this release "
                                       "(expected " + expectedHash.ToString().substr(0, 16) +
                                       ", got " + actualHash.ToString().substr(0, 16) +
                                       "), AND quarantine failed (" + rmEc.message() +
                                       "). Leave the original in place and review it manually: " +
                                       stagedSnap.string();
                        } else {
                            localErr = "staged utxo-snapshot.bin hash does not match this release "
                                       "(expected " + expectedHash.ToString().substr(0, 16) +
                                       ", got " + actualHash.ToString().substr(0, 16) +
                                       "). File moved to " + quarantine.filename().string() +
                                       " — review or delete it manually.";
                        }
                        printf("IntroDialog: %s\n", localErr.c_str());
                    } else {
                        loaded = UtxoSnapshot::LoadSnapshot(stagedSnap, dataDirPath,
                                                             localErr, /*requireCheckpoint=*/true);
                    }
                } else {
                    localErr = "cannot hash staged snapshot: " + hashErr;
                }
            } else {
                localErr = "no compiled-in snapshot hash available in this release";
            }

            if (loaded) {
                printf("IntroDialog: loaded staged utxo-snapshot.bin successfully\n");
                return true;
            }

            // Staged file failed to load — fall through to HTTPS bootstrap.
            // Remember the local-load reason so we can show it to the user
            // if HTTPS also fails (the failure dialog below concatenates it).
            printf("IntroDialog: staged snapshot unusable (%s); falling back to network bootstrap\n",
                   localErr.c_str());
            lastLocalLoadError = localErr;
        }

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

        // Try the fast UTXO snapshot path. The GUI has already probed the data
        // dir for a staged utxo-snapshot.bin above; if that didn't find one,
        // DownloadUtxoSnapshot is the canonical HTTPS path to the bootstrap
        // server. TLS validation is now handled in bootstrap.cpp's StartTLS
        // via a layered trust store (exedir cacert.pem → SSL_CERT_FILE →
        // system default paths → embedded ISRG X1 + X2 as belt-and-suspenders),
        // so this should succeed on Windows GUI builds where the Qt-bundled
        // libssl-3-x64.dll ships without a default cert path. The system-path
        // call is trusted on its return value (OpenSSL's hashed-directory
        // lookups are lazy and would otherwise show 0 eagerly loaded store
        // objects even on a valid install); the embedded fallbacks are
        // always attempted as cross-sign resilience and become load-bearing
        // on a stripped Windows GUI with no cacert.pem and no usable system
        // CA directory.
        std::string utxoError;
        bool success = Bootstrap::DownloadUtxoSnapshot(host, dataDirPath, progressFn, utxoError);
        if (!success) {
            strError = utxoError;
        }
        if (!success) {
            // The TLS-detection strings are matched against the standard error
            // messages produced by the bootstrap OpenSSL path; they cover
            // the common GUI-bundled OpenSSL failure modes without dumping
            // the raw error to the user.
            bool isTlsError = (strError.find("TLS handshake failed") != std::string::npos ||
                               strError.find("certificate verify failed") != std::string::npos ||
                               strError.find("TLS certificate verification failed") != std::string::npos);
            if (isTlsError) {
                QString localNote;
                if (!lastLocalLoadError.empty()) {
                    localNote = QString("\n\nLocal snapshot note: %1")
                        .arg(QString::fromStdString(lastLocalLoadError));
                }
                QMessageBox::warning(0, "Triangles",
                    QString("Could not download blockchain snapshot automatically.\n\n"
                            "The bundled network stack cannot validate the certificate of the\n"
                            "bootstrap server. To skip this, place a file named\n"
                            "    utxo-snapshot.bin\n"
                            "in your Triangles data directory:\n"
                            "    %1\n\n"
                            "Then restart the wallet — the snapshot will load automatically.\n\n"
                            "Otherwise, the wallet will sync from the network instead.%2")
                        .arg(dataDir)
                        .arg(localNote));
            } else {
                QString localNote;
                if (!lastLocalLoadError.empty()) {
                    localNote = QString("\n\nLocal snapshot note: %1")
                        .arg(QString::fromStdString(lastLocalLoadError));
                }
                QMessageBox::warning(0, "Triangles",
                    QString("Could not download blockchain snapshot:\n%1\n\n"
                            "The wallet will sync from the network instead.%2")
                        .arg(QString::fromStdString(strError))
                        .arg(localNote));
            }
        } else {
            progress.setValue(100);
        }
    }

    return true;
}

static void copyDirectoryRecursive(const std::filesystem::path& src,
                                   const std::filesystem::path& dst)
{
    namespace fs = std::filesystem;
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
    namespace fs = std::filesystem;

    fs::path srcDir = qstringToPath(oldPath);
    fs::path dstDir = qstringToPath(newPath);

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
