/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "ShareFolderService.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QFileSystemWatcher>
#include <QHostAddress>
#include <QStandardPaths>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTimer>
#include <QtEndian>

#include <cstring>

namespace {

const char kMagic[4] = { 'I', 'L', 'S', 'F' };

void appendU8(QByteArray& out, quint8 value)
{
    out.append(static_cast<char>(value));
}

void appendU16(QByteArray& out, quint16 value)
{
    const quint16 le = qToLittleEndian(value);
    out.append(reinterpret_cast<const char*>(&le), 2);
}

void appendU64(QByteArray& out, quint64 value)
{
    const quint64 le = qToLittleEndian(value);
    out.append(reinterpret_cast<const char*>(&le), 8);
}

bool peekU16(const QByteArray& buf, int offset, quint16* out)
{
    if (buf.size() < offset + 2) {
        return false;
    }
    quint16 le = 0;
    memcpy(&le, buf.constData() + offset, 2);
    *out = qFromLittleEndian(le);
    return true;
}

quint8 readU8(QByteArray& buf)
{
    const auto value = static_cast<quint8>(buf.at(0));
    buf.remove(0, 1);
    return value;
}

quint16 readU16(QByteArray& buf)
{
    quint16 le = 0;
    memcpy(&le, buf.constData(), 2);
    buf.remove(0, 2);
    return qFromLittleEndian(le);
}

quint64 readU64(QByteArray& buf)
{
    quint64 le = 0;
    memcpy(&le, buf.constData(), 8);
    buf.remove(0, 8);
    return qFromLittleEndian(le);
}

QByteArray makeHeader(quint8 type)
{
    QByteArray header;
    header.append(kMagic, 4);
    appendU8(header, 1);
    appendU8(header, type);
    appendU16(header, 0);
    return header;
}

} // namespace

ShareFolderService::ShareFolderService(QObject* parent) :
    QObject(parent)
{
    scanTimer_ = new QTimer(this);
    scanTimer_->setInterval(800);
    connect(scanTimer_, &QTimer::timeout, this, &ShareFolderService::onScanTimer);

    reconnectTimer_ = new QTimer(this);
    reconnectTimer_->setInterval(3000);
    connect(reconnectTimer_, &QTimer::timeout, this, &ShareFolderService::onReconnectTimer);
}

ShareFolderService::~ShareFolderService()
{
    stop();
}

QString ShareFolderService::defaultFolder()
{
    const QString documents = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    return QDir(documents).filePath(QStringLiteral("InputLeap Share"));
}

void ShareFolderService::setFolder(const QString& path)
{
    const QString cleaned = QDir::cleanPath(path);
    if (cleaned == folder_) {
        ensureFolder();
        return;
    }

    folder_ = cleaned;
    ensureFolder();
    snapshots_.clear();
    lastSeenSize_.clear();
    stableTicks_.clear();
    startWatching();
}

void ShareFolderService::ensureFolder()
{
    if (folder_.isEmpty()) {
        folder_ = defaultFolder();
    }
    QDir dir;
    if (!dir.exists(folder_)) {
        dir.mkpath(folder_);
    }
}

void ShareFolderService::startWatching()
{
    stopWatching();
    ensureFolder();

    watcher_ = new QFileSystemWatcher(this);
    watcher_->addPath(folder_);
    connect(watcher_, &QFileSystemWatcher::directoryChanged,
            this, [this](const QString&) { onScanTimer(); });
}

void ShareFolderService::stopWatching()
{
    delete watcher_;
    watcher_ = nullptr;
}

void ShareFolderService::startServer(quint16 port)
{
    stop();
    ensureFolder();
    startWatching();

    role_ = Role::Server;
    port_ = port;

    server_ = new QTcpServer(this);
    connect(server_, &QTcpServer::newConnection, this, &ShareFolderService::onNewConnection);

    if (!server_->listen(QHostAddress::Any, port_)) {
        const QString err = tr("Could not listen for shared-folder transfers on port %1: %2")
                                .arg(port_)
                                .arg(server_->errorString());
        emit errorOccurred(err);
        emit statusChanged(err);
        return;
    }

    scanTimer_->start();
    emitStatus();
}

