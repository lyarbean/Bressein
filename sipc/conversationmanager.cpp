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


#include "conversationmanager.h"
#include <QTcpSocket>
#include <QTimer>

namespace Bressein
{
//private class
Conversation::Conversation (const QByteArray &sipuri, QObject *parent)
    : QObject (parent)
{
    this->sipuri = sipuri;
    socket = new QTcpSocket (this);
    socket->setReadBufferSize (0);
    socket->setSocketOption (QAbstractSocket::KeepAliveOption, 1);
    socket->setSocketOption (QAbstractSocket::LowDelayOption, 1);
    ticker = new QTimer (this);
    //TODO handle socket's signals
    this->moveToThread (&workerThread);
    workerThread.start();
}
Conversation::~Conversation()
{
    socket->close();
    socket->deleteLater();
    if (workerThread.isRunning())
    {
        workerThread.quit();
        workerThread.wait();
    }
}
const QByteArray &Conversation::name () const
{
    return sipuri;
}
void Conversation::connectToHost (const QByteArray &ip, const quint16 port)
{
    mutex.lock();
    this->ip = ip;
    this->port = port;
    mutex.unlock();
    QMetaObject::invokeMethod (this, "setHost");
}
void Conversation::sendData (const QByteArray &data)
{
    QMetaObject::invokeMethod (this, "writeData", Q_ARG (QByteArray, data));
}
// in the workerThread
void Conversation::setHost ()
{
    socket->connectToHost (ip, port);
    if (not socket->waitForConnected ())
    {
        qDebug() << "waitForConnected" << socket->errorString();
        return;
    }
    // now start ticker
    connect (ticker, SIGNAL (timeout()), this, SLOT (listen()));
    ticker->start (3000);
}
void Conversation::writeData (const QByteArray &in)
{
    if (socket->state() not_eq QAbstractSocket::ConnectedState)
    {
        qDebug() <<  "Error: socket is not connected";
        // TODO handle this,
        // ask manager to remove this instance
        return;
    }
    int length = 0;
    while (length < in.length())
    {
        length += socket->write (in.right (in.size() - length));
    }
    socket->waitForBytesWritten ();
    socket->flush();
}

void Conversation::readData (QByteArray &out)
{
    while (socket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket->waitForReadyRead (10000))
        {
            if (socket->error() not_eq QAbstractSocket::SocketTimeoutError)
            {
                qDebug() << "readData 1";
                qDebug() << "When waitForReadyRead"
                         << socket->error() << socket->errorString();
            }
            return;
        }
    }
    QByteArray responseData = socket->readLine ();
    while (responseData.indexOf ("\r\n\r\n") < 0)
    {
        while (socket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket->waitForReadyRead ())
            {
                qDebug() << "readData 2";
                qDebug() << "When waitForReadyRead"
                         << socket->error() << socket->errorString();
                return;
            }
        }
        responseData.append (socket->readLine ());
    }
    QByteArray delimit = "L: ";
    int pos = responseData.indexOf (delimit);
    if (pos < 0)
    {
        delimit = "content-length: ";
        pos = responseData.indexOf (delimit);

        if (pos < 0) //still not found
        {
            out = responseData;
            return;
        }
    }
    int pos_ = responseData.indexOf ("\r\n", pos);
    bool ok;
    int length = responseData.mid
                 (pos + delimit.size(), pos_ - pos - delimit.size()).toUInt (&ok);
    if (not ok)
    {
        qDebug() << "Not ok" << responseData;
        out = responseData;
        return;
    }
    pos = responseData.indexOf ("\r\n\r\n");
    int received = responseData.size();
    while (received < length + pos + 4)
    {
        while (socket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket->waitForReadyRead ())
            {
                // TODO handle socket.error() or inform user what happened
                qDebug() << "readData 3  waitForReadyRead"
                         << socket->error() << socket->errorString();
                return;
            }
        }
        responseData.append (socket->read (length + pos + 4 - received));
        received = responseData.size();
    }
    out = responseData;
}

void Conversation::listen()
{
    QByteArray data;
    readData (data);
    if (data.isEmpty())
    {
        return;
    }
    qDebug() << "Conversation::listen";
    qDebug() << data;
    emit dataReceived (data);
}


ConversationManager::ConversationManager (QObject *parent) : QObject (parent)
{

}

ConversationManager::~ConversationManager()
{

}

void ConversationManager::addConversation (const QByteArray &sipuri)
{
    Conversation *conversation = new Conversation (sipuri);
    conversations.insert (sipuri, conversation);
    connect (conversation, SIGNAL (dataReceived (const QByteArray &)),
             this, SLOT (onDataReceived (const QByteArray &)));
}
void ConversationManager::setHost (const QByteArray &sipuri, const QByteArray &ip, const quint16 port)
{
    QMap<QByteArray, Conversation *>::iterator iterator =
        conversations.find (sipuri);
    if (iterator not_eq  conversations.end() && iterator.key() == sipuri)
    {
        iterator.value()->connectToHost (ip, port);
    }
}

void ConversationManager::removeConversation (const QByteArray &sipuri)
{
    QMap<QByteArray, Conversation *>::iterator iterator =
        conversations.find (sipuri);
    if (iterator not_eq  conversations.end() && iterator.key() == sipuri)
    {
        conversations.remove (sipuri);
        Conversation *conversation = iterator.value();
        if (conversation->name() not_eq sipuri)
        {
            qDebug () << " sipuri not matched";
        }
        disconnect (conversation, SIGNAL (dataReceived (const QByteArray &)),
                    this, SLOT (onDataReceived (const QByteArray &)));
        conversation->deleteLater();
        // FIXME check bugs here
    }
}

void ConversationManager::sendData (const QByteArray &sipuri, const QByteArray &data)
{
    QMap<QByteArray, Conversation *>::iterator iterator =
        conversations.find (sipuri);
    if (iterator not_eq  conversations.end() && iterator.key() == sipuri)
    {
        iterator.value()->sendData (data);
    }
}

bool ConversationManager::isOnConversation (const QByteArray &sipuri) const
{
    QMap<QByteArray, Conversation *>::const_iterator iterator =
        conversations.find (sipuri);
    if (iterator not_eq  conversations.end() && iterator.key() == sipuri)
    {
        return true;
    }
    return false;
}

void ConversationManager::onDataReceived (const QByteArray &data)
{
    emit receiveData (data);
}

}
#include "conversationmanager.moc"
