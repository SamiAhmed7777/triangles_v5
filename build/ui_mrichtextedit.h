/********************************************************************************
** Form generated from reading UI file 'mrichtextedit.ui'
**
** Created by: Qt User Interface Compiler version 5.15.18
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MRICHTEXTEDIT_H
#define UI_MRICHTEXTEDIT_H

#include <QtCore/QVariant>
#include <QtGui/QIcon>
#include <QtWidgets/QApplication>
#include <QtWidgets/QComboBox>
#include <QtWidgets/QFrame>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QSpacerItem>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QToolButton>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MRichTextEdit
{
public:
    QVBoxLayout *verticalLayout;
    QWidget *f_toolbar;
    QHBoxLayout *horizontalLayout;
    QComboBox *f_paragraph;
    QFrame *line_4;
    QToolButton *f_undo;
    QToolButton *f_redo;
    QToolButton *f_cut;
    QToolButton *f_copy;
    QToolButton *f_paste;
    QFrame *line;
    QToolButton *f_link;
    QFrame *line_3;
    QToolButton *f_bold;
    QToolButton *f_italic;
    QToolButton *f_underline;
    QToolButton *f_strikeout;
    QFrame *line_5;
    QToolButton *f_list_bullet;
    QToolButton *f_list_ordered;
    QToolButton *f_indent_dec;
    QToolButton *f_indent_inc;
    QFrame *line_2;
    QToolButton *f_bgcolor;
    QComboBox *f_fontsize;
    QFrame *line_6;
    QSpacerItem *horizontalSpacer;
    QTextEdit *f_textedit;

    void setupUi(QWidget *MRichTextEdit)
    {
        if (MRichTextEdit->objectName().isEmpty())
            MRichTextEdit->setObjectName(QString::fromUtf8("MRichTextEdit"));
        MRichTextEdit->resize(819, 312);
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
        MRichTextEdit->setPalette(palette);
        MRichTextEdit->setWindowTitle(QString::fromUtf8(""));
        MRichTextEdit->setStyleSheet(QString::fromUtf8("color: #f26522;\n"
"background-color: #000;\n"
"\n"
""));
        verticalLayout = new QVBoxLayout(MRichTextEdit);
        verticalLayout->setSpacing(1);
        verticalLayout->setObjectName(QString::fromUtf8("verticalLayout"));
        verticalLayout->setContentsMargins(1, 1, 1, 1);
        f_toolbar = new QWidget(MRichTextEdit);
        f_toolbar->setObjectName(QString::fromUtf8("f_toolbar"));
        f_toolbar->setStyleSheet(QString::fromUtf8("QToolTip {\n"
"                             color: #f26522;\n"
"                             border: 1px solid #61280E;\n"
"                             background-color: #000;\n"
"                             /*padding: 10px 10px;*/\n"
"                             border-radius: 3px;\n"
"                             /*opacity: 200;*/\n"
"                             min-height: 20px;\n"
"                             /*min-width:60px;*/\n"
"                        }\n"
"QToolButton {\n"
"	background-color: #000;\n"
"	color: #f26522;\n"
"	border: 1px solid #f26522;\n"
"}\n"
"\n"
"QToolButton:hover {\n"
"	color: #f26522;\n"
"	background-color: #61280E;\n"
"}\n"
"\n"
"QToolButton:pressed:flat {\n"
"	color: #000;\n"
"	background-color: #f26522;\n"
"}\n"
"\n"
"QToolButton:!enabled {\n"
"	color: #61280E;\n"
"	border: 1px solid #61280E;\n"
"}\n"
"\n"
"QComboBox {\n"
"    border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"    subcontrol-origin: padding;\n"
"    subcon"
                        "trol-position: top right;\n"
"    /*width: 15px;*/\n"
"    border-left-width: 1px;\n"
"    border-left-color: #f26522;\n"
"    border-left-style: solid;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"    image: url(:/icons/down_arrow);\n"
"}\n"
"\n"
"QComboBox QAbstractItemView\n"
"{\n"
"background-color: #1c1c1c;;\n"
"selection-background-color:#61280E;\n"
"selection-color: #f26522; \n"
"border: 1px solid #f26522;\n"
"color:#f26522;\n"
"}\n"
""));
        horizontalLayout = new QHBoxLayout(f_toolbar);
        horizontalLayout->setSpacing(2);
        horizontalLayout->setObjectName(QString::fromUtf8("horizontalLayout"));
        horizontalLayout->setContentsMargins(0, 0, 0, 0);
        f_paragraph = new QComboBox(f_toolbar);
        f_paragraph->setObjectName(QString::fromUtf8("f_paragraph"));
        f_paragraph->setFocusPolicy(Qt::ClickFocus);
        f_paragraph->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"    border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"    subcontrol-origin: padding;\n"
