/********************************************************************************
** Form generated from reading UI file 'sendmessagesentry.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SENDMESSAGESENTRY_H
#define UI_SENDMESSAGESENTRY_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include "qvalidatedlineedit.h"
#include "qvalidatedtextedit.h"

QT_BEGIN_NAMESPACE

class Ui_SendMessagesEntry
{
public:
    QGridLayout *gridLayout;
    QHBoxLayout *sendToLayout;
    QValidatedLineEdit *sendTo;
    QToolButton *addressBookButton;
    QToolButton *pasteButton;
    QToolButton *deleteButton;
    QLabel *messageLabel;
    QLabel *label_2;
    QValidatedTextEdit *messageText;
    QValidatedLineEdit *addAsLabel;
    QLabel *label_4;
    QLabel *publicKeyLabel;
    QValidatedLineEdit *publicKey;

    void setupUi(QFrame *SendMessagesEntry)
    {
        if (SendMessagesEntry->objectName().isEmpty())
            SendMessagesEntry->setObjectName(QString::fromUtf8("SendMessagesEntry"));
        SendMessagesEntry->resize(729, 236);
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
        SendMessagesEntry->setPalette(palette);
        SendMessagesEntry->setStyleSheet(QString::fromUtf8("#SendMessagesEntry{\n"
"	background-color: #000;\n"
"	border: 1px solid #f26522;\n"
"	}\n"
"\n"
"QLineEdit, QPlainTextEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        SendMessagesEntry->setFrameShape(QFrame::StyledPanel);
        SendMessagesEntry->setFrameShadow(QFrame::Sunken);
        gridLayout = new QGridLayout(SendMessagesEntry);
        gridLayout->setSpacing(12);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        sendToLayout = new QHBoxLayout();
        sendToLayout->setSpacing(0);
        sendToLayout->setObjectName(QString::fromUtf8("sendToLayout"));
        sendTo = new QValidatedLineEdit(SendMessagesEntry);
        sendTo->setObjectName(QString::fromUtf8("sendTo"));
        sendTo->setMinimumSize(QSize(0, 22));
        sendTo->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        sendTo->setMaxLength(32767);

        sendToLayout->addWidget(sendTo);

        addressBookButton = new QToolButton(SendMessagesEntry);
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

        sendToLayout->addWidget(addressBookButton);

        pasteButton = new QToolButton(SendMessagesEntry);
        pasteButton->setObjectName(QString::fromUtf8("pasteButton"));
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

        sendToLayout->addWidget(pasteButton);

        deleteButton = new QToolButton(SendMessagesEntry);
        deleteButton->setObjectName(QString::fromUtf8("deleteButton"));
        deleteButton->setStyleSheet(QString::fromUtf8("QToolButton {\n"
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
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
        deleteButton->setIcon(icon2);

        sendToLayout->addWidget(deleteButton);


        gridLayout->addLayout(sendToLayout, 3, 2, 1, 1);

        messageLabel = new QLabel(SendMessagesEntry);
        messageLabel->setObjectName(QString::fromUtf8("messageLabel"));
        messageLabel->setStyleSheet(QString::fromUtf8("color: #f26522;"));
        messageLabel->setAlignment(Qt::AlignRight|Qt::AlignTop|Qt::AlignTrailing);

        gridLayout->addWidget(messageLabel, 6, 0, 1, 1);

        label_2 = new QLabel(SendMessagesEntry);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setStyleSheet(QString::fromUtf8("color: #f26522;"));
        label_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(label_2, 3, 0, 1, 1);

        messageText = new QValidatedTextEdit(SendMessagesEntry);
        messageText->setObjectName(QString::fromUtf8("messageText"));
        QPalette palette1;
        QBrush brush9(QColor(28, 28, 28, 255));
        brush9.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::Button, brush9);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush9);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush9);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush9);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush9);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush9);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush9);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush9);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush9);
        messageText->setPalette(palette1);
        messageText->setMouseTracking(true);
        messageText->setFocusPolicy(Qt::WheelFocus);
        messageText->setStyleSheet(QString::fromUtf8("\n"
"	/* style set in qvalidatedtextedit.cpp */\n"
"      "));
        messageText->setTabChangesFocus(false);

        gridLayout->addWidget(messageText, 6, 2, 1, 1);

        addAsLabel = new QValidatedLineEdit(SendMessagesEntry);
        addAsLabel->setObjectName(QString::fromUtf8("addAsLabel"));
        addAsLabel->setEnabled(true);
        addAsLabel->setMinimumSize(QSize(0, 22));
        addAsLabel->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        gridLayout->addWidget(addAsLabel, 4, 2, 1, 1);

        label_4 = new QLabel(SendMessagesEntry);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setStyleSheet(QString::fromUtf8("color: #f26522;"));
        label_4->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(label_4, 4, 0, 1, 1);

        publicKeyLabel = new QLabel(SendMessagesEntry);
        publicKeyLabel->setObjectName(QString::fromUtf8("publicKeyLabel"));
        publicKeyLabel->setStyleSheet(QString::fromUtf8("color: #f26522;"));

        gridLayout->addWidget(publicKeyLabel, 5, 0, 1, 1);

        publicKey = new QValidatedLineEdit(SendMessagesEntry);
        publicKey->setObjectName(QString::fromUtf8("publicKey"));
        publicKey->setMinimumSize(QSize(0, 22));
        publicKey->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        gridLayout->addWidget(publicKey, 5, 2, 1, 1);

