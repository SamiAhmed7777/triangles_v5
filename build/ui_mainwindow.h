/********************************************************************************
** Form generated from reading UI file 'mainwindow.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAINWINDOW_H
#define UI_MAINWINDOW_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QStackedWidget>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QVBoxLayout *verticalLayout_8;
    QVBoxLayout *verticalLayout_6;
    QWidget *wHeader;
    QVBoxLayout *verticalLayout_2;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout;
    QLabel *lbLogo;
    QSpacerItem *horizontalSpacer_1;
    QPushButton *pushButton_File;
    QSpacerItem *horizontalSpacer_2;
    QPushButton *pushButton_Help;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *pushButton_Settings;
    QSpacerItem *horizontalSpacer_4;
    QPushButton *pushButton_minimize;
    QPushButton *pushButton_close;
    QHBoxLayout *horizontalLayout_Menu;
    QSpacerItem *horizontalSpacer_first;
    QPushButton *pushButton_Overview;
    QPushButton *pushButton_Send;
    QPushButton *pushButton_Receive;
    QPushButton *pushButton_Addressbook;
    QPushButton *pushButton_Transactions;
    QPushButton *pushButton_Messages;
    QSpacerItem *horizontalSpacer_last;
    QStackedWidget *stackedWidget;
    QFrame *line_4;
    QWidget *wStatusBar;
    QHBoxLayout *horizontalLayout_6;
    QSpacerItem *horizontalSpacer_8;
    QLabel *label_synchronization;
    QProgressBar *progressBar;
    QLabel *label_blocks;
    QSpacerItem *horizontalSpacer_6;
    QLabel *label_encryption;
    QLabel *label_staking;
    QLabel *label_connections;
    QLabel *label_synced;
    QSpacerItem *horizontalSpacer_7;
    QHBoxLayout *horizontalLayout_8;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName(QString::fromUtf8("MainWindow"));
        MainWindow->resize(808, 694);
        QPalette palette;
        QBrush brush(QColor(242, 101, 34, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        QBrush brush1(QColor(0, 0, 0, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Button, brush1);
        QBrush brush2(QColor(255, 180, 144, 255));
        brush2.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Light, brush2);
        QBrush brush3(QColor(248, 140, 89, 255));
        brush3.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Midlight, brush3);
        QBrush brush4(QColor(121, 50, 17, 255));
        brush4.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Dark, brush4);
        QBrush brush5(QColor(161, 67, 22, 255));
        brush5.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Mid, brush5);
        palette.setBrush(QPalette::Active, QPalette::Text, brush1);
        QBrush brush6(QColor(255, 255, 255, 255));
        brush6.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::BrightText, brush6);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush1);
        palette.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette.setBrush(QPalette::Active, QPalette::Shadow, brush1);
        QBrush brush7(QColor(248, 178, 144, 255));
        brush7.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::AlternateBase, brush7);
        QBrush brush8(QColor(255, 255, 220, 255));
        brush8.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::ToolTipBase, brush8);
        palette.setBrush(QPalette::Active, QPalette::ToolTipText, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Light, brush2);
        palette.setBrush(QPalette::Inactive, QPalette::Midlight, brush3);
        palette.setBrush(QPalette::Inactive, QPalette::Dark, brush4);
        palette.setBrush(QPalette::Inactive, QPalette::Mid, brush5);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::BrightText, brush6);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Shadow, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush7);
        palette.setBrush(QPalette::Inactive, QPalette::ToolTipBase, brush8);
        palette.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush4);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Light, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::Midlight, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::Dark, brush4);
        palette.setBrush(QPalette::Disabled, QPalette::Mid, brush5);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush4);
        palette.setBrush(QPalette::Disabled, QPalette::BrightText, brush6);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Shadow, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ToolTipBase, brush8);
        palette.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush1);
        MainWindow->setPalette(palette);
        QFont font;
        font.setStyleStrategy(QFont::PreferAntialias);
        MainWindow->setFont(font);
        MainWindow->setStyleSheet(QString::fromUtf8("#MainWindow {\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QMenu {\n"
"    	background-color: #AF272E;\n"
"    	border: 0px solid black;\n"
"	}\n"
"\n"
"QMenu::item {\n"
"	background-color: transparent;\n"
"	color: #deeef1;\n"
"	padding: 4px 10px 4px 20px;	\n"
"	}\n"
"\n"
"QMenu::item:selected {\n"
"	background-color: #9D0B17;\n"
"	color: #E9D9D8;\n"
"	border: none;\n"
"	}\n"
"\n"
"/* ================= combobox */\n"
"QComboBox {\n"
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
"	}\n"
"\n"
"/* ===="
                        "======= QLineEdit =============*/\n"
