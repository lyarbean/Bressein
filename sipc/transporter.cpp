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

#include "transporter.h"
#include <QTimer>
#include <QDebug>
namespace Bressein
{

Transporter::Transporter (QObject *parent)
    : QObject (parent), socket (new QTcpSocket (this)), writerTicker (new QTimer (this))
{
    socket->setReadBufferSize (0);
    socket->setSocketOption (QAbstractSocket::KeepAliveOption, 1);
    socket->setSocketOption (QAbstractSocket::LowDelayOption, 1);
    connect (socket, SIGNAL (readyRead()), this, SLOT (readData()));
    connect (socket, SIGNAL (error (QAbstractSocket::SocketError)),
             this, SLOT (onSocketError (QAbstractSocket::SocketError)));
    this->moveToThread (&workerThread);
    workerThread.start();
}

Transporter::~Transporter()
{

}

void Transporter::connectToHost (const QByteArray &ip, const quint16 port)
{
    mutex.lock();
    this->ip = ip;
    this->port = port;
    mutex.unlock();
    QMetaObject::invokeMethod (this, "setHost", Qt::QueuedConnection);
}

void Transporter::stop()
{
    writerTicker->stop();
    writerTicker->disconnect();
    socket->disconnectFromHost();
    toSendMessages.clear();
}

void Transporter::close()
{
    writerTicker->stop();
    writerTicker->disconnect();
    writerTicker->deleteLater();
    QMetaObject::invokeMethod (this, "removeSocket", Qt::QueuedConnection);
    if (workerThread.isRunning())
    {
        workerThread.quit();
        workerThread.wait();
    }
    //FIXME is this correct?
    this->deleteLater();
}

void Transporter::sendData (const QByteArray &data)
{
    QMetaObject::invokeMethod (this, "queueMessages",
                               Qt::QueuedConnection,
                               Q_ARG (QByteArray, data));
}

// private
void Transporter::setHost()
{
    socket->connectToHost (ip, port);
    if (not socket->waitForConnected ())
    {
        qDebug() << this->metaObject()->className()
                 << "::waitForConnected" << socket->errorString();
        return;
    }
    // now start ticker to check and write if has data
    connect (writerTicker, SIGNAL (timeout()), this, SLOT (dequeueMessages()),
             Qt::DirectConnection);
    writerTicker->start (1000);
}

void Transporter::onSocketError (QAbstractSocket::SocketError se)
{
    qDebug() << this->metaObject()->className() << "SocketError" << se;
    emit socketError ( (int) se);
}

void Transporter::removeSocket()
{
    socket->disconnect();
    socket->disconnectFromHost();
    while (socket->state() not_eq QAbstractSocket::UnconnectedState)
        socket->waitForDisconnected();
    socket->deleteLater();
    qDebug() << this->metaObject()->className() << "removeSocket";
}

void Transporter::writeData (const QByteArray &data)
{
    if (data.isEmpty()) return;
    qDebug() << this->metaObject()->className() << "::write data";
    qDebug() << QString::fromUtf8 (data);
    if (socket->state() not_eq QAbstractSocket::ConnectedState)
    {
        qDebug() <<  "writeData::Error: socket is not connected";
        qDebug() << socket->state();
        return;
    }
    int length = 0;
    while (length < data.length())
    {
        length += socket->write (data.right (data.size() - length));
    }
    socket->waitForBytesWritten ();
    socket->flush();
    qDebug() << this->metaObject()->className() << "::data written";
}

void Transporter::readData()
{
    while (socket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket->waitForReadyRead (1000))
        {
            if (socket->error() not_eq QAbstractSocket::SocketTimeoutError)
            {
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
            pos = buffer.indexOf (delimit_2);
            delimit = delimit_2;
            if (pos < 0 or pos > seperator)
            {
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

void Transporter::queueMessages (const QByteArray &data)
{
    mutex.lock();
    toSendMessages.append (data);
    mutex.unlock();
}

void Transporter::dequeueMessages()
{
    mutex.lock();
    QByteArray data;
    bool empty = toSendMessages.isEmpty();
    if (not empty)
    {
        data = toSendMessages.takeFirst();
    }
    mutex.unlock();
    while (not empty)
    {
        if (not data.isEmpty())
        {
            writeData (data);
        }
        mutex.lock();
        if (not toSendMessages.isEmpty())
        {
            data = toSendMessages.takeFirst();
        }
        else
        {
            empty = true;
        }
        mutex.unlock();
    }
}

}

#include "transporter.moc"
