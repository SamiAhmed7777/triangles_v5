#include "optionsdialog.h"
#include "ui_optionsdialog.h"

#include "trianglesunits.h"
#include "monitoreddatamapper.h"
#include "netbase.h"
#include "optionsmodel.h"
#include "dialog_move_handler.h"

#include "init.h"
#include "util.h"

#include <filesystem>

#include <QDir>
#include <QFileDialog>
#include <QGroupBox>
#include <QIntValidator>
#include <QLocale>
#include <QMessageBox>
#include <QProcess>
#include <QRegExp>
#include <QRegExpValidator>
#include <QSettings>

OptionsDialog::OptionsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::OptionsDialog),
    model(0),
    mapper(0),
    fRestartWarningDisplayed_Proxy(false),
    fRestartWarningDisplayed_Lang(false),
    fProxyIpValid(true),
    dataDirPath(0),
    dataDirFreeSpaceLabel(0)
{
    ui->setupUi(this);
    setWindowFlags(Qt::CustomizeWindowHint | Qt::FramelessWindowHint | Qt::Window);
    ui->wCaption->installEventFilter(new DialogMoveHandler(this));

    /* Data Directory section in Main tab */
    m_currentDataDir = QString::fromStdString(GetDataDir(false).string());
    m_pendingDataDir.clear();

    QGroupBox *groupDataDir = new QGroupBox(tr("Data Directory"), this);
    groupDataDir->setStyleSheet(
        "QGroupBox { border: 1px solid #3d0e04; margin-top: 8px; padding-top: 16px; color: #e32105; }"
        "QGroupBox::title { subcontrol-origin: margin; left: 10px; padding: 0 3px; }");

    QVBoxLayout *dataDirLayout = new QVBoxLayout(groupDataDir);

    QHBoxLayout *dataDirPathLayout = new QHBoxLayout();
    dataDirPath = new QLineEdit(m_currentDataDir, groupDataDir);
    dataDirPath->setReadOnly(true);
    dataDirPath->setStyleSheet("QLineEdit { background-color: #1c1c1c; border: 1px solid #e32105; color: #e32105; padding: 2px; }");

    QPushButton *dataDirBrowseButton = new QPushButton(tr("Browse..."), groupDataDir);
    dataDirBrowseButton->setStyleSheet(
        "QPushButton { background-color: #000; color: #e32105; border: 1px solid #e32105; padding: 2px 12px; min-height: 20px; }"
        "QPushButton:hover { background-color: #3d0e04; }"
        "QPushButton:pressed:flat { color: #000; background-color: #e32105; }");

    dataDirPathLayout->addWidget(dataDirPath);
    dataDirPathLayout->addWidget(dataDirBrowseButton);
    dataDirLayout->addLayout(dataDirPathLayout);

    dataDirFreeSpaceLabel = new QLabel(groupDataDir);
    dataDirFreeSpaceLabel->setStyleSheet("color: #999; font-size: 11px;");
    dataDirLayout->addWidget(dataDirFreeSpaceLabel);

    // Insert into Main tab layout, before the vertical spacer (last item)
    QVBoxLayout *mainTabLayout = qobject_cast<QVBoxLayout*>(ui->tabWidget->widget(0)->layout());
    if (mainTabLayout) {
        int spacerIndex = mainTabLayout->count() - 1; // vertical spacer is last
        mainTabLayout->insertWidget(spacerIndex, groupDataDir);
    }

    connect(dataDirBrowseButton, SIGNAL(clicked()), this, SLOT(on_dataDirBrowseButton_clicked()));
    updateDataDirFreeSpace();

    /* Network elements init */
#ifndef USE_UPNP
    ui->mapPortUpnp->setEnabled(false);
#endif

    ui->proxyIp->setEnabled(false);
    ui->proxyPort->setEnabled(false);
    ui->proxyPort->setValidator(new QIntValidator(1, 65535, this));
    ui->connectSocks->setText(tr("&Use Tor (SOCKS5):"));

    ui->socksVersion->setEnabled(false);
    ui->socksVersion->addItem("5", 5);
    ui->socksVersion->addItem("4", 4);
    ui->socksVersion->setCurrentIndex(0);

    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyIp, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->proxyPort, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), ui->socksVersion, SLOT(setEnabled(bool)));
    connect(ui->connectSocks, SIGNAL(clicked(bool)), this, SLOT(showRestartWarning_Proxy()));
    connect(ui->connectSocks, SIGNAL(toggled(bool)), this, SLOT(applyTorDefaults(bool)));

    ui->proxyIp->installEventFilter(this);

    /* Window elements init */
