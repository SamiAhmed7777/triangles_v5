#ifndef RPCCONSOLE_H
#define RPCCONSOLE_H

#include <QDialog>

namespace Ui {
    class RPCConsole;
}
class ClientModel;

/** Local Triangles RPC console. */
class RPCConsole: public QDialog
{
    Q_OBJECT

public:
    explicit RPCConsole(QWidget *parent = 0);
    ~RPCConsole();

    void setClientModel(ClientModel *model);

    enum MessageClass {
        MC_ERROR,
        MC_DEBUG,
        CMD_REQUEST,
        CMD_REPLY,
        CMD_ERROR
    };

protected:
    virtual bool eventFilter(QObject* obj, QEvent *event);

private slots:
    void on_lineEdit_returnPressed();
    void on_tabWidget_currentChanged(int index);
    /** open the debug.log from the current datadir */
    void on_openDebugLogfileButton_clicked();
    /** display messagebox with program parameters (same as triangles-qt --help) */
    void on_showCLOptionsButton_clicked();
    void on_showRequestsCheckBox_toggled(bool checked);
    void on_showRepliesCheckBox_toggled(bool checked);
    void on_showErrorsCheckBox_toggled(bool checked);
    void showClientError(const QString &title, const QString &message, bool modal);

public slots:
    void clear();
    void message(int category, const QString &message, bool html = false);
    /** Set number of connections shown in the UI */
    void setNumConnections(int count);
    /** Set number of blocks shown in the UI */
    void setNumBlocks(int count, int countOfPeers);
    /** Go forward or back in history */
    void browseHistory(int offset);
    /** Scroll console view to end */
    void scrollToEnd();
signals:
    // For RPC command executor
    void stopExecutor();
    void cmdRequest(const QString &command);

private:
    struct ConsoleEntry
    {
        int category;
        QString text;
        QString time;
        bool html;
    };

    Ui::RPCConsole *ui;
    ClientModel *clientModel;
    QStringList history;
    QList<ConsoleEntry> consoleEntries;
    int historyPtr;

    bool categoryVisible(int category) const;
    QString formatEntry(const ConsoleEntry &entry) const;
    void refreshMessages();
    void startExecutor();
};

#endif // RPCCONSOLE_H
