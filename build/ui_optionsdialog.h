/********************************************************************************
** Form generated from reading UI file 'optionsdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_OPTIONSDIALOG_H
#define UI_OPTIONSDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "qvalidatedlineedit.h"
#include "qvaluecombobox.h"
#include "trianglesamountfield.h"

QT_BEGIN_NAMESPACE

class Ui_OptionsDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *borderframe;
    QVBoxLayout *verticalLayout_2;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout_19;
    QSpacerItem *horizontalSpacer_26;
    QLabel *picAdd;
    QLabel *lbTitle;
    QTabWidget *tabWidget;
    QWidget *tabMain;
    QVBoxLayout *verticalLayout_Main;
    QLabel *transactionFeeInfoLabel;
    QHBoxLayout *horizontalLayoutFee;
    QLabel *transactionFeeLabel;
    TrianglesAmountField *transactionFee;
    QSpacerItem *horizontalSpacerFee;
    QLabel *reserveBalanceInfoLabel;
    QHBoxLayout *horizontalLayoutReserveBalance;
    QLabel *reserveBalanceLabel;
    TrianglesAmountField *reserveBalance;
    QSpacerItem *horizontalSpacerReserveBalance;
    QCheckBox *trianglesAtStartup;
    QCheckBox *detachDatabases;
    QSpacerItem *verticalSpacer_Main;
    QWidget *tabNetwork;
    QCheckBox *mapPortUpnp;
    QCheckBox *connectSocks;
    QWidget *layoutWidget;
    QHBoxLayout *horizontalLayout;
    QLabel *proxyIpLabel;
    QValidatedLineEdit *proxyIp;
    QLabel *proxyPortLabel;
    QLineEdit *proxyPort;
    QLabel *socksVersionLabel;
    QValueComboBox *socksVersion;
    QSpacerItem *horizontalSpacer_Network;
    QWidget *tabWindow;
    QVBoxLayout *verticalLayout_Window;
    QCheckBox *minimizeToTray;
    QCheckBox *minimizeOnClose;
    QSpacerItem *verticalSpacer_Window;
    QWidget *tabDisplay;
    QVBoxLayout *verticalLayout_Display;
    QHBoxLayout *horizontalLayout_1_Display;
    QLabel *langLabel;
    QValueComboBox *lang;
    QHBoxLayout *horizontalLayout_2_Display;
    QLabel *unitLabel;
    QValueComboBox *unit;
    QCheckBox *displayAddresses;
    QCheckBox *coinControlFeatures;
    QSpacerItem *verticalSpacer_Display;
    QHBoxLayout *horizontalLayout_Buttons;
    QSpacerItem *horizontalSpacer_1;
    QLabel *statusLabel;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *okButton;
    QPushButton *cancelButton;
    QPushButton *applyButton;

    void setupUi(QDialog *OptionsDialog)
    {
        if (OptionsDialog->objectName().isEmpty())
            OptionsDialog->setObjectName(QString::fromUtf8("OptionsDialog"));
        OptionsDialog->resize(540, 380);
        OptionsDialog->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"	background-color: #000;\n"
"   "));
        OptionsDialog->setModal(true);
        verticalLayout = new QVBoxLayout(OptionsDialog);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        borderframe = new QFrame(OptionsDialog);
        borderframe->setObjectName(QString::fromUtf8("borderframe"));
        borderframe->setStyleSheet(QString::fromUtf8("#borderframe {\n"
"	border: 2px solid #f26522;\n"
"      	}"));
        borderframe->setFrameShape(QFrame::StyledPanel);
        borderframe->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(borderframe);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        wCaption = new QWidget(borderframe);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 32));
        wCaption->setMaximumSize(QSize(16777215, 32));
        wCaption->setStyleSheet(QString::fromUtf8(""));
        wCaption->setProperty("is_header", QVariant(true));
        horizontalLayout_19 = new QHBoxLayout(wCaption);
        horizontalLayout_19->setObjectName(QString::fromUtf8("horizontalLayout_19"));
        horizontalLayout_19->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_26 = new QSpacerItem(7, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_19->addItem(horizontalSpacer_26);

        picAdd = new QLabel(wCaption);
        picAdd->setObjectName(QString::fromUtf8("picAdd"));
        picAdd->setEnabled(true);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(picAdd->sizePolicy().hasHeightForWidth());
        picAdd->setSizePolicy(sizePolicy);
        picAdd->setMinimumSize(QSize(30, 30));
        picAdd->setMaximumSize(QSize(30, 30));
        picAdd->setPixmap(QPixmap(QString::fromUtf8(":/icons/toolbar")));

        horizontalLayout_19->addWidget(picAdd);

        lbTitle = new QLabel(wCaption);
        lbTitle->setObjectName(QString::fromUtf8("lbTitle"));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        lbTitle->setFont(font);
        lbTitle->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_19->addWidget(lbTitle);


        verticalLayout_2->addWidget(wCaption);

        tabWidget = new QTabWidget(borderframe);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabWidget->setStyleSheet(QString::fromUtf8("QTabWidget::pane {\n"
"	border: 1px solid #f26522;\n"
"    	}\n"
"\n"
"QTabBar::tab   {\n"
"	padding-top: 3px;\n"
"	padding-bottom: 3px;\n"
"	padding-left: 10px;\n"
"	padding-right: 10px;\n"
"	}\n"
"                        \n"
"\n"
"QTabBar::tab:!selected {\n"
"	color: #61280E;\n"
"	margin-right: -1px;\n"
"	border-left: 1px solid #61280E;\n"
"	border-right: 1px solid #61280E;\n"
"	border-top: 1px solid #61280E;\n"
"	}\n"
"\n"
"QTabBar::tab:!selected:last {\n"
"	margin-right: 0px;\n"
"	}\n"
"                        \n"
"QTabBar::tab:selected {\n"
"	color: #f26522;\n"
"	margin-right: -1px;\n"
"	border-left: 1px solid #f26522;\n"
"	border-right: 1px solid #f26522;\n"
"	border-top: 1px solid #f26522;\n"
"	}\n"
"\n"
"QTabBar::tab:selected:last {\n"
"	margin-right: 0px;\n"
"	}\n"
"                        \n"
"QTabBar::tab:!selected:hover {\n"
"	background-color: #61280E;\n"
"	color: #f26522;\n"
"	}\n"
"\n"
"#transactionFee TrianglesAmountField {\n"
"	background-color: #1c1c1c;\n"
"	selection-background-color:#ff0000"
                        ";\n"
"	selection-color: #00ff00; \n"
"	border: 1px solid #f26522;\n"
"	color:#f26522;\n"
"	}\n"
"\n"
"TrianglesAmountField QAbstractItemView {\n"
"	background-color: #1c1c1c;\n"
"	selection-background-color:#61280E;\n"
"	outline: 0px;\n"
"	selection-color: #f26522; \n"
"	border: 1px solid #f26522;\n"
"	color:#f26522;\n"
"	}"));
        tabWidget->setTabPosition(QTabWidget::North);
        tabMain = new QWidget();
        tabMain->setObjectName(QString::fromUtf8("tabMain"));
        verticalLayout_Main = new QVBoxLayout(tabMain);
        verticalLayout_Main->setObjectName(QString::fromUtf8("verticalLayout_Main"));
        transactionFeeInfoLabel = new QLabel(tabMain);
        transactionFeeInfoLabel->setObjectName(QString::fromUtf8("transactionFeeInfoLabel"));
        transactionFeeInfoLabel->setTextFormat(Qt::PlainText);
        transactionFeeInfoLabel->setWordWrap(true);

        verticalLayout_Main->addWidget(transactionFeeInfoLabel);

        horizontalLayoutFee = new QHBoxLayout();
        horizontalLayoutFee->setObjectName(QString::fromUtf8("horizontalLayoutFee"));
        transactionFeeLabel = new QLabel(tabMain);
        transactionFeeLabel->setObjectName(QString::fromUtf8("transactionFeeLabel"));
        transactionFeeLabel->setTextFormat(Qt::PlainText);

        horizontalLayoutFee->addWidget(transactionFeeLabel);

        transactionFee = new TrianglesAmountField(tabMain);
        transactionFee->setObjectName(QString::fromUtf8("transactionFee"));
        transactionFee->setMinimumSize(QSize(0, 20));
        QPalette palette;
        QBrush brush(QColor(242, 101, 34, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        QBrush brush1(QColor(28, 28, 28, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette.setBrush(QPalette::Active, QPalette::Window, brush1);
        QBrush brush2(QColor(97, 40, 14, 255));
        brush2.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Highlight, brush2);
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        QBrush brush3(QColor(51, 153, 255, 255));
        brush3.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Disabled, QPalette::Highlight, brush3);
        transactionFee->setPalette(palette);
        transactionFee->setContextMenuPolicy(Qt::NoContextMenu);
        transactionFee->setStyleSheet(QString::fromUtf8("border: 1px solid #f26522;\n"
"background-color: #1c1c1c;\n"
""));

        horizontalLayoutFee->addWidget(transactionFee);

        horizontalSpacerFee = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayoutFee->addItem(horizontalSpacerFee);


        verticalLayout_Main->addLayout(horizontalLayoutFee);

        reserveBalanceInfoLabel = new QLabel(tabMain);
        reserveBalanceInfoLabel->setObjectName(QString::fromUtf8("reserveBalanceInfoLabel"));
        reserveBalanceInfoLabel->setTextFormat(Qt::PlainText);
        reserveBalanceInfoLabel->setWordWrap(true);

        verticalLayout_Main->addWidget(reserveBalanceInfoLabel);

        horizontalLayoutReserveBalance = new QHBoxLayout();
        horizontalLayoutReserveBalance->setObjectName(QString::fromUtf8("horizontalLayoutReserveBalance"));
        reserveBalanceLabel = new QLabel(tabMain);
        reserveBalanceLabel->setObjectName(QString::fromUtf8("reserveBalanceLabel"));
        reserveBalanceLabel->setTextFormat(Qt::PlainText);

        horizontalLayoutReserveBalance->addWidget(reserveBalanceLabel);

        reserveBalance = new TrianglesAmountField(tabMain);
        reserveBalance->setObjectName(QString::fromUtf8("reserveBalance"));
        reserveBalance->setMinimumSize(QSize(0, 20));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Highlight, brush2);
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        QBrush brush4(QColor(240, 240, 240, 255));
        brush4.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Inactive, QPalette::Highlight, brush4);
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Highlight, brush3);
        reserveBalance->setPalette(palette1);
        reserveBalance->setContextMenuPolicy(Qt::NoContextMenu);
        reserveBalance->setStyleSheet(QString::fromUtf8("border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"\n"
"QAbstractItemView {\n"
"	background-color: #1c1c1c;\n"
"	selection-background-color:#61280E;\n"
"	selection-color: #f26522; \n"
"	border: 1px solid #f26522;\n"
"	color:#f26522;\n"
"	}"));

        horizontalLayoutReserveBalance->addWidget(reserveBalance);

        horizontalSpacerReserveBalance = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayoutReserveBalance->addItem(horizontalSpacerReserveBalance);


        verticalLayout_Main->addLayout(horizontalLayoutReserveBalance);

        trianglesAtStartup = new QCheckBox(tabMain);
        trianglesAtStartup->setObjectName(QString::fromUtf8("trianglesAtStartup"));
        trianglesAtStartup->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        verticalLayout_Main->addWidget(trianglesAtStartup);

        detachDatabases = new QCheckBox(tabMain);
        detachDatabases->setObjectName(QString::fromUtf8("detachDatabases"));
        detachDatabases->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        verticalLayout_Main->addWidget(detachDatabases);

        verticalSpacer_Main = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_Main->addItem(verticalSpacer_Main);

        tabWidget->addTab(tabMain, QString());
        tabNetwork = new QWidget();
        tabNetwork->setObjectName(QString::fromUtf8("tabNetwork"));
        mapPortUpnp = new QCheckBox(tabNetwork);
        mapPortUpnp->setObjectName(QString::fromUtf8("mapPortUpnp"));
        mapPortUpnp->setGeometry(QRect(9, 9, 118, 16));
        mapPortUpnp->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));
        connectSocks = new QCheckBox(tabNetwork);
        connectSocks->setObjectName(QString::fromUtf8("connectSocks"));
        connectSocks->setGeometry(QRect(9, 28, 171, 16));
        connectSocks->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));
        layoutWidget = new QWidget(tabNetwork);
        layoutWidget->setObjectName(QString::fromUtf8("layoutWidget"));
        layoutWidget->setGeometry(QRect(10, 48, 501, 24));
        horizontalLayout = new QHBoxLayout(layoutWidget);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        proxyIpLabel = new QLabel(layoutWidget);
        proxyIpLabel->setObjectName(QString::fromUtf8("proxyIpLabel"));
        proxyIpLabel->setTextFormat(Qt::PlainText);

        horizontalLayout->addWidget(proxyIpLabel);

        proxyIp = new QValidatedLineEdit(layoutWidget);
        proxyIp->setObjectName(QString::fromUtf8("proxyIp"));
        proxyIp->setMinimumSize(QSize(0, 20));
        proxyIp->setMaximumSize(QSize(140, 16777215));
        proxyIp->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        horizontalLayout->addWidget(proxyIp);

        proxyPortLabel = new QLabel(layoutWidget);
        proxyPortLabel->setObjectName(QString::fromUtf8("proxyPortLabel"));
        proxyPortLabel->setTextFormat(Qt::PlainText);

        horizontalLayout->addWidget(proxyPortLabel);

        proxyPort = new QLineEdit(layoutWidget);
        proxyPort->setObjectName(QString::fromUtf8("proxyPort"));
        proxyPort->setMinimumSize(QSize(0, 20));
        proxyPort->setMaximumSize(QSize(55, 16777215));
        proxyPort->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        horizontalLayout->addWidget(proxyPort);

        socksVersionLabel = new QLabel(layoutWidget);
        socksVersionLabel->setObjectName(QString::fromUtf8("socksVersionLabel"));
        socksVersionLabel->setTextFormat(Qt::PlainText);

        horizontalLayout->addWidget(socksVersionLabel);

        socksVersion = new QValueComboBox(layoutWidget);
        socksVersion->setObjectName(QString::fromUtf8("socksVersion"));
        socksVersion->setEnabled(true);
        socksVersion->setMinimumSize(QSize(45, 20));
        socksVersion->setMaximumSize(QSize(45, 16777215));
        QPalette palette2;
        palette2.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Light, brush);
        palette2.setBrush(QPalette::Active, QPalette::Dark, brush);
        palette2.setBrush(QPalette::Active, QPalette::Text, brush);
        palette2.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Highlight, brush2);
        palette2.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Light, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Dark, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
        palette2.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Light, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Dark, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Highlight, brush3);
        socksVersion->setPalette(palette2);
        socksVersion->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"	border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"	}\n"
"\n"
"QComboBox::drop-down {\n"
"	subcontrol-origin: padding;\n"
"	subcontrol-position: top right;\n"
"	/*width: 15px;*/\n"
"	border-left-width: 1px;\n"
"	border-left-color: #f26522;\n"
"	border-left-style: solid;\n"
"	}\n"
"\n"
"QComboBox::down-arrow {\n"
"	image: url(:/icons/down_arrow);\n"
"	}\n"
"\n"
"QComboBox QAbstractItemView {\n"
"	background-color: #1c1c1c;\n"
"	selection-background-color:#61280E;\n"
"	selection-color: #f26522; \n"
"	border: 1px solid #f26522;\n"
"	color:#f26522;\n"
"	}"));
        socksVersion->setEditable(false);

        horizontalLayout->addWidget(socksVersion);

        horizontalSpacer_Network = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_Network);

        tabWidget->addTab(tabNetwork, QString());
        tabWindow = new QWidget();
        tabWindow->setObjectName(QString::fromUtf8("tabWindow"));
        verticalLayout_Window = new QVBoxLayout(tabWindow);
        verticalLayout_Window->setObjectName(QString::fromUtf8("verticalLayout_Window"));
        minimizeToTray = new QCheckBox(tabWindow);
        minimizeToTray->setObjectName(QString::fromUtf8("minimizeToTray"));
        minimizeToTray->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        verticalLayout_Window->addWidget(minimizeToTray);

        minimizeOnClose = new QCheckBox(tabWindow);
        minimizeOnClose->setObjectName(QString::fromUtf8("minimizeOnClose"));
        minimizeOnClose->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        verticalLayout_Window->addWidget(minimizeOnClose);

        verticalSpacer_Window = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_Window->addItem(verticalSpacer_Window);

        tabWidget->addTab(tabWindow, QString());
        tabDisplay = new QWidget();
        tabDisplay->setObjectName(QString::fromUtf8("tabDisplay"));
        verticalLayout_Display = new QVBoxLayout(tabDisplay);
        verticalLayout_Display->setObjectName(QString::fromUtf8("verticalLayout_Display"));
        horizontalLayout_1_Display = new QHBoxLayout();
        horizontalLayout_1_Display->setObjectName(QString::fromUtf8("horizontalLayout_1_Display"));
        langLabel = new QLabel(tabDisplay);
        langLabel->setObjectName(QString::fromUtf8("langLabel"));
        langLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_1_Display->addWidget(langLabel);

        lang = new QValueComboBox(tabDisplay);
        lang->setObjectName(QString::fromUtf8("lang"));
        lang->setMinimumSize(QSize(0, 20));
        lang->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"	border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"	}\n"
