/********************************************************************************
** Form generated from reading UI file 'transactiondescdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TRANSACTIONDESCDIALOG_H
#define UI_TRANSACTIONDESCDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TransactionDescDialog
{
public:
    QVBoxLayout *verticalLayout;
    QFrame *borderframe;
    QVBoxLayout *verticalLayout_2;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout_3;
    QLabel *lbTitleIcon;
    QSpacerItem *horizontalSpacer_224;
    QLabel *label_aboutTriangles;
    QSpacerItem *horizontalSpacer_223;
    QPushButton *bClose;
    QSpacerItem *verticalSpacer;
    QTextEdit *detailText;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *TransactionDescDialog)
    {
        if (TransactionDescDialog->objectName().isEmpty())
            TransactionDescDialog->setObjectName(QString::fromUtf8("TransactionDescDialog"));
        TransactionDescDialog->resize(685, 270);
        TransactionDescDialog->setStyleSheet(QString::fromUtf8("QLabel {\n"
"	color: #f26522;\n"
"}\n"
"\n"
"QDialog {\n"
"	background-color: #000;\n"
"}\n"
"\n"
""));
        verticalLayout = new QVBoxLayout(TransactionDescDialog);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        borderframe = new QFrame(TransactionDescDialog);
        borderframe->setObjectName(QString::fromUtf8("borderframe"));
        borderframe->setStyleSheet(QString::fromUtf8("#borderframe {\n"
"	border: 2px solid #f26522;\n"
"	}"));
        borderframe->setFrameShape(QFrame::StyledPanel);
        borderframe->setFrameShadow(QFrame::Raised);
        verticalLayout_2 = new QVBoxLayout(borderframe);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        wCaption = new QWidget(borderframe);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 38));
        wCaption->setMaximumSize(QSize(16777215, 35));
        wCaption->setStyleSheet(QString::fromUtf8("#wCaption {\n"
"	background-color: #000;\n"
"	}"));
        wCaption->setProperty("is_header", QVariant(true));
        horizontalLayout_3 = new QHBoxLayout(wCaption);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        lbTitleIcon = new QLabel(wCaption);
        lbTitleIcon->setObjectName(QString::fromUtf8("lbTitleIcon"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(lbTitleIcon->sizePolicy().hasHeightForWidth());
        lbTitleIcon->setSizePolicy(sizePolicy);
        lbTitleIcon->setMinimumSize(QSize(30, 30));
        lbTitleIcon->setMaximumSize(QSize(30, 30));
        lbTitleIcon->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/transactions")));

        horizontalLayout_3->addWidget(lbTitleIcon);

        horizontalSpacer_224 = new QSpacerItem(5, 14, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_224);

        label_aboutTriangles = new QLabel(wCaption);
        label_aboutTriangles->setObjectName(QString::fromUtf8("label_aboutTriangles"));
        label_aboutTriangles->setMinimumSize(QSize(400, 32));
        label_aboutTriangles->setBaseSize(QSize(409, 36));
        QFont font;
        font.setPointSize(14);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        label_aboutTriangles->setFont(font);
        label_aboutTriangles->setLineWidth(2);

        horizontalLayout_3->addWidget(label_aboutTriangles);

        horizontalSpacer_223 = new QSpacerItem(10, 14, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_223);

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
        bClose->setFlat(true);

        horizontalLayout_3->addWidget(bClose);


        verticalLayout_2->addWidget(wCaption);

        verticalSpacer = new QSpacerItem(20, 10, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout_2->addItem(verticalSpacer);

        detailText = new QTextEdit(borderframe);
        detailText->setObjectName(QString::fromUtf8("detailText"));
        detailText->setStyleSheet(QString::fromUtf8("QTextEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	color: #f26522;\n"
"	}"));
        detailText->setReadOnly(true);

        verticalLayout_2->addWidget(detailText);

        buttonBox = new QDialogButtonBox(borderframe);
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
        buttonBox->setStandardButtons(QDialogButtonBox::Close);

        verticalLayout_2->addWidget(buttonBox);


        verticalLayout->addWidget(borderframe);


        retranslateUi(TransactionDescDialog);
        QObject::connect(buttonBox, SIGNAL(accepted()), TransactionDescDialog, SLOT(accept()));
        QObject::connect(buttonBox, SIGNAL(rejected()), TransactionDescDialog, SLOT(reject()));
        QObject::connect(bClose, SIGNAL(clicked()), TransactionDescDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(TransactionDescDialog);
    } // setupUi

    void retranslateUi(QDialog *TransactionDescDialog)
    {
        TransactionDescDialog->setWindowTitle(QCoreApplication::translate("TransactionDescDialog", "Transaction details", nullptr));
        lbTitleIcon->setText(QString());
        label_aboutTriangles->setText(QCoreApplication::translate("TransactionDescDialog", "Transaction details", nullptr));
#if QT_CONFIG(tooltip)
        detailText->setToolTip(QCoreApplication::translate("TransactionDescDialog", "This pane shows a detailed description of the transaction", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class TransactionDescDialog: public Ui_TransactionDescDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TRANSACTIONDESCDIALOG_H
