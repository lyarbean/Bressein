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


#include "portraitfetcher.h"
#include <QHostInfo>
#include <QTcpSocket>
#include <QImage>
#include <QFile>
#include "utils.h"
namespace Bressein
{
PortraitFetcher::PortraitFetcher (QObject *parent) : QObject (parent),
    socket (new QTcpSocket (this))
{
    this->moveToThread (&workerThread);
    workerThread.start();
}

PortraitFetcher::~PortraitFetcher()
{
    socket->close();
    if (workerThread.isRunning())
    {
        workerThread.quit();
        workerThread.wait();
    }
}

void PortraitFetcher::setData (const QByteArray &server,
                               const QByteArray &path,
                               const QByteArray &ssic)
{
    mutex.lock();
    this->server = server;
    this->path = path;
    this->ssic = ssic;
    mutex.unlock();
}

void PortraitFetcher::toGet (const QByteArray &sipuri)
{
    //FIXME I'm sure this is buggy!!
    mutex.lock();
    qDebug() << "to invoke";
    QMetaObject::invokeMethod (this, "get", Qt::QueuedConnection,

                               Q_ARG (const QByteArray &, sipuri));
    mutex.unlock();
    enterCondition.wakeOne();
}

void PortraitFetcher::get (const QByteArray &sipuri)
{
    mutex.lock();
    //http://hdss1fta.fetion.com.cn/HDS_S00/geturi.aspx
    if (server.isEmpty() or path.isEmpty())
    {
        //
        qDebug() << "systemconfig.serverNamePath is empty";
        return;
    }
    QString host =
        QHostInfo::fromName (server).addresses().first().toString();
    socket->connectToHost (host, 80);
    if (not socket->waitForConnected ())
    {
        qDebug() << "waitForConnected" << socket->errorString();
        return;
    }
    QByteArray toSendMsg =
        downloadPortraitData (server, path, sipuri, ssic);
    socket->write (toSendMsg);
    QByteArray responseData;
    while (socket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket->waitForReadyRead ())
        {
            // TODO handle socket->error() or inform user what happened
            qDebug() << "ssiLogin  waitForReadyRead"
                     << socket->error() << socket->errorString();
            return;
        }
    }
    responseData = socket->readAll();
    int pos = responseData.indexOf ("Content-Length: ");
    if (pos < 0)
    {
        //TODO
        return;
    }
    int pos_ = responseData.indexOf ("\r\n", pos);
    bool ok;
    int length = responseData.mid (pos + 15, pos_ - pos - 15).toUInt (&ok);
    if (not ok)
    {
        qDebug() << "not ok" << responseData;
        return;
    }
    qDebug() << "not ok" << responseData;
    int received = responseData.size();
    while (received < length + pos_ + 4)
    {
        while (socket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket->waitForReadyRead ())
            {
                // TODO handle socket->error() or inform user what happened
                qDebug() << "ssiLogin  waitForReadyRead"
                         << socket->error() << socket->errorString();
                return;
            }
        }
        responseData.append (socket->readAll());
        received = responseData.size();
    }
    // TODO get image from responseData
    QByteArray bytes = responseData.mid (pos_ + 4);
    qDebug() << "bytes size" << bytes.size();
    QByteArray filename = sipuri;
    filename.replace (":", "_");
    filename.replace ("@", "_");
    filename.replace (";", "_");
    filename.replace ("=", "_");
    filename.append (".jpeg");
    QFile file (filename);
    file.open (QIODevice::WriteOnly);
    QDataStream out (&file);
    out.writeRawData (bytes.data(), bytes.size());
    file.close();
    socket->waitForDisconnected (0);
    enterCondition.wait (&mutex);
    mutex.unlock();
}
}
#include "portraitfetcher.moc"