void ShareFolderService::startClient(const QString& host, quint16 port)
{
    stop();
    ensureFolder();
    startWatching();

    role_ = Role::Client;
    port_ = port;
    clientHost_ = host.trimmed();

    if (clientHost_.startsWith(QLatin1Char('['))) {
        const int end = clientHost_.indexOf(QLatin1Char(']'));
        if (end > 0) {
            clientHost_ = clientHost_.mid(1, end - 1);
        }
    }

    scanTimer_->start();
    reconnectTimer_->start();
    onReconnectTimer();
    emitStatus();
}

void ShareFolderService::stop()
{
    scanTimer_->stop();
    reconnectTimer_->stop();
    stopWatching();

    const auto sockets = peers_;
    for (Peer* peer : sockets) {
        if (peer->socket) {
            peer->socket->disconnect(this);
            peer->socket->abort();
        }
        detachPeer(peer->socket);
    }
    peers_.clear();

    delete server_;
    server_ = nullptr;

    role_ = Role::Idle;
    emitStatus();
}

bool ShareFolderService::isLinked() const
{
    for (const Peer* peer : peers_) {
        if (peer->socket && peer->socket->state() == QAbstractSocket::ConnectedState) {
            return true;
        }
    }
    return false;
}

bool ShareFolderService::sendExistingFile(const QString& sourcePath)
{
    ensureFolder();
    QFileInfo src(sourcePath);
    if (!src.exists() || !src.isFile()) {
        emit errorOccurred(tr("That is not a file that can be sent."));
        return false;
    }

    const QString name = sanitizeName(src.fileName());
    if (name.isEmpty()) {
        emit errorOccurred(tr("The file name is not allowed."));
        return false;
    }

    const QString dest = QDir(folder_).filePath(name);
    if (QFileInfo(sourcePath).canonicalFilePath() != QFileInfo(dest).canonicalFilePath()) {
        QFile::remove(dest);
        if (!QFile::copy(sourcePath, dest)) {
            emit errorOccurred(tr("Could not copy %1 into the shared folder.").arg(src.fileName()));
            return false;
        }
    }

    lastSeenSize_.remove(name);
    stableTicks_.remove(name);
    snapshots_.remove(name);
    enqueueLocalFile(dest, nullptr);
    return true;
}

void ShareFolderService::onNewConnection()
{
    while (server_ && server_->hasPendingConnections()) {
        attachPeer(server_->nextPendingConnection());
    }
    emitStatus();
}

void ShareFolderService::onReconnectTimer()
{
    if (role_ != Role::Client || clientHost_.isEmpty()) {
        return;
    }
    for (const Peer* peer : peers_) {
        if (!peer->socket) {
            continue;
        }
        const auto state = peer->socket->state();
        if (state == QAbstractSocket::ConnectedState ||
            state == QAbstractSocket::ConnectingState ||
            state == QAbstractSocket::HostLookupState) {
            return;
        }
    }

    const auto existing = peers_;
    for (Peer* peer : existing) {
        detachPeer(peer->socket);
    }

    auto* socket = new QTcpSocket(this);
    attachPeer(socket);
    socket->connectToHost(clientHost_, port_);
    emitStatus();
}

void ShareFolderService::attachPeer(QTcpSocket* socket)
{
    auto* peer = new Peer;
    peer->socket = socket;
    peers_.append(peer);

    connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
        if (Peer* p = peerFor(socket)) {
            processRx(p);
        }
    });
    connect(socket, &QTcpSocket::bytesWritten, this, [this, socket](qint64) {
        if (Peer* p = peerFor(socket)) {
            writeChunk(p);
        }
    });
    connect(socket, &QTcpSocket::connected, this, [this, socket]() {
        if (Peer* p = peerFor(socket)) {
            sendHello(p);
            onScanTimer();
        }
        emitStatus();
    });
    connect(socket, &QTcpSocket::disconnected, this, [this, socket]() {
        detachPeer(socket);
        emitStatus();
    });
    connect(socket, &QAbstractSocket::errorOccurred, this, [this, socket](QAbstractSocket::SocketError) {
        detachPeer(socket);
        emitStatus();
    });

    if (socket->state() == QAbstractSocket::ConnectedState) {
        sendHello(peer);
    }
}

void ShareFolderService::detachPeer(QTcpSocket* socket)
{
    for (int i = 0; i < peers_.size(); ++i) {
        Peer* peer = peers_.at(i);
        if (peer->socket != socket) {
            continue;
        }
        abortIncoming(peer);
        abortOutgoing(peer);
        if (peer->socket) {
            peer->socket->disconnect(this);
            peer->socket->deleteLater();
        }
        delete peer;
        peers_.removeAt(i);
        return;
    }
}

