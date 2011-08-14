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


#include "portraitfetcher.h"
#include <QHostInfo>
#include <QTcpSocket>
#include <QFile>
#include <QDir>
#include "utils.h"
namespace Bressein
{
PortraitFetcher::PortraitFetcher (QObject *parent) : QThread (parent)
{
}

PortraitFetcher::~PortraitFetcher()
{
    wait();
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

void PortraitFetcher::requestPortrait (const QByteArray &sipuri)
{
    //FIXME I'm sure this is buggy!!
    mutex.lock();
    queue.append (sipuri);
    mutex.unlock();
    if (not isRunning())
    {
        start();
    }
}

void PortraitFetcher::run ()
{
    mutex.lock();
    QByteArray sipuri;
    if (not queue.isEmpty())
        sipuri = queue.takeFirst();
    bool empty = false;
    mutex.unlock();

    while (not empty)
    {
        qDebug() << "to invoke" << sipuri;
        if (sipuri.isEmpty()) return;
        if (server.isEmpty() or path.isEmpty())
        {
            return;
        }
        QString host =
            QHostInfo::fromName (server).addresses().first().toString();
        QTcpSocket socket;
        socket.connectToHost (host, 80);
        if (not socket.waitForConnected ())
        {
            qDebug() << "PortraitFetcher waitForConnected"
                     << socket.error() << socket.errorString();
            return;
        }
        // TODO error handle

        QByteArray toSendMsg =
            downloadPortraitData (server, path, sipuri, ssic);
        socket.write (toSendMsg);
        QByteArray responseData;
        while (socket.bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket.waitForReadyRead ())
            {
                // TODO handle socket.error() or inform user what happened
                qDebug() << "PortraitFetcher  waitForReadyRead"
                         << socket.error() << socket.errorString();
                return;
            }
        }
        responseData = socket.readAll();
        if (responseData.contains ("HTTP/1.1 404 Not Found"))
        {
            goto end;
        }
        // try download again if not ok
        if (responseData.contains ("HTTP/1.1 302 Found"))
        {
            int b = responseData.indexOf ("Location: http://");
            int e = responseData.indexOf ("\r\n");
            QByteArray fullpath = responseData.mid (b + 17, e - b - 17);
            int m = fullpath.indexOf ("/");
            QByteArray host = fullpath.left (m);
            QByteArray path = fullpath.mid (m); // including "/"
            toSendMsg = downloadPortraitAgainData (path, host);
            while (socket.bytesAvailable() < (int) sizeof (quint16))
            {
                if (not socket.waitForReadyRead ())
                {
                    // TODO handle socket.error() or inform user what happened
                    qDebug() << "PortraitFetcher  waitForReadyRead"
                             << socket.error() << socket.errorString();
                    return;
                }
            }
            responseData = socket.readAll();
        }
        if (responseData.contains ("HTTP/1.1 200 OK"))
        {
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
            int received = responseData.size();
            while (received < length + pos_ + 4)
            {
                while (socket.bytesAvailable() < (int) sizeof (quint16))
                {
                    if (not socket.waitForReadyRead ())
                    {
                        // TODO handle socket.error() or inform user what happened
                        qDebug() << "ssiLogin  waitForReadyRead"
                                 << socket.error() << socket.errorString();
                        return;
                    }
                }
                responseData.append (socket.readAll());
                received = responseData.size();
            }

            QByteArray bytes = responseData.mid (pos_ + 4);
            qDebug() << "bytes size" << bytes.size();
            int a = sipuri.indexOf (":");
            int b = sipuri.indexOf ("@");
            static QByteArray iconsSubDir =
                QDir::homePath().toLocal8Bit().append ("/.bressein/icons/");
            if (not  QDir::home().exists (iconsSubDir))
            {
                QDir::home().mkpath (".bressein/icons/");
            }
            QByteArray filename = iconsSubDir;
            filename.append (sipuri.mid (a + 1, b - a - 1));
            filename.append (".jpeg");
            QFile file (filename);
            file.open (QIODevice::WriteOnly);
            QDataStream out (&file);
            out.writeRawData (bytes.data(), bytes.size());
            file.close();
            socket.disconnectFromHost();
            if (socket.state() not_eq QAbstractSocket::UnconnectedState)
                socket.waitForDisconnected (1000);
        }
        else
        {
            // Unknown
            return;
        }
    end:
        mutex.lock();
        if (not queue.isEmpty())
        {
            sipuri = queue.takeFirst();
        }
        else
        {
            empty = true;
        }
        mutex.unlock();
    }
}
}
#include "portraitfetcher.moc"
