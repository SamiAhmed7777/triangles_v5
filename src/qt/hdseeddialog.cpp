// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license.
#include "hdseeddialog.h"
#include "walletmodel.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QPushButton>
#include <QPlainTextEdit>
#include <QLabel>
#include <QMessageBox>
#include <QFont>

HDSeedDialog::HDSeedDialog(QWidget *parent)
    : QDialog(parent), model(0), seedText(0), statusLabel(0)
{
    setWindowTitle(tr("HD Seed Phrase (BIP39)"));
    resize(560, 360);

    QVBoxLayout *layout = new QVBoxLayout(this);

    QLabel *intro = new QLabel(tr(
        "A 24-word seed phrase is a complete backup of this wallet. Anyone who has it "
        "can spend your coins. Write it down on paper and keep it offline."), this);
    intro->setWordWrap(true);
    layout->addWidget(intro);

    seedText = new QPlainTextEdit(this);
    seedText->setPlaceholderText(tr(
        "Your 24-word phrase appears here when you generate or reveal it. "
        "To restore, paste an existing 24-word phrase here and click 'Restore from Phrase'."));
    QFont mono("monospace");
    mono.setStyleHint(QFont::Monospace);
    seedText->setFont(mono);
    layout->addWidget(seedText);

    statusLabel = new QLabel(this);
    statusLabel->setWordWrap(true);
    layout->addWidget(statusLabel);

    QHBoxLayout *btns = new QHBoxLayout();
    QPushButton *genBtn = new QPushButton(tr("Generate New"), this);
    QPushButton *showBtn = new QPushButton(tr("Reveal for Backup"), this);
    QPushButton *restoreBtn = new QPushButton(tr("Restore from Phrase"), this);
    QPushButton *closeBtn = new QPushButton(tr("Close"), this);
    btns->addWidget(genBtn);
    btns->addWidget(showBtn);
    btns->addWidget(restoreBtn);
    btns->addStretch();
    btns->addWidget(closeBtn);
    layout->addLayout(btns);

    connect(genBtn, SIGNAL(clicked()), this, SLOT(onGenerate()));
    connect(showBtn, SIGNAL(clicked()), this, SLOT(onShow()));
    connect(restoreBtn, SIGNAL(clicked()), this, SLOT(onRestore()));
    connect(closeBtn, SIGNAL(clicked()), this, SLOT(accept()));
}

void HDSeedDialog::setModel(WalletModel *modelIn)
{
    model = modelIn;
    refreshStatus();
}

void HDSeedDialog::refreshStatus()
{
    if (!model || !statusLabel) return;
    if (model->hdEnabled())
        statusLabel->setText(tr("Status: HD seed is ACTIVE. Use 'Reveal for Backup' to view your phrase."));
    else
        statusLabel->setText(tr("Status: no HD seed yet. Use 'Generate New' to create one."));
}

void HDSeedDialog::onGenerate()
{
    if (!model) return;
    if (model->hdEnabled()) {
        QMessageBox::warning(this, tr("HD seed already set"),
            tr("This wallet already has an HD seed. Use 'Reveal for Backup' to view it."));
        return;
    }
    if (QMessageBox::question(this, tr("Generate new seed"),
            tr("Generate a new 24-word HD seed for this wallet?"),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) return;

    QString mnemonic, err;
    if (!model->hdNew(mnemonic, err)) {
        QMessageBox::critical(this, tr("Error"), err);
        return;
    }
    seedText->setPlainText(mnemonic);
    QMessageBox::information(this, tr("Write this down"),
        tr("Your new 24-word seed phrase is shown above. Write it on paper and store it safely "
           "and offline. This is the only backup of this wallet."));
    refreshStatus();
}

void HDSeedDialog::onShow()
{
    if (!model) return;
    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) return;

    QString mnemonic, err;
    if (!model->hdShow(mnemonic, err)) {
        QMessageBox::critical(this, tr("Error"), err);
        return;
    }
    seedText->setPlainText(mnemonic);
}

void HDSeedDialog::onRestore()
{
    if (!model) return;
    QString phrase = seedText->toPlainText().trimmed();
    if (phrase.isEmpty()) {
        QMessageBox::warning(this, tr("No phrase"),
            tr("Paste a 24-word phrase into the box first."));
        return;
    }
    if (QMessageBox::question(this, tr("Restore from phrase"),
            tr("Restore the HD seed from the phrase in the box and rescan the chain? "
               "This replaces the current HD seed."),
            QMessageBox::Yes | QMessageBox::No) != QMessageBox::Yes)
        return;

    WalletModel::UnlockContext ctx(model->requestUnlock());
    if (!ctx.isValid()) return;

    QString err;
    if (!model->hdRestore(phrase, err)) {
        QMessageBox::critical(this, tr("Error"), err);
        return;
    }
    QMessageBox::information(this, tr("Restored"),
        tr("HD seed restored and the chain was rescanned for your funds."));
    refreshStatus();
}