ShareFolderService::Peer* ShareFolderService::peerFor(QTcpSocket* socket)
{
    for (Peer* peer : peers_) {
        if (peer->socket == socket) {
            return peer;
        }
    }
    return nullptr;
}

void ShareFolderService::sendHello(Peer* peer)
{
    if (!peer || !peer->socket) {
        return;
    }
    const QByteArray name = QSysInfo::machineHostName().toUtf8();
    QByteArray packet = makeHeader(kTypeHello);
    appendU16(packet, static_cast<quint16>(qMin(name.size(), 1024)));
    packet.append(name.left(1024));
    peer->socket->write(packet);
}

void ShareFolderService::enqueueLocalFile(const QString& path, Peer* except)
{
    if (!QFileInfo::exists(path)) {
        return;
    }
    bool any = false;
    for (Peer* peer : peers_) {
        if (peer == except || !peer->socket ||
            peer->socket->state() != QAbstractSocket::ConnectedState) {
            continue;
        }
        if (!peer->sendQueue.contains(path)) {
            peer->sendQueue.append(path);
        }
        startNextSend(peer);
        any = true;
    }
    if (!any) {
        emitStatus();
    }
}

void ShareFolderService::startNextSend(Peer* peer)
{
    if (!peer || peer->sending || peer->sendQueue.isEmpty()) {
        return;
    }
    if (!peer->socket || peer->socket->state() != QAbstractSocket::ConnectedState) {
        return;
    }

    const QString path = peer->sendQueue.takeFirst();
    QFileInfo info(path);
    const QString name = sanitizeName(info.fileName());
    if (name.isEmpty() || !info.isFile() || info.size() <= 0 || info.size() > kMaxFileSize) {
        startNextSend(peer);
        return;
    }

    auto* file = new QFile(path);
    if (!file->open(QIODevice::ReadOnly)) {
        delete file;
        startNextSend(peer);
        return;
    }

    peer->outgoingFile = file;
    peer->outgoingName = name;
    peer->outgoingSize = file->size();
    peer->outgoingSent = 0;
    peer->sending = true;

    QByteArray packet = makeHeader(kTypeFile);
    const QByteArray nameUtf8 = name.toUtf8();
    appendU16(packet, static_cast<quint16>(qMin(nameUtf8.size(), 1024)));
    packet.append(nameUtf8.left(1024));
    appendU64(packet, static_cast<quint64>(peer->outgoingSize));
    peer->socket->write(packet);

    emit statusChanged(tr("Sending %1…").arg(name));
    writeChunk(peer);
}

void ShareFolderService::writeChunk(Peer* peer)
{
    if (!peer || !peer->sending || !peer->outgoingFile || !peer->socket) {
        return;
    }

    while (peer->outgoingSent < peer->outgoingSize && peer->socket->bytesToWrite() < kChunkSize * 2) {
        const QByteArray chunk = peer->outgoingFile->read(kChunkSize);
        if (chunk.isEmpty()) {
            break;
        }
        peer->socket->write(chunk);
        peer->outgoingSent += chunk.size();
        emit transferProgress(peer->outgoingName, peer->outgoingSent, peer->outgoingSize);
    }

    if (peer->outgoingSent >= peer->outgoingSize) {
        const QString name = peer->outgoingName;
        const QString path = peer->outgoingFile->fileName();
        abortOutgoing(peer);
        rememberSnapshot(name, path);
        emit fileSent(name);
        emitStatus();
        startNextSend(peer);
    }
}

