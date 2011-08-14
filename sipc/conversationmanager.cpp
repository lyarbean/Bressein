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
    connect (socket, SIGNAL (readyRead()), this, SLOT (readData()));
    connect (socket, SIGNAL (disconnected()), this, SLOT (onSocketError()));
    //TODO handle socket's signals
    this->moveToThread (&workerThread);
    workerThread.start();
}

Conversation::~Conversation()
{
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
    QMetaObject::invokeMethod (this, "setHost", Qt::QueuedConnection);
}

void Conversation::sendData (const QByteArray &data)
{
    QMetaObject::invokeMethod (this, "queueMessages",
                               Qt::QueuedConnection,
                               Q_ARG (QByteArray,data));
}

void Conversation::queueMessages (const QByteArray &data)
{
    //JUST push data to packages for schedule.
    qDebug() << "Conversation::sendData ";
    mutex.lock();
    messages.append (data);
    mutex.unlock();
}

// in the workerThread
void Conversation::setHost ()
{
    socket->connectToHost (ip, port);
    if (not socket->waitForConnected ())
    {
        qDebug() << "Conversation::waitForConnected" << socket->errorString();
        return;
    }
    // now start ticker to check and write if has data
    connect (ticker, SIGNAL (timeout()), this, SLOT (writeData()),
             Qt::DirectConnection);
    ticker->start (1000);
}

void Conversation::writeData ()
{
    qDebug() << "Conversation::writeData";
    mutex.lock();
    bool empty = false;
    QByteArray data;
    if (not messages.isEmpty())
    {
        data = messages.takeFirst();
    }
    else
    {
        empty = true;
    }
    int length = 0;
    mutex.unlock();
    while (not empty)
    {
        if (data.isEmpty()) return;
        qDebug() << "=================================";
        qDebug() << "Conversation::writeData" << data;
        qDebug() << "=================================";
        if (socket->state() not_eq QAbstractSocket::ConnectedState)
        {
            qDebug() <<  "Conversation::Error: socket is not connected";
            // TODO handle this,
            // ask manager to remove this instance
            return;
        }
        length = 0;
        while (length < data.length())
        {
            length += socket->write (data.right (data.size() - length));
        }
        socket->waitForBytesWritten ();
        socket->flush();
        qDebug() << "WRITTEN!!";
    end:
        mutex.lock();
        if (not messages.isEmpty())
        {
            data = messages.takeFirst();
        }
        else
        {
            empty = true;
        }
        mutex.unlock();
    }
}

// same as account
// TODO make a base class for both
void Conversation::readData ()
{
    while (socket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket->waitForReadyRead (1000))
        {
            if (socket->error() not_eq QAbstractSocket::SocketTimeoutError)
            {
                qDebug() << "sipcRead 1";
                qDebug() << "When waitForReadyRead"
                         << socket->error() << socket->errorString();
            }
            return;
        }
    }
    static QByteArray delimit_1 = "L: ";
    static QByteArray delimit_2 = "content-Content-Length: ";
    QByteArray responseData = socket->readAll ();
    buffer.append (responseData);
    QByteArray chunck;
    int seperator = buffer.indexOf ("\r\n\r\n");
    bool ok;
    int length = 0;
    int pos = 0;
    int pos_ = 0;
    QByteArray delimit;
    while (seperator > 0)
    {
        delimit = delimit_1;
        pos = buffer.indexOf (delimit_1);
        if (pos < 0 or pos > seperator)
        {

            qDebug() << "no" << delimit_1;
            pos = buffer.indexOf (delimit_2);
            delimit = delimit_2;
            if (pos < 0 or pos > seperator)
            {
                qDebug() << "no" << delimit_2;
                // if no length tag,
                chunck = buffer.left (seperator + 4);
                buffer.remove (0, seperator + 4);
                emit dataReceived (chunck);
                return;
            }
        }
        pos_ = buffer.indexOf ("\r\n", pos);
        length = buffer
                 .mid (pos + delimit.size(), pos_ - pos - delimit.size())
                 .toUInt (&ok);
        if (not ok)
        {
            qDebug() << "Not ok" << buffer;

            return;
        }
        if (buffer.size() >= length + seperator + 4)
        {
            chunck = buffer.left (length + seperator + 4);
            buffer.remove (0, length + seperator + 4);
            emit dataReceived (chunck);
            seperator = buffer.indexOf ("\r\n\r\n");
        }
        else
        {
            return;
        }
    }
}

void Conversation::onSocketError()
{
    //TODO
    emit toClose (sipuri);
}

void Conversation::close()
{
    qDebug() << "Conversation::close";
    ticker->stop();
    ticker->deleteLater();
    QMetaObject::invokeMethod (this, "removeSocket", Qt::QueuedConnection);
    if (workerThread.isRunning())
    {
        workerThread.quit();
        workerThread.wait();
    }
    //FIXME is this correct?
    this->deleteLater();
}

void Conversation::removeSocket()
{
    qDebug() << "delete sockets";
    socket->disconnectFromHost();
    while (socket->state() not_eq QAbstractSocket::UnconnectedState)
        socket->waitForDisconnected();
    qDebug() << "delete sockets 2";
    socket->deleteLater();
}

void Conversation::dequeueMessages()
{

}

ConversationManager::ConversationManager (QObject *parent) : QObject (parent)
{

}

ConversationManager::~ConversationManager()
{

}

void ConversationManager::addConversation (const QByteArray &sipuri)
{
    if (isOnConversation (sipuri))
    {
        removeConversation (sipuri);
    }
    Conversation *conversation = new Conversation (sipuri);
    conversations.insert (sipuri, conversation);
    connect (conversation, SIGNAL (dataReceived (const QByteArray &)),
             this, SLOT (onDataReceived (const QByteArray &)));
    connect (conversation, SIGNAL (toClose (const QByteArray &)),
             this, SLOT (removeConversation (const QByteArray &)));
}
void ConversationManager::setHost (const QByteArray &sipuri,
                                   const QByteArray &ip,
                                   const quint16 port)
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
    if (isOnConversation (sipuri))
    {
        Conversation *conversation = conversations.value (sipuri);
        if (conversation->name() not_eq sipuri)
        {
            qDebug () << " sipuri not matched";
            //TODO
        }
        disconnect (conversation, SIGNAL (dataReceived (const QByteArray &)),
                    this, SLOT (onDataReceived (const QByteArray &)));
        disconnect (conversation, SIGNAL (toClose (const QByteArray &)),
                    this, SLOT (removeConversation (const QByteArray &)));
        //NOTE call close rather than deleteLater!!
        conversation->close();
        conversations.remove (sipuri);
        // FIXME check bugs here
    }
}

void ConversationManager::sendData (const QByteArray &sipuri,
                                    const QByteArray &data)
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

void ConversationManager::closeAll()
{
    Conversation *conversation;
    QMap<QByteArray, Conversation *>::iterator iterator =
        conversations.begin();
    while (iterator not_eq conversations.end())
    {
        conversation = iterator.value();
        disconnect (conversation, SIGNAL (dataReceived (const QByteArray &)),
                    this, SLOT (onDataReceived (const QByteArray &)));
        //NOTE call close rather than deleteLater!!
        conversation->close();
        iterator++;
        // FIXME check bugs here
    }
    conversations.clear();
}

}
#include "conversationmanager.moc"