"    subcontrol-position: top right;\n"
"    /*width: 15px;*/\n"
"    border-left-width: 1px;\n"
"    border-left-color: #f26522;\n"
"    border-left-style: solid;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"    image: url(:/icons/down_arrow);\n"
"}\n"
"\n"
"QComboBox QAbstractItemView\n"
"{\n"
"background-color: #1c1c1c;\n"
"selection-background-color:#61280E;\n"
"selection-color: #f26522; \n"
"border: 1px solid #f26522;\n"
"color:#f26522;\n"
"}\n"
""));
        f_paragraph->setEditable(true);

        horizontalLayout->addWidget(f_paragraph);

        line_4 = new QFrame(f_toolbar);
        line_4->setObjectName(QString::fromUtf8("line_4"));
        line_4->setFrameShape(QFrame::VLine);
        line_4->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_4);

        f_undo = new QToolButton(f_toolbar);
        f_undo->setObjectName(QString::fromUtf8("f_undo"));
        f_undo->setEnabled(false);
        f_undo->setFocusPolicy(Qt::ClickFocus);
        f_undo->setStyleSheet(QString::fromUtf8(""));
        QIcon icon;
        QString iconThemeName = QString::fromUtf8("edit-undo");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon = QIcon::fromTheme(iconThemeName);
        } else {
            icon.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_undo->setIcon(icon);
        f_undo->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_undo);

        f_redo = new QToolButton(f_toolbar);
        f_redo->setObjectName(QString::fromUtf8("f_redo"));
        f_redo->setEnabled(false);
        f_redo->setFocusPolicy(Qt::ClickFocus);
        f_redo->setStyleSheet(QString::fromUtf8(""));
        QIcon icon1;
        iconThemeName = QString::fromUtf8("edit-redo");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon1 = QIcon::fromTheme(iconThemeName);
        } else {
            icon1.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_redo->setIcon(icon1);
        f_redo->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_redo);

        f_cut = new QToolButton(f_toolbar);
        f_cut->setObjectName(QString::fromUtf8("f_cut"));
        f_cut->setFocusPolicy(Qt::ClickFocus);
        f_cut->setStyleSheet(QString::fromUtf8(""));
        QIcon icon2;
        iconThemeName = QString::fromUtf8("edit-cut");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon2 = QIcon::fromTheme(iconThemeName);
        } else {
            icon2.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_cut->setIcon(icon2);
        f_cut->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_cut);

        f_copy = new QToolButton(f_toolbar);
        f_copy->setObjectName(QString::fromUtf8("f_copy"));
        f_copy->setFocusPolicy(Qt::ClickFocus);
        f_copy->setStyleSheet(QString::fromUtf8(""));
        QIcon icon3;
        iconThemeName = QString::fromUtf8("edit-copy");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon3 = QIcon::fromTheme(iconThemeName);
        } else {
            icon3.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_copy->setIcon(icon3);
        f_copy->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_copy);

        f_paste = new QToolButton(f_toolbar);
        f_paste->setObjectName(QString::fromUtf8("f_paste"));
        f_paste->setFocusPolicy(Qt::ClickFocus);
        f_paste->setStyleSheet(QString::fromUtf8(""));
        QIcon icon4;
        iconThemeName = QString::fromUtf8("edit-paste");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon4 = QIcon::fromTheme(iconThemeName);
        } else {
            icon4.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_paste->setIcon(icon4);
        f_paste->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_paste);

        line = new QFrame(f_toolbar);
        line->setObjectName(QString::fromUtf8("line"));
        line->setFrameShape(QFrame::VLine);
        line->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line);

        f_link = new QToolButton(f_toolbar);
        f_link->setObjectName(QString::fromUtf8("f_link"));
        f_link->setFocusPolicy(Qt::ClickFocus);
        f_link->setStyleSheet(QString::fromUtf8(""));
        QIcon icon5;
        iconThemeName = QString::fromUtf8("applications-internet");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon5 = QIcon::fromTheme(iconThemeName);
        } else {
            icon5.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_link->setIcon(icon5);
        f_link->setIconSize(QSize(16, 16));
        f_link->setCheckable(true);

        horizontalLayout->addWidget(f_link);

        line_3 = new QFrame(f_toolbar);
        line_3->setObjectName(QString::fromUtf8("line_3"));
        line_3->setFrameShape(QFrame::VLine);
        line_3->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_3);

        f_bold = new QToolButton(f_toolbar);
        f_bold->setObjectName(QString::fromUtf8("f_bold"));
        f_bold->setFocusPolicy(Qt::ClickFocus);
