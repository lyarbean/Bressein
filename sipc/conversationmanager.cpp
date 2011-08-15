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
    : Transporter (parent),sipuri (sipuri)
{
    connect (this, SIGNAL (socketError (const int)),
             SLOT (onSocketError (const int)));
}

Conversation::~Conversation()
{
}

const QByteArray &Conversation::name () const
{
    return sipuri;
}

void Conversation::onSocketError (const int se)
{
    //TODO handle almost cases
    switch (se)
    {
        case QAbstractSocket::ConnectionRefusedError:
        case QAbstractSocket::RemoteHostClosedError:
        case QAbstractSocket::HostNotFoundError:
            emit toClose (sipuri);
            break;
        default:
            break;
    }
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

void ConversationManager::sendToAll (const QByteArray &data)
{
    QMap<QByteArray, Conversation *>::iterator iterator = conversations.begin();
    while (iterator not_eq conversations.end())
    {
        iterator.value()->sendData (data);
        iterator++;
    }
}

bool ConversationManager::isOnConversation (const QByteArray &sipuri) const
{
    if (conversations.isEmpty())
    {
        return false;
    }
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
