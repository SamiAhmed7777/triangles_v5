/********************************************************************************
** Form generated from reading UI file 'editaddressdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_EDITADDRESSDIALOG_H
#define UI_EDITADDRESSDIALOG_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_EditAddressDialog
{
public:
    QVBoxLayout *verticalLayout_2;
    QFrame *frame;
    QVBoxLayout *verticalLayout;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout_3;
    QLabel *picSend;
    QLabel *picReceive;
    QSpacerItem *horizontalSpacer_26;
    QLabel *lbTitle;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout_2;
    QSpacerItem *horizontalSpacer;
    QWidget *wContent;
    QGridLayout *gridLayout;
    QLabel *label_label;
    QLineEdit *labelEdit;
    QLabel *label_address;
    QLineEdit *addressEdit;
    QPushButton *pasteButton;
    QHBoxLayout *horizontalLayout;
    QSpacerItem *horizontalSpacer_7;
    QDialogButtonBox *buttonBox;
    QCheckBox *stealthCB;
    QSpacerItem *horizontalSpacer_3;
    QSpacerItem *horizontalSpacer_2;

    void setupUi(QDialog *EditAddressDialog)
    {
        if (EditAddressDialog->objectName().isEmpty())
            EditAddressDialog->setObjectName(QString::fromUtf8("EditAddressDialog"));
        EditAddressDialog->resize(519, 188);
        EditAddressDialog->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"background-color: #000;\n"
""));
        verticalLayout_2 = new QVBoxLayout(EditAddressDialog);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        frame = new QFrame(EditAddressDialog);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setStyleSheet(QString::fromUtf8("#frame {\n"
"	border: 2px solid #f26522;\n"
"	}"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        frame->setLineWidth(1);
        verticalLayout = new QVBoxLayout(frame);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(9, 9, 9, 9);
        wCaption = new QWidget(frame);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 32));
        wCaption->setMaximumSize(QSize(16777215, 32));
        wCaption->setStyleSheet(QString::fromUtf8(""));
        wCaption->setProperty("is_header", QVariant(true));
        horizontalLayout_3 = new QHBoxLayout(wCaption);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(0, 0, 0, 0);
        picSend = new QLabel(wCaption);
        picSend->setObjectName(QString::fromUtf8("picSend"));
        picSend->setEnabled(true);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(picSend->sizePolicy().hasHeightForWidth());
        picSend->setSizePolicy(sizePolicy);
        picSend->setMinimumSize(QSize(32, 32));
        picSend->setMaximumSize(QSize(32, 32));
        picSend->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/send")));
        picSend->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        horizontalLayout_3->addWidget(picSend);

        picReceive = new QLabel(wCaption);
        picReceive->setObjectName(QString::fromUtf8("picReceive"));
        picReceive->setEnabled(true);
        sizePolicy.setHeightForWidth(picReceive->sizePolicy().hasHeightForWidth());
        picReceive->setSizePolicy(sizePolicy);
        picReceive->setMinimumSize(QSize(32, 32));
        picReceive->setMaximumSize(QSize(32, 32));
        picReceive->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/receive")));

        horizontalLayout_3->addWidget(picReceive);

        horizontalSpacer_26 = new QSpacerItem(7, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_26);

        lbTitle = new QLabel(wCaption);
        lbTitle->setObjectName(QString::fromUtf8("lbTitle"));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        lbTitle->setFont(font);
        lbTitle->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_3->addWidget(lbTitle);


        verticalLayout->addWidget(wCaption);

        verticalSpacer = new QSpacerItem(20, 5, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout->addItem(verticalSpacer);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalSpacer = new QSpacerItem(35, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer);

        wContent = new QWidget(frame);
        wContent->setObjectName(QString::fromUtf8("wContent"));
        gridLayout = new QGridLayout(wContent);
        gridLayout->setObjectName(QString::fromUtf8("gridLayout"));
        label_label = new QLabel(wContent);
        label_label->setObjectName(QString::fromUtf8("label_label"));
        label_label->setMinimumSize(QSize(64, 30));
        label_label->setMaximumSize(QSize(64, 30));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(true);
        font1.setWeight(75);
        label_label->setFont(font1);
        label_label->setMargin(5);

        gridLayout->addWidget(label_label, 0, 0, 1, 1);

        labelEdit = new QLineEdit(wContent);
        labelEdit->setObjectName(QString::fromUtf8("labelEdit"));
        QSizePolicy sizePolicy1(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(labelEdit->sizePolicy().hasHeightForWidth());
        labelEdit->setSizePolicy(sizePolicy1);
        labelEdit->setMinimumSize(QSize(350, 20));
        labelEdit->setLayoutDirection(Qt::LeftToRight);
        labelEdit->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	margin: 0px 10px;\n"
"	}"));
        labelEdit->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        gridLayout->addWidget(labelEdit, 0, 1, 1, 1);

        label_address = new QLabel(wContent);
        label_address->setObjectName(QString::fromUtf8("label_address"));
        label_address->setMinimumSize(QSize(64, 30));
        label_address->setMaximumSize(QSize(64, 30));
        label_address->setFont(font1);
        label_address->setMargin(5);

        gridLayout->addWidget(label_address, 1, 0, 1, 1);

        addressEdit = new QLineEdit(wContent);
        addressEdit->setObjectName(QString::fromUtf8("addressEdit"));
        sizePolicy1.setHeightForWidth(addressEdit->sizePolicy().hasHeightForWidth());
        addressEdit->setSizePolicy(sizePolicy1);
        addressEdit->setMinimumSize(QSize(350, 20));
        addressEdit->setLayoutDirection(Qt::LeftToRight);
        addressEdit->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	margin: 0px 10px;\n"
"	}"));

        gridLayout->addWidget(addressEdit, 1, 1, 1, 1);

        pasteButton = new QPushButton(wContent);
        pasteButton->setObjectName(QString::fromUtf8("pasteButton"));
        QSizePolicy sizePolicy2(QSizePolicy::Minimum, QSizePolicy::Minimum);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(pasteButton->sizePolicy().hasHeightForWidth());
        pasteButton->setSizePolicy(sizePolicy2);
        pasteButton->setMinimumSize(QSize(28, 22));
        pasteButton->setMaximumSize(QSize(28, 22));
        pasteButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        pasteButton->setIcon(icon);
        pasteButton->setIconSize(QSize(16, 16));
        pasteButton->setFlat(true);

        gridLayout->addWidget(pasteButton, 1, 2, 1, 1);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalSpacer_7 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_7);

        buttonBox = new QDialogButtonBox(wContent);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        buttonBox->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);

        horizontalLayout->addWidget(buttonBox);

        stealthCB = new QCheckBox(wContent);
        stealthCB->setObjectName(QString::fromUtf8("stealthCB"));
        stealthCB->setEnabled(false);
        stealthCB->setMaximumSize(QSize(0, 0));
        stealthCB->setCheckable(false);

        horizontalLayout->addWidget(stealthCB);

        horizontalSpacer_3 = new QSpacerItem(10, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_3);


        gridLayout->addLayout(horizontalLayout, 2, 0, 1, 2);


        horizontalLayout_2->addWidget(wContent);

        horizontalSpacer_2 = new QSpacerItem(35, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_2);


        verticalLayout->addLayout(horizontalLayout_2);


        verticalLayout_2->addWidget(frame);

#if QT_CONFIG(shortcut)
        label_label->setBuddy(labelEdit);
        label_address->setBuddy(addressEdit);
#endif // QT_CONFIG(shortcut)

        retranslateUi(EditAddressDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), EditAddressDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), EditAddressDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(EditAddressDialog);
    } // setupUi

    void retranslateUi(QDialog *EditAddressDialog)
    {
        EditAddressDialog->setWindowTitle(QString());
        picSend->setText(QString());
        picReceive->setText(QString());
        lbTitle->setText(QCoreApplication::translate("EditAddressDialog", "Edit record", nullptr));
        label_label->setText(QCoreApplication::translate("EditAddressDialog", "&Label", nullptr));
#if QT_CONFIG(tooltip)
        labelEdit->setToolTip(QCoreApplication::translate("EditAddressDialog", "The label associated with this address book entry", nullptr));
#endif // QT_CONFIG(tooltip)
        label_address->setText(QCoreApplication::translate("EditAddressDialog", "&Address", nullptr));
#if QT_CONFIG(tooltip)
        addressEdit->setToolTip(QCoreApplication::translate("EditAddressDialog", "The address associated with this address book entry. This can only be modified for sending addresses.", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        pasteButton->setToolTip(QCoreApplication::translate("EditAddressDialog", "Paste from clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        pasteButton->setText(QString());
#if QT_CONFIG(shortcut)
        pasteButton->setShortcut(QCoreApplication::translate("EditAddressDialog", "Alt+A", nullptr));
#endif // QT_CONFIG(shortcut)
        stealthCB->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class EditAddressDialog: public Ui_EditAddressDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_EDITADDRESSDIALOG_H
