/********************************************************************************
** Form generated from reading UI file 'aboutdialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_ABOUTDIALOG_H
#define UI_ABOUTDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_AboutDialog
{
public:
    QVBoxLayout *verticalLayout_2;
    QFrame *frame;
    QVBoxLayout *verticalLayout;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout_3;
    QLabel *lbTitleIcon;
    QSpacerItem *horizontalSpacer_224;
    QLabel *label_aboutTriangles;
    QSpacerItem *horizontalSpacer_223;
    QPushButton *bClose;
    QSpacerItem *verticalSpacer;
    QHBoxLayout *horizontalLayout_2;
    QSpacerItem *horizontalSpacer5;
    QHBoxLayout *horizontalLayout;
    QLabel *label;
    QSpacerItem *horizontalSpacer_6;
    QLabel *versionLabel;
    QSpacerItem *horizontalSpacer_5;
    QSpacerItem *horizontalSpacer_2;
    QHBoxLayout *horizontalLayout_222;
    QSpacerItem *horizontalSpacer555;
    QHBoxLayout *horizontalLayout_cop2;
    QLabel *copyrightLabel;
    QSpacerItem *horizontalSpacer_222;
    QHBoxLayout *horizontalLayout_23;
    QSpacerItem *horizontalSpacer56;
    QLabel *label_2;
    QSpacerItem *horizontalSpacer_23;
    QSpacerItem *verticalSpacer_3;
    QWidget *wAcceptHolder;
    QHBoxLayout *horizontalLayout1;
    QSpacerItem *horizontalSpacer_3;
    QPushButton *bAccept;
    QSpacerItem *horizontalSpacer_4;
    QSpacerItem *verticalSpacer_2;

    void setupUi(QDialog *AboutDialog)
    {
        if (AboutDialog->objectName().isEmpty())
            AboutDialog->setObjectName(QString::fromUtf8("AboutDialog"));
        AboutDialog->resize(555, 360);
        AboutDialog->setMinimumSize(QSize(555, 360));
        AboutDialog->setMaximumSize(QSize(555, 372));
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
        AboutDialog->setPalette(palette);
        AboutDialog->setStyleSheet(QString::fromUtf8("QLabel {\n"
"	color: #f26522;\n"
"}\n"
"\n"
"QDialog {\n"
"	background-color: #000;\n"
"	border: 2px solid #f26522;\n"
"}\n"
"\n"
""));
        verticalLayout_2 = new QVBoxLayout(AboutDialog);
        verticalLayout_2->setSpacing(0);
        verticalLayout_2->setObjectName(QString::fromUtf8("verticalLayout_2"));
        verticalLayout_2->setContentsMargins(6, 6, 6, 6);
        frame = new QFrame(AboutDialog);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setStyleSheet(QString::fromUtf8("#frame {\n"
"	background-color: #000;\n"
"    border-style: 2 px solid #f26522;\n"
"}"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(frame);
        verticalLayout->setSpacing(0);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(0, 0, 0, 0);
        wCaption = new QWidget(frame);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 38));
        wCaption->setMaximumSize(QSize(16777215, 35));
        wCaption->setStyleSheet(QString::fromUtf8("#wCaption {\n"
"	background-color: #000;\n"
"}"));
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
        lbTitleIcon->setPixmap(QPixmap(QString::fromUtf8(":/icons/toolbar")));

        horizontalLayout_3->addWidget(lbTitleIcon);

        horizontalSpacer_224 = new QSpacerItem(25, 14, QSizePolicy::Fixed, QSizePolicy::Minimum);

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

        horizontalLayout_3->addWidget(bClose);


        verticalLayout->addWidget(wCaption);

        verticalSpacer = new QSpacerItem(20, 15, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout->addItem(verticalSpacer);

        horizontalLayout_2 = new QHBoxLayout();
        horizontalLayout_2->setObjectName(QString::fromUtf8("horizontalLayout_2"));
        horizontalSpacer5 = new QSpacerItem(70, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer5);

        horizontalLayout = new QHBoxLayout();
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        label = new QLabel(frame);
        label->setObjectName(QString::fromUtf8("label"));
        QFont font1;
        font1.setPointSize(10);
        font1.setBold(true);
        font1.setWeight(75);
        label->setFont(font1);
        label->setCursor(QCursor(Qt::IBeamCursor));
        label->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        horizontalLayout->addWidget(label);

        horizontalSpacer_6 = new QSpacerItem(4, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_6);

        versionLabel = new QLabel(frame);
        versionLabel->setObjectName(QString::fromUtf8("versionLabel"));
        versionLabel->setFont(font1);
        versionLabel->setCursor(QCursor(Qt::IBeamCursor));
        versionLabel->setText(QString::fromUtf8("0.1 beta"));
        versionLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        horizontalLayout->addWidget(versionLabel);

        horizontalSpacer_5 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer_5);


        horizontalLayout_2->addLayout(horizontalLayout);

        horizontalSpacer_2 = new QSpacerItem(65, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_2->addItem(horizontalSpacer_2);


        verticalLayout->addLayout(horizontalLayout_2);

        horizontalLayout_222 = new QHBoxLayout();
        horizontalLayout_222->setObjectName(QString::fromUtf8("horizontalLayout_222"));
        horizontalSpacer555 = new QSpacerItem(70, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_222->addItem(horizontalSpacer555);

        horizontalLayout_cop2 = new QHBoxLayout();
        horizontalLayout_cop2->setObjectName(QString::fromUtf8("horizontalLayout_cop2"));
        copyrightLabel = new QLabel(frame);
        copyrightLabel->setObjectName(QString::fromUtf8("copyrightLabel"));
        QFont font2;
        font2.setPointSize(10);
        font2.setBold(false);
        font2.setWeight(50);
        copyrightLabel->setFont(font2);
        copyrightLabel->setCursor(QCursor(Qt::IBeamCursor));
        copyrightLabel->setText(QString::fromUtf8("<html><head/><body><p>Copyright \302\251 2009-2015 The Bitcoin developers<br/>Copyright \302\251 2014-2015 Triangles team</p></body></html>"));
        copyrightLabel->setTextFormat(Qt::RichText);
        copyrightLabel->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        horizontalLayout_cop2->addWidget(copyrightLabel);


        horizontalLayout_222->addLayout(horizontalLayout_cop2);

        horizontalSpacer_222 = new QSpacerItem(65, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_222->addItem(horizontalSpacer_222);


        verticalLayout->addLayout(horizontalLayout_222);

        horizontalLayout_23 = new QHBoxLayout();
        horizontalLayout_23->setObjectName(QString::fromUtf8("horizontalLayout_23"));
        horizontalSpacer56 = new QSpacerItem(70, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_23->addItem(horizontalSpacer56);

        label_2 = new QLabel(frame);
        label_2->setObjectName(QString::fromUtf8("label_2"));
        label_2->setFont(font2);
        label_2->setCursor(QCursor(Qt::IBeamCursor));
        label_2->setWordWrap(true);
        label_2->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        horizontalLayout_23->addWidget(label_2);

        horizontalSpacer_23 = new QSpacerItem(65, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_23->addItem(horizontalSpacer_23);


        verticalLayout->addLayout(horizontalLayout_23);

        verticalSpacer_3 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Expanding);

        verticalLayout->addItem(verticalSpacer_3);

        wAcceptHolder = new QWidget(frame);
        wAcceptHolder->setObjectName(QString::fromUtf8("wAcceptHolder"));
        wAcceptHolder->setMinimumSize(QSize(0, 20));
        horizontalLayout1 = new QHBoxLayout(wAcceptHolder);
        horizontalLayout1->setSpacing(0);
        horizontalLayout1->setObjectName(QString::fromUtf8("horizontalLayout1"));
        horizontalLayout1->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_3 = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout1->addItem(horizontalSpacer_3);

        bAccept = new QPushButton(wAcceptHolder);
        bAccept->setObjectName(QString::fromUtf8("bAccept"));
        QPalette palette1;
        palette1.setBrush(QPalette::Active, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Text, brush);
        palette1.setBrush(QPalette::Active, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Active, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Active, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Text, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Inactive, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Inactive, QPalette::Window, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::WindowText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Button, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Text, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::ButtonText, brush);
        palette1.setBrush(QPalette::Disabled, QPalette::Base, brush1);
        palette1.setBrush(QPalette::Disabled, QPalette::Window, brush1);
        bAccept->setPalette(palette1);
        QFont font3;
        font3.setPointSize(10);
        font3.setStyleStrategy(QFont::PreferAntialias);
        bAccept->setFont(font3);
        bAccept->setStyleSheet(QString::fromUtf8("QPushButton {\n"
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
"}"));
        bAccept->setFlat(true);

        horizontalLayout1->addWidget(bAccept);

        horizontalSpacer_4 = new QSpacerItem(20, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout1->addItem(horizontalSpacer_4);


        verticalLayout->addWidget(wAcceptHolder);

        verticalSpacer_2 = new QSpacerItem(20, 20, QSizePolicy::Minimum, QSizePolicy::Fixed);

        verticalLayout->addItem(verticalSpacer_2);


        verticalLayout_2->addWidget(frame);


        retranslateUi(AboutDialog);
        QObject::connect(bAccept, SIGNAL(clicked()), AboutDialog, SLOT(accept()));
        QObject::connect(bClose, SIGNAL(clicked()), AboutDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(AboutDialog);
    } // setupUi

    void retranslateUi(QDialog *AboutDialog)
    {
        AboutDialog->setWindowTitle(QCoreApplication::translate("AboutDialog", "Dialog", nullptr));
        lbTitleIcon->setText(QString());
        label_aboutTriangles->setText(QCoreApplication::translate("AboutDialog", "About Triangles", nullptr));
        label->setText(QCoreApplication::translate("AboutDialog", "<b>Triangles</b> version", nullptr));
        label_2->setText(QCoreApplication::translate("AboutDialog", "\n"
"This is experimental software.\n"
"\n"
"Distributed under the MIT/X11 software license, see the accompanying file COPYING or http://www.opensource.org/licenses/mit-license.php.\n"
"\n"
"This product includes software developed by the OpenSSL Project for use in the OpenSSL Toolkit (http://www.openssl.org/) and cryptographic software written by Eric Young (eay@cryptsoft.com) and UPnP software written by Thomas Bernard.", nullptr));
        bAccept->setText(QCoreApplication::translate("AboutDialog", "OK", nullptr));
    } // retranslateUi

};

namespace Ui {
    class AboutDialog: public Ui_AboutDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_ABOUTDIALOG_H
