/********************************************************************************
** Form generated from reading UI file 'sendcoinsentry.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_SENDCOINSENTRY_H
#define UI_SENDCOINSENTRY_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QToolButton>
#include "qvalidatedlineedit.h"
#include "trianglesamountfield.h"

QT_BEGIN_NAMESPACE

class Ui_SendCoinsEntry
{
public:
    QGridLayout *gridLayout;
    QLabel *label_2;
    QHBoxLayout *payToLayout;
    QValidatedLineEdit *payTo;
    QToolButton *addressBookButton;
    QToolButton *pasteButton;
    QToolButton *deleteButton;
    QLabel *label_4;
    QValidatedLineEdit *addAsLabel;
    QLabel *label;
    TrianglesAmountField *payAmount;
    QValidatedLineEdit *narration;

    void setupUi(QFrame *SendCoinsEntry)
    {
        if (SendCoinsEntry->objectName().isEmpty())
            SendCoinsEntry->setObjectName(QString::fromUtf8("SendCoinsEntry"));
        SendCoinsEntry->resize(729, 103);
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
        SendCoinsEntry->setPalette(palette);
        SendCoinsEntry->setStyleSheet(QString::fromUtf8("#SendCoinsEntry{\n"
"	background-color: #000;\n"
"	border: 1px solid #f26522;\n"
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
        SendCoinsEntry->setFrameShape(QFrame::StyledPanel);
        SendCoinsEntry->setFrameShadow(QFrame::Sunken);
        gridLayout = new QGridLayout(SendCoinsEntry);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        label_2 = new QLabel(SendCoinsEntry);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setStyleSheet(QString::fromUtf8("color: #f26522;"));
        label_2->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(label_2, 0, 0, 1, 1);

        payToLayout = new QHBoxLayout();
        payToLayout->setSpacing(6);
        payToLayout->setObjectName(QString::fromUtf8("payToLayout"));
        payTo = new QValidatedLineEdit(SendCoinsEntry);
        payTo->setObjectName(QString::fromUtf8("payTo"));
        payTo->setMinimumSize(QSize(0, 22));
        payTo->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));
        payTo->setMaxLength(34);

        payToLayout->addWidget(payTo);

        addressBookButton = new QToolButton(SendCoinsEntry);
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

        payToLayout->addWidget(addressBookButton);

        pasteButton = new QToolButton(SendCoinsEntry);
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

        payToLayout->addWidget(pasteButton);

        deleteButton = new QToolButton(SendCoinsEntry);
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

        payToLayout->addWidget(deleteButton);


        gridLayout->addLayout(payToLayout, 0, 1, 1, 2);

        label_4 = new QLabel(SendCoinsEntry);
        label_4->setObjectName(QString::fromUtf8("label_4"));
        label_4->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"      "));
        label_4->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(label_4, 1, 0, 1, 1);

        addAsLabel = new QValidatedLineEdit(SendCoinsEntry);
        addAsLabel->setObjectName(QString::fromUtf8("addAsLabel"));
        addAsLabel->setEnabled(true);
        addAsLabel->setMinimumSize(QSize(0, 22));
        addAsLabel->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        gridLayout->addWidget(addAsLabel, 1, 1, 1, 2);

        label = new QLabel(SendCoinsEntry);
        label->setObjectName(QString::fromUtf8("label"));
        label->setMinimumSize(QSize(0, 20));
        label->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"      "));
        label->setAlignment(Qt::AlignRight|Qt::AlignTrailing|Qt::AlignVCenter);

        gridLayout->addWidget(label, 2, 0, 1, 1);

        payAmount = new TrianglesAmountField(SendCoinsEntry);
        payAmount->setObjectName(QString::fromUtf8("payAmount"));
        payAmount->setMinimumSize(QSize(70, 22));
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
        payAmount->setPalette(palette1);
        payAmount->setContextMenuPolicy(Qt::NoContextMenu);
        payAmount->setStyleSheet(QString::fromUtf8("border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"      "));

        gridLayout->addWidget(payAmount, 2, 1, 1, 1);

        narration = new QValidatedLineEdit(SendCoinsEntry);
        narration->setObjectName(QString::fromUtf8("narration"));
        narration->setEnabled(true);
        narration->setMinimumSize(QSize(0, 22));
        narration->setStyleSheet(QString::fromUtf8("QLineEdit\n"
"	{\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        gridLayout->addWidget(narration, 2, 2, 1, 1);

#if QT_CONFIG(shortcut)
        label_2->setBuddy(payTo);
        label_4->setBuddy(addAsLabel);
        label->setBuddy(payAmount);
#endif // QT_CONFIG(shortcut)

        retranslateUi(SendCoinsEntry);

        QMetaObject::connectSlotsByName(SendCoinsEntry);
    } // setupUi

    void retranslateUi(QFrame *SendCoinsEntry)
    {
        SendCoinsEntry->setWindowTitle(QCoreApplication::translate("SendCoinsEntry", "Form", nullptr));
        label_2->setText(QCoreApplication::translate("SendCoinsEntry", "Pay &To:", nullptr));
#if QT_CONFIG(tooltip)
        payTo->setToolTip(QCoreApplication::translate("SendCoinsEntry", "The address to send the payment to  (e.g. TXc7mPCNFFpinDonuSH5PNVY9S8nBcvGQm)", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        addressBookButton->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Choose address from address book", nullptr));
#endif // QT_CONFIG(tooltip)
        addressBookButton->setText(QString());
#if QT_CONFIG(shortcut)
        addressBookButton->setShortcut(QCoreApplication::translate("SendCoinsEntry", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        pasteButton->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Paste address from clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        pasteButton->setText(QString());
#if QT_CONFIG(shortcut)
        pasteButton->setShortcut(QCoreApplication::translate("SendCoinsEntry", "Alt+P", nullptr));
#endif // QT_CONFIG(shortcut)
#if QT_CONFIG(tooltip)
        deleteButton->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Remove this recipient", nullptr));
#endif // QT_CONFIG(tooltip)
        deleteButton->setText(QString());
        label_4->setText(QCoreApplication::translate("SendCoinsEntry", "&Label:", nullptr));
#if QT_CONFIG(tooltip)
        addAsLabel->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Enter a label for this address to add it to your address book", nullptr));
#endif // QT_CONFIG(tooltip)
        label->setText(QCoreApplication::translate("SendCoinsEntry", "A&mount:", nullptr));
#if QT_CONFIG(tooltip)
        narration->setToolTip(QCoreApplication::translate("SendCoinsEntry", "Enter a short note to send with payment (max 24 characters)", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class SendCoinsEntry: public Ui_SendCoinsEntry {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_SENDCOINSENTRY_H
