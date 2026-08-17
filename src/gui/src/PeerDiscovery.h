/*
 * InputLeap -- mouse and keyboard sharing utility
 * Copyright (C) 2026 InputLeap contributors
 *
 * This package is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * found in the file LICENSE that should have accompanied this file.
 */

#pragma once

#include <QtCore/QObject>
#include <QtCore/QTimer>
#include <QtCore/QString>
#include <QtCore/QHostAddress>
#include <QtNetwork/QUdpSocket>

class PeerDiscovery : public QObject {
    Q_OBJECT

public:
    explicit PeerDiscovery(QObject* parent = nullptr);
    virtual ~PeerDiscovery();

    void startDiscovery(bool isServer, const QString& screenName, quint16 port = 24800);
    void stopDiscovery();

signals:
    void peerServerFound(const QString& name, const QString& address, quint16 port);
    void peerClientFound(const QString& name, const QString& address);

private slots:
    void sendAnnouncement();
    void processPendingDatagrams();

private:
    QUdpSocket* m_socket;
    QTimer* m_timer;
    bool m_isServer;
    QString m_screenName;
    quint16 m_port;

    static const quint16 kDiscoveryPort = 24801;
};
