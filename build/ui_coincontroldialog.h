/********************************************************************************
** Form generated from reading UI file 'coincontroldialog.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_COINCONTROLDIALOG_H
#define UI_COINCONTROLDIALOG_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QDialog>
#include <QtWidgets/QDialogButtonBox>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QHeaderView>
#include <QtWidgets/QLabel>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QRadioButton>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>
#include "coincontroltreewidget.h"

QT_BEGIN_NAMESPACE

class Ui_CoinControlDialog
{
public:
    QHBoxLayout *horizontalLayout;
    QFrame *borderframe;
    QVBoxLayout *verticalLayout;
    QWidget *wCaption;
    QHBoxLayout *horizontalLayout_19;
    QSpacerItem *horizontalSpacer_26;
    QLabel *picAdd;
    QLabel *lbTitle;
    QPushButton *bClose;
    QHBoxLayout *horizontalLayoutTop;
    QFormLayout *formLayoutCoinControl1;
    QLabel *labelCoinControlQuantityText;
    QLabel *labelCoinControlQuantity;
    QLabel *labelCoinControlBytesText;
    QLabel *labelCoinControlBytes;
    QFormLayout *formLayoutCoinControl2;
    QLabel *labelCoinControlAmountText;
    QLabel *labelCoinControlAmount;
    QLabel *labelCoinControlPriorityText;
    QLabel *labelCoinControlPriority;
    QFormLayout *formLayoutCoinControl3;
    QLabel *labelCoinControlFeeText;
    QLabel *labelCoinControlFee;
    QLabel *labelCoinControlLowOutputText;
    QLabel *labelCoinControlLowOutput;
    QFormLayout *formLayoutCoinControl4;
    QLabel *labelCoinControlAfterFeeText;
    QLabel *labelCoinControlAfterFee;
    QLabel *labelCoinControlChangeText;
    QLabel *labelCoinControlChange;
    QFrame *frame;
    QWidget *horizontalLayoutWidget;
    QHBoxLayout *horizontalLayoutPanel;
    QPushButton *pushButtonSelectAll;
    QRadioButton *radioTreeMode;
    QRadioButton *radioListMode;
    QSpacerItem *horizontalSpacer;
    CoinControlTreeWidget *treeWidget;
    QDialogButtonBox *buttonBox;

    void setupUi(QDialog *CoinControlDialog)
    {
        if (CoinControlDialog->objectName().isEmpty())
            CoinControlDialog->setObjectName(QString::fromUtf8("CoinControlDialog"));
        CoinControlDialog->setEnabled(true);
        CoinControlDialog->resize(1000, 500);
        CoinControlDialog->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"background-color: #000;\n"
"\n"
"\n"
"QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"    border: 1px solid #f26522;\n"
"}\n"
"\n"
"\n"
"QScrollBar:horizontal, QScrollBar:vertical {\n"
"     border: 1px solid #491E0A;\n"
"     margin: 0px 16px 0 16px;\n"
"}\n"
"\n"
"QScrollBar::handle:horizontal, QScrollBar::handle:vertical\n"
"{\n"
"      border: 2px solid #491E0A;\n"
"      color #f26522;\n"
"      min-height: 20px;\n"
"}\n"
"\n"
"QScrollBar::handle:horizontal:hover, QScrollBar::handle:vertical:hover\n"
"{\n"
"      border: 2px solid #f26522;\n"
"      color #f26522;\n"
"      background: #f26522;\n"
"      min-height: 20px;\n"
"}\n"
"\n"
"QScrollBar::add-line:horizontal, QScrollBar::add-line:vertical {\n"
"      border: 1px solid #491E0A;\n"
"      width: 14px;\n"
"      subcontrol-position: right;\n"
"      subcontrol-origin: margin;\n"
"}\n"
"\n"
"QScrollBar::sub-line:horizontal, QScrollBar::sub-line:vertical {\n"
"      border: 1px solid #491E0A;\n"
"      width: 14px;\n"
"      subco"
                        "ntrol-position: left;\n"
"      subcontrol-origin: margin;\n"
"}\n"
"\n"
"QScrollBar::right-arrow:horizontal, QScrollBar::left-arrow:horizontal, QScrollBar::right-arrow:vertical, QScrollBar::left-arrow:vertical\n"
"{\n"
"      border: 2px solid #491E0A;\n"
"}\n"
"\n"
"QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal, QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical\n"
"{\n"
"      background: none;\n"
"}\n"
""));
        horizontalLayout = new QHBoxLayout(CoinControlDialog);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        borderframe = new QFrame(CoinControlDialog);
        borderframe->setObjectName(QString::fromUtf8("borderframe"));
        borderframe->setStyleSheet(QString::fromUtf8("#borderframe {\n"
"	border: 2px solid #f26522;\n"
"}\n"
"\n"
"\n"
"QTreeWidget::item:selected{\n"
"	background-color:#000000;\n"
"	color: #f26526;\n"
"	border: 0px;\n"
"	outline: none;\n"
"}\n"
"\n"
"QTreeView::item:focus {\n"
"	background-color:#000000;\n"
"	color: #f26526;\n"
"	border: 0px;\n"
"	outline: none;\n"
"}\n"
"\n"
"QTreeView {\n"
"	outline:none;\n"
"	}\n"
"\n"
"\n"
"QTreeWidget::item:hover{\n"
"	background-color:#61280E;\n"
"	color: #f26526;\n"
"	border: 0px solid #f26522;\n"
"}\n"
"\n"
"#treeWidget QHeaderView::section {\n"
"    border: 1px solid #f26522;\n"
"    background-color: #1c1c1c;\n"
"	height: 20px;\n"
"	padding: 0px 3px;\n"
" 	}	\n"
"\n"
"\n"
"/* style the sort indicator */\n"
"QHeaderView::down-arrow {\n"
"	image: url(:/icons/down_arrow);\n"
"	}\n"
"\n"
"QHeaderView::up-arrow {\n"
"	 image: url(:/icons/up_arrow);\n"
"	 }\n"
"\n"
"QScrollBar:horizontal {\n"
"  border: 1px solid #f26522;\n"
"  background: #1c1c1c;\n"
"  height: 15px;\n"
"  margin: 0px 16px 0 16px;\n"
"  }\n"
"\n"
"QScroll"
                        "Bar::handle:horizontal {\n"
