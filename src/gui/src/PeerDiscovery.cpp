/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#include "PeerDiscovery.h"
#include <QtNetwork/QNetworkInterface>

PeerDiscovery::PeerDiscovery(QObject* parent) :
    QObject(parent),
    m_socket(new QUdpSocket(this)),
    m_timer(new QTimer(this)),
    m_isServer(false),
    m_port(24800)
{
    connect(m_timer, &QTimer::timeout, this, &PeerDiscovery::sendAnnouncement);
    connect(m_socket, &QUdpSocket::readyRead, this, &PeerDiscovery::processPendingDatagrams);
}

PeerDiscovery::~PeerDiscovery()
{
    stopDiscovery();
}

void PeerDiscovery::startDiscovery(bool isServer, const QString& screenName, quint16 port)
{
    m_isServer = isServer;
    m_screenName = screenName;
    m_port = port;

    if (m_socket->state() == QAbstractSocket::BoundState) {
        m_socket->close();
    }

    m_socket->bind(QHostAddress::AnyIPv4, kDiscoveryPort, QUdpSocket::ShareAddress | QUdpSocket::ReuseAddressHint);
    
    // Broadcast every 2.5 seconds
    m_timer->start(2500);
    sendAnnouncement();
}

void PeerDiscovery::stopDiscovery()
{
    if (m_timer->isActive()) {
        m_timer->stop();
    }
    if (m_socket->isOpen()) {
        m_socket->close();
    }
}

void PeerDiscovery::sendAnnouncement()
{
    if (m_screenName.isEmpty()) {
        return;
    }

    QString message = QString("INPUTLEAP:%1:%2:%3")
                          .arg(m_isServer ? "SERVER" : "CLIENT")
                          .arg(m_screenName)
                          .arg(m_port);

    QByteArray datagram = message.toUtf8();
    m_socket->writeDatagram(datagram, QHostAddress::Broadcast, kDiscoveryPort);
}

void PeerDiscovery::processPendingDatagrams()
{
    while (m_socket->hasPendingDatagrams()) {
        QByteArray buffer;
        buffer.resize(static_cast<int>(m_socket->pendingDatagramSize()));
        QHostAddress sender;
        quint16 senderPort;

        m_socket->readDatagram(buffer.data(), buffer.size(), &sender, &senderPort);
        QString data = QString::fromUtf8(buffer);

        if (!data.startsWith("INPUTLEAP:")) {
            continue;
        }

        QStringList parts = data.split(':');
        if (parts.size() < 4) {
            continue;
        }

        QString role = parts[1];
        QString name = parts[2];
        quint16 port = static_cast<quint16>(parts[3].toUInt());

        // Skip our own broadcasts
        if (name == m_screenName) {
            continue;
        }

        QString ipAddress = sender.toString();
        if (ipAddress.startsWith("::ffff:")) {
            ipAddress = ipAddress.mid(7);
        }

        if (role == "SERVER" && !m_isServer) {
            emit peerServerFound(name, ipAddress, port);
        }
        else if (role == "CLIENT" && m_isServer) {
            emit peerClientFound(name, ipAddress);
        }
    }
}
