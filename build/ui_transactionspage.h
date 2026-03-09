/********************************************************************************
** Form generated from reading UI file 'transactionspage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_TRANSACTIONSPAGE_H
#define UI_TRANSACTIONSPAGE_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QGridLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_TransactionsPage
{
public:
    QVBoxLayout *verticalLayout;
    QWidget *wTransactionsHeader;
    QHBoxLayout *horizontalLayout_3;
    QLabel *icon_send;
    QSpacerItem *horizontalSpacer_send_1;
    QLabel *label_send;
    QWidget *wTransactionsContainer;
    QVBoxLayout *verticalLayout_10;
    QGridLayout *gridLayout_3;
    QLineEdit *addressWidget;
    QComboBox *typeWidget;
    QComboBox *dateWidget;
    QLineEdit *amountWidget;
    QSpacerItem *horizontalSpacer;
    QTableView *transactionView;

    void setupUi(QWidget *TransactionsPage)
    {
        if (TransactionsPage->objectName().isEmpty())
            TransactionsPage->setObjectName(QString::fromUtf8("TransactionsPage"));
        TransactionsPage->resize(760, 380);
        QPalette palette;
        QBrush brush(QColor(242, 101, 34, 255));
        brush.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::WindowText, brush);
        QBrush brush1(QColor(0, 0, 0, 255));
        brush1.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette.setBrush(QPalette::Active, QPalette::Text, brush);
        palette.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette.setBrush(QPalette::Active, QPalette::Window, brush1);
        QBrush brush2(QColor(235, 235, 235, 255));
        brush2.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::Highlight, brush2);
        QBrush brush3(QColor(171, 127, 130, 255));
        brush3.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Active, QPalette::HighlightedText, brush3);
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
        palette.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush3);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        QBrush brush4(QColor(240, 240, 240, 255));
        brush4.setStyle(Qt::SolidPattern);
        palette.setBrush(QPalette::Disabled, QPalette::Highlight, brush4);
        palette.setBrush(QPalette::Disabled, QPalette::HighlightedText, brush3);
        TransactionsPage->setPalette(palette);
        TransactionsPage->setStyleSheet(QString::fromUtf8("QWidget {\n"
"	color: #f26522;\n"
"	background-color: #000;\n"
"	}"));
        verticalLayout = new QVBoxLayout(TransactionsPage);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        wTransactionsHeader = new QWidget(TransactionsPage);
        wTransactionsHeader->setObjectName(QString::fromUtf8("wTransactionsHeader"));
        wTransactionsHeader->setMinimumSize(QSize(0, 32));
        wTransactionsHeader->setMaximumSize(QSize(16777215, 16777215));
        wTransactionsHeader->setStyleSheet(QString::fromUtf8(""));
        wTransactionsHeader->setProperty("is_header", QVariant(true));
        horizontalLayout_3 = new QHBoxLayout(wTransactionsHeader);
        horizontalLayout_3->setSpacing(0);
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(10, 5, -1, 5);
        icon_send = new QLabel(wTransactionsHeader);
        icon_send->setObjectName(QString::fromUtf8("icon_send"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(icon_send->sizePolicy().hasHeightForWidth());
        icon_send->setSizePolicy(sizePolicy);
        icon_send->setMinimumSize(QSize(30, 30));
        icon_send->setMaximumSize(QSize(30, 30));
        icon_send->setBaseSize(QSize(0, 0));
        icon_send->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/transactions")));
        icon_send->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        horizontalLayout_3->addWidget(icon_send);

        horizontalSpacer_send_1 = new QSpacerItem(4, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_send_1);

        label_send = new QLabel(wTransactionsHeader);
        label_send->setObjectName(QString::fromUtf8("label_send"));
        label_send->setMinimumSize(QSize(0, 30));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        label_send->setFont(font);
        label_send->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_3->addWidget(label_send);


        verticalLayout->addWidget(wTransactionsHeader);

        wTransactionsContainer = new QWidget(TransactionsPage);
        wTransactionsContainer->setObjectName(QString::fromUtf8("wTransactionsContainer"));
        wTransactionsContainer->setStyleSheet(QString::fromUtf8("#wTransactionsContainer {\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QLabel {\n"
"	color: #f26522;\n"
"	}"));
        verticalLayout_10 = new QVBoxLayout(wTransactionsContainer);
        verticalLayout_10->setSpacing(6);
        verticalLayout_10->setObjectName(QString::fromUtf8("verticalLayout_10"));
        verticalLayout_10->setContentsMargins(9, 9, 9, 9);
        gridLayout_3 = new QGridLayout();
        gridLayout_3->setObjectName(QString::fromUtf8("gridLayout_3"));
        addressWidget = new QLineEdit(wTransactionsContainer);
        addressWidget->setObjectName(QString::fromUtf8("addressWidget"));
        QSizePolicy sizePolicy1(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(addressWidget->sizePolicy().hasHeightForWidth());
        addressWidget->setSizePolicy(sizePolicy1);
        addressWidget->setMinimumSize(QSize(0, 22));
        addressWidget->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        gridLayout_3->addWidget(addressWidget, 0, 4, 1, 1);

        typeWidget = new QComboBox(wTransactionsContainer);
        typeWidget->setObjectName(QString::fromUtf8("typeWidget"));
        typeWidget->setMinimumSize(QSize(115, 22));
        typeWidget->setMaximumSize(QSize(115, 16777215));
        typeWidget->setStyleSheet(QString::fromUtf8("QComboBox {\n"
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
"	}"));

        gridLayout_3->addWidget(typeWidget, 0, 2, 1, 1);

        dateWidget = new QComboBox(wTransactionsContainer);
        dateWidget->setObjectName(QString::fromUtf8("dateWidget"));
        QSizePolicy sizePolicy2(QSizePolicy::Fixed, QSizePolicy::Fixed);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(dateWidget->sizePolicy().hasHeightForWidth());
        dateWidget->setSizePolicy(sizePolicy2);
        dateWidget->setMinimumSize(QSize(115, 22));
        dateWidget->setMaximumSize(QSize(115, 16777215));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        QBrush brush5(QColor(28, 28, 28, 255));
        brush5.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::Button, brush5);
        QBrush brush6(QColor(255, 180, 144, 255));
        brush6.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::Light, brush6);
        QBrush brush7(QColor(248, 140, 89, 255));
        brush7.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::Midlight, brush7);
        QBrush brush8(QColor(121, 50, 17, 255));
        brush8.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::Dark, brush8);
        QBrush brush9(QColor(161, 67, 22, 255));
        brush9.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::Mid, brush9);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        QBrush brush10(QColor(255, 255, 255, 255));
        brush10.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::BrightText, brush10);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush5);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush5);
        palette1.setBrush(QPalette::Active, QPalette::Shadow, brush1);
        QBrush brush11(QColor(248, 178, 144, 255));
        brush11.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::AlternateBase, brush11);
        QBrush brush12(QColor(255, 255, 220, 255));
        brush12.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::ToolTipBase, brush12);
        palette1.setBrush(QPalette::Active, QPalette::ToolTipText, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush5);
        palette1.setBrush(QPalette::Inactive, QPalette::Light, brush6);
        palette1.setBrush(QPalette::Inactive, QPalette::Midlight, brush7);
        palette1.setBrush(QPalette::Inactive, QPalette::Dark, brush8);
        palette1.setBrush(QPalette::Inactive, QPalette::Mid, brush9);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::BrightText, brush10);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush5);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush5);
        palette1.setBrush(QPalette::Inactive, QPalette::Shadow, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush11);
        palette1.setBrush(QPalette::Inactive, QPalette::ToolTipBase, brush12);
        palette1.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush5);
        palette1.setBrush(QPalette::Disabled, QPalette::Light, brush6);
        palette1.setBrush(QPalette::Disabled, QPalette::Midlight, brush7);
        palette1.setBrush(QPalette::Disabled, QPalette::Dark, brush8);
        palette1.setBrush(QPalette::Disabled, QPalette::Mid, brush9);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::BrightText, brush10);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush5);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush5);
        palette1.setBrush(QPalette::Disabled, QPalette::Shadow, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ToolTipBase, brush12);
        palette1.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush1);
        dateWidget->setPalette(palette1);
        dateWidget->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"    border: 1px solid #f26522;\n"
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
"	}"));
        dateWidget->setIconSize(QSize(16, 20));

        gridLayout_3->addWidget(dateWidget, 0, 1, 1, 1);

        amountWidget = new QLineEdit(wTransactionsContainer);
        amountWidget->setObjectName(QString::fromUtf8("amountWidget"));
        sizePolicy1.setHeightForWidth(amountWidget->sizePolicy().hasHeightForWidth());
        amountWidget->setSizePolicy(sizePolicy1);
        amountWidget->setMinimumSize(QSize(118, 22));
        amountWidget->setMaximumSize(QSize(118, 16777215));
        amountWidget->setStyleSheet(QString::fromUtf8("QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"	border: 1px solid #f26522;\n"
"	}"));

        gridLayout_3->addWidget(amountWidget, 0, 5, 1, 1);

        horizontalSpacer = new QSpacerItem(25, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        gridLayout_3->addItem(horizontalSpacer, 0, 0, 1, 1);


        verticalLayout_10->addLayout(gridLayout_3);

        transactionView = new QTableView(wTransactionsContainer);
        transactionView->setObjectName(QString::fromUtf8("transactionView"));
        QPalette palette2;
        palette2.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Light, brush);
        palette2.setBrush(QPalette::Active, QPalette::Midlight, brush);
        palette2.setBrush(QPalette::Active, QPalette::Dark, brush8);
        palette2.setBrush(QPalette::Active, QPalette::Mid, brush9);
        palette2.setBrush(QPalette::Active, QPalette::Text, brush);
        palette2.setBrush(QPalette::Active, QPalette::BrightText, brush10);
        palette2.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Shadow, brush1);
        QBrush brush13(QColor(97, 40, 14, 255));
        brush13.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Active, QPalette::Highlight, brush13);
        palette2.setBrush(QPalette::Active, QPalette::HighlightedText, brush);
        palette2.setBrush(QPalette::Active, QPalette::AlternateBase, brush5);
        palette2.setBrush(QPalette::Active, QPalette::ToolTipBase, brush1);
        palette2.setBrush(QPalette::Active, QPalette::ToolTipText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Light, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Midlight, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Dark, brush8);
        palette2.setBrush(QPalette::Inactive, QPalette::Mid, brush9);
        palette2.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::BrightText, brush10);
        palette2.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Shadow, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Highlight, brush13);
        palette2.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush5);
        palette2.setBrush(QPalette::Inactive, QPalette::ToolTipBase, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::ToolTipText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Light, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Midlight, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Dark, brush8);
        palette2.setBrush(QPalette::Disabled, QPalette::Mid, brush9);
        palette2.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::BrightText, brush10);
        palette2.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Shadow, brush1);
        QBrush brush14(QColor(51, 153, 255, 255));
        brush14.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Disabled, QPalette::Highlight, brush14);
        palette2.setBrush(QPalette::Disabled, QPalette::HighlightedText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush5);
        palette2.setBrush(QPalette::Disabled, QPalette::ToolTipBase, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::ToolTipText, brush);
        transactionView->setPalette(palette2);
        transactionView->setStyleSheet(QString::fromUtf8("#transactionView {\n"
"	border: 1px solid #f26522;\n"
"	}\n"
"\n"
"QHeaderView::section {\n"
"	border: 1px solid #f26522; \n"
"	background-color: #1c1c1c;\n"
"	height: 20px;\n"
"	padding: 0px 3px; \n"
"	}\n"
"\n"
"/* style the sort indicator */\n"
"QHeaderView::down-arrow {\n"
"	image: url(:/icons/down_arrow);\n"
"	}\n"
"\n"
"QHeaderView::up-arrow {\n"
"	image: url(:/icons/up_arrow);\n"
"	}\n"
"\n"
"QTableView::item:focus {\n"
"	border: 0px solid #f26522;\n"
"	color: #f26522;\n"
"	outline: none;\n"
"	background-color: #61280E;\n"
"	}\n"
"\n"
"QTableView {\n"
"	outline:none;\n"
"	}"));
        transactionView->setFrameShape(QFrame::NoFrame);
        transactionView->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        transactionView->setEditTriggers(QAbstractItemView::NoEditTriggers);
        transactionView->setSelectionMode(QAbstractItemView::SingleSelection);
        transactionView->setSelectionBehavior(QAbstractItemView::SelectRows);
        transactionView->setShowGrid(false);
        transactionView->verticalHeader()->setVisible(false);
        transactionView->verticalHeader()->setMinimumSectionSize(20);
        transactionView->verticalHeader()->setDefaultSectionSize(20);

        verticalLayout_10->addWidget(transactionView);

        verticalLayout_10->setStretch(1, 1);

        verticalLayout->addWidget(wTransactionsContainer);


        retranslateUi(TransactionsPage);

        QMetaObject::connectSlotsByName(TransactionsPage);
    } // setupUi

    void retranslateUi(QWidget *TransactionsPage)
    {
        TransactionsPage->setWindowTitle(QCoreApplication::translate("TransactionsPage", "Form", nullptr));
        icon_send->setText(QString());
        label_send->setText(QCoreApplication::translate("TransactionsPage", "Transactions", nullptr));
    } // retranslateUi

};

namespace Ui {
    class TransactionsPage: public Ui_TransactionsPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_TRANSACTIONSPAGE_H