void ShareFolderService::processRx(Peer* peer)
{
    if (!peer || !peer->socket) {
        return;
    }
    peer->rx.append(peer->socket->readAll());

    while (true) {
        if (peer->rxState == RxState::WaitHeader) {
            if (peer->rx.size() < kHeaderSize) {
                return;
            }
            if (memcmp(peer->rx.constData(), kMagic, 4) != 0) {
                emit errorOccurred(tr("Shared-folder connection closed (bad protocol)."));
                detachPeer(peer->socket);
                return;
            }
            const auto version = static_cast<quint8>(peer->rx.at(4));
            const auto type = static_cast<quint8>(peer->rx.at(5));
            peer->rx.remove(0, kHeaderSize);
            if (version != kVersion) {
                emit errorOccurred(tr("Shared-folder version mismatch. Install this InputLeap build on both computers."));
                detachPeer(peer->socket);
                return;
            }
            peer->pendingType = type;
            if (type == kTypeHello) {
                peer->rxState = RxState::WaitHello;
            } else if (type == kTypeFile) {
                peer->rxState = RxState::WaitFileMeta;
            } else if (type == kTypeAck) {
                peer->rxState = RxState::WaitAck;
            } else {
                emit errorOccurred(tr("Shared-folder connection closed (unknown message)."));
                detachPeer(peer->socket);
                return;
            }
        }

        if (peer->rxState == RxState::WaitHello || peer->rxState == RxState::WaitAck) {
            quint16 nameLen = 0;
            const int extra = (peer->rxState == RxState::WaitAck) ? 1 : 0;
            if (!peekU16(peer->rx, extra, &nameLen) || peer->rx.size() < extra + 2 + nameLen) {
                return;
            }
            if (peer->rxState == RxState::WaitAck) {
                readU8(peer->rx);
            }
            readU16(peer->rx);
            peer->rx.remove(0, nameLen);
            peer->rxState = RxState::WaitHeader;
            continue;
        }

        if (peer->rxState == RxState::WaitFileMeta) {
            quint16 nameLen = 0;
            if (!peekU16(peer->rx, 0, &nameLen) || peer->rx.size() < 2 + nameLen + 8) {
                return;
            }
            readU16(peer->rx);
            const QString name = sanitizeName(QString::fromUtf8(peer->rx.left(nameLen)));
            peer->rx.remove(0, nameLen);
            const qint64 size = static_cast<qint64>(readU64(peer->rx));
            if (!beginIncomingFile(peer, name, size)) {
                detachPeer(peer->socket);
                return;
            }
            peer->rxState = RxState::WaitFileData;
        }

        if (peer->rxState == RxState::WaitFileData) {
            if (!peer->incomingFile) {
                return;
            }
            const qint64 want = peer->incomingSize - peer->incomingGot;
            const qint64 take = qMin(want, static_cast<qint64>(peer->rx.size()));
            if (take > 0) {
                peer->incomingFile->write(peer->rx.constData(), take);
                peer->rx.remove(0, static_cast<int>(take));
                peer->incomingGot += take;
                emit transferProgress(peer->incomingName, peer->incomingGot, peer->incomingSize);
            }
            if (peer->incomingGot < peer->incomingSize) {
                return;
            }
            finishIncomingFile(peer);
            peer->rxState = RxState::WaitHeader;
            continue;
        }
    }
}

bool ShareFolderService::beginIncomingFile(Peer* peer, const QString& name, qint64 size)
{
    abortIncoming(peer);
    if (name.isEmpty() || size <= 0 || size > kMaxFileSize) {
        emit errorOccurred(tr("Rejected incoming file."));
        return false;
    }

    ensureFolder();
    const QString tempPath = QDir(folder_).filePath(name + QStringLiteral(".ilpart"));
    QFile::remove(tempPath);

    auto* file = new QFile(tempPath);
    if (!file->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        delete file;
        emit errorOccurred(tr("Could not write %1 into the shared folder.").arg(name));
        return false;
    }

    peer->incomingFile = file;
    peer->incomingName = name;
    peer->incomingSize = size;
    peer->incomingGot = 0;
    emit statusChanged(tr("Receiving %1…").arg(name));
    return true;
}

void ShareFolderService::finishIncomingFile(Peer* peer)
{
    if (!peer || !peer->incomingFile) {
        return;
    }

    const QString name = peer->incomingName;
    const QString tempPath = peer->incomingFile->fileName();
    peer->incomingFile->close();
    delete peer->incomingFile;
    peer->incomingFile = nullptr;

    const QString finalPath = QDir(folder_).filePath(name);
    QFile::remove(finalPath);
    if (!QFile::rename(tempPath, finalPath)) {
        QFile::remove(tempPath);
        emit errorOccurred(tr("Could not finish saving %1.").arg(name));
        return;
    }

    rememberSnapshot(name, finalPath);

    QByteArray ack = makeHeader(kTypeAck);
    appendU8(ack, 0);
    const QByteArray nameUtf8 = name.toUtf8();
    appendU16(ack, static_cast<quint16>(nameUtf8.size()));
    ack.append(nameUtf8);
    if (peer->socket) {
        peer->socket->write(ack);
    }

    emit fileReceived(name);
    emitStatus();

    // Forward to any other connected machines.
    enqueueLocalFile(finalPath, peer);
}

