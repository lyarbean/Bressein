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
#include "aux.h"

#include <QHostInfo>
#include <QTcpSocket>
#include <QFile>
#include <QDir>
#include <QtEndian>
#include <QDebug>
//===========
// png sample
/* Table of CRCs of all 8-bit messages. */
// static unsigned long crc_table[256];

/* Flag: has the table been computed? Initially false. */
// static int crc_table_computed = 0;

/* Make the table for a fast CRC. */
// static void make_crc_table (void)
// {
//     unsigned long c;
//     int n, k;
//
//     for (n = 0; n < 256; n++)
//     {
//         c = (unsigned long) n;
//         for (k = 0; k < 8; k++)
//         {
//             if (c & 1)
//                 c = 0xBA0DC66B ^ (c >> 1);
//             else
//                 c = c >> 1;
//         }
//         crc_table[n] = c;
//     }
//     crc_table_computed = 1;
// }

/* Update a running CRC with the bytes buf[0..len-1]--the CRC
 *  should be initialized to all 1's, and the transmitted value
 *  is the 1's complement of the final running CRC (see the
 *  crc() routine below)). */

// static unsigned long update_crc (unsigned long crc, unsigned char *buf,
//         int len)
// {
//     unsigned long c = crc;
//     int n;
//
//     if (!crc_table_computed)
//         make_crc_table();
//     for (n = 0; n < len; n++)
//     {
//         c = crc_table[ (c ^ buf[n]) & 0xff] ^ (c >> 8);
//     }
//     return c;
// }

/* Return the CRC of the bytes buf[0..len-1]. */
// static unsigned long crc (unsigned char *buf, int len)
// {
//     return update_crc (0xffffffffL, buf, len) ^ 0xffffffffL;
// }
//===========
namespace Bressein
{
PortraitFetcher::PortraitFetcher (QObject *parent) : QThread (parent)
{
    iconsSubDir =
        QDir::homePath().toLocal8Bit().append ("/.bressein/icons/");
    if (not  QDir::home().exists (iconsSubDir))
    {
        QDir::home().mkpath (".bressein/icons/");
    }
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
    if (not isRunning() and
        not this->server.isEmpty() and
        not this->path.isEmpty() and
        not this->ssic.isEmpty())
    {
        qDebug() << "PortraitFetcher starts;";
        start();
    }
}

void PortraitFetcher::run ()
{
    //FIXME
    mutex.lock();
    QByteArray sipuri;
    bool empty = queue.isEmpty();
    if (not empty)
    {
        sipuri = queue.takeFirst();
    }
    QByteArray bytes;
    QByteArray toSendMsg;
    QByteArray responseData;
    mutex.unlock();

    while (not empty)
    {
        qDebug() << "to invoke" << sipuri;
        if (sipuri.isEmpty()) return;
        if (server.isEmpty() or path.isEmpty())
        {
            emit processed (sipuri);
            return;
        }

        QHostInfo hostInfo = QHostInfo::fromName (server);
        QString host;
        if (not hostInfo.addresses().isEmpty())
        {
            host = hostInfo.addresses().first().toString();
        }
        else
        {
            //FIXME
            emit processed (sipuri);
            return;
        }
        QTcpSocket socket;
        socket.connectToHost (host, 80);
        if (not socket.waitForConnected (5000))
        {
            qDebug() << "PortraitFetcher waitForConnected"
                     << socket.error() << socket.errorString();
            emit processed (sipuri);
            return;
        }
        // TODO error handle

        toSendMsg =
            downloadPortraitData (server, path, sipuri, ssic);
        int length = 0;
        while (length < toSendMsg.length())
        {
            length += socket.write (toSendMsg.right (toSendMsg.size() - length));
        }
        socket.waitForBytesWritten ();
        socket.flush();

        while (socket.bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket.waitForReadyRead ())
            {
                // TODO handle socket.error() or inform user what happened
                qDebug() << "PortraitFetcher  waitForReadyRead"
                         << socket.error() << socket.errorString();
                //FIXME
                goto end;
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
                if (not socket.waitForReadyRead (10000))
                {
                    // TODO handle socket.error() or inform user what happened
                    qDebug() << "PortraitFetcher  waitForReadyRead"
                             << socket.error() << socket.errorString();
                    goto end;
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
                goto end;
            }
            int pos_ = responseData.indexOf ("\r\n", pos);
            bool ok;
            int length = responseData.mid (pos + 15, pos_ - pos - 15).toUInt (&ok);
            if (not ok)
            {
                qDebug() << "not ok" << responseData;
                goto end;
            }
            int received = responseData.size();
            while (received < length + pos_ + 4)
            {
                while (socket.bytesAvailable() < (int) sizeof (quint16))
                {
                    if (not socket.waitForReadyRead (10000))
                    {
                        // TODO handle socket.error() or inform user what happened
                        qDebug() << "ssiLogin  waitForReadyRead"
                                 << socket.error() << socket.errorString();
                        goto end;
                    }
                }
                responseData.append (socket.readAll());
                received = responseData.size();
            }

            bytes = responseData.mid (pos_ + 4);
            QByteArray filename = iconsSubDir;
            filename.append (sipToFetion (sipuri));
            filename.append (".jpeg");
            QFile file (filename);
            file.open (QIODevice::WriteOnly);
            QDataStream out (&file);
            // TODO what CRC does fetion deploy?
//             quint32 CRC = crc ((unsigned char*)bytes.data(),bytes.size());
            out.writeRawData (bytes.data(), bytes.size());
            file.close();
            socket.disconnectFromHost();
        }
    end:
        emit processed (sipuri);
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