"\n"
"QComboBox::drop-down {\n"
"	subcontrol-origin: padding;\n"
"	subcontrol-position: top right;\n"
"	/*width: 15px;*/\n"
"	border-left-width: 1px;\n"
"	border-left-color: #f26522;\n"
"	border-left-style: solid;\n"
"	}\n"
"\n"
"QComboBox::down-arrow {\n"
"	image: url(:/icons/down_arrow);\n"
"	}\n"
"\n"
"QComboBox QAbstractItemView {\n"
"	background-color: #1c1c1c;\n"
"	selection-background-color:#61280E;\n"
"	selection-color: #f26522; \n"
"	border: 1px solid #f26522;\n"
"	color:#f26522;\n"
"	}"));

        horizontalLayout_1_Display->addWidget(lang);


        verticalLayout_Display->addLayout(horizontalLayout_1_Display);

        horizontalLayout_2_Display = new QHBoxLayout();
        horizontalLayout_2_Display->setObjectName(QString::fromUtf8("horizontalLayout_2_Display"));
        unitLabel = new QLabel(tabDisplay);
        unitLabel->setObjectName(QString::fromUtf8("unitLabel"));
        unitLabel->setTextFormat(Qt::PlainText);

        horizontalLayout_2_Display->addWidget(unitLabel);

        unit = new QValueComboBox(tabDisplay);
        unit->setObjectName(QString::fromUtf8("unit"));
        unit->setMinimumSize(QSize(0, 20));
        unit->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"	border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"	}\n"
