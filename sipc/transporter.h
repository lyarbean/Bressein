/*
This file is part of Bressein.
Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>

Bressein is free software; you can redistribute it and/or
modify it under the terms of the GNU Lesser General Public
License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version.

Bressein is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
Lesser General Public License for more details.

You should have received a copy of the GNU Lesser General Public
License along with this library; if not, write to the Free Software
Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA

OpenSSL linking exception
--------------------------
If you modify this Program, or any covered work, by linking or
combining it with the OpenSSL project's "OpenSSL" library (or a
modified version of that library), containing parts covered by
the terms of OpenSSL/SSLeay license, the licensors of this
Program grant you additional permission to convey the resulting
work. Corresponding Source for a non-source form of such a
combination shall include the source code for the parts of the
OpenSSL library used as well as that of the covered work.
*/

#ifndef TRANSPORTER_H
#define TRANSPORTER_H

#include <QtCore/QObject>
#include <QMap>
#include <QThread>
#include <QMutex>
#include <QTcpSocket>
class QTimer;

namespace Bressein
{
// FIXME
// NOTE must call close to destruct
class Transporter : public QObject
{
    Q_OBJECT

public:
    Transporter (QObject *parent = 0);
    virtual ~Transporter();
public slots:
    void connectToHost (const QByteArray &ip, const quint16 port);
    void stop ();
    void close();
    /**
     * @brief activate keepaliveTicker
     *
     * @return void
     **/
//     void toKeepalive ();
    void sendData (const QByteArray &data);
signals:
    // when received an entire message, emit out
    void dataReceived (const QByteArray &);
    void socketError (const int);
private slots:
    void setHost();
    void onSocketError (QAbstractSocket::SocketError);
    void removeSocket();
//     void activateKeepalive();
//     void keepalive();
    void writeData (const QByteArray &);
    void readData ();
    void queueMessages (const QByteArray &data);
    void dequeueMessages();
private:
    QByteArray ip;
    quint16 port;
    QMutex mutex;
    QThread workerThread;
    // all data to send will be in the list messages
    QList<QByteArray> toSendMessages;//incomes
    QByteArray buffer;
    QTcpSocket *socket;
    QTimer *writerTicker;
//     QTimer *keepaliveTicker;
};
}
#endif // TRANSPORTER_H