void ShareFolderService::abortIncoming(Peer* peer)
{
    if (!peer || !peer->incomingFile) {
        return;
    }
    const QString tempPath = peer->incomingFile->fileName();
    peer->incomingFile->close();
    delete peer->incomingFile;
    peer->incomingFile = nullptr;
    QFile::remove(tempPath);
    peer->incomingName.clear();
    peer->incomingSize = 0;
    peer->incomingGot = 0;
}

void ShareFolderService::abortOutgoing(Peer* peer)
{
    if (!peer) {
        return;
    }
    delete peer->outgoingFile;
    peer->outgoingFile = nullptr;
    peer->outgoingName.clear();
    peer->outgoingSize = 0;
    peer->outgoingSent = 0;
    peer->sending = false;
}

QString ShareFolderService::sanitizeName(const QString& name) const
{
    QString base = QFileInfo(name).fileName();
    base.replace(QLatin1Char('\\'), QLatin1Char('/'));
    base = QFileInfo(base).fileName();
    if (base.isEmpty() || base == QLatin1String(".") || base == QLatin1String("..")) {
        return {};
    }
    if (base.contains(QLatin1Char('/')) || base.contains(QChar(0))) {
        return {};
    }
    return base;
}

bool ShareFolderService::isTempName(const QString& name) const
{
    return name.endsWith(QLatin1String(".ilpart"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".tmp"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".crdownload"), Qt::CaseInsensitive) ||
           name.endsWith(QLatin1String(".part"), Qt::CaseInsensitive) ||
           name.startsWith(QLatin1Char('~')) ||
           name.startsWith(QLatin1Char('.'));
}

bool ShareFolderService::matchesSnapshot(const QString& name, const QString& path) const
{
    const auto it = snapshots_.constFind(name);
    if (it == snapshots_.cend()) {
        return false;
    }
    const QFileInfo info(path);
    return it->size == info.size() && it->mtimeMs == info.lastModified().toMSecsSinceEpoch();
}

void ShareFolderService::rememberSnapshot(const QString& name, const QString& path)
{
    QFileInfo info(path);
    Snapshot snap;
    snap.size = info.size();
    snap.mtimeMs = info.lastModified().toMSecsSinceEpoch();
    snapshots_.insert(name, snap);
}

void ShareFolderService::onScanTimer()
{
    if (folder_.isEmpty() || !QDir(folder_).exists()) {
        return;
    }

    const QFileInfoList files = QDir(folder_).entryInfoList(QDir::Files | QDir::NoDotAndDotDot);
    for (const QFileInfo& info : files) {
        const QString name = info.fileName();
        if (isTempName(name) || !info.isFile()) {
            continue;
        }
        if (info.size() <= 0 || info.size() > kMaxFileSize) {
            continue;
        }
        if (matchesSnapshot(name, info.absoluteFilePath())) {
            continue;
        }

        const qint64 size = info.size();
        if (lastSeenSize_.value(name, -1) != size) {
            lastSeenSize_.insert(name, size);
            stableTicks_.insert(name, 0);
            continue;
        }

        const int ticks = stableTicks_.value(name, 0) + 1;
        stableTicks_.insert(name, ticks);
        if (ticks < 2) {
            continue;
        }

        const QString path = info.absoluteFilePath();
        if (isQueuedOrSending(path)) {
            continue;
        }
        if (!isLinked()) {
            continue;
        }

        lastSeenSize_.remove(name);
        stableTicks_.remove(name);
        enqueueLocalFile(path, nullptr);
    }
}

bool ShareFolderService::isQueuedOrSending(const QString& path) const
{
    for (const Peer* peer : peers_) {
        if (peer->sendQueue.contains(path)) {
            return true;
        }
        if (peer->outgoingFile && peer->outgoingFile->fileName() == path) {
            return true;
        }
    }
    return false;
}

void ShareFolderService::emitStatus()
{
    if (role_ == Role::Idle) {
        emit statusChanged(tr("Start InputLeap to link the shared folder."));
        return;
    }
    if (isLinked()) {
        emit statusChanged(tr("Linked — copy a file here to send it to the other computer."));
        return;
    }
    if (role_ == Role::Client) {
        emit statusChanged(tr("Connecting shared folder to %1…").arg(clientHost_));
        return;
    }
    emit statusChanged(tr("Waiting for the other computer to open this InputLeap build."));
}