#ifdef Q_OS_MAC
    ui->tabWindow->setVisible(false);
#endif

    /* Display elements init */
    QDir translations(":translations");
    ui->lang->addItem(QString("(") + tr("default") + QString(")"), QVariant(""));
    foreach(const QString &langStr, translations.entryList())
    {
        QLocale locale(langStr);

        /** check if the locale name consists of 2 parts (language_country) */
        if(langStr.contains("_"))
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language - native country (locale name)", e.g. "Deutsch - Deutschland (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" - ") + locale.nativeCountryName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language - country (locale name)", e.g. "German - Germany (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" - ") + QLocale::countryToString(locale.country()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
        else
        {
#if QT_VERSION >= 0x040800
            /** display language strings as "native language (locale name)", e.g. "Deutsch (de)" */
            ui->lang->addItem(locale.nativeLanguageName() + QString(" (") + langStr + QString(")"), QVariant(langStr));
#else
            /** display language strings as "language (locale name)", e.g. "German (de)" */
            ui->lang->addItem(QLocale::languageToString(locale.language()) + QString(" (") + langStr + QString(")"), QVariant(langStr));
#endif
        }
    }

    ui->unit->setModel(new TrianglesUnits(this));

    /* Widget-to-option mapper */
    mapper = new MonitoredDataMapper(this);
    mapper->setSubmitPolicy(QDataWidgetMapper::ManualSubmit);
    mapper->setOrientation(Qt::Vertical);

    /* enable apply button when data modified */
    connect(mapper, SIGNAL(viewModified()), this, SLOT(enableApplyButton()));
    /* disable apply button when new data loaded */
    connect(mapper, SIGNAL(currentIndexChanged(int)), this, SLOT(disableApplyButton()));
    /* setup/change UI elements when proxy IP is invalid/valid */
    connect(this, SIGNAL(proxyIpValid(QValidatedLineEdit *, bool)), this, SLOT(handleProxyIpValid(QValidatedLineEdit *, bool)));
}

OptionsDialog::~OptionsDialog()
{
    delete ui;
}

void OptionsDialog::setModel(OptionsModel *model)
{
    this->model = model;

    if(model)
    {
        connect(model, SIGNAL(displayUnitChanged(int)), this, SLOT(updateDisplayUnit()));

        mapper->setModel(model);
        setMapper();
        mapper->toFirst();
    }

    /* update the display unit, to not use the default ("TRI") */
    updateDisplayUnit();

    /* warn only when language selection changes by user action (placed here so init via mapper doesn't trigger this) */
    connect(ui->lang, SIGNAL(valueChanged()), this, SLOT(showRestartWarning_Lang()));

    /* disable apply button after settings are loaded as there is nothing to save */
    disableApplyButton();
}

