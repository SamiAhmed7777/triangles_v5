/********************************************************************************
** Form generated from reading UI file 'messagepage.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MESSAGEPAGE_H
#define UI_MESSAGEPAGE_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QGroupBox>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QListView>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTableView>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "mrichtextedit.h"

QT_BEGIN_NAMESPACE

class Ui_MessagePage
{
public:
    QVBoxLayout *verticalLayout_2;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout_4;
    QLabel *icon_messages;
    QSpacerItem *horizontalSpacer_send_1;
    QLabel *label_messages;
    QLabel *labelExplanation;
    QTableView *tableView;
    QVBoxLayout *verticalLayout;
    QGroupBox *messageDetails;
    QVBoxLayout *verticalLayout_4;
    QHBoxLayout *horizontalLayout_3;
    QPushButton *backButton;
    QLabel *labelContact;
    QLabel *contactLabel;
    QSpacerItem *horizontalSpacer_3;
    QListView *listConversation;
    QVBoxLayout *verticalLayout_3;
    MRichTextEdit *messageEdit;
    QHBoxLayout *horizontalLayout;
    QPushButton *newButton;
    QPushButton *sendButton;
    QPushButton *copyFromAddressButton;
    QPushButton *copyToAddressButton;
    QPushButton *deleteButton;
    QSpacerItem *horizontalSpacer;

    void setupUi(QWidget *MessagePage)
    {
        if (MessagePage->objectName().isEmpty())
            MessagePage->setObjectName(QString::fromUtf8("MessagePage"));
        MessagePage->resize(793, 407);
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
        MessagePage->setPalette(palette);
        MessagePage->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"	background-color: #000;\n"
"   "));
        verticalLayout_2 = new QVBoxLayout(MessagePage);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        wCaption = new QWidget(MessagePage);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 40));
        wCaption->setMaximumSize(QSize(16777215, 40));
        wCaption->setStyleSheet(QString::fromUtf8(""));
        wCaption->setProperty("is_header", QVariant(true));
        horizontalLayout_4 = new QHBoxLayout(wCaption);
        horizontalLayout_4->setSpacing(6);
        horizontalLayout_4->setObjectName(QString::fromUtf8("horizontalLayout_4"));
        horizontalLayout_4->setContentsMargins(9, 9, -1, 9);
        icon_messages = new QLabel(wCaption);
        icon_messages->setObjectName(QString::fromUtf8("icon_messages"));
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(icon_messages->sizePolicy().hasHeightForWidth());
        icon_messages->setSizePolicy(sizePolicy);
        icon_messages->setMinimumSize(QSize(30, 30));
        icon_messages->setMaximumSize(QSize(30, 30));
        icon_messages->setBaseSize(QSize(0, 0));
        icon_messages->setPixmap(QPixmap(QString::fromUtf8(":/menu_32/messages")));
        icon_messages->setAlignment(Qt::AlignLeading|Qt::AlignLeft|Qt::AlignVCenter);

        horizontalLayout_4->addWidget(icon_messages);

        horizontalSpacer_send_1 = new QSpacerItem(4, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_4->addItem(horizontalSpacer_send_1);

        label_messages = new QLabel(wCaption);
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


        verticalLayout_2->addWidget(wCaption);

        labelExplanation = new QLabel(MessagePage);
        labelExplanation->setObjectName(QString::fromUtf8("labelExplanation"));
        labelExplanation->setTextFormat(Qt::PlainText);
        labelExplanation->setWordWrap(true);

        verticalLayout_2->addWidget(labelExplanation);

        tableView = new QTableView(MessagePage);
        tableView->setObjectName(QString::fromUtf8("tableView"));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush1);
        QBrush brush2(QColor(28, 28, 28, 255));
        brush2.setStyle(Qt::SolidPattern);
        palette1.setBrush(QPalette::Active, QPalette::AlternateBase, brush2);
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush2);
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush2);
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
"	image: url(:/icons/down_arrow);\n"
"	}\n"
"\n"
"QHeaderView::up-arrow {\n"
"	image: url(:/icons/up_arrow);\n"
"	}"));
        tableView->setAutoScroll(true);
        tableView->setTabKeyNavigation(false);
        tableView->setAlternatingRowColors(true);
        tableView->setSelectionMode(QAbstractItemView::SingleSelection);
        tableView->setSelectionBehavior(QAbstractItemView::SelectRows);
        tableView->setSortingEnabled(true);
        tableView->verticalHeader()->setVisible(false);

        verticalLayout_2->addWidget(tableView);

        verticalLayout = new QVBoxLayout();
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        messageDetails = new QGroupBox(MessagePage);
        messageDetails->setObjectName(QString::fromUtf8("messageDetails"));
        messageDetails->setStyleSheet(QString::fromUtf8("#messageDetails {\n"
"	border: 1px solid #f26522;\n"
"	}"));
        verticalLayout_4 = new QVBoxLayout(messageDetails);
        verticalLayout_4->setSpacing(2);
        verticalLayout_4->setObjectName(QString::fromUtf8("verticalLayout_4"));
        verticalLayout_4->setContentsMargins(0, 6, 0, 0);
        horizontalLayout_3 = new QHBoxLayout();
        horizontalLayout_3->setObjectName(QString::fromUtf8("horizontalLayout_3"));
        horizontalLayout_3->setContentsMargins(6, -1, -1, -1);
        backButton = new QPushButton(messageDetails);
        backButton->setObjectName(QString::fromUtf8("backButton"));
        backButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 60px;\n"
"	min-width: 60px;\n"
"}\n"
"\n"
"QPushButton:hover {\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QPushButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}"));
        QIcon icon;
        icon.addFile(QString::fromUtf8(":/icons/quit"), QSize(), QIcon::Normal, QIcon::Off);
        backButton->setIcon(icon);

        horizontalLayout_3->addWidget(backButton);

        labelContact = new QLabel(messageDetails);
        labelContact->setObjectName(QString::fromUtf8("labelContact"));

        horizontalLayout_3->addWidget(labelContact);

        contactLabel = new QLabel(messageDetails);
        contactLabel->setObjectName(QString::fromUtf8("contactLabel"));

        horizontalLayout_3->addWidget(contactLabel);

        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout_3->addItem(horizontalSpacer_3);


        verticalLayout_4->addLayout(horizontalLayout_3);

        listConversation = new QListView(messageDetails);
        listConversation->setObjectName(QString::fromUtf8("listConversation"));
        QPalette palette2;
        palette2.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Text, brush);
        palette2.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Active, QPalette::Highlight, brush2);
        palette2.setBrush(QPalette::Active, QPalette::HighlightedText, brush1);
        palette2.setBrush(QPalette::Active, QPalette::AlternateBase, brush2);
        palette2.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::Highlight, brush2);
        palette2.setBrush(QPalette::Inactive, QPalette::HighlightedText, brush1);
        palette2.setBrush(QPalette::Inactive, QPalette::AlternateBase, brush2);
        palette2.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette2.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        QBrush brush3(QColor(51, 153, 255, 255));
        brush3.setStyle(Qt::SolidPattern);
        palette2.setBrush(QPalette::Disabled, QPalette::Highlight, brush3);
        palette2.setBrush(QPalette::Disabled, QPalette::HighlightedText, brush1);
        palette2.setBrush(QPalette::Disabled, QPalette::AlternateBase, brush2);
        listConversation->setPalette(palette2);
        listConversation->setStyleSheet(QString::fromUtf8("#listConversation {\n"
"	border: 1px solid #f26522;\n"
"	color: #f26522;\n"
"	}\n"
"\n"
"QListView {color:#f26522;}\n"
"\n"
"QHeaderView::section {\n"
"	border: 1px solid #f26522; \n"
"        background-color: #1c1c1c;\n"
"        height: 20px;\n"
"	}\n"
"\n"
"/* style the sort indicator */\n"
"QHeaderView::down-arrow {\n"
"	image: url(:/icons/down_arrow);\n"
"	}\n"
"\n"
"QHeaderView::up-arrow {\n"
"	image: url(:/icons/up_arrow);\n"
"	}"));

        verticalLayout_4->addWidget(listConversation);

        verticalLayout_3 = new QVBoxLayout();
        verticalLayout_3->setObjectName(QString::fromUtf8("verticalLayout_3"));

        verticalLayout_4->addLayout(verticalLayout_3);


        verticalLayout->addWidget(messageDetails);

        messageEdit = new MRichTextEdit(MessagePage);
        messageEdit->setObjectName(QString::fromUtf8("messageEdit"));
        messageEdit->setMinimumSize(QSize(0, 100));
        messageEdit->setStyleSheet(QString::fromUtf8("border: #f26522;\n"
"	"));

        verticalLayout->addWidget(messageEdit);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        newButton = new QPushButton(MessagePage);
        newButton->setObjectName(QString::fromUtf8("newButton"));
        QPalette palette3;
        palette3.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette3.setBrush(QPalette::Active, QPalette::Text, brush);
        palette3.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette3.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette3.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette3.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette3.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette3.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette3.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        QBrush brush4(QColor(97, 40, 14, 255));
        brush4.setStyle(Qt::SolidPattern);
        palette3.setBrush(QPalette::Disabled, QPalette::WindowText, brush4);
        palette3.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette3.setBrush(QPalette::Disabled, QPalette::Text, brush4);
        palette3.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
        palette3.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette3.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        newButton->setPalette(palette3);
        newButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        icon1.addFile(QString::fromUtf8(":/icons/add"), QSize(), QIcon::Normal, QIcon::Off);
        newButton->setIcon(icon1);

        horizontalLayout->addWidget(newButton);

        sendButton = new QPushButton(MessagePage);
        sendButton->setObjectName(QString::fromUtf8("sendButton"));
        QPalette palette4;
        palette4.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette4.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette4.setBrush(QPalette::Active, QPalette::Text, brush);
        palette4.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette4.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette4.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette4.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette4.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette4.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette4.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette4.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette4.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette4.setBrush(QPalette::Disabled, QPalette::WindowText, brush4);
        palette4.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette4.setBrush(QPalette::Disabled, QPalette::Text, brush4);
        palette4.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
        palette4.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette4.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        sendButton->setPalette(palette4);
        sendButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        icon2.addFile(QString::fromUtf8(":/menu_16/messages"), QSize(), QIcon::Normal, QIcon::Off);
        sendButton->setIcon(icon2);

        horizontalLayout->addWidget(sendButton);

        copyFromAddressButton = new QPushButton(MessagePage);
        copyFromAddressButton->setObjectName(QString::fromUtf8("copyFromAddressButton"));
        QPalette palette5;
        palette5.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette5.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette5.setBrush(QPalette::Active, QPalette::Text, brush);
        palette5.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette5.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette5.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette5.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette5.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette5.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette5.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette5.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette5.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette5.setBrush(QPalette::Disabled, QPalette::WindowText, brush4);
        palette5.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette5.setBrush(QPalette::Disabled, QPalette::Text, brush4);
        palette5.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
        palette5.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette5.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        copyFromAddressButton->setPalette(palette5);
        copyFromAddressButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        icon3.addFile(QString::fromUtf8(":/menu_16/copy"), QSize(), QIcon::Normal, QIcon::Off);
        copyFromAddressButton->setIcon(icon3);

        horizontalLayout->addWidget(copyFromAddressButton);

        copyToAddressButton = new QPushButton(MessagePage);
        copyToAddressButton->setObjectName(QString::fromUtf8("copyToAddressButton"));
        QPalette palette6;
        palette6.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette6.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette6.setBrush(QPalette::Active, QPalette::Text, brush);
        palette6.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette6.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette6.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette6.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette6.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette6.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette6.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette6.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette6.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette6.setBrush(QPalette::Disabled, QPalette::WindowText, brush4);
        palette6.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette6.setBrush(QPalette::Disabled, QPalette::Text, brush4);
        palette6.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
        palette6.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette6.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        copyToAddressButton->setPalette(palette6);
        copyToAddressButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        copyToAddressButton->setIcon(icon3);

        horizontalLayout->addWidget(copyToAddressButton);

        deleteButton = new QPushButton(MessagePage);
        deleteButton->setObjectName(QString::fromUtf8("deleteButton"));
        QPalette palette7;
        palette7.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette7.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette7.setBrush(QPalette::Active, QPalette::Text, brush);
        palette7.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette7.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette7.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette7.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette7.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette7.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette7.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette7.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette7.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette7.setBrush(QPalette::Disabled, QPalette::WindowText, brush4);
        palette7.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette7.setBrush(QPalette::Disabled, QPalette::Text, brush4);
        palette7.setBrush(QPalette::Disabled, QPalette::ButtonText, brush4);
        palette7.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette7.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        deleteButton->setPalette(palette7);
        deleteButton->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
        QIcon icon4;
        icon4.addFile(QString::fromUtf8(":/icons/remove"), QSize(), QIcon::Normal, QIcon::Off);
        deleteButton->setIcon(icon4);

        horizontalLayout->addWidget(deleteButton);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);


        verticalLayout->addLayout(horizontalLayout);


        verticalLayout_2->addLayout(verticalLayout);


        retranslateUi(MessagePage);

        QMetaObject::connectSlotsByName(MessagePage);
    } // setupUi

    void retranslateUi(QWidget *MessagePage)
    {
        MessagePage->setWindowTitle(QCoreApplication::translate("MessagePage", "Address Book", nullptr));
        icon_messages->setText(QString());
        label_messages->setText(QCoreApplication::translate("MessagePage", "Messages", nullptr));
        labelExplanation->setText(QCoreApplication::translate("MessagePage", "These are your sent and received encrypted messages. Click on an item to read it.", nullptr));
#if QT_CONFIG(tooltip)
        tableView->setToolTip(QCoreApplication::translate("MessagePage", "Click on a message to view it", nullptr));
#endif // QT_CONFIG(tooltip)
        messageDetails->setTitle(QString());
        backButton->setText(QCoreApplication::translate("MessagePage", "&Back", nullptr));
        labelContact->setText(QCoreApplication::translate("MessagePage", "Contact:", nullptr));
        contactLabel->setText(QString());
        newButton->setText(QCoreApplication::translate("MessagePage", "&Conversation", nullptr));
#if QT_CONFIG(tooltip)
        sendButton->setToolTip(QCoreApplication::translate("MessagePage", "Sign a message to prove you own a TRI address", nullptr));
#endif // QT_CONFIG(tooltip)
        sendButton->setText(QCoreApplication::translate("MessagePage", "&Send", nullptr));
#if QT_CONFIG(tooltip)
        copyFromAddressButton->setToolTip(QCoreApplication::translate("MessagePage", "Copy the currently selected address to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copyFromAddressButton->setText(QCoreApplication::translate("MessagePage", "&Copy From Address", nullptr));
#if QT_CONFIG(tooltip)
        copyToAddressButton->setToolTip(QCoreApplication::translate("MessagePage", "Copy the currently selected address to the system clipboard", nullptr));
#endif // QT_CONFIG(tooltip)
        copyToAddressButton->setText(QCoreApplication::translate("MessagePage", "Copy To &Address", nullptr));
#if QT_CONFIG(tooltip)
        deleteButton->setToolTip(QCoreApplication::translate("MessagePage", "Delete the currently selected address from the list", nullptr));
#endif // QT_CONFIG(tooltip)
        deleteButton->setText(QCoreApplication::translate("MessagePage", "&Delete", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MessagePage: public Ui_MessagePage {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MESSAGEPAGE_H
