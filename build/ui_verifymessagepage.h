/********************************************************************************
** Form generated from reading UI file 'verifymessagepage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_VERIFYMESSAGEPAGE_H
#define UI_VERIFYMESSAGEPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPlainTextEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "qvalidatedlineedit.h"

QT_BEGIN_NAMESPACE

class Ui_VerifyMessagePage
{
public:
    QVBoxLayout *verticalLayout_3;
    QWidget *wHeader;
    QHBoxLayout *horizontalLayout_4;
    QLabel *icon_messages;
    QSpacerItem *horizontalSpacer_send_1;
    QLabel *label_messages;
    QWidget *wContainer;
    QVBoxLayout *verticalLayout_2;
    QSpacerItem *verticalSpacer_3;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_29;
    QVBoxLayout *verticalLayout;
    QGridLayout *gridLayout_5;
    QPushButton *addressBookButton_VM;
    QLabel *label;
    QValidatedLineEdit *addressIn_VM;
    QValidatedLineEdit *signatureIn_VM;
    QLabel *label_41;
    QLabel *label_2;
    QLabel *label_39;
    QLabel *label_40;
    QPlainTextEdit *messageIn_VM;
    QLabel *statusLabel_VM;
    QWidget *wAcceptHolder;
    QHBoxLayout *horizontalLayout_22;
    QSpacerItem *horizontalSpacer_30;
    QPushButton *clearButton_VM;
    QPushButton *verifyMessageButton_VM;
    QSpacerItem *horizontalSpacer;
    QSpacerItem *horizontalSpacer_31;
    QSpacerItem *verticalSpacer;

    void setupUi(QWidget *VerifyMessagePage)
    {
        if (VerifyMessagePage->objectName().isEmpty())
            VerifyMessagePage->setObjectName(QString::fromUtf8("VerifyMessagePage"));
        VerifyMessagePage->resize(750, 600);
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
        VerifyMessagePage->setPalette(palette);
        VerifyMessagePage->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"	background-color: #000;\n"
"\n"
"QLineEdit {\n"
"	border: 1px solid #f26522;\n"
"	}"));
        verticalLayout_3 = new QVBoxLayout(VerifyMessagePage);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        wHeader = new QWidget(VerifyMessagePage);
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
        icon_messages->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/verify")));
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


        verticalLayout_3->addWidget(wHeader);

        wContainer = new QWidget(VerifyMessagePage);
        wContainer->setObjectName(QString::fromUtf8("wContainer"));
        wContainer->setStyleSheet(QString::fromUtf8(""));
        verticalLayout_2 = new QVBoxLayout(wContainer);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalSpacer_3 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer_3);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setSpacing(0);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer_29 = new QSpacerItem(136, 216, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_29);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(-1, 0, -1, -1);
        gridLayout_5 = new QGridLayout();
        gridLayout_5->setSpacing(6);
        gridLayout_5->setObjectName(QString::fromUtf8("gridLayout_5"));
        gridLayout_5->setContentsMargins(-1, 6, -1, 3);
        addressBookButton_VM = new QPushButton(wContainer);
        addressBookButton_VM->setObjectName(QString::fromUtf8("addressBookButton_VM"));
        addressBookButton_VM->setMinimumSize(QSize(28, 28));
        addressBookButton_VM->setMaximumSize(QSize(28, 28));
        addressBookButton_VM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        icon.addFile(QString::fromUtf8(":/menu_16/addressbook"), QSize(), QIcon::Normal, QIcon::Off);
        addressBookButton_VM->setIcon(icon);
        addressBookButton_VM->setIconSize(QSize(30, 30));
        addressBookButton_VM->setFlat(true);

        gridLayout_5->addWidget(addressBookButton_VM, 0, 2, 1, 1);

        label = new QLabel(wContainer);
        label->setObjectName(QString::fromUtf8("label"));
        label->setMinimumSize(QSize(28, 0));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(true);
        font1.setWeight(75);
        label->setFont(font1);
        label->setAlignment(Qt::AlignCenter);
        label->setWordWrap(false);

        gridLayout_5->addWidget(label, 0, 3, 1, 1);

        addressIn_VM = new QValidatedLineEdit(wContainer);
        addressIn_VM->setObjectName(QString::fromUtf8("addressIn_VM"));
        addressIn_VM->setMinimumSize(QSize(0, 22));
        QFont font2;
        font2.setPointSize(10);
        addressIn_VM->setFont(font2);
        addressIn_VM->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        addressIn_VM->setMaxLength(34);

        gridLayout_5->addWidget(addressIn_VM, 0, 1, 1, 1);

        signatureIn_VM = new QValidatedLineEdit(wContainer);
        signatureIn_VM->setObjectName(QString::fromUtf8("signatureIn_VM"));
        signatureIn_VM->setMinimumSize(QSize(0, 22));
        signatureIn_VM->setFont(font2);
        signatureIn_VM->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        signatureIn_VM->setReadOnly(false);

        gridLayout_5->addWidget(signatureIn_VM, 2, 1, 1, 1);

        label_41 = new QLabel(wContainer);
        label_41->setObjectName(QString::fromUtf8("label_41"));
        label_41->setFont(font1);
        label_41->setStyleSheet(QString::fromUtf8(""));
        label_41->setAlignment(Qt::AlignRight|Qt::AlignTop|Qt::AlignTrailing);
        label_41->setMargin(5);

        gridLayout_5->addWidget(label_41, 1, 0, 1, 1);

        label_2 = new QLabel(wContainer);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setMinimumSize(QSize(28, 28));
        label_2->setFont(font1);
        label_2->setAlignment(Qt::AlignCenter);
        label_2->setWordWrap(false);

        gridLayout_5->addWidget(label_2, 2, 2, 1, 1);

        label_39 = new QLabel(wContainer);
        label_39->setObjectName(QString::fromUtf8("label_39"));
        label_39->setFont(font1);
        label_39->setStyleSheet(QString::fromUtf8(""));
        label_39->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_39->setMargin(5);

        gridLayout_5->addWidget(label_39, 0, 0, 1, 1);

        label_40 = new QLabel(wContainer);
        label_40->setObjectName(QString::fromUtf8("label_40"));
        label_40->setFont(font1);
        label_40->setStyleSheet(QString::fromUtf8(""));
        label_40->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);
        label_40->setMargin(5);

        gridLayout_5->addWidget(label_40, 2, 0, 1, 1);

        messageIn_VM = new QPlainTextEdit(wContainer);
        messageIn_VM->setObjectName(QString::fromUtf8("messageIn_VM"));
        messageIn_VM->setMinimumSize(QSize(0, 300));
        messageIn_VM->setMaximumSize(QSize(16777215, 300));
        messageIn_VM->setFont(font2);
        messageIn_VM->setStyleSheet(QString::fromUtf8("QPlainTextEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        messageIn_VM->setBackgroundVisible(false);

        gridLayout_5->addWidget(messageIn_VM, 1, 1, 1, 1);


        verticalLayout->addLayout(gridLayout_5);

        statusLabel_VM = new QLabel(wContainer);
        statusLabel_VM->setObjectName(QString::fromUtf8("statusLabel_VM"));
        statusLabel_VM->setFont(font1);
        statusLabel_VM->setAlignment(Qt::AlignCenter);
        statusLabel_VM->setWordWrap(true);

        verticalLayout->addWidget(statusLabel_VM);

        wAcceptHolder = new QWidget(wContainer);
        wAcceptHolder->setObjectName(QString::fromUtf8("wAcceptHolder"));
        wAcceptHolder->setMinimumSize(QSize(0, 26));
        horizontalLayout_22 = new QHBoxLayout(wAcceptHolder);
        horizontalLayout_22->setObjectName(QString::fromUtf8("horizontalLayout_22"));
        horizontalLayout_22->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_30 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_22->addItem(horizontalSpacer_30);

        clearButton_VM = new QPushButton(wAcceptHolder);
        clearButton_VM->setObjectName(QString::fromUtf8("clearButton_VM"));
        clearButton_VM->setMinimumSize(QSize(122, 22));
        clearButton_VM->setMaximumSize(QSize(122, 22));
        QFont font3;
        font3.setPointSize(10);
        font3.setStyleStrategy(QFont::PreferAntialias);
        clearButton_VM->setFont(font3);
        clearButton_VM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        clearButton_VM->setFlat(true);

        horizontalLayout_22->addWidget(clearButton_VM);

        verifyMessageButton_VM = new QPushButton(wAcceptHolder);
        verifyMessageButton_VM->setObjectName(QString::fromUtf8("verifyMessageButton_VM"));
        verifyMessageButton_VM->setFont(font3);
        verifyMessageButton_VM->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/menu_16/verify"), QSize(), QIcon::Normal, QIcon::Off);
        verifyMessageButton_VM->setIcon(icon1);
        verifyMessageButton_VM->setFlat(true);

        horizontalLayout_22->addWidget(verifyMessageButton_VM);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_22->addItem(horizontalSpacer);


        verticalLayout->addWidget(wAcceptHolder);


        horizontalLayout->addLayout(verticalLayout);

        horizontalSpacer_31 = new QSpacerItem(136, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_31);

        horizontalLayout->setStretch(0, 1);
        horizontalLayout->setStretch(1, 7);
        horizontalLayout->setStretch(2, 1);

        verticalLayout_2->addLayout(horizontalLayout);

        verticalSpacer = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout_2->addItem(verticalSpacer);


        verticalLayout_3->addWidget(wContainer);


        retranslateUi(VerifyMessagePage);

        QMetaObject::connectSlotsByName(VerifyMessagePage);
    } // setupUi

    void retranslateUi(QWidget *VerifyMessagePage)
    {
        VerifyMessagePage->setWindowTitle(QCoreApplication::translate("VerifyMessagePage", "Form", nullptr));
        icon_messages->setText(QString());
        label_messages->setText(QCoreApplication::translate("VerifyMessagePage", "Verify Message", nullptr));
#if QT_CONFIG(tooltip)
        addressBookButton_VM->setToolTip(QCoreApplication::translate("VerifyMessagePage", "Choose an address from the address book", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(shortcut)
        addressBookButton_VM->setShortcut(QCoreApplication::translate("VerifyMessagePage", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
        label->setText(QString());
#if QT_CONFIG(tooltip)
        addressIn_VM->setToolTip(QCoreApplication::translate("VerifyMessagePage", "The address the message was signed with (e.g. MNS17iag9jJgTHD1VXjvLCEnZuQ3rJDE9L)", nullptr));
#endif // QT_CONFIG(tooltip)
        label_41->setText(QCoreApplication::translate("VerifyMessagePage", "Text", nullptr));
        label_2->setText(QString());
        label_39->setText(QCoreApplication::translate("VerifyMessagePage", "Address", nullptr));
        label_40->setText(QCoreApplication::translate("VerifyMessagePage", "Signature", nullptr));
        messageIn_VM->setPlainText(QString());
        statusLabel_VM->setText(QString());
#if QT_CONFIG(tooltip)
        clearButton_VM->setToolTip(QCoreApplication::translate("VerifyMessagePage", "Reset all verify message fields", nullptr));
#endif // QT_CONFIG(tooltip)
        clearButton_VM->setText(QCoreApplication::translate("VerifyMessagePage", "Clear &All", nullptr));
#if QT_CONFIG(tooltip)
        verifyMessageButton_VM->setToolTip(QCoreApplication::translate("VerifyMessagePage", "Verify the message to ensure it was signed with the specified Triangles address", nullptr));
#endif // QT_CONFIG(tooltip)
        verifyMessageButton_VM->setText(QCoreApplication::translate("VerifyMessagePage", "Verify &Message", nullptr));
    } // retranslateUi

};

namespace Ui {
    class VerifyMessagePage: public Ui_VerifyMessagePage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_VERIFYMESSAGEPAGE_H