void OptionsDialog::setMapper()
{
    /* Main */
    mapper->addMapping(ui->transactionFee, OptionsModel::Fee);
    mapper->addMapping(ui->reserveBalance, OptionsModel::ReserveBalance);
    mapper->addMapping(ui->trianglesAtStartup, OptionsModel::StartAtStartup);
    mapper->addMapping(ui->detachDatabases, OptionsModel::DetachDatabases);

    /* Network */
    mapper->addMapping(ui->mapPortUpnp, OptionsModel::MapPortUPnP);

    mapper->addMapping(ui->connectSocks, OptionsModel::ProxyUse);
    mapper->addMapping(ui->proxyIp, OptionsModel::ProxyIP);
    mapper->addMapping(ui->proxyPort, OptionsModel::ProxyPort);
    mapper->addMapping(ui->socksVersion, OptionsModel::ProxySocksVersion);

    /* Window */
#ifndef Q_OS_MAC
    mapper->addMapping(ui->minimizeToTray, OptionsModel::MinimizeToTray);
    mapper->addMapping(ui->minimizeOnClose, OptionsModel::MinimizeOnClose);
#endif

    /* Display */
    mapper->addMapping(ui->lang, OptionsModel::Language);
    mapper->addMapping(ui->unit, OptionsModel::DisplayUnit);
    mapper->addMapping(ui->displayAddresses, OptionsModel::DisplayAddresses);
    mapper->addMapping(ui->coinControlFeatures, OptionsModel::CoinControlFeatures);
    mapper->addMapping(ui->showOnionAddress, OptionsModel::ShowOnionAddress);
}

void OptionsDialog::enableApplyButton()
{
    ui->applyButton->setEnabled(true);
}

void OptionsDialog::disableApplyButton()
{
    ui->applyButton->setEnabled(false);
}

void OptionsDialog::enableSaveButtons()
{
    /* prevent enabling of the save buttons when data modified, if there is an invalid proxy address present */
    if(fProxyIpValid)
        setSaveButtonState(true);
}

void OptionsDialog::disableSaveButtons()
{
    setSaveButtonState(false);
}

void OptionsDialog::setSaveButtonState(bool fState)
{
    ui->applyButton->setEnabled(fState);
    ui->okButton->setEnabled(fState);
}

void OptionsDialog::on_okButton_clicked()
{
    mapper->submit();
    if (handleDataDirChange())
        return; // restart flow handles closing
    accept();
}

void OptionsDialog::on_cancelButton_clicked()
{
    reject();
}

void OptionsDialog::on_applyButton_clicked()
{
    mapper->submit();
    handleDataDirChange();
    disableApplyButton();
}

void OptionsDialog::showRestartWarning_Proxy()
{
    if(!fRestartWarningDisplayed_Proxy)
    {
        QMessageBox* msgBox = new QMessageBox(QMessageBox::Warning,
                                          tr("Warning"),
                                          tr("This setting will take effect after restarting Triangles!\n" "You shouldn't tamper with proxy settings unless you know exactly what you are doing!"),
                                          QMessageBox::Ok, this,
                                          Qt::FramelessWindowHint);
    
        msgBox->setIconPixmap(QPixmap(":/msgbox/warning"));
        msgBox->setStyleSheet("QMessageBox { border: 2px solid #e22104;}");
        msgBox->button(QMessageBox::Ok)->setStyleSheet("\
                          QMessageBox QPushButton {background-color: #000;color: #e32105;border: 1px solid #e32105;\
                              min-width: 120px;max-width: 120px;max-height: 20px;min-height: 20px;}\
                          QMessageBox QPushButton:hover {background-color: #3d0e04;}\
                          QMessageBox QPushButton:pressed:flat {color: #000;background-color: #e32105;}\
                          ");
        msgBox->exec();
                                  
        fRestartWarningDisplayed_Proxy = true;
    }
}

