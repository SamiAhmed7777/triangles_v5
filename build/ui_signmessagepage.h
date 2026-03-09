/********************************************************************************
** Form generated from reading UI file 'signmessagepage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SIGNMESSAGEPAGE_H
#define UI_SIGNMESSAGEPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "qvalidatedlineedit.h"

QT_BEGIN_NAMESPACE

class Ui_SignMessagePage
{
public:
    QVBoxLayout *verticalLayout;
    QWidget *wHeader;
    QHBoxLayout *horizontalLayout_4;
    QLabel *icon_messages;
    QSpacerItem *horizontalSpacer_send_1;
    QLabel *label_messages;
    QWidget *wContainer;
    QVBoxLayout *verticalLayout_2;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_29;
    QVBoxLayout *verticalLayout_3;
    QGridLayout *gridLayout_5;
    QLabel *label_39;
    QPushButton *pasteButton_SM;
    QLabel *label_40;
    QPushButton *addressBookButton_SM;
    QValidatedLineEdit *addressIn_SM;
    QLabel *label_41;
    QPlainTextEdit *messageIn_SM;
    QPushButton *copySignatureButton_SM;
    QLineEdit *signatureOut_SM;
    QLabel *statusLabel_SM;
    QWidget *wAcceptHolder;
    QHBoxLayout *horizontalLayout_22;
    QSpacerItem *horizontalSpacer_30;
    QPushButton *clearButton_SM;
    QPushButton *signMessageButton_SM;
    QSpacerItem *horizontalSpacer;
    QSpacerItem *horizontalSpacer_31;
    QSpacerItem *verticalSpacer_2;

    void setupUi(QWidget *SignMessagePage)
    {
        if (SignMessagePage->objectName().isEmpty())
            SignMessagePage->setObjectName(QString::fromUtf8("SignMessagePage"));
        SignMessagePage->resize(750, 600);
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
        SignMessagePage->setPalette(palette);
        SignMessagePage->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"background-color: #000;\n"
"\n"
"QLineEdit {\n"
"	border: 1px solid #f26522;\n"
"	}"));
        verticalLayout = new QVBoxLayout(SignMessagePage);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        wHeader = new QWidget(SignMessagePage);
        wHeader->setObjectName(QString::fromUtf8("wHeader"));
        wHeader->setMinimumSize(QSize(0, 40));
        wHeader->setMaximumSize(QSize(16777215, 40));
        wHeader->setStyleSheet(QString::fromUtf8(""));
        wHeader->setProperty("is_header", QVariant(true));
        horizontalLayout_4 = new QHBoxLayout(wHeader);
        horizontalLayout_4->setSpacing(6);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(9, 9, -1, 9);
        icon_messages = new QLabel(wHeader);
        icon_messages->setObjectName(QString::fromUtf8("icon_messages"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(icon_messages->sizePolicy().hasHeightForWidth());
        icon_messages->setSizePolicy(sizePolicy);
        icon_messages->setMinimumSize(QSize(30, 30));
        icon_messages->setMaximumSize(QSize(30, 30));
        icon_messages->setBaseSize(QSize(0, 0));
        icon_messages->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/sign")));
        icon_messages->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        horizontalLayout_4->addWidget(icon_messages);

        horizontalSpacer_send_1 = new QSpacerItem(4, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer_send_1);

        label_messages = new QLabel(wHeader);
        label_messages->setObjectName(QString::fromUtf8("label_messages"));
        label_messages->setMinimumSize(QSize(0, 30));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        label_messages->setFont(font);
        label_messages->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_4->addWidget(label_messages);


        verticalLayout->addWidget(wHeader);

        wContainer = new QWidget(SignMessagePage);
        wContainer->setObjectName(QString::fromUtf8("wContainer"));
        wContainer->setStyleSheet(QString::fromUtf8(""));
        verticalLayout_2 = new QVBoxLayout(wContainer);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer_29 = new QSpacerItem(136, 216, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_29);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        verticalLayout_3->setContentsMargins(-1, 0, -1, -1);
        gridLayout_5 = new QGridLayout();
        gridLayout_5->setSpacing(6);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        gridLayout_5->setContentsMargins(-1, 6, -1, 3);
        label_39 = new QLabel(wContainer);
        label_39->setObjectName(QString::fromUtf8("label_39"));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(true);
        font1.setWeight(75);
        label_39->setFont(font1);
        label_39->setStyleSheet(QString::fromUtf8(""));
        label_39->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_39->setMargin(5);

        gridLayout_5->addWidget(label_39, 0, 0, 1, 1);

        pasteButton_SM = new QPushButton(wContainer);
        pasteButton_SM->setObjectName(QString::fromUtf8("pasteButton_SM"));
        pasteButton_SM->setMinimumSize(QSize(28, 28));
        pasteButton_SM->setMaximumSize(QSize(28, 28));
        pasteButton_SM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/menu_16/paste"), QSize(), QIcon::Normal, QIcon::Off);
        pasteButton_SM->setIcon(icon);
        pasteButton_SM->setIconSize(QSize(30, 30));
        pasteButton_SM->setFlat(true);

        gridLayout_5->addWidget(pasteButton_SM, 0, 2, 1, 1);

        label_40 = new QLabel(wContainer);
        label_40->setObjectName(QString::fromUtf8("label_40"));
        label_40->setFont(font1);
        label_40->setStyleSheet(QString::fromUtf8(""));
        label_40->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_40->setMargin(5);

        gridLayout_5->addWidget(label_40, 2, 0, 1, 1);

        addressBookButton_SM = new QPushButton(wContainer);
        addressBookButton_SM->setObjectName(QString::fromUtf8("addressBookButton_SM"));
        addressBookButton_SM->setMinimumSize(QSize(28, 28));
        addressBookButton_SM->setMaximumSize(QSize(28, 28));
        addressBookButton_SM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/menu_16/addressbook"), QSize(), QIcon::Normal, QIcon::Off);
        addressBookButton_SM->setIcon(icon1);
        addressBookButton_SM->setIconSize(QSize(30, 30));
        addressBookButton_SM->setFlat(true);

        gridLayout_5->addWidget(addressBookButton_SM, 0, 3, 1, 1);

        addressIn_SM = new QValidatedLineEdit(wContainer);
        addressIn_SM->setObjectName(QString::fromUtf8("addressIn_SM"));
        addressIn_SM->setMinimumSize(QSize(0, 22));
        QFont font2;
        font2.setPointSize(10);
        addressIn_SM->setFont(font2);
        addressIn_SM->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        addressIn_SM->setMaxLength(34);

        gridLayout_5->addWidget(addressIn_SM, 0, 1, 1, 1);

        label_41 = new QLabel(wContainer);
        label_41->setObjectName(QString::fromUtf8("label_41"));
        label_41->setFont(font1);
        label_41->setStyleSheet(QString::fromUtf8(""));
        label_41->setAlignment(Qt::AlignRight|Qt::AlignTop|Qt::AlignTrailing);
        label_41->setMargin(5);

        gridLayout_5->addWidget(label_41, 1, 0, 1, 1);

        messageIn_SM = new QPlainTextEdit(wContainer);
        messageIn_SM->setObjectName(QString::fromUtf8("messageIn_SM"));
        messageIn_SM->setMinimumSize(QSize(0, 300));
        messageIn_SM->setMaximumSize(QSize(16777215, 300));
        messageIn_SM->setFont(font2);
        messageIn_SM->setStyleSheet(QString::fromUtf8("QPlainTextEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        messageIn_SM->setBackgroundVisible(false);

        gridLayout_5->addWidget(messageIn_SM, 1, 1, 1, 1);

        copySignatureButton_SM = new QPushButton(wContainer);
        copySignatureButton_SM->setObjectName(QString::fromUtf8("copySignatureButton_SM"));
        copySignatureButton_SM->setMinimumSize(QSize(28, 28));
        copySignatureButton_SM->setMaximumSize(QSize(28, 28));
        copySignatureButton_SM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        icon2.addFile(QString::fromUtf8(":/menu_16/copy"), QSize(), QIcon::Normal, QIcon::Off);
        copySignatureButton_SM->setIcon(icon2);
        copySignatureButton_SM->setIconSize(QSize(30, 30));
        copySignatureButton_SM->setFlat(true);

        gridLayout_5->addWidget(copySignatureButton_SM, 2, 2, 1, 1);

        signatureOut_SM = new QLineEdit(wContainer);
        signatureOut_SM->setObjectName(QString::fromUtf8("signatureOut_SM"));
        signatureOut_SM->setMinimumSize(QSize(0, 22));
        signatureOut_SM->setFont(font2);
        signatureOut_SM->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        signatureOut_SM->setReadOnly(true);

        gridLayout_5->addWidget(signatureOut_SM, 2, 1, 1, 1);


        verticalLayout_3->addLayout(gridLayout_5);

        statusLabel_SM = new QLabel(wContainer);
        statusLabel_SM->setObjectName(QString::fromUtf8("statusLabel_SM"));
        statusLabel_SM->setFont(font1);
        statusLabel_SM->setAlignment(Qt::AlignCenter);
        statusLabel_SM->setWordWrap(true);

        verticalLayout_3->addWidget(statusLabel_SM);

        wAcceptHolder = new QWidget(wContainer);
        wAcceptHolder->setObjectName(QString::fromUtf8("wAcceptHolder"));
        wAcceptHolder->setMinimumSize(QSize(0, 26));
        horizontalLayout_22 = new QHBoxLayout(wAcceptHolder);
        horizontalLayout_22->setObjectName(QString::fromUtf8("horizontalLayout_22"));
        horizontalLayout_22->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_30 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_22->addItem(horizontalSpacer_30);

        clearButton_SM = new QPushButton(wAcceptHolder);
        clearButton_SM->setObjectName(QString::fromUtf8("clearButton_SM"));
        clearButton_SM->setMinimumSize(QSize(122, 22));
        clearButton_SM->setMaximumSize(QSize(122, 22));
        QFont font3;
        font3.setPointSize(10);
        font3.setStyleStrategy(QFont::PreferAntialias);
        clearButton_SM->setFont(font3);
        clearButton_SM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 120px;\n"
"	min-width: 120px;\n"
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
        clearButton_SM->setFlat(true);

        horizontalLayout_22->addWidget(clearButton_SM);

        signMessageButton_SM = new QPushButton(wAcceptHolder);
        signMessageButton_SM->setObjectName(QString::fromUtf8("signMessageButton_SM"));
        signMessageButton_SM->setFont(font3);
        signMessageButton_SM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 120px;\n"
"	min-width: 120px;\n"
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
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/menu_16/sign"), QSize(), QIcon::Normal, QIcon::Off);
        signMessageButton_SM->setIcon(icon3);
        signMessageButton_SM->setFlat(true);

        horizontalLayout_22->addWidget(signMessageButton_SM);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_22->addItem(horizontalSpacer);


        verticalLayout_3->addWidget(wAcceptHolder);


        horizontalLayout->addLayout(verticalLayout_3);

        horizontalSpacer_31 = new QSpacerItem(136, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_31);

        horizontalLayout->setStretch(0, 1);
        horizontalLayout->setStretch(1, 7);
        horizontalLayout->setStretch(2, 1);

        verticalLayout_2->addLayout(horizontalLayout);

        verticalSpacer_2 = new QSpacerItem(20, 48, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer_2);


        verticalLayout->addWidget(wContainer);


        retranslateUi(SignMessagePage);

        QMetaObject::connectSlotsByName(SignMessagePage);
    } // setupUi

    void retranslateUi(QWidget *SignMessagePage)
    {
        SignMessagePage->setWindowTitle(QCoreApplication::translate("SignMessagePage", "Form", nullptr));
        icon_messages->setText(QString());
        label_messages->setText(QCoreApplication::translate("SignMessagePage", "Sign Message", nullptr));
        label_39->setText(QCoreApplication::translate("SignMessagePage", "Address", nullptr));
#if QT_CONFIG(tooltip)
        pasteButton_SM->setToolTip(QCoreApplication::translate("SignMessagePage", "Paste address from clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        pasteButton_SM->setShortcut(QCoreApplication::translate("SignMessagePage", "Alt+P", nullptr));
#endif // QT_CONFIG(shortcut)
        label_40->setText(QCoreApplication::translate("SignMessagePage", "Signature", nullptr));
#if QT_CONFIG(tooltip)
        addressBookButton_SM->setToolTip(QCoreApplication::translate("SignMessagePage", "Choose an address from the address book", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        addressBookButton_SM->setShortcut(QCoreApplication::translate("SignMessagePage", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        addressIn_SM->setToolTip(QCoreApplication::translate("SignMessagePage", "The address to sign the message with (e.g. MNS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L)", nullptr));
#endif // QT_CONFIG(tooltip)
        label_41->setText(QCoreApplication::translate("SignMessagePage", "Text", nullptr));
#if QT_CONFIG(tooltip)
        messageIn_SM->setToolTip(QCoreApplication::translate("SignMessagePage", "Enter the message you want to sign here", nullptr));
#endif // QT_CONFIG(tooltip)
        messageIn_SM->setPlainText(QString());
#if QT_CONFIG(tooltip)
        copySignatureButton_SM->setToolTip(QCoreApplication::translate("SignMessagePage", "Copy the current signature to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copySignatureButton_SM->setText(QString());
        statusLabel_SM->setText(QString());
#if QT_CONFIG(tooltip)
        clearButton_SM->setToolTip(QCoreApplication::translate("SignMessagePage", "Reset all sign message fields", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton_SM->setText(QCoreApplication::translate("SignMessagePage", "Clear &All", nullptr));
#if QT_CONFIG(tooltip)
        signMessageButton_SM->setToolTip(QCoreApplication::translate("SignMessagePage", "Sign the message to prove you own this Triangles address", nullptr));
#endif // QT_CONFIG(tooltip)
        signMessageButton_SM->setText(QCoreApplication::translate("SignMessagePage", "Sign &Message", nullptr));
    } // retranslateUi

};

namespace Ui {
    class SignMessagePage: public Ui_SignMessagePage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SIGNMESSAGEPAGE_H
