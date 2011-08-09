/*
    <one line to give the library's name and an idea of what it does.>
    Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation; either
    version 2.1 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Lesser General Public License for more details.

    You should have received a copy of the GNU Lesser General Public
    License along with this library; if not, write to the Free Software
    Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
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
    void removeConversation (const QByteArray &sipuri);
    void sendData (const QByteArray &sipuri, const QByteArray &data);
    bool isOnConversation (const QByteArray &sipuri) const;
signals:
    void receiveData (const QByteArray &);
public slots:
    void onDataReceived (const QByteArray &);
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
public slots:
    void sendData (const QByteArray &data);
    // in the workerThread
    void setHost();
    void writeData (const QByteArray &in);
    void readData (QByteArray &out);
signals:
    void dataReceived (const QByteArray &);
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