void OptionsDialog::showRestartWarning_Lang()
{
    if(!fRestartWarningDisplayed_Lang)
    {
        QMessageBox* msgBox = new QMessageBox(QMessageBox::Warning,
                                          tr("Warning"),
                                          tr("This setting will take effect after restarting Triangles!"),
                                          QMessageBox::Ok, this,
                                          Qt::FramelessWindowHint);
    

        msgBox->setIconPixmap(QPixmap(":/msgbox/warning"));
        msgBox->setStyleSheet("QMessageBox { border: 2px solid #e22104;}");
        msgBox->button(QMessageBox::Ok)->setStyleSheet("\
                          QMessageBox QPushButton {background-color: #000;color: #e32105;border: 1px solid #e32105;\
                              min-width: 120px;max-width: 120px;max-height: 20px;min-height: 20px;}\
                          QMessageBox QPushButton:hover {background-color: #3d0e04;}\
                          QMessageBox QPushButton:pressed:flat {color: #000;background-color: #e32105;}\
                          ");
        msgBox->exec();

        fRestartWarningDisplayed_Lang = true;
    }
}

void OptionsDialog::updateDisplayUnit()
{
    if(model)
    {
        /* Update transactionFee with the current unit */
        ui->transactionFee->setDisplayUnit(model->getDisplayUnit());
    }
}

void OptionsDialog::handleProxyIpValid(QValidatedLineEdit *object, bool fState)
{
    // this is used in a check before re-enabling the save buttons
    fProxyIpValid = fState;

    if(fProxyIpValid)
    {
        enableSaveButtons();
        ui->statusLabel->clear();
    }
    else
    {
        disableSaveButtons();
        object->setValid(fProxyIpValid);
        ui->statusLabel->setStyleSheet("QLabel { color: red; }");
        ui->statusLabel->setText(tr("The supplied proxy address is invalid."));
    }
}

void OptionsDialog::applyTorDefaults(bool enabled)
{
    if (!enabled)
        return;

    // One-click Tor mode: populate standard local Tor SOCKS settings.
    ui->proxyIp->setText("127.0.0.1");
    ui->proxyPort->setText("9050");
    ui->socksVersion->setCurrentIndex(ui->socksVersion->findData(5));
}

bool OptionsDialog::eventFilter(QObject *object, QEvent *event)
{
    if(event->type() == QEvent::FocusOut)
    {
        if(object == ui->proxyIp)
        {
            CService addr;
            /* Check proxyIp for a valid IPv4/IPv6 address and emit the proxyIpValid signal */
            emit proxyIpValid(ui->proxyIp, LookupNumeric(ui->proxyIp->text().toStdString().c_str(), addr));
        }
    }
    return QDialog::eventFilter(object, event);
}

void OptionsDialog::on_dataDirBrowseButton_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(
        this, tr("Choose data directory"), m_currentDataDir);
    if (!dir.isEmpty() && dir != m_currentDataDir)
    {
        m_pendingDataDir = dir;
        dataDirPath->setText(dir);
        updateDataDirFreeSpace();
        enableApplyButton();
    }
}

void OptionsDialog::updateDataDirFreeSpace()
{
    namespace fs = std::filesystem;
    QString path = dataDirPath->text();
    fs::path fsPath(path.toStdString());
    try {
        while (!fsPath.empty() && !fs::exists(fsPath))
            fsPath = fsPath.parent_path();
        if (!fsPath.empty()) {
            fs::space_info si = fs::space(fsPath);
            double freeGB = (double)si.available / (1024.0 * 1024.0 * 1024.0);
            dataDirFreeSpaceLabel->setText(
                tr("Free space: %1 GB").arg(QString::number(freeGB, 'f', 2)));
        } else {
            dataDirFreeSpaceLabel->setText(tr("Cannot determine free space"));
        }
    } catch (const fs::filesystem_error &) {
        dataDirFreeSpaceLabel->setText(tr("Cannot determine free space"));
    }
}

quint64 OptionsDialog::calculateDirSize(const QString& path)
{
    namespace fs = std::filesystem;
    quint64 totalSize = 0;
    try {
        for (fs::recursive_directory_iterator it(path.toStdString()), end; it != end; ++it) {
            if (fs::is_regular_file(*it))
                totalSize += fs::file_size(*it);
        }
    } catch (...) {}
    return totalSize;
}