"\n"
"QComboBox::drop-down {\n"
"	subcontrol-origin: padding;\n"
"	subcontrol-position: top right;\n"
"	/*width: 15px;*/\n"
"	border-left-width: 1px;\n"
"	border-left-color: #f26522;\n"
"	border-left-style: solid;\n"
"	}\n"
"\n"
"QComboBox::down-arrow {\n"
"	image: url(:/icons/down_arrow);\n"
"	}\n"
"\n"
"QComboBox QAbstractItemView {\n"
"	background-color: #1c1c1c;\n"
"	selection-background-color:#61280E;\n"
"	selection-color: #f26522; \n"
"	border: 1px solid #f26522;\n"
"	color:#f26522;\n"
"	}"));

        horizontalLayout_2_Display->addWidget(unit);


        verticalLayout_Display->addLayout(horizontalLayout_2_Display);

        displayAddresses = new QCheckBox(tabDisplay);
        displayAddresses->setObjectName(QString::fromUtf8("displayAddresses"));
        displayAddresses->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        verticalLayout_Display->addWidget(displayAddresses);

        coinControlFeatures = new QCheckBox(tabDisplay);
        coinControlFeatures->setObjectName(QString::fromUtf8("coinControlFeatures"));
        coinControlFeatures->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        verticalLayout_Display->addWidget(coinControlFeatures);

        verticalSpacer_Display = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_Display->addItem(verticalSpacer_Display);

        tabWidget->addTab(tabDisplay, QString());

        verticalLayout_2->addWidget(tabWidget);

        horizontalLayout_Buttons = new QHBoxLayout();
        horizontalLayout_Buttons->setObjectName(QString::fromUtf8("horizontalLayout_Buttons"));
        horizontalSpacer_1 = new QSpacerItem(40, 48, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Buttons->addItem(horizontalSpacer_1);

        statusLabel = new QLabel(borderframe);
        statusLabel->setObjectName(QString::fromUtf8("statusLabel"));
        QFont font1;
        font1.setBold(true);
        font1.setWeight(75);
        statusLabel->setFont(font1);
        statusLabel->setTextFormat(Qt::PlainText);
        statusLabel->setWordWrap(true);

        horizontalLayout_Buttons->addWidget(statusLabel);

        horizontalSpacer_2 = new QSpacerItem(40, 48, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Buttons->addItem(horizontalSpacer_2);

        okButton = new QPushButton(borderframe);
        okButton->setObjectName(QString::fromUtf8("okButton"));
        okButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 80px;\n"
"	min-width: 80px;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}"));

        horizontalLayout_Buttons->addWidget(okButton);

        cancelButton = new QPushButton(borderframe);
        cancelButton->setObjectName(QString::fromUtf8("cancelButton"));
        cancelButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 80px;\n"
