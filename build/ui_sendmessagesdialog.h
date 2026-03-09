/********************************************************************************
** Form generated from reading UI file 'sendmessagesdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SENDMESSAGESDIALOG_H
#define UI_SENDMESSAGESDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QScrollArea>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "qvalidatedlineedit.h"

QT_BEGIN_NAMESPACE

class Ui_SendMessagesDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *borderframe;
    QVBoxLayout *verticalLayout_3;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout_20;
    QSpacerItem *horizontalSpacer_27;
    QLabel *picAdd_2;
    QLabel *lbTitle_2;
    QPushButton *bClose_2;
    QFrame *frameAddressFrom;
    QHBoxLayout *horizontalLayout_3;
    QLabel *addressFromLabel;
    QHBoxLayout *horizontalLayout_4;
    QValidatedLineEdit *addressFrom;
    QToolButton *addressBookButton;
    QToolButton *pasteButton;
    QScrollArea *scrollArea;
    QWidget *scrollAreaWidgetContents;
    QVBoxLayout *verticalLayout_2;
    QVBoxLayout *entries;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout;
    QPushButton *addButton;
    QPushButton *clearButton;
    QSpacerItem *horizontalSpacer;
    QPushButton *sendButton;
    QPushButton *closeButton;

    void setupUi(QDialog *SendMessagesDialog)
    {
        if (SendMessagesDialog->objectName().isEmpty())
            SendMessagesDialog->setObjectName(QString::fromUtf8("SendMessagesDialog"));
        SendMessagesDialog->resize(850, 432);
        SendMessagesDialog->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"	background-color: #000;\n"
"   "));
        verticalLayout = new QVBoxLayout(SendMessagesDialog);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        borderframe = new QFrame(SendMessagesDialog);
        borderframe->setObjectName(QString::fromUtf8("borderframe"));
        borderframe->setStyleSheet(QString::fromUtf8("#borderframe {\n"
"	border: 2px solid #f26522;\n"
"	}\n"
"\n"
"QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        borderframe->setFrameShape(QFrame::StyledPanel);
        borderframe->setFrameShadow(QFrame::Raised);
        verticalLayout_3 = new QVBoxLayout(borderframe);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        wCaption = new QWidget(borderframe);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 32));
        wCaption->setMaximumSize(QSize(16777215, 32));
        wCaption->setStyleSheet(QString::fromUtf8(""));
        wCaption->setProperty("is_header", QVariant(true));
        horizontalLayout_20 = new QHBoxLayout(wCaption);
        horizontalLayout_20->setObjectName(QString::fromUtf8("horizontalLayout_20"));
        horizontalLayout_20->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_27 = new QSpacerItem(7, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_20->addItem(horizontalSpacer_27);

        picAdd_2 = new QLabel(wCaption);
        picAdd_2->setObjectName(QString::fromUtf8("picAdd_2"));
        picAdd_2->setEnabled(true);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(picAdd_2->sizePolicy().hasHeightForWidth());
        picAdd_2->setSizePolicy(sizePolicy);
        picAdd_2->setMinimumSize(QSize(30, 30));
        picAdd_2->setMaximumSize(QSize(30, 30));
        picAdd_2->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/messages")));

        horizontalLayout_20->addWidget(picAdd_2);

        lbTitle_2 = new QLabel(wCaption);
        lbTitle_2->setObjectName(QString::fromUtf8("lbTitle_2"));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        lbTitle_2->setFont(font);
        lbTitle_2->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_20->addWidget(lbTitle_2);

        bClose_2 = new QPushButton(wCaption);
        bClose_2->setObjectName(QString::fromUtf8("bClose_2"));
        bClose_2->setMinimumSize(QSize(30, 30));
        bClose_2->setMaximumSize(QSize(30, 30));
        bClose_2->setStyleSheet(QString::fromUtf8("QPushButton {	\n"
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
        bClose_2->setText(QString::fromUtf8(""));
        bClose_2->setFlat(true);

        horizontalLayout_20->addWidget(bClose_2);


        verticalLayout_3->addWidget(wCaption);

        frameAddressFrom = new QFrame(borderframe);
        frameAddressFrom->setObjectName(QString::fromUtf8("frameAddressFrom"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(frameAddressFrom->sizePolicy().hasHeightForWidth());
        frameAddressFrom->setSizePolicy(sizePolicy1);
        frameAddressFrom->setMaximumSize(QSize(16777215, 45));
        frameAddressFrom->setStyleSheet(QString::fromUtf8("#frameAddressFrom {\n"
"	border: 1px solid #f26522;\n"
"	}"));
        frameAddressFrom->setFrameShape(QFrame::StyledPanel);
        frameAddressFrom->setFrameShadow(QFrame::Sunken);
        horizontalLayout_3 = new QHBoxLayout(frameAddressFrom);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        addressFromLabel = new QLabel(frameAddressFrom);
        addressFromLabel->setObjectName(QString::fromUtf8("addressFromLabel"));

        horizontalLayout_3->addWidget(addressFromLabel);

        horizontalLayout_4 = new QHBoxLayout();
        horizontalLayout_4->setSpacing(1);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        addressFrom = new QValidatedLineEdit(frameAddressFrom);
        addressFrom->setObjectName(QString::fromUtf8("addressFrom"));
        addressFrom->setMinimumSize(QSize(0, 22));
        addressFrom->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_4->addWidget(addressFrom);

        addressBookButton = new QToolButton(frameAddressFrom);
        addressBookButton->setObjectName(QString::fromUtf8("addressBookButton"));
        addressBookButton->setStyleSheet(QString::fromUtf8("QToolButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QToolButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}\n"
"\n"
"QToolButton:hover {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QToolButton:!enabled {\n"
"	color: #61280E;\n"
"	}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/menu_16/addressbook"), QSize(), QIcon::Normal, QIcon::Off);
        addressBookButton->setIcon(icon);

        horizontalLayout_4->addWidget(addressBookButton);

        pasteButton = new QToolButton(frameAddressFrom);
        pasteButton->setObjectName(QString::fromUtf8("pasteButton"));
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
        QBrush brush9(QColor(97, 40, 14, 255));
        brush9.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush9);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Light, brush2);
        palette.setBrush(QPalette::Disabled, QPalette::Midlight, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::Dark, brush4);
        palette.setBrush(QPalette::Disabled, QPalette::Mid, brush5);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush9);
        palette.setBrush(QPalette::Disabled, QPalette::BrightText, brush6);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush9);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Shadow, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ToolTipBase, brush8);
        palette.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush1);
        pasteButton->setPalette(palette);
        pasteButton->setStyleSheet(QString::fromUtf8("QToolButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"	}\n"
"\n"
"QToolButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"	}\n"
"\n"
"QToolButton:hover {\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QToolButton:!enabled {\n"
"	color: #61280E;\n"
"	}"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/menu_16/paste"), QSize(), QIcon::Normal, QIcon::Off);
        pasteButton->setIcon(icon1);

        horizontalLayout_4->addWidget(pasteButton);


        horizontalLayout_3->addLayout(horizontalLayout_4);


        verticalLayout_3->addWidget(frameAddressFrom);

        scrollArea = new QScrollArea(borderframe);
        scrollArea->setObjectName(QString::fromUtf8("scrollArea"));
        scrollArea->setStyleSheet(QString::fromUtf8("#scrollArea {border: 0px solid #f26522;}"));
        scrollArea->setWidgetResizable(true);
        scrollAreaWidgetContents = new QWidget();
        scrollAreaWidgetContents->setObjectName(QString::fromUtf8("scrollAreaWidgetContents"));
        scrollAreaWidgetContents->setGeometry(QRect(0, 0, 828, 321));
        verticalLayout_2 = new QVBoxLayout(scrollAreaWidgetContents);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        entries = new QVBoxLayout();
        entries->setSpacing(6);
        entries->setObjectName(QString::fromUtf8("entries"));

        verticalLayout_2->addLayout(entries);

        verticalSpacer = new QSpacerItem(20, 40, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        addButton = new QPushButton(scrollAreaWidgetContents);
        addButton->setObjectName(QString::fromUtf8("addButton"));
        addButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	}"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/add"), QSize(), QIcon::Normal, QIcon::Off);
        addButton->setIcon(icon2);
        addButton->setAutoDefault(false);

        horizontalLayout->addWidget(addButton);

        clearButton = new QPushButton(scrollAreaWidgetContents);
        clearButton->setObjectName(QString::fromUtf8("clearButton"));
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(clearButton->sizePolicy().hasHeightForWidth());
        clearButton->setSizePolicy(sizePolicy2);
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
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	}"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
        clearButton->setIcon(icon3);
        clearButton->setAutoRepeatDelay(300);
        clearButton->setAutoDefault(false);

        horizontalLayout->addWidget(clearButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        sendButton = new QPushButton(scrollAreaWidgetContents);
        sendButton->setObjectName(QString::fromUtf8("sendButton"));
        sendButton->setMinimumSize(QSize(80, 0));
        sendButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	}"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/menu_16/messages"), QSize(), QIcon::Normal, QIcon::Off);
        sendButton->setIcon(icon4);

        horizontalLayout->addWidget(sendButton);

        closeButton = new QPushButton(scrollAreaWidgetContents);
        closeButton->setObjectName(QString::fromUtf8("closeButton"));
        closeButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	}"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/quit"), QSize(), QIcon::Normal, QIcon::Off);
        closeButton->setIcon(icon5);

        horizontalLayout->addWidget(closeButton);


        verticalLayout_2->addLayout(horizontalLayout);

        scrollArea->setWidget(scrollAreaWidgetContents);

        verticalLayout_3->addWidget(scrollArea);


        verticalLayout->addWidget(borderframe);

#if QT_CONFIG(shortcut)
        addressFromLabel->setBuddy(addressFrom);
#endif // QT_CONFIG(shortcut)

        retranslateUi(SendMessagesDialog);
        QObject::connect(bClose_2, SIGNAL(clicked()), SendMessagesDialog, SLOT(reject()));

        sendButton->setDefault(true);


        QMetaObject::connectSlotsByName(SendMessagesDialog);
    } // setupUi

    void retranslateUi(QDialog *SendMessagesDialog)
    {
        SendMessagesDialog->setWindowTitle(QCoreApplication::translate("SendMessagesDialog", "Send Messages", nullptr));
        picAdd_2->setText(QString());
        lbTitle_2->setText(QCoreApplication::translate("SendMessagesDialog", "Send messages", nullptr));
        addressFromLabel->setText(QCoreApplication::translate("SendMessagesDialog", "Address &From:", nullptr));
#if QT_CONFIG(tooltip)
        addressBookButton->setToolTip(QCoreApplication::translate("SendMessagesDialog", "Choose address from address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addressBookButton->setText(QString());
#if QT_CONFIG(shortcut)
        addressBookButton->setShortcut(QCoreApplication::translate("SendMessagesDialog", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        pasteButton->setToolTip(QCoreApplication::translate("SendMessagesDialog", "Paste address from clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        pasteButton->setText(QString());
#if QT_CONFIG(shortcut)
        pasteButton->setShortcut(QCoreApplication::translate("SendMessagesDialog", "Alt+P", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        addButton->setToolTip(QCoreApplication::translate("SendMessagesDialog", "Send to multiple recipients at once", nullptr));
#endif // QT_CONFIG(tooltip)
        addButton->setText(QCoreApplication::translate("SendMessagesDialog", "Add &Recipient", nullptr));
#if QT_CONFIG(tooltip)
        clearButton->setToolTip(QCoreApplication::translate("SendMessagesDialog", "Remove all transaction fields", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton->setText(QCoreApplication::translate("SendMessagesDialog", "Clear &All", nullptr));
#if QT_CONFIG(tooltip)
        sendButton->setToolTip(QCoreApplication::translate("SendMessagesDialog", "Confirm the send action", nullptr));
#endif // QT_CONFIG(tooltip)
        sendButton->setText(QCoreApplication::translate("SendMessagesDialog", "S&end", nullptr));
        closeButton->setText(QCoreApplication::translate("SendMessagesDialog", "&Close", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SendMessagesDialog: public Ui_SendMessagesDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SENDMESSAGESDIALOG_H
