// Copyright (c) 2009-2012 The Bitcoin developers
// Copyright (c) 2026 The Triangles developers
// Distributed under the MIT/X11 software license, see the accompanying
// file COPYING or http://www.opensource.org/licenses/mit-license.php.
//
// Single-instance "triangles:" URI handoff. When the wallet is launched with a
// URI argument and an instance is already running, the URI is relayed to the
// running instance over a local socket; otherwise this instance becomes the
// listener. Reworked from Boost.Interprocess message queues onto Qt's
// QLocalServer/QLocalSocket (QtNetwork) — no Boost dependency.

#include "qtipcserver.h"
#include "guiconstants.h"
#include "ui_interface.h"
#include "util.h"

#include <algorithm>
#include <cctype>
#include <string>

#include <QByteArray>
#include <QLocalServer>
#include <QLocalSocket>
#include <QString>

#if defined MAC_OSX || defined __FreeBSD__
// URI handling not implemented on OSX yet

void ipcScanRelay(int argc, char *argv[]) { }
void ipcInit(int argc, char *argv[]) { }

#else

// Local-socket server name. QLocalServer maps this to a named pipe on Windows
// and a filesystem socket on Unix.
static const QString IPC_SERVER_NAME = QStringLiteral(TRIANGLESURI_QUEUE_NAME);

static void ipcThread2(void* pArg);

static bool IsTrianglesURI(const char* arg)
{
    // Case-insensitive match of the "Triangles:" scheme prefix.
    return std::equal(std::begin("Triangles:"), std::end("Triangles:") - 1, arg,
                      [](char a, char b) {
                          return std::tolower(static_cast<unsigned char>(a)) ==
                                 std::tolower(static_cast<unsigned char>(b));
                      });
}

static bool ipcScanCmd(int argc, char *argv[], bool fRelay)
{
    // Check for URI in argv and relay it to a running instance, if any.
    bool fSent = false;
    for (int i = 1; i < argc; i++)
    {
        if (!IsTrianglesURI(argv[i]))
            continue;

        const char *strURI = argv[i];
        QLocalSocket socket;
        socket.connectToServer(IPC_SERVER_NAME);
        if (socket.waitForConnected(1000))
        {
            socket.write(strURI, static_cast<qint64>(strlen(strURI)));
            socket.flush();
            socket.waitForBytesWritten(1000);
            socket.disconnectFromServer();
            fSent = true;
        }
        else if (fRelay)
        {
            // No running instance accepted the URI; this process should become
            // the listener instead of relaying.
            break;
        }
    }
    return fSent;
}

void ipcScanRelay(int argc, char *argv[])
{
    if (ipcScanCmd(argc, argv, true))
        exit(0);
}

static void ipcThread(void* pArg)
{
    // Make this thread recognisable as the GUI-IPC thread
    RenameThread("Triangles-gui-ipc");

    try
    {
        ipcThread2(pArg);
    }
    catch (std::exception& e) {
        PrintExceptionContinue(&e, "ipcThread()");
    } catch (...) {
        PrintExceptionContinue(NULL, "ipcThread()");
    }
    printf("ipcThread exited\n");
}

static void ipcThread2(void* pArg)
{
    printf("ipcThread started\n");

    QLocalServer* server = static_cast<QLocalServer*>(pArg);

    // Poll for inbound connections without requiring a Qt event loop:
    // waitForNewConnection(timeout) pumps the socket internally.
    while (true)
    {
        if (server->waitForNewConnection(100))
        {
            QLocalSocket* client = server->nextPendingConnection();
            if (client)
            {
                if (client->waitForReadyRead(1000))
                {
                    QByteArray data = client->readAll();
                    if (data.size() > MAX_URI_LENGTH)
                        data.truncate(MAX_URI_LENGTH);
                    uiInterface.ThreadSafeHandleURI(std::string(data.constData(), data.size()));
                    MilliSleep(1000);
                }
                client->disconnectFromServer();
                delete client;
            }
        }

        if (fShutdown)
            break;
    }

    server->close();
    delete server;
}

void ipcInit(int argc, char *argv[])
{
    // Clear any stale socket/pipe left by a previous crashed instance, then
    // listen. If listen() fails, another instance already owns the name — in
    // that case relay our own URI args (below) and don't start a server.
    QLocalServer::removeServer(IPC_SERVER_NAME);

    QLocalServer* server = new QLocalServer();
    server->setSocketOptions(QLocalServer::UserAccessOption);  // owner-only access
    if (!server->listen(IPC_SERVER_NAME))
    {
        printf("ipcInit() - QLocalServer listen failed: %s\n",
               server->errorString().toUtf8().constData());
        delete server;
        // Still try to relay any URI passed on our command line to whoever is
        // listening.
        ipcScanCmd(argc, argv, false);
        return;
    }

    if (!NewThread(ipcThread, server))
    {
        server->close();
        delete server;
        return;
    }

    // Handle a URI passed on our own command line (relayed to the server we
    // just started).
    ipcScanCmd(argc, argv, false);
}

#endif