"	min-width: 80px;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}"));
        cancelButton->setAutoDefault(false);

        horizontalLayout_Buttons->addWidget(cancelButton);

        applyButton = new QPushButton(borderframe);
        applyButton->setObjectName(QString::fromUtf8("applyButton"));
        applyButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 80px;\n"
"	min-width: 80px;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}"));
        applyButton->setAutoDefault(false);

        horizontalLayout_Buttons->addWidget(applyButton);


        verticalLayout_2->addLayout(horizontalLayout_Buttons);


        verticalLayout->addWidget(borderframe);

#if QT_CONFIG(shortcut)
        transactionFeeLabel->setBuddy(transactionFee);
        reserveBalanceLabel->setBuddy(reserveBalance);
        proxyIpLabel->setBuddy(proxyIp);
        proxyPortLabel->setBuddy(proxyPort);
        socksVersionLabel->setBuddy(socksVersion);
        langLabel->setBuddy(lang);
        unitLabel->setBuddy(unit);
#endif // QT_CONFIG(shortcut)

        retranslateUi(OptionsDialog);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(OptionsDialog);
    } // setupUi

    void retranslateUi(QDialog *OptionsDialog)
    {
        OptionsDialog->setWindowTitle(QCoreApplication::translate("OptionsDialog", "Options", nullptr));
        picAdd->setText(QString());
        lbTitle->setText(QCoreApplication::translate("OptionsDialog", "Options", nullptr));
        transactionFeeInfoLabel->setText(QCoreApplication::translate("OptionsDialog", "Optional transaction fee per kB that helps make sure your transactions are processed quickly. Most transactions are 1 kB. Fee 0.01 recommended.", nullptr));
        transactionFeeLabel->setText(QCoreApplication::translate("OptionsDialog", "Pay transaction &fee", nullptr));
        reserveBalanceInfoLabel->setText(QCoreApplication::translate("OptionsDialog", "Reserved amount does not participate in staking and is therefore spendable at any time.", nullptr));
        reserveBalanceLabel->setText(QCoreApplication::translate("OptionsDialog", "Reserve", nullptr));
#if QT_CONFIG(tooltip)
        trianglesAtStartup->setToolTip(QCoreApplication::translate("OptionsDialog", "Automatically start Triangles after logging in to the system.", nullptr));
#endif // QT_CONFIG(tooltip)
        trianglesAtStartup->setText(QCoreApplication::translate("OptionsDialog", "&Start Triangles on system login", nullptr));
#if QT_CONFIG(tooltip)
        detachDatabases->setToolTip(QCoreApplication::translate("OptionsDialog", "Detach block and address databases at shutdown. This means they can be moved to another data directory, but it slows down shutdown. The wallet is always detached.", nullptr));
#endif // QT_CONFIG(tooltip)
        detachDatabases->setText(QCoreApplication::translate("OptionsDialog", "&Detach databases at shutdown", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabMain), QCoreApplication::translate("OptionsDialog", "&Main", nullptr));
#if QT_CONFIG(tooltip)
        mapPortUpnp->setToolTip(QCoreApplication::translate("OptionsDialog", "Automatically open the Triangles client port on the router. This only works when your router supports UPnP and it is enabled.", nullptr));
#endif // QT_CONFIG(tooltip)
        mapPortUpnp->setText(QCoreApplication::translate("OptionsDialog", "Map port using &UPnP", nullptr));
#if QT_CONFIG(tooltip)
        connectSocks->setToolTip(QCoreApplication::translate("OptionsDialog", "Connect to the Triangles network through a SOCKS proxy (e.g. when connecting through Tor).", nullptr));
#endif // QT_CONFIG(tooltip)
        connectSocks->setText(QCoreApplication::translate("OptionsDialog", "&Connect through SOCKS proxy:", nullptr));
        proxyIpLabel->setText(QCoreApplication::translate("OptionsDialog", "Proxy &IP:", nullptr));
#if QT_CONFIG(tooltip)
        proxyIp->setToolTip(QCoreApplication::translate("OptionsDialog", "IP address of the proxy (e.g. 127.0.0.1)", nullptr));
#endif // QT_CONFIG(tooltip)
        proxyPortLabel->setText(QCoreApplication::translate("OptionsDialog", "&Port:", nullptr));
#if QT_CONFIG(tooltip)
        proxyPort->setToolTip(QCoreApplication::translate("OptionsDialog", "Port of the proxy (e.g. 9050)", nullptr));
#endif // QT_CONFIG(tooltip)
        socksVersionLabel->setText(QCoreApplication::translate("OptionsDialog", "SOCKS &Version:", nullptr));
#if QT_CONFIG(tooltip)
        socksVersion->setToolTip(QCoreApplication::translate("OptionsDialog", "SOCKS version of the proxy (e.g. 5)", nullptr));
#endif // QT_CONFIG(tooltip)
        tabWidget->setTabText(tabWidget->indexOf(tabNetwork), QCoreApplication::translate("OptionsDialog", "&Network", nullptr));
#if QT_CONFIG(tooltip)
        minimizeToTray->setToolTip(QCoreApplication::translate("OptionsDialog", "Show only a tray icon after minimizing the window.", nullptr));
#endif // QT_CONFIG(tooltip)
        minimizeToTray->setText(QCoreApplication::translate("OptionsDialog", "&Minimize to the tray instead of the taskbar", nullptr));
#if QT_CONFIG(tooltip)
        minimizeOnClose->setToolTip(QCoreApplication::translate("OptionsDialog", "Minimize instead of exit the application when the window is closed. When this option is enabled, the application will be closed only after selecting Quit in the menu.", nullptr));
#endif // QT_CONFIG(tooltip)
        minimizeOnClose->setText(QCoreApplication::translate("OptionsDialog", "M&inimize on close", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabWindow), QCoreApplication::translate("OptionsDialog", "&Window", nullptr));
        langLabel->setText(QCoreApplication::translate("OptionsDialog", "User Interface &language:", nullptr));
#if QT_CONFIG(tooltip)
        lang->setToolTip(QCoreApplication::translate("OptionsDialog", "The user interface language can be set here. This setting will take effect after restarting Triangles.", nullptr));
#endif // QT_CONFIG(tooltip)
        unitLabel->setText(QCoreApplication::translate("OptionsDialog", "&Unit to show amounts in:", nullptr));
#if QT_CONFIG(tooltip)
        unit->setToolTip(QCoreApplication::translate("OptionsDialog", "Choose the default subdivision unit to show in the interface and when sending coins.", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        displayAddresses->setToolTip(QCoreApplication::translate("OptionsDialog", "Whether to show TRI addresses in the transaction list or not.", nullptr));
#endif // QT_CONFIG(tooltip)
        displayAddresses->setText(QCoreApplication::translate("OptionsDialog", "&Display addresses in transaction list", nullptr));
#if QT_CONFIG(tooltip)
        coinControlFeatures->setToolTip(QCoreApplication::translate("OptionsDialog", "Whether to show coin control features or not.", nullptr));
#endif // QT_CONFIG(tooltip)
        coinControlFeatures->setText(QCoreApplication::translate("OptionsDialog", "Display coin &control features (experts only!)", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tabDisplay), QCoreApplication::translate("OptionsDialog", "&Display", nullptr));
        statusLabel->setText(QString());
        okButton->setText(QCoreApplication::translate("OptionsDialog", "&OK", nullptr));
        cancelButton->setText(QCoreApplication::translate("OptionsDialog", "&Cancel", nullptr));
        applyButton->setText(QCoreApplication::translate("OptionsDialog", "&Apply", nullptr));
    } // retranslateUi

};

namespace Ui {
    class OptionsDialog: public Ui_OptionsDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_OPTIONSDIALOG_H
