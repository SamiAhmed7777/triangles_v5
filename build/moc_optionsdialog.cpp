/****************************************************************************
** Meta object code from reading C++ file 'optionsdialog.h'
**
** Created by: The Qt Meta Object Compiler version 67 (Qt 5.15.18)
**
** WARNING! All changes made in this file will be lost!
*****************************************************************************/

#include <memory>
#include "../src/qt/optionsdialog.h"
#include <QtCore/qbytearray.h>
#include <QtCore/qmetatype.h>
#if !defined(Q_MOC_OUTPUT_REVISION)
#error "The header file 'optionsdialog.h' doesn't include <QObject>."
#elif Q_MOC_OUTPUT_REVISION != 67
#error "This file was generated using the moc from 5.15.18. It"
#error "cannot be used with the include files from this version of Qt."
#error "(The moc has changed too much.)"
#endif

QT_BEGIN_MOC_NAMESPACE
QT_WARNING_PUSH
QT_WARNING_DISABLE_DEPRECATED
struct qt_meta_stringdata_OptionsDialog_t {
    QByteArrayData data[21];
    char stringdata0[340];
};
#define QT_MOC_LITERAL(idx, ofs, len) \
    Q_STATIC_BYTE_ARRAY_DATA_HEADER_INITIALIZER_WITH_OFFSET(len, \
    qptrdiff(offsetof(qt_meta_stringdata_OptionsDialog_t, stringdata0) + ofs \
        - idx * sizeof(QByteArrayData)) \
    )
static const qt_meta_stringdata_OptionsDialog_t qt_meta_stringdata_OptionsDialog = {
    {
QT_MOC_LITERAL(0, 0, 13), // "OptionsDialog"
QT_MOC_LITERAL(1, 14, 12), // "proxyIpValid"
QT_MOC_LITERAL(2, 27, 0), // ""
QT_MOC_LITERAL(3, 28, 19), // "QValidatedLineEdit*"
QT_MOC_LITERAL(4, 48, 6), // "object"
QT_MOC_LITERAL(5, 55, 6), // "fValid"
QT_MOC_LITERAL(6, 62, 17), // "enableApplyButton"
QT_MOC_LITERAL(7, 80, 18), // "disableApplyButton"
QT_MOC_LITERAL(8, 99, 17), // "enableSaveButtons"
QT_MOC_LITERAL(9, 117, 18), // "disableSaveButtons"
QT_MOC_LITERAL(10, 136, 18), // "setSaveButtonState"
QT_MOC_LITERAL(11, 155, 6), // "fState"
QT_MOC_LITERAL(12, 162, 19), // "on_okButton_clicked"
QT_MOC_LITERAL(13, 182, 23), // "on_cancelButton_clicked"
QT_MOC_LITERAL(14, 206, 22), // "on_applyButton_clicked"
QT_MOC_LITERAL(15, 229, 24), // "showRestartWarning_Proxy"
QT_MOC_LITERAL(16, 254, 23), // "showRestartWarning_Lang"
QT_MOC_LITERAL(17, 278, 17), // "updateDisplayUnit"
QT_MOC_LITERAL(18, 296, 18), // "handleProxyIpValid"
QT_MOC_LITERAL(19, 315, 16), // "applyTorDefaults"
QT_MOC_LITERAL(20, 332, 7) // "enabled"

    },
    "OptionsDialog\0proxyIpValid\0\0"
    "QValidatedLineEdit*\0object\0fValid\0"
    "enableApplyButton\0disableApplyButton\0"
    "enableSaveButtons\0disableSaveButtons\0"
    "setSaveButtonState\0fState\0on_okButton_clicked\0"
    "on_cancelButton_clicked\0on_applyButton_clicked\0"
    "showRestartWarning_Proxy\0"
    "showRestartWarning_Lang\0updateDisplayUnit\0"
    "handleProxyIpValid\0applyTorDefaults\0"
    "enabled"
};
#undef QT_MOC_LITERAL