"  border: 1px solid #f26522;\n"
"  background: #1c1c1c;\n"
"  min-height: 20px;\n"
"  /*border-radius: 2px;*/\n"
"  }\n"
"\n"
"QScrollBar::add-line:horizontal {\n"
"  border: 1px solid #f26522;\n"
"  /*border-radius: 2px;*/\n"
"  background: #1c1c1c;\n"
"  width: 14px;\n"
"  subcontrol-position: right;\n"
"  subcontrol-origin: margin;\n"
"  }\n"
"\n"
"QScrollBar::sub-line:horizontal {\n"
"  border: 1px solid #f26522;\n"
"  /*border-radius: 2px;*/\n"
"  background: #1c1c1c;\n"
"  width: 14px;\n"
"  subcontrol-position: left;\n"
"  subcontrol-origin: margin;\n"
"  }\n"
"\n"
"QScrollBar::right-arrow:horizontal, QScrollBar::left-arrow:horizontal {\n"
"  border: 1px solid #f26522;\n"
"  width: 1px;\n"
"  height: 1px;\n"
"  background: #f26522;\n"
"  }\n"
"\n"
"QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {\n"
"  background: none;\n"
"  }\n"
"\n"
"QScrollBar:vertical {\n"
"  background: #000;\n"
"  width: 15px;\n"
"  margin: 16px 0 16px 0;\n"
"  border: 1px solid #f26"
                        "522;\n"
