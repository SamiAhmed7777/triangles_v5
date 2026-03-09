/********************************************************************************
** Form generated from reading UI file 'addressbookpage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ADDRESSBOOKPAGE_H
#define UI_ADDRESSBOOKPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AddressBookPage
{
public:
    QVBoxLayout *verticalLayout_2;
    QFrame *borderframe;
    QVBoxLayout *verticalLayout_3;
    QWidget *wAddressBookHeader;
    QHBoxLayout *horizontalLayout_2;
    QLabel *icon_ab_send;
    QLabel *icon_ab_receive;
    QSpacerItem *horizontalSpacer_ab_1;
    QLabel *label_ab;
    QFrame *frameMain;
    QVBoxLayout *verticalLayout;
    QLabel *labelExplanation;
    QTableView *tableView;
    QHBoxLayout *horizontalLayout;
    QPushButton *newAddressButton;
    QPushButton *copyToClipboard;
    QPushButton *showQRCode;
    QPushButton *signMessage;
    QPushButton *verifyMessage;
    QPushButton *deleteButton;
    QSpacerItem *horizontalSpacer;
    QDialogButtonBox *cancelbuttonBox;
    QDialogButtonBox *buttonBox;

    void setupUi(QWidget *AddressBookPage)
    {
        if (AddressBookPage->objectName().isEmpty())
            AddressBookPage->setObjectName(QString::fromUtf8("AddressBookPage"));
        AddressBookPage->resize(770, 380);
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
        palette.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        AddressBookPage->setPalette(palette);
        AddressBookPage->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"background-color: #000;\n"
"\n"
"/*QTableView {\n"
"	outline: 0 ;\n"
"	}*/\n"
"\n"
""));
        verticalLayout_2 = new QVBoxLayout(AddressBookPage);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(0, 0, 0, 0);
        borderframe = new QFrame(AddressBookPage);
        borderframe->setObjectName(QString::fromUtf8("borderframe"));
        borderframe->setStyleSheet(QString::fromUtf8(""));
        borderframe->setFrameShape(QFrame::StyledPanel);
        borderframe->setFrameShadow(QFrame::Raised);
        verticalLayout_3 = new QVBoxLayout(borderframe);
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));
        wAddressBookHeader = new QWidget(borderframe);
        wAddressBookHeader->setObjectName(QString::fromUtf8("wAddressBookHeader"));
        wAddressBookHeader->setMinimumSize(QSize(0, 40));
        wAddressBookHeader->setMaximumSize(QSize(16777215, 40));
        wAddressBookHeader->setProperty("is_header", QVariant(true));
        horizontalLayout_2 = new QHBoxLayout(wAddressBookHeader);
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        icon_ab_send = new QLabel(wAddressBookHeader);
        icon_ab_send->setObjectName(QString::fromUtf8("icon_ab_send"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(icon_ab_send->sizePolicy().hasHeightForWidth());
        icon_ab_send->setSizePolicy(sizePolicy);
        icon_ab_send->setMinimumSize(QSize(30, 30));
        icon_ab_send->setMaximumSize(QSize(30, 30));
        icon_ab_send->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/addressbook")));

        horizontalLayout_2->addWidget(icon_ab_send);

        icon_ab_receive = new QLabel(wAddressBookHeader);
        icon_ab_receive->setObjectName(QString::fromUtf8("icon_ab_receive"));
        sizePolicy.setHeightForWidth(icon_ab_receive->sizePolicy().hasHeightForWidth());
        icon_ab_receive->setSizePolicy(sizePolicy);
        icon_ab_receive->setMinimumSize(QSize(30, 30));
        icon_ab_receive->setMaximumSize(QSize(30, 30));
        icon_ab_receive->setBaseSize(QSize(0, 0));
        icon_ab_receive->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/receive")));
        icon_ab_receive->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        horizontalLayout_2->addWidget(icon_ab_receive);

        horizontalSpacer_ab_1 = new QSpacerItem(4, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_ab_1);

        label_ab = new QLabel(wAddressBookHeader);
        label_ab->setObjectName(QString::fromUtf8("label_ab"));
        label_ab->setMinimumSize(QSize(0, 30));
        label_ab->setBaseSize(QSize(0, 0));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        label_ab->setFont(font);
        label_ab->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_2->addWidget(label_ab);


        verticalLayout_3->addWidget(wAddressBookHeader);

        frameMain = new QFrame(borderframe);
        frameMain->setObjectName(QString::fromUtf8("frameMain"));
        frameMain->setFrameShape(QFrame::StyledPanel);
        frameMain->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frameMain);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        labelExplanation = new QLabel(frameMain);
        labelExplanation->setObjectName(QString::fromUtf8("labelExplanation"));
        labelExplanation->setTextFormat(Qt::PlainText);
        labelExplanation->setWordWrap(true);

        verticalLayout->addWidget(labelExplanation);

        tableView = new QTableView(frameMain);
        tableView->setObjectName(QString::fromUtf8("tableView"));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush1);
        QBrush brush2(QColor(97, 40, 14, 255));
        brush2.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::Highlight, brush2);
        palette1.setBrush(QPalette::Active, QPalette::HighlightedText, brush);
        QBrush brush3(QColor(28, 28, 28, 255));
        brush3.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::AlternateBase, brush3);
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
        palette1.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush3);
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        QBrush brush4(QColor(51, 153, 255, 255));
        brush4.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Disabled, QPalette::Highlight, brush4);
        palette1.setBrush(QPalette::Disabled, QPalette::HighlightedText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush3);
        tableView->setPalette(palette1);
        tableView->setContextMenuPolicy(Qt::CustomContextMenu);
        tableView->setStyleSheet(QString::fromUtf8("#tableView {\n"
"	border: 1px solid #f26522;\n"
"	}\n"
"\n"
"QHeaderView::section {\n"
"	border: 1px solid #f26522; \n"
"        background-color: #1c1c1c;\n"
"        height: 20px;\n"
"	}\n"
"\n"
"/* style the sort indicator */\n"
"QHeaderView::down-arrow {\n"
"    image: url(:/icons/down_arrow);\n"
"}\n"
"\n"
"QHeaderView::up-arrow {\n"
"    image: url(:/icons/up_arrow);\n"
"}\n"
"\n"
"QTableView::item:focus {\n"
"	border: 0px solid #f26522;\n"
"	color: #f26522;\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"\n"
"QTableView {\n"
"	/*border: 2px solid #ff0000;*/\n"
"	outline: 0;\n"
"}\n"
""));
        tableView->setFrameShape(QFrame::NoFrame);
        tableView->setTabKeyNavigation(false);
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionMode(QAbstractItemView::SingleSelection);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setSortingEnabled(true);
        tableView->verticalHeader()->setVisible(false);

        verticalLayout->addWidget(tableView);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        newAddressButton = new QPushButton(frameMain);
        newAddressButton->setObjectName(QString::fromUtf8("newAddressButton"));
        newAddressButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/add"), QSize(), QIcon::Normal, QIcon::Off);
        newAddressButton->setIcon(icon);

        horizontalLayout->addWidget(newAddressButton);

        copyToClipboard = new QPushButton(frameMain);
        copyToClipboard->setObjectName(QString::fromUtf8("copyToClipboard"));
        copyToClipboard->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"}"));
        QIcon icon1;
        icon1.addFile(QString::fromUtf8(":/menu_16/copy"), QSize(), QIcon::Normal, QIcon::Off);
        copyToClipboard->setIcon(icon1);

        horizontalLayout->addWidget(copyToClipboard);

        showQRCode = new QPushButton(frameMain);
        showQRCode->setObjectName(QString::fromUtf8("showQRCode"));
        showQRCode->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"}"));
        QIcon icon2;
        icon2.addFile(QString::fromUtf8(":/icons/qrcode"), QSize(), QIcon::Normal, QIcon::Off);
        showQRCode->setIcon(icon2);

        horizontalLayout->addWidget(showQRCode);

        signMessage = new QPushButton(frameMain);
        signMessage->setObjectName(QString::fromUtf8("signMessage"));
        signMessage->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"}"));
        QIcon icon3;
        icon3.addFile(QString::fromUtf8(":/menu_16/sign"), QSize(), QIcon::Normal, QIcon::Off);
        signMessage->setIcon(icon3);

        horizontalLayout->addWidget(signMessage);

        verifyMessage = new QPushButton(frameMain);
        verifyMessage->setObjectName(QString::fromUtf8("verifyMessage"));
        verifyMessage->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"}"));
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/menu_16/verify"), QSize(), QIcon::Normal, QIcon::Off);
        verifyMessage->setIcon(icon4);

        horizontalLayout->addWidget(verifyMessage);

        deleteButton = new QPushButton(frameMain);
        deleteButton->setObjectName(QString::fromUtf8("deleteButton"));
        deleteButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	text-align:left;\n"