static const uint qt_meta_data_OptionsDialog[] = {

 // content:
       8,       // revision
       0,       // classname
       0,    0, // classinfo
      14,   14, // methods
       0,    0, // properties
       0,    0, // enums/sets
       0,    0, // constructors
       0,       // flags
       1,       // signalCount

 // signals: name, argc, parameters, tag, flags
       1,    2,   84,    2, 0x06 /* Public */,

 // slots: name, argc, parameters, tag, flags
       6,    0,   89,    2, 0x08 /* Private */,
       7,    0,   90,    2, 0x08 /* Private */,
       8,    0,   91,    2, 0x08 /* Private */,
       9,    0,   92,    2, 0x08 /* Private */,
      10,    1,   93,    2, 0x08 /* Private */,
      12,    0,   96,    2, 0x08 /* Private */,
      13,    0,   97,    2, 0x08 /* Private */,
      14,    0,   98,    2, 0x08 /* Private */,
      15,    0,   99,    2, 0x08 /* Private */,
      16,    0,  100,    2, 0x08 /* Private */,
      17,    0,  101,    2, 0x08 /* Private */,
      18,    2,  102,    2, 0x08 /* Private */,
      19,    1,  107,    2, 0x08 /* Private */,

 // signals: parameters
    QMetaType::Void, 0x80000000 | 3, QMetaType::Bool,    4,    5,

 // slots: parameters
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, QMetaType::Bool,   11,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void,
    QMetaType::Void, 0x80000000 | 3, QMetaType::Bool,    4,   11,
    QMetaType::Void, QMetaType::Bool,   20,

       0        // eod
};

void OptionsDialog::qt_static_metacall(QObject *_o, QMetaObject::Call _c, int _id, void **_a)
{
    if (_c == QMetaObject::InvokeMetaMethod) {
        auto *_t = static_cast<OptionsDialog *>(_o);
        (void)_t;
        switch (_id) {
        case 0: _t->proxyIpValid((*reinterpret_cast< QValidatedLineEdit*(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 1: _t->enableApplyButton(); break;
        case 2: _t->disableApplyButton(); break;
        case 3: _t->enableSaveButtons(); break;
        case 4: _t->disableSaveButtons(); break;
        case 5: _t->setSaveButtonState((*reinterpret_cast< bool(*)>(_a[1]))); break;
        case 6: _t->on_okButton_clicked(); break;
        case 7: _t->on_cancelButton_clicked(); break;
        case 8: _t->on_applyButton_clicked(); break;
        case 9: _t->showRestartWarning_Proxy(); break;
        case 10: _t->showRestartWarning_Lang(); break;
        case 11: _t->updateDisplayUnit(); break;
        case 12: _t->handleProxyIpValid((*reinterpret_cast< QValidatedLineEdit*(*)>(_a[1])),(*reinterpret_cast< bool(*)>(_a[2]))); break;
        case 13: _t->applyTorDefaults((*reinterpret_cast< bool(*)>(_a[1]))); break;
        default: ;
        }
    } else if (_c == QMetaObject::IndexOfMethod) {
        int *result = reinterpret_cast<int *>(_a[0]);
        {
            using _t = void (OptionsDialog::*)(QValidatedLineEdit * , bool );
            if (*reinterpret_cast<_t *>(_a[1]) == static_cast<_t>(&OptionsDialog::proxyIpValid)) {
                *result = 0;
                return;
            }
        }
    }
}

QT_INIT_METAOBJECT const QMetaObject OptionsDialog::staticMetaObject = { {
    QMetaObject::SuperData::link<QDialog::staticMetaObject>(),
    qt_meta_stringdata_OptionsDialog.data,
    qt_meta_data_OptionsDialog,
    qt_static_metacall,
    nullptr,
    nullptr
} };


const QMetaObject *OptionsDialog::metaObject() const
{
    return QObject::d_ptr->metaObject ? QObject::d_ptr->dynamicMetaObject() : &staticMetaObject;
}

void *OptionsDialog::qt_metacast(const char *_clname)
{
    if (!_clname) return nullptr;
    if (!strcmp(_clname, qt_meta_stringdata_OptionsDialog.stringdata0))
        return static_cast<void*>(this);
    return QDialog::qt_metacast(_clname);
}

int OptionsDialog::qt_metacall(QMetaObject::Call _c, int _id, void **_a)
{
    _id = QDialog::qt_metacall(_c, _id, _a);
    if (_id < 0)
        return _id;
    if (_c == QMetaObject::InvokeMetaMethod) {
        if (_id < 14)
            qt_static_metacall(this, _c, _id, _a);
        _id -= 14;
    } else if (_c == QMetaObject::RegisterMethodArgumentMetaType) {
        if (_id < 14)
            *reinterpret_cast<int*>(_a[0]) = -1;
        _id -= 14;
    }
    return _id;
}

// SIGNAL 0
void OptionsDialog::proxyIpValid(QValidatedLineEdit * _t1, bool _t2)
{
    void *_a[] = { nullptr, const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t1))), const_cast<void*>(reinterpret_cast<const void*>(std::addressof(_t2))) };
    QMetaObject::activate(this, &staticMetaObject, 0, _a);
}
QT_WARNING_POP
QT_END_MOC_NAMESPACE