#if QT_CONFIG(shortcut)
        messageLabel->setBuddy(messageText);
        label_2->setBuddy(sendTo);
        label_4->setBuddy(addAsLabel);
        publicKeyLabel->setBuddy(publicKey);
#endif // QT_CONFIG(shortcut)

        retranslateUi(SendMessagesEntry);

        QMetaObject::connectSlotsByName(SendMessagesEntry);
    } // setupUi

    void retranslateUi(QFrame *SendMessagesEntry)
    {
        SendMessagesEntry->setWindowTitle(QCoreApplication::translate("SendMessagesEntry", "Form", nullptr));
#if QT_CONFIG(tooltip)
        sendTo->setToolTip(QCoreApplication::translate("SendMessagesEntry", "The address to send the message to  (e.g. TXc7mPCNFFpinDonuSH5PNVY9S8nBcvGQm)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        addressBookButton->setToolTip(QCoreApplication::translate("SendMessagesEntry", "Choose address from address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addressBookButton->setText(QString());
#if QT_CONFIG(shortcut)
        addressBookButton->setShortcut(QCoreApplication::translate("SendMessagesEntry", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        pasteButton->setToolTip(QCoreApplication::translate("SendMessagesEntry", "Paste address from clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        pasteButton->setText(QString());
#if QT_CONFIG(shortcut)
        pasteButton->setShortcut(QCoreApplication::translate("SendMessagesEntry", "Alt+P", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        deleteButton->setToolTip(QCoreApplication::translate("SendMessagesEntry", "Remove this recipient", nullptr));
#endif // QT_CONFIG(tooltip)
        deleteButton->setText(QString());
        messageLabel->setText(QCoreApplication::translate("SendMessagesEntry", "&Message:", nullptr));
        label_2->setText(QCoreApplication::translate("SendMessagesEntry", "Send &To:", nullptr));
#if QT_CONFIG(tooltip)
        addAsLabel->setToolTip(QCoreApplication::translate("SendMessagesEntry", "Enter a label for this address to add it to your address book", nullptr));
#endif // QT_CONFIG(tooltip)
        label_4->setText(QCoreApplication::translate("SendMessagesEntry", "&Label:", nullptr));
        publicKeyLabel->setText(QCoreApplication::translate("SendMessagesEntry", "&Public Key:", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SendMessagesEntry: public Ui_SendMessagesEntry {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SENDMESSAGESENTRY_H