#if QT_CONFIG(tooltip)
        f_bold->setToolTip(QString::fromUtf8("Bold (CTRL+B)"));
#endif // QT_CONFIG(tooltip)
        f_bold->setStyleSheet(QString::fromUtf8(""));
        QIcon icon6;
        iconThemeName = QString::fromUtf8("format-text-bold");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon6 = QIcon::fromTheme(iconThemeName);
        } else {
            icon6.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_bold->setIcon(icon6);
        f_bold->setIconSize(QSize(16, 16));
        f_bold->setCheckable(true);

        horizontalLayout->addWidget(f_bold);

        f_italic = new QToolButton(f_toolbar);
        f_italic->setObjectName(QString::fromUtf8("f_italic"));
        f_italic->setFocusPolicy(Qt::ClickFocus);
        f_italic->setStyleSheet(QString::fromUtf8(""));
        QIcon icon7;
        iconThemeName = QString::fromUtf8("format-text-italic");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon7 = QIcon::fromTheme(iconThemeName);
        } else {
            icon7.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_italic->setIcon(icon7);
        f_italic->setIconSize(QSize(16, 16));
        f_italic->setCheckable(true);

        horizontalLayout->addWidget(f_italic);

        f_underline = new QToolButton(f_toolbar);
        f_underline->setObjectName(QString::fromUtf8("f_underline"));
        f_underline->setFocusPolicy(Qt::ClickFocus);
        f_underline->setStyleSheet(QString::fromUtf8(""));
        QIcon icon8;
        iconThemeName = QString::fromUtf8("format-text-underline");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon8 = QIcon::fromTheme(iconThemeName);
        } else {
            icon8.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_underline->setIcon(icon8);
        f_underline->setIconSize(QSize(16, 16));
        f_underline->setCheckable(true);

        horizontalLayout->addWidget(f_underline);

        f_strikeout = new QToolButton(f_toolbar);
        f_strikeout->setObjectName(QString::fromUtf8("f_strikeout"));
        f_strikeout->setStyleSheet(QString::fromUtf8(""));
        f_strikeout->setCheckable(true);

        horizontalLayout->addWidget(f_strikeout);

        line_5 = new QFrame(f_toolbar);
        line_5->setObjectName(QString::fromUtf8("line_5"));
        line_5->setFrameShape(QFrame::VLine);
        line_5->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_5);

        f_list_bullet = new QToolButton(f_toolbar);
        f_list_bullet->setObjectName(QString::fromUtf8("f_list_bullet"));
        f_list_bullet->setFocusPolicy(Qt::ClickFocus);
        f_list_bullet->setStyleSheet(QString::fromUtf8(""));
        f_list_bullet->setIconSize(QSize(16, 16));
        f_list_bullet->setCheckable(true);

        horizontalLayout->addWidget(f_list_bullet);

        f_list_ordered = new QToolButton(f_toolbar);
        f_list_ordered->setObjectName(QString::fromUtf8("f_list_ordered"));
        f_list_ordered->setFocusPolicy(Qt::ClickFocus);
        f_list_ordered->setStyleSheet(QString::fromUtf8(""));
        f_list_ordered->setIconSize(QSize(16, 16));
        f_list_ordered->setCheckable(true);

        horizontalLayout->addWidget(f_list_ordered);

        f_indent_dec = new QToolButton(f_toolbar);
        f_indent_dec->setObjectName(QString::fromUtf8("f_indent_dec"));
        f_indent_dec->setFocusPolicy(Qt::ClickFocus);
        f_indent_dec->setStyleSheet(QString::fromUtf8(""));
        QIcon icon9;
        iconThemeName = QString::fromUtf8("format-indent-less");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon9 = QIcon::fromTheme(iconThemeName);
        } else {
            icon9.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_indent_dec->setIcon(icon9);
        f_indent_dec->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_indent_dec);

        f_indent_inc = new QToolButton(f_toolbar);
        f_indent_inc->setObjectName(QString::fromUtf8("f_indent_inc"));
        f_indent_inc->setFocusPolicy(Qt::ClickFocus);
        f_indent_inc->setStyleSheet(QString::fromUtf8(""));
        QIcon icon10;
        iconThemeName = QString::fromUtf8("format-indent-more");
        if (QIcon::hasThemeIcon(iconThemeName)) {
            icon10 = QIcon::fromTheme(iconThemeName);
        } else {
            icon10.addFile(QString::fromUtf8(""), QSize(), QIcon::Normal, QIcon::Off);
        }
        f_indent_inc->setIcon(icon10);
        f_indent_inc->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_indent_inc);

        line_2 = new QFrame(f_toolbar);
        line_2->setObjectName(QString::fromUtf8("line_2"));
        line_2->setFrameShape(QFrame::VLine);
        line_2->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_2);

        f_bgcolor = new QToolButton(f_toolbar);
        f_bgcolor->setObjectName(QString::fromUtf8("f_bgcolor"));
        f_bgcolor->setMinimumSize(QSize(16, 16));
        f_bgcolor->setMaximumSize(QSize(16, 16));
        f_bgcolor->setFocusPolicy(Qt::ClickFocus);
        f_bgcolor->setStyleSheet(QString::fromUtf8(""));
        f_bgcolor->setIconSize(QSize(16, 16));

        horizontalLayout->addWidget(f_bgcolor);

        f_fontsize = new QComboBox(f_toolbar);
        f_fontsize->setObjectName(QString::fromUtf8("f_fontsize"));
        f_fontsize->setMinimumSize(QSize(40, 0));
        f_fontsize->setFocusPolicy(Qt::ClickFocus);
        f_fontsize->setStyleSheet(QString::fromUtf8("QComboBox {\n"
"    border: 1px solid #f26522;\n"
"	background-color: #1c1c1c;\n"
"}\n"
"\n"
"QComboBox::drop-down {\n"
"    subcontrol-origin: padding;\n"
"    subcontrol-position: top right;\n"
"    /*width: 15px;*/\n"
"    border-left-width: 1px;\n"
"    border-left-color: #f26522;\n"
"    border-left-style: solid;\n"
"}\n"
"\n"
"QComboBox::down-arrow {\n"
"    image: url(:/icons/down_arrow);\n"
"}\n"
"\n"
"QComboBox QAbstractItemView\n"
"{\n"
"background-color: #1c1c1c;\n"
"selection-background-color:#61280E;\n"
"selection-color: #f26522; \n"
"border: 1px solid #f26522;\n"
"color:#f26522;\n"
"}\n"
""));
        f_fontsize->setEditable(true);

        horizontalLayout->addWidget(f_fontsize);

        line_6 = new QFrame(f_toolbar);
        line_6->setObjectName(QString::fromUtf8("line_6"));
        line_6->setFrameShape(QFrame::VLine);
        line_6->setFrameShadow(QFrame::Sunken);

        horizontalLayout->addWidget(line_6);

        horizontalSpacer = new QSpacerItem(40, 20, QSizePolicy::Expanding, QSizePolicy::Minimum);

        horizontalLayout->addItem(horizontalSpacer);

        f_paragraph->raise();
        line_4->raise();
        f_undo->raise();
        f_redo->raise();
        f_cut->raise();
        f_copy->raise();
        f_paste->raise();
        line->raise();
        f_link->raise();
        line_3->raise();
        f_italic->raise();
        f_underline->raise();
        line_2->raise();
        f_fontsize->raise();
        line_5->raise();
        f_list_bullet->raise();
        f_list_ordered->raise();
        f_indent_dec->raise();
        f_indent_inc->raise();
        f_bold->raise();
        f_bgcolor->raise();
        f_strikeout->raise();
        line_6->raise();

        verticalLayout->addWidget(f_toolbar);

        f_textedit = new QTextEdit(MRichTextEdit);
        f_textedit->setObjectName(QString::fromUtf8("f_textedit"));
        f_textedit->setStyleSheet(QString::fromUtf8("border: 1px solid #f26522;"));
        f_textedit->setAutoFormatting(QTextEdit::AutoNone);
        f_textedit->setTabChangesFocus(true);

        verticalLayout->addWidget(f_textedit);


        retranslateUi(MRichTextEdit);

        QMetaObject::connectSlotsByName(MRichTextEdit);
    } // setupUi

    void retranslateUi(QWidget *MRichTextEdit)
    {
#if QT_CONFIG(tooltip)
        f_paragraph->setToolTip(QCoreApplication::translate("MRichTextEdit", "Paragraph formatting", nullptr));
#endif // QT_CONFIG(tooltip)
#if QT_CONFIG(tooltip)
        f_undo->setToolTip(QCoreApplication::translate("MRichTextEdit", "Undo (CTRL+Z)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_undo->setText(QCoreApplication::translate("MRichTextEdit", "Undo", nullptr));
#if QT_CONFIG(tooltip)
        f_redo->setToolTip(QCoreApplication::translate("MRichTextEdit", "Redo", nullptr));
#endif // QT_CONFIG(tooltip)
        f_redo->setText(QCoreApplication::translate("MRichTextEdit", "Redo", nullptr));
#if QT_CONFIG(tooltip)
        f_cut->setToolTip(QCoreApplication::translate("MRichTextEdit", "Cut (CTRL+X)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_cut->setText(QCoreApplication::translate("MRichTextEdit", "Cut", nullptr));
#if QT_CONFIG(tooltip)
        f_copy->setToolTip(QCoreApplication::translate("MRichTextEdit", "Copy (CTRL+C)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_copy->setText(QCoreApplication::translate("MRichTextEdit", "Copy", nullptr));
#if QT_CONFIG(tooltip)
        f_paste->setToolTip(QCoreApplication::translate("MRichTextEdit", "Paste (CTRL+V)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_paste->setText(QCoreApplication::translate("MRichTextEdit", "Paste", nullptr));
#if QT_CONFIG(tooltip)
        f_link->setToolTip(QCoreApplication::translate("MRichTextEdit", "Link (CTRL+L)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_link->setText(QCoreApplication::translate("MRichTextEdit", "Link", nullptr));
        f_bold->setText(QCoreApplication::translate("MRichTextEdit", "Bold", nullptr));
#if QT_CONFIG(tooltip)
        f_italic->setToolTip(QCoreApplication::translate("MRichTextEdit", "Italic (CTRL+I)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_italic->setText(QCoreApplication::translate("MRichTextEdit", "Italic", nullptr));
#if QT_CONFIG(tooltip)
        f_underline->setToolTip(QCoreApplication::translate("MRichTextEdit", "Underline (CTRL+U)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_underline->setText(QCoreApplication::translate("MRichTextEdit", "Underline", nullptr));
        f_strikeout->setText(QCoreApplication::translate("MRichTextEdit", "Strike Out", nullptr));
#if QT_CONFIG(tooltip)
        f_list_bullet->setToolTip(QCoreApplication::translate("MRichTextEdit", "Bullet list (CTRL+-)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_list_bullet->setText(QCoreApplication::translate("MRichTextEdit", "Bullet list", nullptr));
#if QT_CONFIG(tooltip)
        f_list_ordered->setToolTip(QCoreApplication::translate("MRichTextEdit", "Ordered list (CTRL+=)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_list_ordered->setText(QCoreApplication::translate("MRichTextEdit", "Ordered list", nullptr));
#if QT_CONFIG(tooltip)
        f_indent_dec->setToolTip(QCoreApplication::translate("MRichTextEdit", "Decrease indentation (CTRL+,)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_indent_dec->setText(QCoreApplication::translate("MRichTextEdit", "Decrease indentation", nullptr));
#if QT_CONFIG(tooltip)
        f_indent_inc->setToolTip(QCoreApplication::translate("MRichTextEdit", "Increase indentation (CTRL+.)", nullptr));
#endif // QT_CONFIG(tooltip)
        f_indent_inc->setText(QCoreApplication::translate("MRichTextEdit", "Increase indentation", nullptr));
#if QT_CONFIG(tooltip)
        f_bgcolor->setToolTip(QCoreApplication::translate("MRichTextEdit", "Text background color", nullptr));
#endif // QT_CONFIG(tooltip)
        f_bgcolor->setText(QCoreApplication::translate("MRichTextEdit", ".", nullptr));
#if QT_CONFIG(tooltip)
        f_fontsize->setToolTip(QCoreApplication::translate("MRichTextEdit", "Font size", nullptr));
#endif // QT_CONFIG(tooltip)
        (void)MRichTextEdit;
    } // retranslateUi

};

namespace Ui {
    class MRichTextEdit: public Ui_MRichTextEdit {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MRICHTEXTEDIT_H
