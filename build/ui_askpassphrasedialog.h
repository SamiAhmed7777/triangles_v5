/********************************************************************************
** Form generated from reading UI file 'askpassphrasedialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ASKPASSPHRASEDIALOG_H
#define UI_ASKPASSPHRASEDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QCheckBox>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AskPassphraseDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *frame;
    QVBoxLayout *verticalLayout_2;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout;
    QLabel *lbTitleIcon_Lock;
    QLabel *lbTitleIcon_Unlock;
    QLabel *lbTitleIcon_Passphrase;
    QSpacerItem *horizontalSpacer_26;
    QLabel *lbTitle;
    QPushButton *bClose;
    QHBoxLayout *horizontalLayout_3;
    QLabel *warningLabel;
    QFormLayout *formLayout;
    QLabel *passLabel1;
    QLineEdit *passEdit1;
    QLabel *passLabel2;
    QLineEdit *passEdit2;
    QLabel *passLabel3;
    QLineEdit *passEdit3;
    QCheckBox *stakingCheckBox;
    QLabel *capsLabel;
    QHBoxLayout *horizontalLayout_2;
    QDialogButtonBox *buttonBox;
    QSpacerItem *verticalSpacer;

    void setupUi(QDialog *AskPassphraseDialog)
    {
        if (AskPassphraseDialog->objectName().isEmpty())
            AskPassphraseDialog->setObjectName(QString::fromUtf8("AskPassphraseDialog"));
        AskPassphraseDialog->resize(550, 274);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(AskPassphraseDialog->sizePolicy().hasHeightForWidth());
        AskPassphraseDialog->setSizePolicy(sizePolicy);
        AskPassphraseDialog->setMinimumSize(QSize(550, 0));
        AskPassphraseDialog->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"background-color: #000;\n"
"\n"
"QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"    border: 1px solid #f26522;\n"
"}\n"
"\n"
"\n"
""));
        verticalLayout = new QVBoxLayout(AskPassphraseDialog);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        frame = new QFrame(AskPassphraseDialog);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setStyleSheet(QString::fromUtf8("#frame {\n"
"	border: 2px solid #f26522;\n"
"}"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(frame);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(9, 9, 9, 9);
        wCaption = new QWidget(frame);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 35));
        wCaption->setMaximumSize(QSize(16777215, 35));
        wCaption->setStyleSheet(QString::fromUtf8(""));
        wCaption->setProperty("is_header", QVariant(true));
        horizontalLayout = new QHBoxLayout(wCaption);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, -1);
        lbTitleIcon_Lock = new QLabel(wCaption);
        lbTitleIcon_Lock->setObjectName(QString::fromUtf8("lbTitleIcon_Lock"));
        sizePolicy.setHeightForWidth(lbTitleIcon_Lock->sizePolicy().hasHeightForWidth());
        lbTitleIcon_Lock->setSizePolicy(sizePolicy);
        lbTitleIcon_Lock->setMinimumSize(QSize(32, 32));
        lbTitleIcon_Lock->setMaximumSize(QSize(32, 32));
        lbTitleIcon_Lock->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/lock")));

        horizontalLayout->addWidget(lbTitleIcon_Lock);

        lbTitleIcon_Unlock = new QLabel(wCaption);
        lbTitleIcon_Unlock->setObjectName(QString::fromUtf8("lbTitleIcon_Unlock"));
        sizePolicy.setHeightForWidth(lbTitleIcon_Unlock->sizePolicy().hasHeightForWidth());
        lbTitleIcon_Unlock->setSizePolicy(sizePolicy);
        lbTitleIcon_Unlock->setMinimumSize(QSize(32, 32));
        lbTitleIcon_Unlock->setMaximumSize(QSize(32, 32));
        lbTitleIcon_Unlock->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/unlock")));

        horizontalLayout->addWidget(lbTitleIcon_Unlock);

        lbTitleIcon_Passphrase = new QLabel(wCaption);
        lbTitleIcon_Passphrase->setObjectName(QString::fromUtf8("lbTitleIcon_Passphrase"));
        sizePolicy.setHeightForWidth(lbTitleIcon_Passphrase->sizePolicy().hasHeightForWidth());
        lbTitleIcon_Passphrase->setSizePolicy(sizePolicy);
        lbTitleIcon_Passphrase->setMinimumSize(QSize(32, 32));
        lbTitleIcon_Passphrase->setMaximumSize(QSize(32, 32));
        lbTitleIcon_Passphrase->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/passphrase")));

        horizontalLayout->addWidget(lbTitleIcon_Passphrase);

        horizontalSpacer_26 = new QSpacerItem(7, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_26);

        lbTitle = new QLabel(wCaption);
        lbTitle->setObjectName(QString::fromUtf8("lbTitle"));
        sizePolicy.setHeightForWidth(lbTitle->sizePolicy().hasHeightForWidth());
        lbTitle->setSizePolicy(sizePolicy);
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        lbTitle->setFont(font);
        lbTitle->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout->addWidget(lbTitle);

        bClose = new QPushButton(wCaption);
        bClose->setObjectName(QString::fromUtf8("bClose"));
        bClose->setMinimumSize(QSize(30, 30));
        bClose->setMaximumSize(QSize(30, 30));
        bClose->setStyleSheet(QString::fromUtf8("QPushButton {	\n"
"	image: url(:/icons/close_normal);\n"
"	border: 0px solid gray;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	image: url(:/icons/close_normal);\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	image: url(:/icons/close_hover);\n"
"}"));
        bClose->setText(QString::fromUtf8(""));
        bClose->setFlat(true);

        horizontalLayout->addWidget(bClose);


        verticalLayout_2->addWidget(wCaption);

        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setSpacing(0);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(25, 9, 25, 9);
        warningLabel = new QLabel(frame);
        warningLabel->setObjectName(QString::fromUtf8("warningLabel"));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(false);
        font1.setWeight(50);
        warningLabel->setFont(font1);
        warningLabel->setTextFormat(Qt::RichText);
        warningLabel->setWordWrap(true);
        warningLabel->setMargin(0);

        horizontalLayout_3->addWidget(warningLabel);


        verticalLayout_2->addLayout(horizontalLayout_3);

        formLayout = new QFormLayout();
        formLayout->setObjectName(QString::fromUtf8("formLayout"));
        formLayout->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        formLayout->setHorizontalSpacing(9);
        formLayout->setVerticalSpacing(9);
        formLayout->setContentsMargins(25, 15, 25, 9);
        passLabel1 = new QLabel(frame);
        passLabel1->setObjectName(QString::fromUtf8("passLabel1"));
        QFont font2;
        font2.setPointSize(10);
        font2.setBold(true);
        font2.setWeight(75);
        passLabel1->setFont(font2);

        formLayout->setWidget(0, QFormLayout::LabelRole, passLabel1);

        passEdit1 = new QLineEdit(frame);
        passEdit1->setObjectName(QString::fromUtf8("passEdit1"));
        passEdit1->setMinimumSize(QSize(0, 20));
        passEdit1->setStyleSheet(QString::fromUtf8("QLineEdit\n"
"{\n"
"    background-color: #1c1c1c;\n"
"    border: 1px solid #f26522;\n"
"}"));
        passEdit1->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(0, QFormLayout::FieldRole, passEdit1);

        passLabel2 = new QLabel(frame);
        passLabel2->setObjectName(QString::fromUtf8("passLabel2"));
        passLabel2->setFont(font2);

        formLayout->setWidget(1, QFormLayout::LabelRole, passLabel2);

        passEdit2 = new QLineEdit(frame);
        passEdit2->setObjectName(QString::fromUtf8("passEdit2"));
        passEdit2->setMinimumSize(QSize(0, 20));
        passEdit2->setStyleSheet(QString::fromUtf8("QLineEdit\n"
"{\n"
"    background-color: #1c1c1c;\n"
"    border: 1px solid #f26522;\n"
"}"));
        passEdit2->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(1, QFormLayout::FieldRole, passEdit2);

        passLabel3 = new QLabel(frame);
        passLabel3->setObjectName(QString::fromUtf8("passLabel3"));
        passLabel3->setFont(font2);

        formLayout->setWidget(2, QFormLayout::LabelRole, passLabel3);

        passEdit3 = new QLineEdit(frame);
        passEdit3->setObjectName(QString::fromUtf8("passEdit3"));
        passEdit3->setMinimumSize(QSize(0, 20));
        passEdit3->setStyleSheet(QString::fromUtf8("QLineEdit\n"
"{\n"
"    background-color: #1c1c1c;\n"
"    border: 1px solid #f26522;\n"
"}"));
        passEdit3->setEchoMode(QLineEdit::Password);

        formLayout->setWidget(2, QFormLayout::FieldRole, passEdit3);

        stakingCheckBox = new QCheckBox(frame);
        stakingCheckBox->setObjectName(QString::fromUtf8("stakingCheckBox"));
        QSizePolicy sizePolicy1(QSizePolicy::Minimum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(stakingCheckBox->sizePolicy().hasHeightForWidth());
        stakingCheckBox->setSizePolicy(sizePolicy1);
        stakingCheckBox->setVisible(true);
        stakingCheckBox->setStyleSheet(QString::fromUtf8("QCheckBox::indicator { \n"
"	border:1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QCheckBox::indicator:checked {\n"
"	image:url(:/icons/checkbox);\n"
"	padding: 1px;\n"
"	}"));

        formLayout->setWidget(4, QFormLayout::FieldRole, stakingCheckBox);

        capsLabel = new QLabel(frame);
        capsLabel->setObjectName(QString::fromUtf8("capsLabel"));
        capsLabel->setEnabled(true);
        capsLabel->setFont(font2);
        capsLabel->setAlignment(Qt::AlignCenter);

        formLayout->setWidget(3, QFormLayout::FieldRole, capsLabel);


        verticalLayout_2->addLayout(formLayout);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setSpacing(0);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalLayout_2->setContentsMargins(-1, -1, 0, -1);
        buttonBox = new QDialogButtonBox(frame);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(buttonBox->sizePolicy().hasHeightForWidth());
        buttonBox->setSizePolicy(sizePolicy2);
        buttonBox->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 120px;\n"
"	min-width: 120px;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	background-color: #000;\n"
"	border: 1px solid #61280E;\n"
"	color: #61280E;\n"
"}"));
        buttonBox->setOrientation(Qt::Horizontal);
        buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
        buttonBox->setCenterButtons(true);

        horizontalLayout_2->addWidget(buttonBox);


        verticalLayout_2->addLayout(horizontalLayout_2);

        verticalSpacer = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer);


        verticalLayout->addWidget(frame);


        retranslateUi(AskPassphraseDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), AskPassphraseDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), AskPassphraseDialog, SLOT(reject()));
        QObject::connect(bClose, SIGNAL(clicked()), AskPassphraseDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(AskPassphraseDialog);
    } // setupUi

    void retranslateUi(QDialog *AskPassphraseDialog)
    {
        AskPassphraseDialog->setWindowTitle(QCoreApplication::translate("AskPassphraseDialog", "Passphrase Dialog", nullptr));
        lbTitleIcon_Lock->setText(QString());
        lbTitleIcon_Unlock->setText(QString());
        lbTitleIcon_Passphrase->setText(QString());
        lbTitle->setText(QCoreApplication::translate("AskPassphraseDialog", "Change password", nullptr));
        passLabel1->setText(QCoreApplication::translate("AskPassphraseDialog", "Enter passphrase", nullptr));
        passLabel2->setText(QCoreApplication::translate("AskPassphraseDialog", "New passphrase", nullptr));
        passLabel3->setText(QCoreApplication::translate("AskPassphraseDialog", "Repeat new passphrase", nullptr));
        stakingCheckBox->setText(QCoreApplication::translate("AskPassphraseDialog", "Unlock for staking only", nullptr));
        capsLabel->setText(QString());
    } // retranslateUi

};

namespace Ui {
    class AskPassphraseDialog: public Ui_AskPassphraseDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ASKPASSPHRASEDIALOG_H
