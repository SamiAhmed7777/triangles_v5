/********************************************************************************
** Form generated from reading UI file 'rpcconsole.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_RPCCONSOLE_H
#define UI_RPCCONSOLE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_RPCConsole
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
    QPushButton *bClose;
    QTabWidget *tabWidget;
    QWidget *tab_info;
    QGridLayout *gridLayout;
    QLabel *label_9;
    QLabel *label_5;
    QLabel *clientName;
    QLabel *label_6;
    QLabel *clientVersion;
    QLabel *label_14;
    QLabel *openSSLVersion;
    QLabel *label_12;
    QLabel *buildDate;
    QLabel *label_13;
    QLabel *startupTime;
    QLabel *label_11;
    QLabel *label_7;
    QLabel *numberOfConnections;
    QLabel *label_8;
    QCheckBox *isTestNet;
    QLabel *label_10;
    QLabel *label_3;
    QLabel *numberOfBlocks;
    QLabel *label_4;
    QLabel *totalBlocks;
    QLabel *label_2;
    QLabel *lastBlockTime;
    QSpacerItem *verticalSpacer_2;
    QLabel *labelDebugLogfile;
    QPushButton *openDebugLogfileButton;
    QLabel *labelCLOptions;
    QPushButton *showCLOptionsButton;
    QSpacerItem *verticalSpacer;
    QWidget *tab_console;
    QVBoxLayout *verticalLayout_3;
    QTextEdit *messagesWidget;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QLineEdit *lineEdit;
    QPushButton *clearButton;

    void setupUi(QDialog *RPCConsole)
    {
        if (RPCConsole->objectName().isEmpty())
            RPCConsole->setObjectName(QString::fromUtf8("RPCConsole"));
        RPCConsole->resize(740, 450);
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
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        QBrush brush6(QColor(255, 255, 255, 255));
        brush6.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::BrightText, brush6);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
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
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::BrightText, brush6);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Shadow, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush7);
        palette.setBrush(QPalette::Inactive, QPalette::ToolTipBase, brush8);
        palette.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Light, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::Midlight, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::Dark, brush4);
        palette.setBrush(QPalette::Disabled, QPalette::Mid, brush5);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette.setBrush(QPalette::Disabled, QPalette::BrightText, brush6);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Shadow, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ToolTipBase, brush8);
        palette.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush1);
        RPCConsole->setPalette(palette);
        RPCConsole->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"	background-color: #000;\n"
"   "));
        verticalLayout = new QVBoxLayout(RPCConsole);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        borderframe = new QFrame(RPCConsole);
        borderframe->setObjectName(QString::fromUtf8("borderframe"));
        borderframe->setStyleSheet(QString::fromUtf8("#borderframe {\n"
"	border: 2px solid #f26522;\n"
"      }"));
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
        picAdd->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/debug")));

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

        bClose = new QPushButton(wCaption);
        bClose->setObjectName(QString::fromUtf8("bClose"));
        bClose->setMinimumSize(QSize(30, 30));
        bClose->setMaximumSize(QSize(30, 30));
        bClose->setStyleSheet(QString::fromUtf8("QPushButton {	\n"
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
        bClose->setText(QString::fromUtf8(""));
        bClose->setAutoDefault(false);
        bClose->setFlat(true);

        horizontalLayout_19->addWidget(bClose);


        verticalLayout_2->addWidget(wCaption);

        tabWidget = new QTabWidget(borderframe);
        tabWidget->setObjectName(QString::fromUtf8("tabWidget"));
        tabWidget->setStyleSheet(QString::fromUtf8("QTabWidget::pane {\n"
"	border: 1px solid #f26522;\n"
"	}\n"
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
"	}"));
        tab_info = new QWidget();
        tab_info->setObjectName(QString::fromUtf8("tab_info"));
        gridLayout = new QGridLayout(tab_info);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        gridLayout->setHorizontalSpacing(12);
        label_9 = new QLabel(tab_info);
        label_9->setObjectName(QString::fromUtf8("label_9"));
        QFont font1;
        font1.setBold(true);
        font1.setWeight(75);
        label_9->setFont(font1);

        gridLayout->addWidget(label_9, 0, 0, 1, 1);

        label_5 = new QLabel(tab_info);
        label_5->setObjectName(QString::fromUtf8("label_5"));

        gridLayout->addWidget(label_5, 1, 0, 1, 1);

        clientName = new QLabel(tab_info);
        clientName->setObjectName(QString::fromUtf8("clientName"));
        clientName->setCursor(QCursor(Qt::IBeamCursor));
        clientName->setTextFormat(Qt::PlainText);
        clientName->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(clientName, 1, 1, 1, 1);

        label_6 = new QLabel(tab_info);
        label_6->setObjectName(QString::fromUtf8("label_6"));

        gridLayout->addWidget(label_6, 2, 0, 1, 1);

        clientVersion = new QLabel(tab_info);
        clientVersion->setObjectName(QString::fromUtf8("clientVersion"));
        clientVersion->setCursor(QCursor(Qt::IBeamCursor));
        clientVersion->setTextFormat(Qt::PlainText);
        clientVersion->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(clientVersion, 2, 1, 1, 1);

        label_14 = new QLabel(tab_info);
        label_14->setObjectName(QString::fromUtf8("label_14"));
        label_14->setIndent(10);

        gridLayout->addWidget(label_14, 3, 0, 1, 1);

        openSSLVersion = new QLabel(tab_info);
        openSSLVersion->setObjectName(QString::fromUtf8("openSSLVersion"));
        openSSLVersion->setCursor(QCursor(Qt::IBeamCursor));
        openSSLVersion->setTextFormat(Qt::PlainText);
        openSSLVersion->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(openSSLVersion, 3, 1, 1, 1);

        label_12 = new QLabel(tab_info);
        label_12->setObjectName(QString::fromUtf8("label_12"));

        gridLayout->addWidget(label_12, 4, 0, 1, 1);

        buildDate = new QLabel(tab_info);
        buildDate->setObjectName(QString::fromUtf8("buildDate"));
        buildDate->setCursor(QCursor(Qt::IBeamCursor));
        buildDate->setTextFormat(Qt::PlainText);
        buildDate->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(buildDate, 4, 1, 1, 1);

        label_13 = new QLabel(tab_info);
        label_13->setObjectName(QString::fromUtf8("label_13"));

        gridLayout->addWidget(label_13, 5, 0, 1, 1);

        startupTime = new QLabel(tab_info);
        startupTime->setObjectName(QString::fromUtf8("startupTime"));
        startupTime->setCursor(QCursor(Qt::IBeamCursor));
        startupTime->setTextFormat(Qt::PlainText);
        startupTime->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(startupTime, 5, 1, 1, 1);

        label_11 = new QLabel(tab_info);
        label_11->setObjectName(QString::fromUtf8("label_11"));
        label_11->setFont(font1);

        gridLayout->addWidget(label_11, 6, 0, 1, 1);

        label_7 = new QLabel(tab_info);
        label_7->setObjectName(QString::fromUtf8("label_7"));

        gridLayout->addWidget(label_7, 7, 0, 1, 1);

        numberOfConnections = new QLabel(tab_info);
        numberOfConnections->setObjectName(QString::fromUtf8("numberOfConnections"));
        numberOfConnections->setCursor(QCursor(Qt::IBeamCursor));
        numberOfConnections->setTextFormat(Qt::PlainText);
        numberOfConnections->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(numberOfConnections, 7, 1, 1, 1);

        label_8 = new QLabel(tab_info);
        label_8->setObjectName(QString::fromUtf8("label_8"));

        gridLayout->addWidget(label_8, 8, 0, 1, 1);

        isTestNet = new QCheckBox(tab_info);
        isTestNet->setObjectName(QString::fromUtf8("isTestNet"));
        isTestNet->setEnabled(false);
        isTestNet->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        gridLayout->addWidget(isTestNet, 8, 1, 1, 1);

        label_10 = new QLabel(tab_info);
        label_10->setObjectName(QString::fromUtf8("label_10"));
        label_10->setFont(font1);

        gridLayout->addWidget(label_10, 9, 0, 1, 1);

        label_3 = new QLabel(tab_info);
        label_3->setObjectName(QString::fromUtf8("label_3"));

        gridLayout->addWidget(label_3, 10, 0, 1, 1);

        numberOfBlocks = new QLabel(tab_info);
        numberOfBlocks->setObjectName(QString::fromUtf8("numberOfBlocks"));
        numberOfBlocks->setCursor(QCursor(Qt::IBeamCursor));
        numberOfBlocks->setTextFormat(Qt::PlainText);
        numberOfBlocks->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(numberOfBlocks, 10, 1, 1, 1);

        label_4 = new QLabel(tab_info);
        label_4->setObjectName(QString::fromUtf8("label_4"));

        gridLayout->addWidget(label_4, 11, 0, 1, 1);

        totalBlocks = new QLabel(tab_info);
        totalBlocks->setObjectName(QString::fromUtf8("totalBlocks"));
        totalBlocks->setCursor(QCursor(Qt::IBeamCursor));
        totalBlocks->setTextFormat(Qt::PlainText);
        totalBlocks->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(totalBlocks, 11, 1, 1, 1);

        label_2 = new QLabel(tab_info);
        label_2->setObjectName(QString::fromUtf8("label_2"));

        gridLayout->addWidget(label_2, 12, 0, 1, 1);

        lastBlockTime = new QLabel(tab_info);
        lastBlockTime->setObjectName(QString::fromUtf8("lastBlockTime"));
        lastBlockTime->setCursor(QCursor(Qt::IBeamCursor));
        lastBlockTime->setTextFormat(Qt::PlainText);
        lastBlockTime->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        gridLayout->addWidget(lastBlockTime, 12, 1, 1, 1);

        verticalSpacer_2 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(verticalSpacer_2, 13, 0, 1, 1);

        labelDebugLogfile = new QLabel(tab_info);
        labelDebugLogfile->setObjectName(QString::fromUtf8("labelDebugLogfile"));
        labelDebugLogfile->setFont(font1);

        gridLayout->addWidget(labelDebugLogfile, 14, 0, 1, 1);

        openDebugLogfileButton = new QPushButton(tab_info);
        openDebugLogfileButton->setObjectName(QString::fromUtf8("openDebugLogfileButton"));
        openDebugLogfileButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	border: 1px solid #f26522;\n"
"	padding: 3px 20px 3px 20px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	border: 1px solid #61280E;\n"
"	}"));
        openDebugLogfileButton->setAutoDefault(false);

        gridLayout->addWidget(openDebugLogfileButton, 15, 0, 1, 1);

        labelCLOptions = new QLabel(tab_info);
        labelCLOptions->setObjectName(QString::fromUtf8("labelCLOptions"));
        labelCLOptions->setFont(font1);

        gridLayout->addWidget(labelCLOptions, 16, 0, 1, 1);

        showCLOptionsButton = new QPushButton(tab_info);
        showCLOptionsButton->setObjectName(QString::fromUtf8("showCLOptionsButton"));
        showCLOptionsButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	border: 1px solid #f26522;\n"
"	padding: 3px 20px 3px 20px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	border: 1px solid #61280E;\n"
"	}"));
        showCLOptionsButton->setAutoDefault(false);

        gridLayout->addWidget(showCLOptionsButton, 17, 0, 1, 1);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        gridLayout->addItem(verticalSpacer, 18, 0, 1, 1);

        gridLayout->setColumnStretch(1, 1);
        tabWidget->addTab(tab_info, QString());
        tab_console = new QWidget();
        tab_console->setObjectName(QString::fromUtf8("tab_console"));
        verticalLayout_3 = new QVBoxLayout(tab_console);
        verticalLayout_3->setSpacing(3);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        messagesWidget = new QTextEdit(tab_console);
        messagesWidget->setObjectName(QString::fromUtf8("messagesWidget"));
        messagesWidget->setMinimumSize(QSize(0, 100));
        messagesWidget->setStyleSheet(QString::fromUtf8("border: 1px solid #f26522;"));
        messagesWidget->setReadOnly(true);
        messagesWidget->setProperty("tabKeyNavigation", QVariant(false));
        messagesWidget->setProperty("columnCount", QVariant(2));

        verticalLayout_3->addWidget(messagesWidget);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(3);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label = new QLabel(tab_console);
        label->setObjectName(QString::fromUtf8("label"));
        label->setText(QString::fromUtf8(">"));

        horizontalLayout->addWidget(label);

        lineEdit = new QLineEdit(tab_console);
        lineEdit->setObjectName(QString::fromUtf8("lineEdit"));
        lineEdit->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        horizontalLayout->addWidget(lineEdit);

        clearButton = new QPushButton(tab_console);
        clearButton->setObjectName(QString::fromUtf8("clearButton"));
        clearButton->setMaximumSize(QSize(24, 24));
        clearButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}\n"
"\n"
"QPushButton:hover {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
        clearButton->setIcon(icon);
#if QT_CONFIG(shortcut)
        clearButton->setShortcut(QString::fromUtf8("Ctrl+L"));
#endif // QT_CONFIG(shortcut)
        clearButton->setAutoDefault(false);

        horizontalLayout->addWidget(clearButton);


        verticalLayout_3->addLayout(horizontalLayout);

        tabWidget->addTab(tab_console, QString());

        verticalLayout_2->addWidget(tabWidget);


        verticalLayout->addWidget(borderframe);


        retranslateUi(RPCConsole);
        QObject::connect(bClose, SIGNAL(clicked()), RPCConsole, SLOT(reject()));

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(RPCConsole);
    } // setupUi

    void retranslateUi(QDialog *RPCConsole)
    {
        RPCConsole->setWindowTitle(QCoreApplication::translate("RPCConsole", "Triangles - Debug window", nullptr));
        picAdd->setText(QString());
        lbTitle->setText(QCoreApplication::translate("RPCConsole", "Debug window", nullptr));
        label_9->setText(QCoreApplication::translate("RPCConsole", "Triangles Core", nullptr));
        label_5->setText(QCoreApplication::translate("RPCConsole", "Client name", nullptr));
        clientName->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_6->setText(QCoreApplication::translate("RPCConsole", "Client version", nullptr));
        clientVersion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_14->setText(QCoreApplication::translate("RPCConsole", "Using OpenSSL version", nullptr));
        openSSLVersion->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_12->setText(QCoreApplication::translate("RPCConsole", "Build date", nullptr));
        buildDate->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_13->setText(QCoreApplication::translate("RPCConsole", "Startup time", nullptr));
        startupTime->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_11->setText(QCoreApplication::translate("RPCConsole", "Network", nullptr));
        label_7->setText(QCoreApplication::translate("RPCConsole", "Number of connections", nullptr));
        numberOfConnections->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_8->setText(QCoreApplication::translate("RPCConsole", "On testnet", nullptr));
        isTestNet->setText(QString());
        label_10->setText(QCoreApplication::translate("RPCConsole", "Block chain", nullptr));
        label_3->setText(QCoreApplication::translate("RPCConsole", "Current number of blocks", nullptr));
        numberOfBlocks->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_4->setText(QCoreApplication::translate("RPCConsole", "Estimated total blocks", nullptr));
        totalBlocks->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        label_2->setText(QCoreApplication::translate("RPCConsole", "Last block time", nullptr));
        lastBlockTime->setText(QCoreApplication::translate("RPCConsole", "N/A", nullptr));
        labelDebugLogfile->setText(QCoreApplication::translate("RPCConsole", "Debug log file", nullptr));
#if QT_CONFIG(tooltip)
        openDebugLogfileButton->setToolTip(QCoreApplication::translate("RPCConsole", "Open the Triangles debug log file from the current data directory. This can take a few seconds for large log files.", nullptr));
#endif // QT_CONFIG(tooltip)
        openDebugLogfileButton->setText(QCoreApplication::translate("RPCConsole", "&Open", nullptr));
        labelCLOptions->setText(QCoreApplication::translate("RPCConsole", "Command-line options", nullptr));
#if QT_CONFIG(tooltip)
        showCLOptionsButton->setToolTip(QCoreApplication::translate("RPCConsole", "Show the Triangles-Qt help message to get a list with possible Triangles command-line options.", nullptr));
#endif // QT_CONFIG(tooltip)
        showCLOptionsButton->setText(QCoreApplication::translate("RPCConsole", "&Show", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(tab_info), QCoreApplication::translate("RPCConsole", "&Information", nullptr));
#if QT_CONFIG(tooltip)
        clearButton->setToolTip(QCoreApplication::translate("RPCConsole", "Clear console", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton->setText(QString());
        tabWidget->setTabText(tabWidget->indexOf(tab_console), QCoreApplication::translate("RPCConsole", "&Console", nullptr));
    } // retranslateUi

};

namespace Ui {
    class RPCConsole: public Ui_RPCConsole {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_RPCCONSOLE_H
