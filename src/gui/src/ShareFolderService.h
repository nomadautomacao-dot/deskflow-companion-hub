/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <QHash>
#include <QObject>
#include <QSet>
#include <QStringList>

class QFile;
class QFileSystemWatcher;
class QTcpServer;
class QTcpSocket;
class QTimer;

// Side-channel that copies files dropped into a local folder to the
// other InputLeap machine. Uses TCP on InputLeap port + 1 (default 24801).
class ShareFolderService : public QObject
{
    Q_OBJECT

public:
    enum class Role {
        Idle,
        Server,
        Client
    };

    explicit ShareFolderService(QObject* parent = nullptr);
    ~ShareFolderService() override;

    static QString defaultFolder();

    void setFolder(const QString& path);
    QString folder() const { return folder_; }

    void startServer(quint16 port);
    void startClient(const QString& host, quint16 port);
    void stop();

    bool isLinked() const;
    int peerCount() const { return peers_.size(); }
    Role role() const { return role_; }

    bool sendExistingFile(const QString& sourcePath);

signals:
    void statusChanged(const QString& text);
    void fileSent(const QString& name);
    void fileReceived(const QString& name);
    void transferProgress(const QString& name, qint64 done, qint64 total);
    void errorOccurred(const QString& text);

private:
    enum class RxState {
        WaitHeader,
        WaitHello,
        WaitFileMeta,
        WaitFileData,
        WaitAck
    };

    struct Snapshot {
        qint64 size = -1;
        qint64 mtimeMs = 0;
    };

    struct Peer {
        QTcpSocket* socket = nullptr;
        QByteArray rx;
        RxState rxState = RxState::WaitHeader;
        quint8 pendingType = 0;
        QString incomingName;
        qint64 incomingSize = 0;
        qint64 incomingGot = 0;
        QFile* incomingFile = nullptr;
        QFile* outgoingFile = nullptr;
        QString outgoingName;
        qint64 outgoingSize = 0;
        qint64 outgoingSent = 0;
        QStringList sendQueue;
        bool sending = false;
    };

    static const quint8 kVersion = 1;
    static const quint8 kTypeHello = 1;
    static const quint8 kTypeFile = 2;
    static const quint8 kTypeAck = 3;
    static const qint64 kMaxFileSize = 2LL * 1024 * 1024 * 1024;
    static const int kChunkSize = 64 * 1024;
    static const int kHeaderSize = 8;

    void ensureFolder();
    void startWatching();
    void stopWatching();
    void attachPeer(QTcpSocket* socket);
    void detachPeer(QTcpSocket* socket);
    Peer* peerFor(QTcpSocket* socket);
    void sendHello(Peer* peer);
    void enqueueLocalFile(const QString& path, Peer* except);
    void startNextSend(Peer* peer);
    void writeChunk(Peer* peer);
    void processRx(Peer* peer);
    bool beginIncomingFile(Peer* peer, const QString& name, qint64 size);
    void finishIncomingFile(Peer* peer);
    void abortIncoming(Peer* peer);
    void abortOutgoing(Peer* peer);
    QString sanitizeName(const QString& name) const;
    bool isTempName(const QString& name) const;
    bool matchesSnapshot(const QString& name, const QString& path) const;
    void rememberSnapshot(const QString& name, const QString& path);
    bool isQueuedOrSending(const QString& path) const;
    void emitStatus();

    void onNewConnection();
    void onScanTimer();
    void onReconnectTimer();

    QString folder_;
    Role role_ = Role::Idle;
    quint16 port_ = 24801;
    QString clientHost_;
    QTcpServer* server_ = nullptr;
    QList<Peer*> peers_;
    QFileSystemWatcher* watcher_ = nullptr;
    QTimer* scanTimer_ = nullptr;
    QTimer* reconnectTimer_ = nullptr;
    QHash<QString, Snapshot> snapshots_;
    QHash<QString, qint64> lastSeenSize_;
    QHash<QString, int> stableTicks_;
};
