// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license.
#ifndef HDSEEDDIALOG_H
#define HDSEEDDIALOG_H

#include <QDialog>

class WalletModel;
QT_BEGIN_NAMESPACE
class QPlainTextEdit;
class QLabel;
QT_END_NAMESPACE

/** Generate, reveal (for backup), and restore the wallet's BIP39 HD seed phrase. */
class HDSeedDialog : public QDialog
{
    Q_OBJECT
public:
    explicit HDSeedDialog(QWidget *parent = 0);
    void setModel(WalletModel *model);

private:
    WalletModel *model;
    QPlainTextEdit *seedText;
    QLabel *statusLabel;
    void refreshStatus();

private slots:
    void onGenerate();
    void onShow();
    void onRestore();
};

#endif // HDSEEDDIALOG_H