"	border: 0px solid gray;\n"
"	padding: 3px 5px 3px 5px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"}"));
        QIcon icon5;
        icon5.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
        deleteButton->setIcon(icon5);

        horizontalLayout->addWidget(deleteButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        cancelbuttonBox = new QDialogButtonBox(frameMain);
        cancelbuttonBox->setObjectName(QString::fromUtf8("cancelbuttonBox"));
        QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(cancelbuttonBox->sizePolicy().hasHeightForWidth());
        cancelbuttonBox->setSizePolicy(sizePolicy1);
        cancelbuttonBox->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	border: 1px solid #f26522;\n"
"	padding: 3px 20px 3px 20px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	border: 1px solid #61280E;\n"
"}"));
        cancelbuttonBox->setStandardButtons(QDialogButtonBox::Cancel);

        horizontalLayout->addWidget(cancelbuttonBox);

        buttonBox = new QDialogButtonBox(frameMain);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        sizePolicy1.setHeightForWidth(buttonBox->sizePolicy().hasHeightForWidth());
        buttonBox->setSizePolicy(sizePolicy1);
        buttonBox->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	border: 1px solid #f26522;\n"
"	padding: 3px 20px 3px 20px;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:!enabled {\n"
"	color: #61280E;\n"
"	border: 1px solid #61280E;\n"
"}"));
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        horizontalLayout->addWidget(buttonBox);


        verticalLayout->addLayout(horizontalLayout);


        verticalLayout_3->addWidget(frameMain);


        verticalLayout_2->addWidget(borderframe);


        retranslateUi(AddressBookPage);

        QMetaObject::connectSlotsByName(AddressBookPage);
    } // setupUi

    void retranslateUi(QWidget *AddressBookPage)
    {
        AddressBookPage->setWindowTitle(QCoreApplication::translate("AddressBookPage", "Address Book", nullptr));
        icon_ab_send->setText(QString());
        icon_ab_receive->setText(QString());
        label_ab->setText(QCoreApplication::translate("AddressBookPage", "Address book", nullptr));
        labelExplanation->setText(QCoreApplication::translate("AddressBookPage", "These are your Triangles addresses for receiving payments. You may want to give a different one to each sender so you can keep track of who is paying you.", nullptr));
#if QT_CONFIG(tooltip)
        tableView->setToolTip(QCoreApplication::translate("AddressBookPage", "Double-click to edit address or label", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        newAddressButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Create a new address", nullptr));
#endif // QT_CONFIG(tooltip)
        newAddressButton->setText(QCoreApplication::translate("AddressBookPage", "&New Address", nullptr));
#if QT_CONFIG(tooltip)
        copyToClipboard->setToolTip(QCoreApplication::translate("AddressBookPage", "Copy the currently selected address to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copyToClipboard->setText(QCoreApplication::translate("AddressBookPage", "&Copy Address", nullptr));
        showQRCode->setText(QCoreApplication::translate("AddressBookPage", "Show &QR Code", nullptr));
#if QT_CONFIG(tooltip)
        signMessage->setToolTip(QCoreApplication::translate("AddressBookPage", "Sign a message to prove you own a TRI address", nullptr));
#endif // QT_CONFIG(tooltip)
        signMessage->setText(QCoreApplication::translate("AddressBookPage", "Sign &Message", nullptr));
#if QT_CONFIG(tooltip)
        verifyMessage->setToolTip(QCoreApplication::translate("AddressBookPage", "Verify a message to ensure it was signed with a specified TRI address", nullptr));
#endif // QT_CONFIG(tooltip)
        verifyMessage->setText(QCoreApplication::translate("AddressBookPage", "&Verify Message", nullptr));
#if QT_CONFIG(tooltip)
        deleteButton->setToolTip(QCoreApplication::translate("AddressBookPage", "Delete the currently selected address from the list", nullptr));
#endif // QT_CONFIG(tooltip)
        deleteButton->setText(QCoreApplication::translate("AddressBookPage", "&Delete", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AddressBookPage: public Ui_AddressBookPage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ADDRESSBOOKPAGE_H