bool OptionsDialog::handleDataDirChange()
{
    if (m_pendingDataDir.isEmpty() || m_pendingDataDir == m_currentDataDir)
        return false;

    namespace fs = std::filesystem;
    fs::path destPath(m_pendingDataDir.toStdString());

    // Check destination is writable
    try {
        fs::create_directories(destPath);
    } catch (const fs::filesystem_error& e) {
        QMessageBox::critical(this, tr("Error"),
            tr("Cannot create directory: %1").arg(QString::fromStdString(e.what())));
        m_pendingDataDir.clear();
        dataDirPath->setText(m_currentDataDir);
        updateDataDirFreeSpace();
        return false;
    }

    // Check free space vs current data dir size
    quint64 dataDirSize = calculateDirSize(m_currentDataDir);
    try {
        fs::space_info si = fs::space(destPath);
        quint64 required = dataDirSize + (dataDirSize / 10); // 10% headroom
        if (si.available < required) {
            QMessageBox::critical(this, tr("Insufficient Space"),
                tr("The destination has %1 MB free but the data directory requires approximately %2 MB.")
                    .arg(si.available / (1024*1024))
                    .arg(required / (1024*1024)));
            m_pendingDataDir.clear();
            dataDirPath->setText(m_currentDataDir);
            updateDataDirFreeSpace();
            return false;
        }
    } catch (const fs::filesystem_error&) {
        // If we can't check space, proceed anyway
    }

    // Save migration state to QSettings
    QSettings settings;
    settings.setValue("strDataDirPrevious", m_currentDataDir);
    settings.setValue("strDataDir", m_pendingDataDir);
    settings.setValue("fPendingDataDirMigration", true);

    // Ask about restart
    QMessageBox msgBox(this);
    msgBox.setWindowFlags(Qt::FramelessWindowHint);
    msgBox.setWindowTitle(tr("Data Directory Changed"));
    msgBox.setText(tr("The data directory will be moved from:\n%1\n\nTo:\n%2\n\n"
                      "This will happen when the wallet restarts.")
                   .arg(m_currentDataDir).arg(m_pendingDataDir));
    msgBox.setIcon(QMessageBox::Information);
    msgBox.setIconPixmap(QPixmap(":/msgbox/information"));
    msgBox.setStyleSheet("QMessageBox { border: 2px solid #e32105; background-color: #000; color: #e32105; }");

    QPushButton *restartBtn = msgBox.addButton(tr("Restart Now"), QMessageBox::AcceptRole);
    QPushButton *laterBtn = msgBox.addButton(tr("Later"), QMessageBox::RejectRole);

    QString btnStyle =
        "QPushButton { background-color: #000; color: #e32105; border: 1px solid #e32105; "
        "min-width: 120px; max-width: 120px; max-height: 20px; min-height: 20px; }"
        "QPushButton:hover { background-color: #3d0e04; }"
        "QPushButton:pressed:flat { color: #000; background-color: #e32105; }";
    restartBtn->setStyleSheet(btnStyle);
    laterBtn->setStyleSheet(btnStyle);

    msgBox.exec();

    if (msgBox.clickedButton() == restartBtn) {
        performRestart();
        return true;
    }
    return false;
}

void OptionsDialog::performRestart()
{
    // Launch a new instance of ourselves
    QString exePath = QApplication::applicationFilePath();
    QStringList args = QApplication::arguments();
    args.removeFirst(); // remove argv[0]

    // Remove any existing -datadir argument so the new instance
    // reads strDataDir from QSettings and performs migration
    QMutableStringListIterator it(args);
    while (it.hasNext()) {
        QString arg = it.next();
        if (arg.startsWith("-datadir") || arg.startsWith("/datadir"))
            it.remove();
    }

    // Start new process detached so it survives our shutdown
    QProcess::startDetached(exePath, args);

    // Close dialog and trigger wallet shutdown
    accept();
    StartShutdown();
}
