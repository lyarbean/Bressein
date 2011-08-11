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


#ifndef CONVERSATIONMANAGER_H
#define CONVERSATIONMANAGER_H

#include <QtCore/QObject>
#include <QMap>
#include <QThread>
#include <QMutex>
class QTcpSocket;
class QTimer;
namespace Bressein
{
// forward declaration
class Conversation;
class ConversationManager : public QObject
{
    Q_OBJECT
public:
    ConversationManager (QObject *parent = 0);
    virtual ~ConversationManager();
    void addConversation (const QByteArray &sipuri);
    void setHost (const QByteArray &sipuri,
                  const QByteArray &ip,
                  const quint16 port);

    void sendData (const QByteArray &sipuri, const QByteArray &data);
    bool isOnConversation (const QByteArray &sipuri) const;
signals:
    void receiveData (const QByteArray &);
public slots:
    void onDataReceived (const QByteArray &);
    void removeConversation (const QByteArray &sipuri);
    void closeAll();
private:
    QMap<QByteArray, Conversation *> conversations;
};

class Conversation : public QObject
{
    Q_OBJECT
public:
    Conversation (const QByteArray &sipuri, QObject *parent = 0);
    ~Conversation();
    const QByteArray &name () const;

    void connectToHost (const QByteArray &ip, const quint16 port);
    void close();
public slots:
    void sendData (const QByteArray &data);
    // in the workerThread
    void setHost();
    void writeData (const QByteArray &in);
    void readData (QByteArray &out);
signals:
    void dataReceived (const QByteArray &);
    void toClose (const QByteArray &);

private slots:
    void listen();
private:
    QByteArray sipuri;
    QByteArray ip;
    quint16 port;
    QMutex mutex;
    QThread workerThread;
    QTcpSocket *socket;
    QTimer *ticker;
};
}
#endif // CONVERSATIONMANAGER_H