"QLineEdit {\n"
"	border: 0px solid gray;\n"
"	}\n"
"	\n"
"QLineEdit:disabled {\n"
"	background-color: #e0e0e0;\n"
"	}\n"
"\n"
"/* =========== header ===============*/\n"
"QWidget[is_header=\"true\"] {\n"
"	background-color: #AB7F82;\n"
"	}\n"
"/* =========== CheckBox ==============*/\n"
"QCheckBox {\n"
"	spacing: 5px;\n"
"	color: #AF272E;\n"
"	}\n"
"\n"
"QCheckBox::indicator {\n"
"	width: 12px;\n"
"	height: 12px;\n"
"	}\n"
"\n"
"QCheckBox::indicator:unchecked {\n"
"	image: url(:/res/check_unchecked.png);\n"
"	}\n"
"\n"
"QCheckBox::indicator:unchecked:hover {\n"
"	image: url(:/res/check_unchecked.png);\n"
"	}\n"
"\n"
"QCheckBox::indicator:unchecked:pressed {\n"
"	image: url(:/res/check_unchecked.png);\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image: url(:/res/check_checked.png);\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked:hover {\n"
"	image: url(:/res/check_checked.png);\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked:pressed {\n"
"	image: url(:/res/check_checked.pn"
                        "g);\n"