"  }\n"
"\n"
"QScrollBar::handle:vertical {\n"
"  border: 1px solid #f26522;\n"
"  background: #1c1c1c;\n"
"  min-height: 20px;\n"
"  /*border-radius: 2px;*/\n"
"  }\n"
"\n"
"QScrollBar::add-line:vertical {\n"
"  border: 1px solid #f26522;\n"
"  /*border-radius: 2px;*/\n"
"  background: #1c1c1c;\n"
"  height: 14px;\n"
"  subcontrol-position: bottom;\n"
"  subcontrol-origin: margin;\n"
"  }\n"
"\n"
"QScrollBar::sub-line:vertical {\n"
"  border: 1px solid #f26522;\n"
"  /*border-radius: 2px;*/\n"
"  background: #1c1c1c;\n"
"  height: 14px;\n"
"  subcontrol-position: top;\n"
"  subcontrol-origin: margin;\n"
"  }\n"
"\n"
"QScrollBar::up-arrow:vertical, QScrollBar::down-arrow:vertical {\n"
"  border: 1px solid #f26522;\n"
"  width: 1px;\n"
"  height: 1px;\n"
"  background: #f26522;\n"
"  }\n"
"\n"
"QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {\n"
"  background: none;\n"
"  }"));
        borderframe->setFrameShape(QFrame::StyledPanel);
        borderframe->setFrameShadow(QFrame::Raised);
        verticalLayout = new QVBoxLayout(borderframe);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        wCaption = new QWidget(borderframe);
        wCaption->setObjectName(QString::fromUtf8("wCaption"));
        wCaption->setMinimumSize(QSize(0, 32));
        wCaption->setMaximumSize(QSize(16777215, 32));
        wCaption->setStyleSheet(QString::fromUtf8(""));
        wCaption->setProperty("is_header", QVariant(true));
        horizontalLayout_19 = new QHBoxLayout(wCaption);
        horizontalLayout_19->setObjectName(QString::fromUtf8("horizontalLayout_19"));
        horizontalLayout_19->setContentsMargins(0, 0, 0, 0);
        horizontalSpacer_26 = new QSpacerItem(7, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);

        horizontalLayout_19->addItem(horizontalSpacer_26);

        picAdd = new QLabel(wCaption);
        picAdd->setObjectName(QString::fromUtf8("picAdd"));
        picAdd->setEnabled(true);
        QSizePolicy sizePolicy(QSizePolicy::Preferred, QSizePolicy::Preferred);
        sizePolicy.setHorizontalStretch(0);
        sizePolicy.setVerticalStretch(0);
        sizePolicy.setHeightForWidth(picAdd->sizePolicy().hasHeightForWidth());
        picAdd->setSizePolicy(sizePolicy);
        picAdd->setMinimumSize(QSize(30, 30));
        picAdd->setMaximumSize(QSize(30, 30));
        picAdd->setPixmap(QPixmap(QString::fromUtf8(":/icons/toolbar")));

        horizontalLayout_19->addWidget(picAdd);

        lbTitle = new QLabel(wCaption);
        lbTitle->setObjectName(QString::fromUtf8("lbTitle"));
        QFont font;
        font.setPointSize(11);
        font.setBold(true);
        font.setWeight(75);
        font.setStyleStrategy(QFont::PreferAntialias);
        lbTitle->setFont(font);
        lbTitle->setStyleSheet(QString::fromUtf8(""));

        horizontalLayout_19->addWidget(lbTitle);

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

        horizontalLayout_19->addWidget(bClose);


        verticalLayout->addWidget(wCaption);

        horizontalLayoutTop = new QHBoxLayout();
        horizontalLayoutTop->setObjectName(QString::fromUtf8("horizontalLayoutTop"));
        horizontalLayoutTop->setContentsMargins(-1, 0, -1, 10);
        formLayoutCoinControl1 = new QFormLayout();
        formLayoutCoinControl1->setObjectName(QString::fromUtf8("formLayoutCoinControl1"));
        formLayoutCoinControl1->setHorizontalSpacing(10);
        formLayoutCoinControl1->setVerticalSpacing(10);
        formLayoutCoinControl1->setContentsMargins(6, -1, 6, -1);
        labelCoinControlQuantityText = new QLabel(borderframe);
        labelCoinControlQuantityText->setObjectName(QString::fromUtf8("labelCoinControlQuantityText"));
        labelCoinControlQuantityText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl1->setWidget(0, QFormLayout::LabelRole, labelCoinControlQuantityText);

        labelCoinControlQuantity = new QLabel(borderframe);
        labelCoinControlQuantity->setObjectName(QString::fromUtf8("labelCoinControlQuantity"));
        QFont font1;
        font1.setFamily(QString::fromUtf8("Monospace"));
        font1.setPointSize(10);
        labelCoinControlQuantity->setFont(font1);
        labelCoinControlQuantity->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlQuantity->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlQuantity->setText(QString::fromUtf8("0"));
        labelCoinControlQuantity->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl1->setWidget(0, QFormLayout::FieldRole, labelCoinControlQuantity);

        labelCoinControlBytesText = new QLabel(borderframe);
        labelCoinControlBytesText->setObjectName(QString::fromUtf8("labelCoinControlBytesText"));
        labelCoinControlBytesText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl1->setWidget(1, QFormLayout::LabelRole, labelCoinControlBytesText);

        labelCoinControlBytes = new QLabel(borderframe);
        labelCoinControlBytes->setObjectName(QString::fromUtf8("labelCoinControlBytes"));
        labelCoinControlBytes->setFont(font1);
        labelCoinControlBytes->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlBytes->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlBytes->setText(QString::fromUtf8("0"));
        labelCoinControlBytes->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl1->setWidget(1, QFormLayout::FieldRole, labelCoinControlBytes);


        horizontalLayoutTop->addLayout(formLayoutCoinControl1);

        formLayoutCoinControl2 = new QFormLayout();
        formLayoutCoinControl2->setObjectName(QString::fromUtf8("formLayoutCoinControl2"));
        formLayoutCoinControl2->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        formLayoutCoinControl2->setHorizontalSpacing(10);
        formLayoutCoinControl2->setVerticalSpacing(10);
        formLayoutCoinControl2->setContentsMargins(6, -1, 6, -1);
        labelCoinControlAmountText = new QLabel(borderframe);
        labelCoinControlAmountText->setObjectName(QString::fromUtf8("labelCoinControlAmountText"));
        labelCoinControlAmountText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl2->setWidget(0, QFormLayout::LabelRole, labelCoinControlAmountText);

        labelCoinControlAmount = new QLabel(borderframe);
        labelCoinControlAmount->setObjectName(QString::fromUtf8("labelCoinControlAmount"));
        labelCoinControlAmount->setFont(font1);
        labelCoinControlAmount->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlAmount->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlAmount->setText(QString::fromUtf8("0.00 TRI"));
        labelCoinControlAmount->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl2->setWidget(0, QFormLayout::FieldRole, labelCoinControlAmount);

        labelCoinControlPriorityText = new QLabel(borderframe);
        labelCoinControlPriorityText->setObjectName(QString::fromUtf8("labelCoinControlPriorityText"));
        labelCoinControlPriorityText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl2->setWidget(1, QFormLayout::LabelRole, labelCoinControlPriorityText);

        labelCoinControlPriority = new QLabel(borderframe);
        labelCoinControlPriority->setObjectName(QString::fromUtf8("labelCoinControlPriority"));
        labelCoinControlPriority->setFont(font1);
        labelCoinControlPriority->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlPriority->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlPriority->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl2->setWidget(1, QFormLayout::FieldRole, labelCoinControlPriority);


        horizontalLayoutTop->addLayout(formLayoutCoinControl2);

        formLayoutCoinControl3 = new QFormLayout();
        formLayoutCoinControl3->setObjectName(QString::fromUtf8("formLayoutCoinControl3"));
        formLayoutCoinControl3->setHorizontalSpacing(10);
        formLayoutCoinControl3->setVerticalSpacing(10);
        formLayoutCoinControl3->setContentsMargins(6, -1, 6, -1);
        labelCoinControlFeeText = new QLabel(borderframe);
        labelCoinControlFeeText->setObjectName(QString::fromUtf8("labelCoinControlFeeText"));
        labelCoinControlFeeText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl3->setWidget(0, QFormLayout::LabelRole, labelCoinControlFeeText);

        labelCoinControlFee = new QLabel(borderframe);
        labelCoinControlFee->setObjectName(QString::fromUtf8("labelCoinControlFee"));
        labelCoinControlFee->setFont(font1);
        labelCoinControlFee->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlFee->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlFee->setText(QString::fromUtf8("0.00 TRI"));
        labelCoinControlFee->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl3->setWidget(0, QFormLayout::FieldRole, labelCoinControlFee);

        labelCoinControlLowOutputText = new QLabel(borderframe);
        labelCoinControlLowOutputText->setObjectName(QString::fromUtf8("labelCoinControlLowOutputText"));
        labelCoinControlLowOutputText->setEnabled(false);
        labelCoinControlLowOutputText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl3->setWidget(1, QFormLayout::LabelRole, labelCoinControlLowOutputText);

        labelCoinControlLowOutput = new QLabel(borderframe);
        labelCoinControlLowOutput->setObjectName(QString::fromUtf8("labelCoinControlLowOutput"));
        labelCoinControlLowOutput->setEnabled(false);
        labelCoinControlLowOutput->setFont(font1);
        labelCoinControlLowOutput->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlLowOutput->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlLowOutput->setText(QString::fromUtf8("no"));
        labelCoinControlLowOutput->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl3->setWidget(1, QFormLayout::FieldRole, labelCoinControlLowOutput);


        horizontalLayoutTop->addLayout(formLayoutCoinControl3);

        formLayoutCoinControl4 = new QFormLayout();
        formLayoutCoinControl4->setObjectName(QString::fromUtf8("formLayoutCoinControl4"));
        formLayoutCoinControl4->setHorizontalSpacing(10);
        formLayoutCoinControl4->setVerticalSpacing(10);
        formLayoutCoinControl4->setContentsMargins(6, -1, 6, -1);
        labelCoinControlAfterFeeText = new QLabel(borderframe);
        labelCoinControlAfterFeeText->setObjectName(QString::fromUtf8("labelCoinControlAfterFeeText"));
        labelCoinControlAfterFeeText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl4->setWidget(0, QFormLayout::LabelRole, labelCoinControlAfterFeeText);

        labelCoinControlAfterFee = new QLabel(borderframe);
        labelCoinControlAfterFee->setObjectName(QString::fromUtf8("labelCoinControlAfterFee"));
        labelCoinControlAfterFee->setFont(font1);
        labelCoinControlAfterFee->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlAfterFee->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlAfterFee->setText(QString::fromUtf8("0.00 TRI"));
        labelCoinControlAfterFee->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl4->setWidget(0, QFormLayout::FieldRole, labelCoinControlAfterFee);

        labelCoinControlChangeText = new QLabel(borderframe);
        labelCoinControlChangeText->setObjectName(QString::fromUtf8("labelCoinControlChangeText"));
        labelCoinControlChangeText->setEnabled(false);
        labelCoinControlChangeText->setStyleSheet(QString::fromUtf8("font-weight:bold;"));

        formLayoutCoinControl4->setWidget(1, QFormLayout::LabelRole, labelCoinControlChangeText);

        labelCoinControlChange = new QLabel(borderframe);
        labelCoinControlChange->setObjectName(QString::fromUtf8("labelCoinControlChange"));
        labelCoinControlChange->setEnabled(false);
        labelCoinControlChange->setFont(font1);
        labelCoinControlChange->setCursor(QCursor(Qt::IBeamCursor));
        labelCoinControlChange->setContextMenuPolicy(Qt::ActionsContextMenu);
        labelCoinControlChange->setText(QString::fromUtf8("0.00 TRI"));
        labelCoinControlChange->setTextInteractionFlags(Qt::LinksAccessibleByMouse|Qt::TextSelectableByKeyboard|Qt::TextSelectableByMouse);

        formLayoutCoinControl4->setWidget(1, QFormLayout::FieldRole, labelCoinControlChange);


        horizontalLayoutTop->addLayout(formLayoutCoinControl4);


        verticalLayout->addLayout(horizontalLayoutTop);

        frame = new QFrame(borderframe);
        frame->setObjectName(QString::fromUtf8("frame"));
        frame->setMinimumSize(QSize(0, 40));
        frame->setStyleSheet(QString::fromUtf8("#frame {\n"
"	border: 1 px solid #f26522;\n"
"	}"));
        frame->setFrameShape(QFrame::StyledPanel);
        frame->setFrameShadow(QFrame::Sunken);
        horizontalLayoutWidget = new QWidget(frame);
        horizontalLayoutWidget->setObjectName(QString::fromUtf8("horizontalLayoutWidget"));
        horizontalLayoutWidget->setGeometry(QRect(10, 0, 781, 41));
        horizontalLayoutPanel = new QHBoxLayout(horizontalLayoutWidget);
        horizontalLayoutPanel->setSpacing(14);
        horizontalLayoutPanel->setObjectName(QString::fromUtf8("horizontalLayoutPanel"));
        horizontalLayoutPanel->setContentsMargins(0, 0, 0, 0);
        pushButtonSelectAll = new QPushButton(horizontalLayoutWidget);
        pushButtonSelectAll->setObjectName(QString::fromUtf8("pushButtonSelectAll"));
        QSizePolicy sizePolicy1(QSizePolicy::Maximum, QSizePolicy::Fixed);
        sizePolicy1.setHorizontalStretch(0);
        sizePolicy1.setVerticalStretch(0);
        sizePolicy1.setHeightForWidth(pushButtonSelectAll->sizePolicy().hasHeightForWidth());
        pushButtonSelectAll->setSizePolicy(sizePolicy1);
        pushButtonSelectAll->setStyleSheet(QString::fromUtf8("QPushButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"	max-height: 20px;\n"
"	min-height: 20px;\n"
"	max-width: 100px;\n"
"	min-width: 100px;\n"
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

        horizontalLayoutPanel->addWidget(pushButtonSelectAll);

        radioTreeMode = new QRadioButton(horizontalLayoutWidget);
        radioTreeMode->setObjectName(QString::fromUtf8("radioTreeMode"));
        sizePolicy1.setHeightForWidth(radioTreeMode->sizePolicy().hasHeightForWidth());
        radioTreeMode->setSizePolicy(sizePolicy1);
        radioTreeMode->setStyleSheet(QString::fromUtf8("QRadioButton {\n"
"	background-color: #000;\n"
"    	color: #f26522;\n"
"    	}\n"
" \n"
"QRadioButton::indicator {\n"
"	border-radius: 6px;\n"
"    	color: #f26522;\n"
"	}\n"
"\n"
"QRadioButton::indicator:unchecked {\n"
"	border: 1px solid #f26522;\n"
"	background-color: #000;\n"
"    	}\n"
"\n"
"QRadioButton::indicator:checked {\n"
"	 border: 1px solid #f26522;\n"
"	background-color: qradialgradient(\n"
"        cx: 0.5, cy: 0.5,\n"
"        fx: 0.5, fy: 0.5,\n"
"        radius: 1.0,\n"
"        stop: 0.15 #f26522,\n"
"        stop: 0.25 #000\n"
"	);\n"
"    	}\n"
"\n"
"\n"
"RadioButton::indicator:disabled {\n"
"	background-color: #000;\n"
"	color: #61280E;\n"
"	border: 1px solid #61280E;\n"
"    	}"));
        radioTreeMode->setChecked(true);

        horizontalLayoutPanel->addWidget(radioTreeMode);

        radioListMode = new QRadioButton(horizontalLayoutWidget);
        radioListMode->setObjectName(QString::fromUtf8("radioListMode"));
        radioListMode->setEnabled(true);
        sizePolicy1.setHeightForWidth(radioListMode->sizePolicy().hasHeightForWidth());
        radioListMode->setSizePolicy(sizePolicy1);
        radioListMode->setStyleSheet(QString::fromUtf8("QRadioButton {\n"
"	background-color: #000;\n"
"    	color: #f26522;\n"
"    	}\n"
" \n"
" QRadioButton::indicator {\n"
"	border-radius: 6px;\n"
" 	color: #f26522;\n"
"	}\n"
"\n"
"QRadioButton::indicator:unchecked {\n"
"	border: 1px solid #f26522;\n"
"	background-color: #000;\n"
"	}\n"
"\n"
"QRadioButton::indicator:checked {\n"
"	 border: 1px solid #f26522;\n"
"	background-color: qradialgradient(\n"
"        cx: 0.5, cy: 0.5,\n"
"        fx: 0.5, fy: 0.5,\n"
"        radius: 1.0,\n"
"        stop: 0.15 #f26522,\n"
"        stop: 0.25 #000\n"
"	);\n"
"	}\n"
"\n"
"\n"
"RadioButton::indicator:disabled {\n"
"	background-color: #000;\n"
"	color: #61280E;\n"
"	border: 1px solid #61280E;\n"
"	}"));
        radioListMode->setChecked(false);

        horizontalLayoutPanel->addWidget(radioListMode);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayoutPanel->addItem(horizontalSpacer);


        verticalLayout->addWidget(frame);

        treeWidget = new CoinControlTreeWidget(borderframe);
        treeWidget->headerItem()->setText(0, QString());
        treeWidget->headerItem()->setText(7, QString());
        treeWidget->headerItem()->setText(8, QString());
        treeWidget->headerItem()->setText(9, QString());
        treeWidget->headerItem()->setText(10, QString());
        treeWidget->setObjectName(QString::fromUtf8("treeWidget"));
        QSizePolicy sizePolicy2(QSizePolicy::Expanding, QSizePolicy::Expanding);
        sizePolicy2.setHorizontalStretch(0);
        sizePolicy2.setVerticalStretch(0);
        sizePolicy2.setHeightForWidth(treeWidget->sizePolicy().hasHeightForWidth());
        treeWidget->setSizePolicy(sizePolicy2);
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
        treeWidget->setPalette(palette);
        treeWidget->setContextMenuPolicy(Qt::CustomContextMenu);
        treeWidget->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"background-color: #000;\n"
"\n"
"\n"
"border: 1px solid #f26522;\n"
"\n"
"QLineEdit {\n"
"	background-color: #1c1c1c;\n"
"    border: 1px solid #f26522;\n"
"}\n"
"\n"
""));
        treeWidget->setSortingEnabled(false);
        treeWidget->setColumnCount(11);
        treeWidget->header()->setProperty("showSortIndicator", QVariant(true));
        treeWidget->header()->setStretchLastSection(false);

        verticalLayout->addWidget(treeWidget);

        buttonBox = new QDialogButtonBox(borderframe);
        buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
        sizePolicy1.setHeightForWidth(buttonBox->sizePolicy().hasHeightForWidth());
        buttonBox->setSizePolicy(sizePolicy1);
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
        buttonBox->setStandardButtons(QDialogButtonBox::Ok);

        verticalLayout->addWidget(buttonBox);


        horizontalLayout->addWidget(borderframe);


        retranslateUi(CoinControlDialog);
        QObject::connect(bClose, SIGNAL(clicked()), CoinControlDialog, SLOT(reject()));

        QMetaObject::connectSlotsByName(CoinControlDialog);
    } // setupUi

    void retranslateUi(QDialog *CoinControlDialog)
    {
        CoinControlDialog->setWindowTitle(QCoreApplication::translate("CoinControlDialog", "Coin Control", nullptr));
        picAdd->setText(QString());
        lbTitle->setText(QCoreApplication::translate("CoinControlDialog", "Triangles Coin Control", nullptr));
        labelCoinControlQuantityText->setText(QCoreApplication::translate("CoinControlDialog", "Quantity:", nullptr));
        labelCoinControlBytesText->setText(QCoreApplication::translate("CoinControlDialog", "Bytes:", nullptr));
        labelCoinControlAmountText->setText(QCoreApplication::translate("CoinControlDialog", "Amount:", nullptr));
        labelCoinControlPriorityText->setText(QCoreApplication::translate("CoinControlDialog", "Priority:", nullptr));
        labelCoinControlPriority->setText(QString());
        labelCoinControlFeeText->setText(QCoreApplication::translate("CoinControlDialog", "Fee:", nullptr));
        labelCoinControlLowOutputText->setText(QCoreApplication::translate("CoinControlDialog", "Low Output:", nullptr));
        labelCoinControlAfterFeeText->setText(QCoreApplication::translate("CoinControlDialog", "After Fee:", nullptr));
        labelCoinControlChangeText->setText(QCoreApplication::translate("CoinControlDialog", "Change:", nullptr));
        pushButtonSelectAll->setText(QCoreApplication::translate("CoinControlDialog", "(un)select all", nullptr));
        radioTreeMode->setText(QCoreApplication::translate("CoinControlDialog", "Tree mode", nullptr));
        radioListMode->setText(QCoreApplication::translate("CoinControlDialog", "List mode", nullptr));
        QTreeWidgetItem *___qtreewidgetitem = treeWidget->headerItem();
        ___qtreewidgetitem->setText(6, QCoreApplication::translate("CoinControlDialog", "Priority", nullptr));
        ___qtreewidgetitem->setText(5, QCoreApplication::translate("CoinControlDialog", "Confirmations", nullptr));
        ___qtreewidgetitem->setText(4, QCoreApplication::translate("CoinControlDialog", "Date", nullptr));
        ___qtreewidgetitem->setText(3, QCoreApplication::translate("CoinControlDialog", "Address", nullptr));
        ___qtreewidgetitem->setText(2, QCoreApplication::translate("CoinControlDialog", "Label", nullptr));
        ___qtreewidgetitem->setText(1, QCoreApplication::translate("CoinControlDialog", "Amount", nullptr));
#if QT_CONFIG(tooltip)
        ___qtreewidgetitem->setToolTip(5, QCoreApplication::translate("CoinControlDialog", "Confirmed", nullptr));
#endif // QT_CONFIG(tooltip)
    } // retranslateUi

};

namespace Ui {
    class CoinControlDialog: public Ui_CoinControlDialog {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_COINCONTROLDIALOG_H