"	}"));
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName(QString::fromUtf8("centralWidget"));
        centralWidget->setStyleSheet(QString::fromUtf8("#centralWidget {\n"
"	background-color: #000;\n"
"	border: 2px solid #f26522;\n"
"	}"));
        verticalLayout_8 = new QVBoxLayout(centralWidget);
        verticalLayout_8->setSpacing(0);
        verticalLayout_8->setContentsMargins(11, 11, 11, 11);
        verticalLayout_8->setObjectName(QString::fromUtf8("verticalLayout_8"));
        verticalLayout_8->setContentsMargins(0, 0, 0, 0);
        verticalLayout_6 = new QVBoxLayout();
        verticalLayout_6->setSpacing(0);
        verticalLayout_6->setObjectName(QString::fromUtf8("verticalLayout_6"));
        verticalLayout_6->setContentsMargins(6, 6, 6, -1);
        wHeader = new QWidget(centralWidget);
        wHeader->setObjectName(QString::fromUtf8("wHeader"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Fixed);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(wHeader->sizePolicy().hasHeightForWidth());
        wHeader->setSizePolicy(sizePolicy);
        wHeader->setMinimumSize(QSize(0, 127));
        wHeader->setMaximumSize(QSize(16777215, 127));
        wHeader->setStyleSheet(QString::fromUtf8("#wHeader {\n"
"	background-color: #000;\n"
"}\n"
"\n"
"QWidget {\n"
"	color: #f26522;\n"
"	}"));
        verticalLayout_2 = new QVBoxLayout(wHeader);
        verticalLayout_2->setSpacing(6);
        verticalLayout_2->setContentsMargins(11, 11, 11, 11);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        wCaption = new QWidget(wHeader);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setStyleSheet(QString::fromUtf8("#wCaption {\n"
"	background-color: #000;\n"
"}\n"
"\n"
"QWidget {\n"
"	color: #f26522;\n"
"	}"));
        horizontalLayout = new QHBoxLayout(wCaption);
        horizontalLayout->setSpacing(6);
        horizontalLayout->setContentsMargins(11, 11, 11, 11);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(9, -1, 0, -1);
        lbLogo = new QLabel(wCaption);
        lbLogo->setObjectName(QString::fromUtf8("lbLogo"));
        lbLogo->setMinimumSize(QSize(300, 63));
        lbLogo->setMaximumSize(QSize(300, 63));
        lbLogo->setText(QString::fromUtf8(""));
        lbLogo->setPixmap(QPixmap(QString::fromUtf8(":/images/header_logo")));
        lbLogo->setScaledContents(true);

        horizontalLayout->addWidget(lbLogo);

        horizontalSpacer_1 = new QSpacerItem(220, 17, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_1);

        pushButton_File = new QPushButton(wCaption);
        pushButton_File->setObjectName(QString::fromUtf8("pushButton_File"));
        QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(pushButton_File->sizePolicy().hasHeightForWidth());
        pushButton_File->setSizePolicy(sizePolicy1);
        pushButton_File->setMaximumSize(QSize(16777215, 24));
        QFont font1;
        font1.setPointSize(11);
        font1.setStyleStrategy(QFont::PreferAntialias);
        pushButton_File->setFont(font1);
        pushButton_File->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 15px 3px 15px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}"));
        pushButton_File->setFlat(true);

        horizontalLayout->addWidget(pushButton_File);

        horizontalSpacer_2 = new QSpacerItem(13, 17, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_2);

        pushButton_Help = new QPushButton(wCaption);
        pushButton_Help->setObjectName(QString::fromUtf8("pushButton_Help"));
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(pushButton_Help->sizePolicy().hasHeightForWidth());
        pushButton_Help->setSizePolicy(sizePolicy2);
        pushButton_Help->setMaximumSize(QSize(16777215, 24));
        pushButton_Help->setFont(font1);
        pushButton_Help->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 15px 3px 15px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}"));
        pushButton_Help->setFlat(true);

        horizontalLayout->addWidget(pushButton_Help);

        horizontalSpacer_3 = new QSpacerItem(13, 17, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_3);

        pushButton_Settings = new QPushButton(wCaption);
        pushButton_Settings->setObjectName(QString::fromUtf8("pushButton_Settings"));
        sizePolicy2.setHeightForWidth(pushButton_Settings->sizePolicy().hasHeightForWidth());
        pushButton_Settings->setSizePolicy(sizePolicy2);
        pushButton_Settings->setMaximumSize(QSize(16777215, 24));
        pushButton_Settings->setFont(font1);
        pushButton_Settings->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 15px 3px 15px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}"));
        pushButton_Settings->setFlat(true);

        horizontalLayout->addWidget(pushButton_Settings);

        horizontalSpacer_4 = new QSpacerItem(100, 17, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_4);

        pushButton_minimize = new QPushButton(wCaption);
        pushButton_minimize->setObjectName(QString::fromUtf8("pushButton_minimize"));
        pushButton_minimize->setMinimumSize(QSize(16, 30));
        pushButton_minimize->setMaximumSize(QSize(16, 30));
        pushButton_minimize->setStyleSheet(QString::fromUtf8("QPushButton {	\n"
"	image: url(:/icons/minimize_normal);\n"
"	border: 0px solid gray;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	image: url(:/icons/minimize_normal);\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	image: url(:/icons/minimize_hover);\n"
"	}"));
        pushButton_minimize->setText(QString::fromUtf8(""));
        pushButton_minimize->setFlat(true);

        horizontalLayout->addWidget(pushButton_minimize);

        pushButton_close = new QPushButton(wCaption);
        pushButton_close->setObjectName(QString::fromUtf8("pushButton_close"));
        pushButton_close->setMinimumSize(QSize(30, 30));
        pushButton_close->setMaximumSize(QSize(30, 30));
        pushButton_close->setStyleSheet(QString::fromUtf8("QPushButton {	\n"
"	image: url(:/icons/close_normal);\n"
"	border: 0px solid gray;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	image: url(:/icons/close_normal);\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	image: url(:/icons/close_hover);\n"
"	}"));
        pushButton_close->setText(QString::fromUtf8(""));
        pushButton_close->setFlat(true);

        horizontalLayout->addWidget(pushButton_close);


        verticalLayout_2->addWidget(wCaption);

        horizontalLayout_Menu = new QHBoxLayout();
        horizontalLayout_Menu->setSpacing(6);
        horizontalLayout_Menu->setObjectName(QString::fromUtf8("horizontalLayout_Menu"));
        horizontalSpacer_first = new QSpacerItem(20, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Menu->addItem(horizontalSpacer_first);

        pushButton_Overview = new QPushButton(wHeader);
        pushButton_Overview->setObjectName(QString::fromUtf8("pushButton_Overview"));
        sizePolicy2.setHeightForWidth(pushButton_Overview->sizePolicy().hasHeightForWidth());
        pushButton_Overview->setSizePolicy(sizePolicy2);
        pushButton_Overview->setMaximumSize(QSize(16777215, 32));
        QFont font2;
        font2.setPointSize(10);
        font2.setStyleStrategy(QFont::PreferAntialias);
        pushButton_Overview->setFont(font2);
        pushButton_Overview->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/menu_32/home"), QSize(), QIcon::Normal, QIcon::Off);
        pushButton_Overview->setIcon(icon);
        pushButton_Overview->setIconSize(QSize(32, 32));
        pushButton_Overview->setFlat(true);

        horizontalLayout_Menu->addWidget(pushButton_Overview);

        pushButton_Send = new QPushButton(wHeader);
        pushButton_Send->setObjectName(QString::fromUtf8("pushButton_Send"));
        sizePolicy2.setHeightForWidth(pushButton_Send->sizePolicy().hasHeightForWidth());
        pushButton_Send->setSizePolicy(sizePolicy2);
        pushButton_Send->setMaximumSize(QSize(16777215, 32));
        pushButton_Send->setFont(font2);
        pushButton_Send->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/menu_32/send"), QSize(), QIcon::Normal, QIcon::Off);
        pushButton_Send->setIcon(icon1);
        pushButton_Send->setIconSize(QSize(32, 32));
        pushButton_Send->setFlat(true);

        horizontalLayout_Menu->addWidget(pushButton_Send);

        pushButton_Receive = new QPushButton(wHeader);
        pushButton_Receive->setObjectName(QString::fromUtf8("pushButton_Receive"));
        sizePolicy2.setHeightForWidth(pushButton_Receive->sizePolicy().hasHeightForWidth());
        pushButton_Receive->setSizePolicy(sizePolicy2);
        pushButton_Receive->setMaximumSize(QSize(16777215, 32));
        pushButton_Receive->setFont(font2);
        pushButton_Receive->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/menu_32/receive"), QSize(), QIcon::Normal, QIcon::Off);
        pushButton_Receive->setIcon(icon2);
        pushButton_Receive->setIconSize(QSize(32, 32));
        pushButton_Receive->setFlat(true);

        horizontalLayout_Menu->addWidget(pushButton_Receive);

        pushButton_Addressbook = new QPushButton(wHeader);
        pushButton_Addressbook->setObjectName(QString::fromUtf8("pushButton_Addressbook"));
        sizePolicy2.setHeightForWidth(pushButton_Addressbook->sizePolicy().hasHeightForWidth());
        pushButton_Addressbook->setSizePolicy(sizePolicy2);
        pushButton_Addressbook->setMaximumSize(QSize(16777215, 32));
        pushButton_Addressbook->setFont(font2);
        pushButton_Addressbook->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/menu_32/addressbook"), QSize(), QIcon::Normal, QIcon::Off);
        pushButton_Addressbook->setIcon(icon3);
        pushButton_Addressbook->setIconSize(QSize(32, 32));
        pushButton_Addressbook->setFlat(true);

        horizontalLayout_Menu->addWidget(pushButton_Addressbook);

        pushButton_Transactions = new QPushButton(wHeader);
        pushButton_Transactions->setObjectName(QString::fromUtf8("pushButton_Transactions"));
        sizePolicy2.setHeightForWidth(pushButton_Transactions->sizePolicy().hasHeightForWidth());
        pushButton_Transactions->setSizePolicy(sizePolicy2);
        pushButton_Transactions->setMaximumSize(QSize(16777215, 32));
        pushButton_Transactions->setFont(font2);
        pushButton_Transactions->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/menu_32/transactions"), QSize(), QIcon::Normal, QIcon::Off);
        pushButton_Transactions->setIcon(icon4);
        pushButton_Transactions->setIconSize(QSize(32, 32));
        pushButton_Transactions->setFlat(true);

        horizontalLayout_Menu->addWidget(pushButton_Transactions);

        pushButton_Messages = new QPushButton(wHeader);
        pushButton_Messages->setObjectName(QString::fromUtf8("pushButton_Messages"));
        sizePolicy2.setHeightForWidth(pushButton_Messages->sizePolicy().hasHeightForWidth());
        pushButton_Messages->setSizePolicy(sizePolicy2);
        pushButton_Messages->setMaximumSize(QSize(16777215, 32));
        pushButton_Messages->setFont(font2);
        pushButton_Messages->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/menu_32/messages"), QSize(), QIcon::Normal, QIcon::Off);
        pushButton_Messages->setIcon(icon5);
        pushButton_Messages->setIconSize(QSize(32, 32));
        pushButton_Messages->setFlat(true);

        horizontalLayout_Menu->addWidget(pushButton_Messages);

        horizontalSpacer_last = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_Menu->addItem(horizontalSpacer_last);


        verticalLayout_2->addLayout(horizontalLayout_Menu);


        verticalLayout_6->addWidget(wHeader);

        stackedWidget = new QStackedWidget(centralWidget);
        stackedWidget->setObjectName(QString::fromUtf8("stackedWidget"));

        verticalLayout_6->addWidget(stackedWidget);

        line_4 = new QFrame(centralWidget);
        line_4->setObjectName(QString::fromUtf8("line_4"));
        line_4->setMaximumSize(QSize(16777215, 1));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::Button, brush);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush);
        line_4->setPalette(palette1);
        line_4->setStyleSheet(QString::fromUtf8("background-color: #f26522;"));
        line_4->setLineWidth(0);
        line_4->setFrameShape(QFrame::HLine);
        line_4->setFrameShadow(QFrame::Sunken);

        verticalLayout_6->addWidget(line_4);

        wStatusBar = new QWidget(centralWidget);
        wStatusBar->setObjectName(QString::fromUtf8("wStatusBar"));
        wStatusBar->setMinimumSize(QSize(0, 37));
        wStatusBar->setMaximumSize(QSize(16777215, 37));
        wStatusBar->setStyleSheet(QString::fromUtf8("#wStatusBar {\n"
"	background-color: #000;\n"
"	}\n"
"QLabel {\n"
"	color: #f26522;\n"
"	}"));
        horizontalLayout_6 = new QHBoxLayout(wStatusBar);
        horizontalLayout_6->setSpacing(6);
        horizontalLayout_6->setContentsMargins(11, 11, 11, 11);
        horizontalLayout_6->setObjectName(QString::fromUtf8("horizontalLayout_6"));
        horizontalLayout_6->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_8 = new QSpacerItem(70, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_6->addItem(horizontalSpacer_8);

        label_synchronization = new QLabel(wStatusBar);
        label_synchronization->setObjectName(QString::fromUtf8("label_synchronization"));
        QFont font3;
        font3.setPointSize(10);
        label_synchronization->setFont(font3);
        label_synchronization->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        horizontalLayout_6->addWidget(label_synchronization);

        progressBar = new QProgressBar(wStatusBar);
        progressBar->setObjectName(QString::fromUtf8("progressBar"));
        progressBar->setMinimumSize(QSize(200, 15));
        progressBar->setMaximumSize(QSize(16777215, 15));
        progressBar->setStyleSheet(QString::fromUtf8("QProgressBar {\n"
"    border: 1px solid#f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QProgressBar::chunk {\n"
"	background-color: #61280E;\n"
"	/*width: 20px;*/\n"
"	}"));
        progressBar->setValue(24);
        progressBar->setTextVisible(false);

        horizontalLayout_6->addWidget(progressBar);

        label_blocks = new QLabel(wStatusBar);
        label_blocks->setObjectName(QString::fromUtf8("label_blocks"));
        label_blocks->setFont(font3);

        horizontalLayout_6->addWidget(label_blocks);

        horizontalSpacer_6 = new QSpacerItem(10, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_6->addItem(horizontalSpacer_6);

        label_encryption = new QLabel(wStatusBar);
        label_encryption->setObjectName(QString::fromUtf8("label_encryption"));
        label_encryption->setMaximumSize(QSize(16, 16));
        label_encryption->setPixmap(QPixmap(QString::fromUtf8(":/icons/lock_closed")));

        horizontalLayout_6->addWidget(label_encryption);

        label_staking = new QLabel(wStatusBar);
        label_staking->setObjectName(QString::fromUtf8("label_staking"));
        label_staking->setText(QString::fromUtf8(""));
        label_staking->setPixmap(QPixmap(QString::fromUtf8(":/icons/staking_off")));

        horizontalLayout_6->addWidget(label_staking);

        label_connections = new QLabel(wStatusBar);
        label_connections->setObjectName(QString::fromUtf8("label_connections"));
        label_connections->setText(QString::fromUtf8(""));
        label_connections->setPixmap(QPixmap(QString::fromUtf8(":/icons/connect_1")));

        horizontalLayout_6->addWidget(label_connections);

        label_synced = new QLabel(wStatusBar);
        label_synced->setObjectName(QString::fromUtf8("label_synced"));
        label_synced->setPixmap(QPixmap(QString::fromUtf8(":/icons/synced")));

        horizontalLayout_6->addWidget(label_synced);

        horizontalSpacer_7 = new QSpacerItem(5, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_6->addItem(horizontalSpacer_7);

        horizontalLayout_6->setStretch(1, 1);
        horizontalLayout_6->setStretch(3, 1);

        verticalLayout_6->addWidget(wStatusBar);


        verticalLayout_8->addLayout(verticalLayout_6);

        horizontalLayout_8 = new QHBoxLayout();
        horizontalLayout_8->setSpacing(0);
        horizontalLayout_8->setObjectName(QString::fromUtf8("horizontalLayout_8"));

        verticalLayout_8->addLayout(horizontalLayout_8);

        MainWindow->setCentralWidget(centralWidget);

        retranslateUi(MainWindow);
        QObject::connect(pushButton_close, SIGNAL(clicked()), MainWindow, SLOT(close()));
        QObject::connect(pushButton_minimize, SIGNAL(clicked()), MainWindow, SLOT(showMinimized()));
        QObject::connect(pushButton_Transactions, SIGNAL(clicked()), MainWindow, SLOT(gotoHistoryPage()));
        QObject::connect(pushButton_Receive, SIGNAL(clicked()), MainWindow, SLOT(gotoReceiveCoinsPage()));
        QObject::connect(pushButton_Addressbook, SIGNAL(clicked()), MainWindow, SLOT(gotoAddressBookPage()));
        QObject::connect(pushButton_Send, SIGNAL(clicked()), MainWindow, SLOT(gotoSendCoinsPage()));
        QObject::connect(pushButton_Overview, SIGNAL(clicked()), MainWindow, SLOT(gotoOverviewPage()));
        QObject::connect(pushButton_Messages, SIGNAL(clicked()), MainWindow, SLOT(gotoMessagePage()));
        QObject::connect(pushButton_Settings, SIGNAL(clicked()), MainWindow, SLOT(menuSettingsRequested()));
        QObject::connect(pushButton_File, SIGNAL(clicked()), MainWindow, SLOT(menuFileRequested()));
        QObject::connect(pushButton_Help, SIGNAL(clicked()), MainWindow, SLOT(menuOperationsRequested()));

        stackedWidget->setCurrentIndex(-1);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "MainWindow", nullptr));
        pushButton_File->setText(QCoreApplication::translate("MainWindow", "File", nullptr));
        pushButton_Help->setText(QCoreApplication::translate("MainWindow", "Operations", nullptr));
        pushButton_Settings->setText(QCoreApplication::translate("MainWindow", "Help", nullptr));
        pushButton_Overview->setText(QCoreApplication::translate("MainWindow", "Overview", nullptr));
        pushButton_Send->setText(QCoreApplication::translate("MainWindow", "Send Coins", nullptr));
        pushButton_Receive->setText(QCoreApplication::translate("MainWindow", "Receive Coins", nullptr));
        pushButton_Addressbook->setText(QCoreApplication::translate("MainWindow", "Address Book", nullptr));
        pushButton_Transactions->setText(QCoreApplication::translate("MainWindow", "Transactions", nullptr));
        pushButton_Messages->setText(QCoreApplication::translate("MainWindow", "Messages", nullptr));
        label_synchronization->setText(QCoreApplication::translate("MainWindow", "Synchronization", nullptr));
        progressBar->setFormat(QString());
        label_blocks->setText(QCoreApplication::translate("MainWindow", "33333 blocks", nullptr));
        label_encryption->setText(QString());
        label_synced->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAINWINDOW_H
