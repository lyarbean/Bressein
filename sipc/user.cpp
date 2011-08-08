/*
 *  This file is part of Bressein.
 *  Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2.1 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

/*
 *  OpenSSL linking exception
 *  --------------------------
 *  If you modify this Program, or any covered work, by linking or
 *  combining it with the OpenSSL project's "OpenSSL" library (or a
 *  modified version of that library), containing parts covered by
 *  the terms of OpenSSL/SSLeay license, the licensors of this
 *  Program grant you additional permission to convey the resulting
 *  work. Corresponding Source for a non-source form of such a
 *  combination shall include the source code for the parts of the
 *  OpenSSL library used as well as that of the covered work.
 */

#include <QSslSocket>
#include <QDomDocument>
#include <QHostInfo>
#include <QTimer>
#include <QtConcurrentRun>
#include <QFuture>
//#include "user.h"
#include "userprivate.h"
#include "utils.h"

// N.B. all data through socket are utf-8
// N.B. we call buddyList contacts
namespace Bressein
{
User::User (QObject *parent) : QObject (parent)
{
    keepAliveTimer = new QTimer (this);
    receiverTimer = new QTimer (this);
    info = new UserInfo (this);
    sipcSocket = new QTcpSocket (this);
    sipcSocket->setReadBufferSize (0);
    sipcSocket->setSocketOption (QAbstractSocket::KeepAliveOption, 1);
    sipcSocket->setSocketOption (QAbstractSocket::LowDelayOption, 1);
    connect (this, SIGNAL (ssiResponseParsed()), SLOT (systemConfig()));
    connect (this, SIGNAL (serverConfigParsed()), SLOT (sipcRegister()));
    connect (this, SIGNAL (sipcRegisterParsed()), SLOT (sipcAuthorize()));
    connect (this, SIGNAL (sipcAuthorizeParsed()), SLOT (activateTimer()));
    conversationManager = new ConversationManager (this);
    connect (conversationManager, SIGNAL (receiveData (const QByteArray &)),
             this, SLOT (parseReceivedData (const QByteArray &)));
    this->moveToThread (&workerThread);
    workerThread.start();
}

User::~User()
{
    if (info)
    {
        qDebug() << "delete info";
        delete info;
    }
    qDebug() << "delete contacts";
    if (not contacts.isEmpty())
    {
        QList<ContactInfo *> list = contacts.values();
        while (not list.isEmpty())
            delete list.takeFirst();
    }
    qDebug() << "delete groups";
    while (not groups.isEmpty())
        delete groups.takeFirst();
    sipcSocket->close();
    sipcSocket->deleteLater();
    if (workerThread.isRunning())
    {
        workerThread.quit();
        workerThread.wait();
    }

}

bool User::operator== (const User &other)
{
    return this->info->fetionNumber == other.info->fetionNumber;
}

/**--------------**/
/** public slots **/
/**--------------**/
void User::login()
{
    qDebug() << "To Login";
    QMetaObject::invokeMethod (this, "ssiLogin");
    // ssiLogin();
}

void User::loginVerify (const QByteArray &code)
{
    info->verification->code = code;
    QMetaObject::invokeMethod (this, "ssiVerify", Qt::QueuedConnection);
}

void User::close()
{
    // note this is called from main thread
    workerThread.quit();
    workerThread.terminate();
}

void User::startChat (const QByteArray &sipUri)
{
    qDebug() << "start Chat called";
    QMetaObject::invokeMethod (this, "inviteFriend",
                               Qt::QueuedConnection,
                               Q_ARG (QByteArray, sipUri));
}

const Bressein::ContactInfo &User::getContactInfo (const QByteArray &sipuri)
{
    return * contacts.value (sipuri);
}

void User::setAccount (QByteArray number, QByteArray password)
{
    // TODO try to load saved configuration for this user
    if (number.isEmpty() or password.isEmpty()) return;
    info->loginNumber = number;
    if (number.size() == 11)
    {
        info->mobileNumber = number;
    }
    else
    {
        info->fetionNumber = number;
    }
    info->password = password;
    info->state = "400";// online
    info->cnonce = cnouce();
    info->callId = 1;
}

const Bressein::Contacts &User::getContacts() const
{
    return contacts;
}

/**---------**/
/** private **/
/**---------**/
void User::sipcWriteRead (QByteArray &in, QByteArray &out, QTcpSocket *socket)
{
    sipcWrite (in, socket);
    sipcRead (out, socket);
}

void User::sipcWrite (const QByteArray &in, QTcpSocket *socket)
{
    qDebug() << "@ sipcWrite" << in;
    if (socket->state() not_eq QAbstractSocket::ConnectedState)
    {
        qDebug() <<  "Error: socket is not connected";
        // TODO error handle
        keepAliveTimer->stop();
        receiverTimer->stop();
    }
    int length = 0;
    while (length < in.length())
    {
        length += socket->write (in.right (in.size() - length));
    }
    socket->waitForBytesWritten ();
    socket->flush();
}

// delimt should either "L: " or "content-Content-Length: ".
void User::sipcRead (QByteArray &out, QTcpSocket *socket, QByteArray delimit)
{
    while (socket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket->waitForReadyRead (10000))
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
    QByteArray responseData = socket->readLine ();
    while (responseData.indexOf ("\r\n\r\n") < 0)
    {
        while (socket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket->waitForReadyRead ())
            {
                qDebug() << "sipcRead 2";
                qDebug() << "When waitForReadyRead"
                         << socket->error() << socket->errorString();
                return;
            }
        }
        responseData.append (socket->readLine ());
    }
    int pos = responseData.indexOf (delimit);
    if (pos < 0)
    {
        out = responseData;
        qDebug() << "no L!";
        return;
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
                qDebug() << "sipcRead 3  waitForReadyRead"
                         << socket->error() << socket->errorString();
                return;
            }
        }
        responseData.append (socket->read (length + pos + 4 - received));
        received = responseData.size();
    }
    out = responseData;
}

/**---------------**/
/** private slots **/
/**---------------**/
void User::keepAlive()
{
    QByteArray toSendMsg = keepAliveData (info->fetionNumber, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    if (responseData.isEmpty())
    {
        qDebug() << "keepAlive: empty return";
        return;
    }
    parseReceivedData (responseData);
}

void User::ssiLogin()
{
    if (info->loginNumber.isEmpty()) return;
    // Do we need to clean up first
    //TODO if proxy
    QByteArray password = hashV4 (info->userId, info->password);
    QByteArray data = ssiLoginData (info->loginNumber, password);
    QSslSocket socket (this);
    socket.connectToHostEncrypted (UID_URI, 443);

    if (not socket.waitForEncrypted (-1))
    {
        // TODO handle socket.error() or inform user what happened
        qDebug() << "waitForEncrypted" << socket.error() <<
                 socket.errorString();
        if (socket.error() == QAbstractSocket::SslHandshakeFailedError)
        {
            ssiLogin();
        }
        return;
    }
    socket.write (data);
    QByteArray delimit = "Content-Length: ";
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket.waitForReadyRead ())
        {
            qDebug() << "When waitForReadyRead"
                     << socket.error() << socket.errorString();
            return;
        }
    }
    QByteArray responseData = socket.readLine ();
    while (responseData.indexOf ("\r\n\r\n") < 0)
    {
        while (socket.bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket.waitForReadyRead ())
            {
                qDebug() << "When waitForReadyRead"
                         << socket.error() << socket.errorString();
                return;
            }
        }
        responseData.append (socket.readLine ());
    }
    int pos = responseData.indexOf (delimit);
    if (pos < 0)
    {
        //TODO
        qDebug() << "ssiLogin";
        qDebug() << "No " << delimit;
        return;
    }
    int pos_ = responseData.indexOf ("\r\n", pos);
    bool ok;
    int length = responseData.mid
                 (pos + delimit.size(), pos_ - pos - delimit.size()).toUInt (&ok);
    if (not ok)
    {
        qDebug() << "Not ok" << responseData;
        return;
    }
    pos = responseData.indexOf ("\r\n\r\n");
    int received = responseData.size();
    while (received < length + pos + 4)
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
        responseData.append (socket.read (length + pos + 4 - received));
        received = responseData.size();
    }
    qDebug() << "=========================";
    qDebug() << "ssiLogin" << responseData;
    qDebug() << "=========================";
    parseSsiResponse (responseData);
}

void User::systemConfig()
{
    qDebug() << "systemConfig";
    QByteArray body = configData (info->loginNumber);
    QTcpSocket socket (this);
    QHostInfo info = QHostInfo::fromName ("nav.fetion.com.cn");
    socket.connectToHost (info.addresses().first().toString(), 80);
    if (not socket.waitForConnected (-1))
    {
        qDebug() << "waitForEncrypted" << socket.errorString();
        return;
    }
    socket.write (body);
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket.waitForReadyRead ())
        {
            qDebug() << "When waitForReadyRead"
                     << socket.error() << socket.errorString();
            return;
        }
    }
    QByteArray responseData = socket.readLine ();
    if (responseData.contains ("HTTP/1.1 200 OK"))
    {
        //TODO handle errors
    }
    while (responseData.indexOf ("\r\n\r\n") < 0)
    {
        while (socket.bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket.waitForReadyRead ())
            {
                qDebug() << "When waitForReadyRead"
                         << socket.error() << socket.errorString();
                return;
            }
        }
        responseData.append (socket.readLine ());
    }
    int pos = responseData.indexOf ("Content-Length: ");
    if (pos < 0)
    {
        //TODO
        qDebug() << "No Content-Length!";
        qDebug() << responseData;
        return;
    }
    int pos_ = responseData.indexOf ("\r\n", pos);
    bool ok;
    int length = responseData.mid (pos + 16, pos_ - pos - 16).toUInt (&ok);
    if (not ok)
    {
        qDebug() << "Not ok" << responseData;
        return;
    }
    pos = responseData.indexOf ("\r\n\r\n");
    int received = responseData.size();
    while (received < length + pos + 4)
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
        responseData.append (socket.read (length + pos + 4 - received));
        received = responseData.size();
    }
//     qDebug() << QString::fromUtf8(responseData);
    parseServerConfig (responseData);
}


void User::downloadPortrait (const QByteArray &sipuri)
{
    //http://hdss1fta.fetion.com.cn/HDS_S00/geturi.aspx
    fetcher.requestPortrait (sipuri);
}



/**
 * R fetion.com.cn SIP-C/4.0
 * F: 916098834
 * I: 1
 * Q: 1 R
 * CN: 1CF1A05B2DD0281755997ADC70F82B16
 * CL: type=”pc” ,version=”3.6.1900″
 **/
void User::sipcRegister()
{
    //TODO move to utils
    int seperator = info->systemconfig.proxyIpPort.indexOf (':');
    QString ip = QString (info->systemconfig.proxyIpPort.left (seperator));
    quint16 port = info->systemconfig.proxyIpPort.mid (seperator + 1).toUInt();
    sipcSocket->connectToHost (ip, port);
    if (not sipcSocket->waitForConnected (-1))     /*no time out*/
    {
        qDebug() << "#sipcRegister waitForConnected"
                 << sipcSocket->errorString();
        return;
    }
    QByteArray toSendMsg ("R fetion.com.cn SIP-C/4.0\r\n");
    toSendMsg.append ("F: ").append (info->fetionNumber).append ("\r\n");
    toSendMsg.append ("I: ").append (QByteArray::number (info->callId++));
    toSendMsg.append ("\r\nQ: 2 R\r\nCN: ");
    toSendMsg.append (info->cnonce).append ("\r\nCL: type=\"pc\" ,version=\"");
    toSendMsg.append (PROTOCOL_VERSION + "\"\r\n\r\n");
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    qDebug() << "^^^^^^";
    qDebug() << responseData;
    qDebug() << "^^^^^^";
    parseSipcRegister (responseData);
}



/**
 * R fetion.com.cn SIP-C/4.0
 * F: 916*098834
 * I: 1
 * Q: 2 R
 * A: Digest response=”5041619..6036118″,algorithm=”SHA1-sess-v4″
 * AK: ak-value
 * L: 426
 * \r\n\r\n
 * <body>
 **/
void User::sipcAuthorize()
{
    qDebug() << "sipcAuthorize";
    if (sipcSocket->state() not_eq QAbstractSocket::ConnectedState)
    {
        printf ("socket closed.");
        return;
    }
    QByteArray toSendMsg = sipcAuthorizeData
                           (info->loginNumber, info->fetionNumber,
                            info->userId, info->callId, info->response,
                            info->client.version,
                            info->client.customConfigVersion,
                            info->client.contactVersion, info->state);
    int length = 0;

    while (length < toSendMsg.length())
    {
        length +=
            sipcSocket->write (toSendMsg.right (toSendMsg.size() - length));
    }
    sipcSocket->waitForBytesWritten (-1);
    sipcSocket->flush();

    while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not sipcSocket->waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
            qDebug() << "sipcAuthorize waitForReadyRead"
                     << sipcSocket->error() << sipcSocket->errorString();
            return;
        }
    }
    // FIXME try use the 'L' attribute to guide to read
    QByteArray responseData = sipcSocket->readLine();
    while (not responseData.contains ("L: "))
    {
        while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (not sipcSocket->waitForReadyRead ())
            {
                // TODO handle socket.error() or inform user what happened
                qDebug() << "ssiLogin  waitForReadyRead"
                         << sipcSocket->error() << sipcSocket->errorString();
                return;
            }
        }
        responseData.append (sipcSocket->readLine ());
    }
    int pos = responseData.indexOf ("L: ");
    if (pos < 0)
    {
        //TODO
        qDebug() << "sipcAuthorize";
        qDebug() << responseData;
        return;
    }
    int pos_ = responseData.indexOf ("\r\n", pos);
    responseData.append (sipcSocket->readLine ());
    bool ok;
    length = responseData.mid (pos + 3, pos_ - pos - 3).toUInt (&ok);
    if (not ok)
    {
        qDebug() << "VVVVV";
        qDebug() << "not ok" << length;
        qDebug() << responseData;
        return;
    }
    while (not responseData.contains ("\r\n\r\n"))
    {
        qDebug() << "eat spaces";
        responseData.append (sipcSocket->read (length));
    }
    pos = responseData.indexOf ("\r\n\r\n");
    int received = responseData.size();
    while (received < length + pos + 4)
    {
        while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (not sipcSocket->waitForReadyRead ())
            {
                // TODO handle socket.error() or inform user what happened
                qDebug() << "ssiLogin  waitForReadyRead"
                         << sipcSocket->error() << sipcSocket->errorString();
                return;
            }
        }
        responseData.append (sipcSocket->read (length + pos + 4 - received));
        received = responseData.size();
    }
    //TODO ensure full of <results/> downloaded
    parseSipcAuthorize (responseData);
}

// functions for parsing data
void User::parseSsiResponse (QByteArray &data)
{
    if (data.isEmpty())
    {
        qDebug() << "Ssi response is empty!";
        // TODO info user about this
        // pop-up dialog ask user if to relogin
        return;
    }
    int b = data.indexOf ("Set-Cookie: ssic=");
    int e = data.indexOf ("; path", b);
    info->ssic = data.mid (b + 17, e - b - 17);
    QByteArray xml = data.mid (data.indexOf ("<?xml version=\"1.0\""));
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine, errorColumn;
    bool ok = domDoc.setContent
              (xml, false, &errorMsg, &errorLine, &errorColumn);
    if (not ok)
    {
        qDebug() << "Failed to parse Ssi response!";
        qDebug() << errorMsg << errorLine << errorColumn;
        qDebug() << xml;
        return;
    }
    domDoc.normalize();
    QDomElement domRoot = domDoc.documentElement();

    if (domRoot.tagName() == "results")
    {
        if (domRoot.isNull() or not domRoot.hasAttribute ("status-code"))
        {
            qDebug() << "Null or  status-code";
            return;
        }
        QString statusCode = domRoot.attribute ("status-code");
        if (statusCode == "421" or statusCode == "420")
        {
            //420 wrong input validation numbers
            //421 requires validation

            if (not info->verification)
            {
                info->verification = new UserInfo::Verfication;
            }
            QDomElement domE =  domRoot.firstChildElement ("verification");
            if (domE.hasAttribute ("algorithm") and
                domE.hasAttribute ("type=") and
                domE.hasAttribute ("text") and
                domE.hasAttribute ("tips"))
            {
                info->verification->algorithm =
                    domE.attribute ("algorithm").toUtf8();
                info->verification->text = domE.attribute ("text").toUtf8();
                info->verification->type = domE.attribute ("type").toUtf8();
                info->verification->tips = domE.attribute ("tips").toUtf8();
                ssiPic();
            }
            return;
        }
        else if (statusCode == "401" or statusCode == "404")
        {
            //401 wrong password
            //TODO inform user that password is wrong
            //404
            //TODO as user to re-input password
            return;
        }
        else if (statusCode == "200")
        {
            QDomElement domChild =  domRoot.firstChildElement ("user");

            if (domChild.hasAttribute ("uri") and
                domChild.hasAttribute ("mobile-no") and
                domChild.hasAttribute ("user-status") and
                domChild.hasAttribute ("user-id"))
            {
                info->sipuri = domChild.attribute ("uri").toUtf8();
                b = info->sipuri.indexOf ("sip:");
                e = info->sipuri.indexOf ("@", b);
                info->fetionNumber = info->sipuri.mid (b + 4, e - b - 4);
                info->mobileNumber = domChild.attribute ("mobile-no").toUtf8();
                // info->state = domE.attribute ("user-status").toUtf8();
                info->userId = domChild.attribute ("user-id").toUtf8();
            }
            else
            {
                qDebug() << "fail to pass ssi response";
                // TODO ask for re-login
                return;
            }
            domChild = domChild.firstChildElement ("credentials");
            if (not domChild.isNull())
            {
                domChild  = domChild.firstChildElement ("credential");

                if (not domChild.isNull() and
                    domChild.hasAttribute ("domain") and
                    domChild.hasAttribute ("c"))
                {
                    info->credential = domChild.attribute ("c").toUtf8();
                }
            }
            emit ssiResponseParsed();
        }
    }
}

void User::parseSipcRegister (QByteArray &data)
{
    if (not data.startsWith ("SIP-C/4.0 401 Unauthoried"))
    {
        qDebug() << "Wrong Sipc register response.";
        qDebug() << data;
        return;
    }
    qDebug() << "parseSipcRegister" << QString::fromUtf8 (data);
    int b, e;
    b = data.indexOf ("\",nonce=\"");
    e = data.indexOf ("\",key=\"", b);
    info->nonce = data.mid (b + 9, e - b - 9);   // need to store?
    b = data.indexOf ("\",key=\"");
    e = data.indexOf ("\",signature=\"", b);
    info->key = data.mid (b + 7, e - b - 7);   // need to store?
    b = data.indexOf ("\",signature=\"");
    e = data.indexOf ("\"", b);
    info->signature = data.mid (b + 14, e - b - 14);
    info->aeskey = QByteArray::fromHex (cnouce (8)).left (32);
    // need to store?
    // generate response
    // check if emptyq
    info->response = RSAPublicEncrypt (info->userId, info->password,
                                       info->nonce, info->aeskey, info->key);
    emit sipcRegisterParsed();
}


void User::parseServerConfig (QByteArray &data)
{
    if (data.isEmpty())
    {
        qDebug() << "Server Configuration response is empty!";
        return;
    }
    QDomDocument domDoc;
    QByteArray xml = data.mid (data.indexOf ("<?xml version=\"1.0\""));
    QString errorMsg;
    int errorLine, errorColumn;
    bool ok = domDoc.setContent
              (xml, false, &errorMsg, &errorLine, &errorColumn);
    if (not ok)
    {
        qDebug() << "Failed to parse server config response!";
        qDebug() << xml;
        return;
    }
    domDoc.normalize();
//     qDebug() << QString::fromUtf8 (domDoc.toByteArray (4));
    QDomElement domRoot = domDoc.documentElement();
    QDomElement domChild;
    if (domRoot.tagName() == "config")
    {
        domRoot = domRoot.firstChildElement ("servers");
        if (not domRoot.isNull() and domRoot.hasAttribute ("version"))
        {
            // process children
            info->systemconfig.serverVersion =
                domRoot.attribute ("version").toUtf8();
            domChild = domRoot.firstChildElement ("sipc-proxy");

            if (not domChild.isNull() and not domChild.text().isEmpty())
            {
                info->systemconfig.proxyIpPort = domChild.text().toUtf8();
            }

            domChild = domRoot.firstChildElement ("get-uri");

            if (not domChild.isNull() and not domChild.text().isEmpty())
            {
                info->systemconfig.serverNamePath = domChild.text().toUtf8();
                int a = info->systemconfig.serverNamePath.indexOf ("//");
                int b = info->systemconfig.serverNamePath.indexOf ("/", a + 2);
                int c = info->systemconfig.serverNamePath.indexOf ("/", b + 1);
                info->systemconfig.portraitServerName =
                    info->systemconfig.serverNamePath.mid (a + 2, b - a - 2);
                info->systemconfig.portraitServerPath =
                    info->systemconfig.serverNamePath.mid (b, c - b);
                fetcher.setData (info->systemconfig.portraitServerName,
                                 info->systemconfig.portraitServerPath,
                                 info->ssic);
            }
        }
        domRoot = domRoot.nextSiblingElement ("parameters");
        if (not domRoot.isNull() and domRoot.hasAttribute ("version"))
        {
            info->systemconfig.parametersVersion =
                domRoot.attribute ("version").toUtf8();
        }
        domRoot = domRoot.nextSiblingElement ("hints");
        if (not domRoot.isNull() and domRoot.hasAttribute ("version"))
        {
            info->systemconfig.hintsVersion =
                domRoot.attribute ("version").toUtf8();
            domChild = domRoot.firstChildElement ("addbuddy-phrases");
            QDomElement domGrand = domChild.firstChildElement ("phrases");
            while (not domGrand.isNull() and domGrand.hasAttribute ("id"))
            {
                info->phrases.append (domGrand.text().toUtf8());
                domGrand.nextSiblingElement ("phrases");
            }
        }
        //when finish, start sip-c
        emit serverConfigParsed();
    }
}


void User::parseSipcAuthorize (QByteArray &data)
{
    //TODO check if statusCode is 200
    // if 420 or 421, inform user to verify // parse_add_buddy_verification
    // if 200 goes following
    // extract :
    // public-ip, last-login-ip,last-login-time,
    // custom-config,contact-list,chat-friends
    // quota-frequency
    if (data.isEmpty())
    {
        qDebug() << "data are empty";
    }
    qDebug() << QString::fromUtf8 (data);
    int b = data.indexOf ("<results>");
    int e = data.indexOf ("</results>");
    QByteArray xml = data.mid (b, e - b + 10);
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine, errorColumn;
    bool ok = domDoc.setContent
              (xml, false, &errorMsg, &errorLine, &errorColumn);
    if (not ok)
    {
        // perhaps need more data from  socket
        qDebug() << "Wrong sipc authorize response";
        return;
    }
    // begin to parse
    domDoc.normalize();
    QDomElement domRoot = domDoc.documentElement();
    QDomElement domChild, domGrand;
    if (domRoot.tagName() == "results")
    {
        domChild = domRoot.firstChildElement ("client");

        if (not domChild.isNull() and domChild.hasAttribute ("public-ip") and
            domChild.hasAttribute ("last-login-time") and
            domChild.hasAttribute ("last-login-ip"))
        {
            info->client.publicIp =
                domChild.attribute ("public-ip").toUtf8();
            info->client.lastLoginIp =
                domChild.attribute ("last-login-ip").toUtf8();
            info->client.lastLoginTime =
                domChild.attribute ("last-login-time").toUtf8();
        }
        domChild = domRoot.firstChildElement ("user-info");
        domChild = domChild.firstChildElement ("personal");
        if (not domChild.isNull())
        {
            QList<QByteArray> attributes;
            attributes << "user-id" << "carrier"//should be CMCC?
                       << "version" << "nickname" << "gender" << "birth-date"
                       << "mobile-no" << "sms-online-status" << "carrier-region"
                       << "carrier-status" << "impresa";
            bool ok = true;
            foreach (const QByteArray& attribute, attributes)
            {
                if (not domChild.hasAttribute (attribute))
                {
                    qDebug() << "error sipc authorize";
                    qDebug() << xml;
                    ok = false;
                    break;
                }
            }
            if (ok)
            {
                info->client.birthDate =
                    domChild.attribute ("birth-date").toUtf8();
                info->client.carrierRegion =
                    domChild.attribute ("carrier-region").toUtf8();
                info->client.carrierStatus =
                    domChild.attribute ("carrier-status").toUtf8();
                info->client.gender = domChild.attribute ("gender").toUtf8();
                info->client.impresa = domChild.attribute ("impresa").toUtf8();
                info->client.mobileNo =
                    domChild.attribute ("mobile-no").toUtf8();
                info->client.nickname =
                    domChild.attribute ("nickname").toUtf8();
                info->client.smsOnLineStatus =
                    domChild.attribute ("sms-online-status").toUtf8();
                info->client.version = domChild.attribute ("version").toUtf8();
            }
        }
        domChild = domChild.nextSiblingElement ("custom-config");
        if (not domChild.isNull() and domChild.hasAttribute ("version"))
        {
            info->client.customConfigVersion =
                domChild.attribute ("version").toUtf8();
            //FIXME is that a xml segment?

            if (not domChild.text().isEmpty())
                info->client.customConfig = domChild.text().toUtf8();
        }
        domChild = domChild.nextSiblingElement ("contact-list");

// < contact-list version = "363826296" >
//   < buddy-lists >
//        < buddy-list id = "1" name = "我的好友" / >
//   < / buddy-lists >
//   <buddies>
//         < b f="0" i="999999999" l="1" n="" o="0" p="identity=1;"
//            r="1" u="sip:777777777@fetion.com.cn;p=4444" / >
//         < b f="0" i="888888888" l="0" n="" o="0" p="identity=1;"
//            r="2" u="sip:666666666@fetion.com.cn;p=3333" / >
//   < / buddies >
//   <chat-friends / >
//   < blacklist / >
//< / contact - list >
// in buddies,

// i: userId,  n:local name
// l: group id, p: identity
// r; relationStatus, u: sipuri
//what about o

        if (not domChild.isNull() and domChild.hasAttribute ("version"))
        {
            //TODO if the version is the same as that in local cache, means
            // the list is the same too.
            info->client.contactVersion =
                domChild.attribute ("version").toUtf8();
            domGrand = domChild.firstChildElement ("buddy-lists");
            if (not domGrand.isNull())
            {
                domGrand = domGrand.firstChildElement ("buddy-list");
                while (not domGrand.isNull() and
                       domGrand.hasAttribute ("id") and
                       domGrand.hasAttribute ("name"))
                {
                    Group *group = new Group;
                    group->groupId = domGrand.attribute ("id").toUtf8();
                    group->groupname = domGrand.attribute ("name").toUtf8();
                    groups.append (group);
                    domGrand = domGrand.nextSiblingElement ("buddy-list");
                }
            }
            domGrand = domChild.firstChildElement ("buddies");
            if (not domGrand.isNull())
            {
                domGrand = domGrand.firstChildElement ("b");
                QList<QByteArray> attributes;
                attributes << "i" << "l" << "n" << "p" << "r" << "u";
                bool ok = true;
                while (not domGrand.isNull())
                {
                    ok = true;
                    foreach (const QByteArray& attribute, attributes)
                    {
                        if (not domGrand.hasAttribute (attribute))
                        {
                            qDebug() << "error sipc authorize";
                            qDebug() << xml;
                            qDebug() << attribute;
                            ok = false;
                            break;
                        }
                    }
                    if (ok)
                    {
                        ContactInfo *contact = new ContactInfo;
                        contact->basic.userId =
                            domGrand.attribute ("i").toUtf8();
                        contact->basic.groupId =
                            domGrand.attribute ("l").toUtf8();
                        contact->basic.localName =
                            domGrand.attribute ("n").toUtf8();
//                         contact-> = domGrand.attribute ("o").toUtf8();
                        contact->basic.identity =
                            domGrand.attribute ("p").toUtf8();
                        contact->basic.relationStatus =
                            domGrand.attribute ("r").toUtf8();
                        QByteArray sipuri = domGrand.attribute ("u").toUtf8();
                        contacts.insert (sipuri, contact);
                    }
                    domGrand = domGrand.nextSiblingElement ("b");
                }
            }
        }
        domChild = domChild.nextSiblingElement ("quotas");
        if (not domChild.isNull())
        {
            domChild = domChild.firstChildElement ("quota-frequency");

            if (not domChild.isNull())
            {
                domChild = domChild.firstChildElement ("frequency");

                if (not domChild.isNull() and
                    domChild.hasAttribute ("day-limit") and
                    domChild.hasAttribute ("day-count") and
                    domChild.hasAttribute ("month-limit") and
                    domChild.hasAttribute ("month-count"))
                {
                    info->client.smsDayLimit =
                        domChild.attribute ("day-limit").toUtf8();
                    info->client.smsDayCount =
                        domChild.attribute ("day-count").toUtf8();
                    info->client.smsMonthCount =
                        domChild.attribute ("month-limit").toUtf8();
                    info->client.smsMonthCount =
                        domChild.attribute ("month-count").toUtf8();
                }
            }
        }
        emit sipcAuthorizeParsed();
    }
}

void User::ssiPic()
{
    QByteArray data = SsiPicData (info->verification->algorithm, info->ssic);
    QTcpSocket socket;
    QHostInfo hostinfo = QHostInfo::fromName ("nav.fetion.com.cn");
    socket.connectToHost (hostinfo.addresses().first().toString(), 80);

    if (not socket.waitForConnected (-1))
    {
        //TODO handle exception here
        qDebug() << "waitForEncrypted" << socket.errorString();
        return;
    }
    socket.write (data);
    QByteArray responseData;
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket.waitForReadyRead (-1))
        {
            qDebug() << "ssiLogin  waitForReadyRead"
                     << socket.error() << socket.errorString();
        }
        responseData += socket.readAll();
    }
    // extract code
    if (responseData.isEmpty())
    {
        // TODO
        return;
    }
    int b = responseData.indexOf ("<results>");
    int e = responseData.indexOf ("</results>");
    QByteArray xml = responseData.mid (b, e - b + 10);
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine, errorColumn;
    bool ok = domDoc.setContent
              (xml, false, &errorMsg, &errorLine, &errorColumn);
    if (not ok)
    {
        // perhaps need more data from  socket
        qDebug() << "Wrong response";
        return;
    }
    QDomElement domRoot = domDoc.documentElement();
    if (domRoot.tagName() == "results")
    {
        domRoot = domRoot.firstChildElement ("pic-certificate");
        if (not domRoot.isNull() and domRoot.hasAttribute ("id") and
            domRoot.hasAttribute ("pic"))
        {
            info->verification->id = domRoot.attribute ("id").toUtf8();
            info->verification->pic = domRoot.attribute ("pic").toUtf8();
            // base-64 encoded
        }
        // emit signal to inform user to inout code
    }
    else
    {
        // TODO
    }

}

void User::ssiVerify()
{
    QByteArray password = (hashV4 (info->userId, info->password));
    QByteArray data = ssiVerifyData (info->loginNumber, password,
                                     info->verification->id,
                                     info->verification->code,
                                     info->verification->algorithm);
    QSslSocket socket (this);
    socket.connectToHostEncrypted (UID_URI, 443);

    if (not socket.waitForEncrypted (-1))
    {
        // TODO handle socket.error() or inform user what happened
        qDebug() << "waitForEncrypted" << socket.errorString();
        //             ssiLogin();
        return;
    }
    socket.write (data);
    QByteArray responseData;
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket.waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
            qDebug() << "ssiLogin  waitForReadyRead"
                     << socket.error() << socket.errorString();
        }
    }
    responseData = socket.readAll();
    parseSsiResponse (responseData);
}

void User::contactInfo (const QByteArray &userId)
{
    QByteArray toSendMsg = contactInfoData
                           (info->fetionNumber, userId, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    //TODO handle responseData
    // carrier-region
}

void User::contactInfo (const QByteArray &Number, bool mobile)
{
    QByteArray toSendMsg = contactInfoData
                           (info->fetionNumber, Number, info->callId, mobile);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    //TODO handle responseData
    // carrier-region
}


void User::sendMessage (const QByteArray &toSipuri, const QByteArray &message)
{
    QByteArray toSendMsg = catMsgData
                           (info->fetionNumber, toSipuri, info->callId, message);
    QByteArray responseData;
    // if a contact whose sipuri is toSipuri is already on conversation
    // that is its socket is non-null
    if (contacts.value (toSipuri)->basic.state == StateType::ONLINE and
        not conversationManager->isOnConversation (toSendMsg))
    {
        inviteFriend (toSipuri);
    }
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    // TODO handle responseData and do record
    // create dataBase instance
}

void User::addBuddy (
    const QByteArray &number,
    QByteArray buddyLists,
    QByteArray localName,
    QByteArray desc,
    QByteArray phraseId)
{
    QByteArray toSendMsg = addBuddyV4Data (info->fetionNumber, number,
                                           info->callId, buddyLists, localName,
                                           desc, phraseId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);;
    // TODO handle responseData
}

void User::deleteBuddy (const QByteArray &userId)
{
    QByteArray toSendMsg = deleteBuddyV4Data
                           (info->fetionNumber, userId, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    // TODO handle responseData
}

void User::contactSubscribe()
{
    QByteArray toSendMsg = presenceV4Data (info->fetionNumber, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    onReceiveData();
}

// move another thread -- conversation
void User::inviteFriend (const QByteArray &sipUri)
{
//     downloadPortrait (sipUri);
    QByteArray toSendMsg = startChatData (info->fetionNumber, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    if (responseData.isEmpty() or not responseData.contains ("A: CS address="))
    {
        qDebug() << "startChat";
        qDebug() << responseData;
        return;
    }
    int b, e;
    b = responseData.indexOf ("A: CS address=\"");
    e = responseData.indexOf (";", b);
    QByteArray address = responseData.mid (b + 15, e - b - 15);
    b = responseData.indexOf ("credential=\"");
    e = responseData.indexOf ("\"", b + 12);
    QByteArray credential = responseData.mid (b + 12, e - b - 12);
    int seperator = address.indexOf (':');
    QByteArray ip = address.left (seperator);
    quint16 port = address.mid (seperator + 1).toUInt();
    qDebug() << "registerData";
    toSendMsg = registerData (info->fetionNumber, info->callId, credential);
    // append conversation to conversation manager
    /** **/
    conversationManager->addConversation (sipUri);
    conversationManager->setHost (sipUri, ip, port);
    conversationManager->sendData (sipUri, toSendMsg);
    toSendMsg = inviteBuddyData (info->fetionNumber, info->callId, sipUri);
    conversationManager->sendData (sipUri, toSendMsg);

    // TODO check if 200
    //FIXME Check contact correctly
//     QByteArray message ("Hi!");
//     toSendMsg = catMsgData
//             (info->fetionNumber, sipUri, info->callId, message);
//     responseData.clear();
//     sipcWriteRead (toSendMsg, responseData, sipsocket);
//     qDebug() << responseData;
// demo only
    toSendMsg = catMsgData (info->fetionNumber, sipUri, info->callId,
                            "Hello, This is from Bressein!");
    sipcWrite (toSendMsg, sipcSocket);

}

void User::createBuddylist (const QByteArray &name)
{
    QByteArray toSendMsg = createBuddyListData
                           (info->fetionNumber, info->callId, name);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    // TODO handle responseData, get version, name, id
}

void User::deleteBuddylist (const QByteArray &id)
{
    QByteArray toSendMsg = deleteBuddyListData
                           (info->fetionNumber, info->callId, id);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    // TODO handle responseData, get version, name, id
}

void User::renameBuddylist (const QByteArray &id, const QByteArray &name)
{
    QByteArray toSendMsg = setBuddyListInfoData
                           (info->fetionNumber, info->callId, id, name);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    // TODO
}

void User::updateInfo()
{
    QByteArray toSendMsg = setUserInfoV4Data (info->fetionNumber, info->callId,
                                              info->client.impresa,
                                              info->client.nickname,
                                              info->client.gender,
                                              info->client.customConfig,
                                              info->client.customConfigVersion);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    // TODO handle responseData
}

void User::setImpresa (const QByteArray &impresa)
{
    QByteArray toSendMsg = setUserInfoV4Data (info->fetionNumber, info->callId,
                                              impresa, info->client.version,
                                              info->client.customConfig,
                                              info->client.customConfigVersion);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    // TODO handle responseData
}

void User::setMessageStatus (int days)
{
    QByteArray toSendMsg = setUserInfoV4Data
                           (info->fetionNumber, info->callId, days);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
}

void User::setClientState (StateType state)
{
    QByteArray statetype = QByteArray::number ( (int) state);
    QByteArray toSendMsg = setPresenceV4Data
                           (info->fetionNumber, info->callId, statetype);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
}

void User::activateTimer()
{
    connect (keepAliveTimer, SIGNAL (timeout()), this, SLOT (keepAlive()));
    keepAliveTimer->start (30000);//
    contactSubscribe();
    QTimer::singleShot (3000, this, SLOT (onReceiveData()));
    connect (receiverTimer, SIGNAL (timeout()),
             this, SLOT (onConversationsCheck()));
    receiverTimer->start (3000);
}


void User::onReceiveData()
{
    QByteArray responseData;
    qDebug() << "onReceiveData()";
    sipcRead (responseData, sipcSocket);
    if (responseData.isEmpty())
    {
        QTimer::singleShot (3000, this, SLOT (onReceiveData()));
        return;
    }
    parseReceivedData (responseData);
}

void User::parseReceivedData (const QByteArray &responseData)
{
    //TODO wrap as parseData;
    // case 1
    /**
     * BN xxxxyyyy SIP-C/4.0
     * N: SyncUserInfoV4
     * L: 144
     * I: 2
     * Q: 1 BN
     *
     * <events><event type="SyncUserInfo"><user-info><quotas><quota-sendsms>
     * <sendsms count="83"/></quota-sendsms></quotas></user-info></event></events>
     **/
    /**
     *    B N xxxxyyyy* SIP-C/4.0
     *    N: contact
     *    I: 2
     *    Q: 18 BN
     *    L: 211
     *
     *    <events><event type="AddBuddyApplication">
     *    <application uri="sip:XXXXXXX@fetion.com.cn;p=4162" desc="dsdsd"
     *    type="1" time="2011-08-06 12:35:28" addbuddy-phrase-id="0"
     *    user-id="655196756"/></event></events>"
     */
    // case 2 MSG
    /**
     *   M 577240426 SIP-C/4.0
     *   F: sip:598264381@fetion.com.cn;p=2234
     *   I: -5
     *   Q: 6 M
     *   K: SaveHistory
     *   K: NewEmotion
     *   C: text/html-fragment
     *   L: 65
     *   D: Sat, 03 Aug 2011 07:49:48 GMT
     *   XI: 783E4B86FF5ACA6FBE0B1A28B20C6A848
     */
    int space = responseData.indexOf (" ");
    QByteArray code = responseData.left (space);
    qDebug() << "code" << code;
    if (code == "M")
    {
        // handle Message
        onReceivedMessage (responseData);
    }
    else if (code == "BN")
    {
        int b = responseData.indexOf ("N: ");
        int e = responseData.indexOf ("\r\n", b);
        QByteArray event = responseData.mid (b + 3, e - b - 3);
        if (event == "PresenceV4")
        {
            onBNPresenceV4 (responseData);
        }
        else if (event == "SyncUserInfoV4")
        {
            onBNSyncUserInfoV4 (responseData);
        }
        else if (event == "Conversation")
        {
            onBNConversation (responseData);
        }
        else if (event == "contact")
        {
            onBNcontact (responseData);
        }
        else if (event == "registration")
        {
            onBNregistration (responseData);
        }
        else if (event == "PGGroup")
        {
            onBNPGGroup (responseData);
        }
    }
    else if (code == "I")
    {
        onInvite (responseData);
    }
    else if (code == "IN")
    {
        onIncoming (responseData);
    }
    else if (code == "O")
    {

    }
    else if (code == "SIP-C/4.0")
    {
        qDebug() << responseData;
    }
    else
    {

    }
    QTimer::singleShot (3000, this, SLOT (onReceiveData()));
    // blocking?
}

void User::onReceivedMessage (const QByteArray &data)
{
    // save to dataBase
    // do reply
    int b = data.indexOf ("F: ");
    int e = data.indexOf ("\r\n", b);
    QByteArray fromSipuri = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("I: ");
    e = data.indexOf ("\r\n", b);
    QByteArray callId = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("Q: ");
    e = data.indexOf ("\r\n", b);
    QByteArray sequence = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("D: ");
    e = data.indexOf ("\r\n", b);
    QByteArray datetime = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("\r\n\r\n");
    QByteArray content = data.mid (b + 4);
    //TODO save {fromSiprui, datetime, content} into dataBase
    // if fromSipuri contains PG, but work for no PG
    QByteArray reply = "SIP-C/4.0 200 OK\r\nI: ";
    reply.append (callId);
    reply.append ("\r\nQ: ");
    reply.append (sequence);
    reply.append ("\r\nF: ");
    reply.append (fromSipuri);
    reply.append ("\r\n\r\n");
    // ?fromsiprui , callid , sequence
    QByteArray responseData;
    qDebug() << "to reply " << reply;
    sipcWriteRead (reply, responseData, sipcSocket);
}

void User::onBNPresenceV4 (const QByteArray &data)
{
    qDebug() << "onBNPresenceV4 >>>>";
    qDebug() << QString::fromUtf8 (data);
    qDebug() << "onBNPresenceV4 <<<<";
    int b = data.indexOf ("\r\n\r\n");
    QByteArray xml = data.mid (b + 4);
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine, errorColumn;
    bool ok = domDoc.setContent
              (xml, false, &errorMsg, &errorLine, &errorColumn);
    if (not ok)
    {
        qDebug() << "Failed onBNPresenceV4!";
        qDebug() << errorMsg << errorLine << errorColumn;
        qDebug() << xml;
        return;
    }
    domDoc.normalize();
    QDomElement domRoot = domDoc.documentElement();
    if (domRoot.tagName() not_eq "events")
    {
        qDebug() << "onBNPresenceV4 " << domRoot.tagName();
        return;
    }
    domRoot = domRoot.firstChildElement ("event");
    if (not domRoot.isNull() and domRoot.hasAttribute ("type") and
        domRoot.attribute ("type") == "PresenceChanged")
    {
        domRoot = domRoot.firstChildElement ("contacts");
        if (not domRoot.isNull())
        {
            QDomElement child;
            domRoot = domRoot.firstChildElement ("c");
            QByteArray sipuri;
            ContactInfo *contactInfo;
            while (not domRoot.isNull())
            {
                QByteArray userId = domRoot.attribute ("id").toUtf8();
                child = domRoot.firstChildElement ("p");
                if (child.isNull() or not child.hasAttribute ("su"))
                {
                    goto next;
                }
                sipuri = child.attribute ("su").toUtf8();
                if (contacts.contains (sipuri))
                {
                    contactInfo = contacts.value (sipuri);
                }
                else
                {
                    qDebug() << "no contact info" << sipuri;
                    goto next;
                }
                if (not contactInfo)
                {
                    goto next;
                }
                contactInfo->basic.userId = userId;
                contactInfo->detail.mobileno = child.attribute ("m").toUtf8();
                contactInfo->detail.nickName = child.attribute ("n").toUtf8();
                contactInfo->basic.imprea = child.attribute ("i").toUtf8();
                contactInfo->detail.scoreLevel = child.attribute ("l").toUtf8();
                // FIXME
//             contactInfo->detail.imageChanged = child.attribute("p").toUtf8();
                contactInfo->detail.carrier = child.attribute ("c").toUtf8();
                contactInfo->detail.carrierStatus =
                    child.attribute ("cs").toUtf8();
                contactInfo->detail.serviceStatus =
                    child.attribute ("s").toUtf8();
                // go to <pr/>
                child = child.nextSiblingElement ("pr");
                if (child.isNull() or not child.hasAttribute ("dt"))
                {
                    return;
                }
                contactInfo->basic.devicetype = child.attribute ("dt").toUtf8();
                contactInfo->basic.state =
                    (StateType) child.attribute ("b").toInt();
                contacts.insert (sipuri, contactInfo);

                qDebug() << "Contact Changed";
                static int contactCount = 0;
                qDebug() << "contactCount" << ++contactCount;
                emit contactChanged (sipuri);
                // TODO inform conversationManager to update state,
                // that is, to remove if OFFLINE
            next:
                domRoot = domRoot.nextSiblingElement ("c");
            }
        }
    }
}


void User::onBNConversation (const QByteArray &data)
{
    //<events><event type="UserFailed"><member uri="sip:xxxxx@fetion.com.cn;p=aaaa" status="502"/></event></events>
    // when UserEntered, UserFailed, UserLeft, we destroy the conversation corresponding.
    qDebug() << data;
    // if UserLeft, get node member, if ok, get property uri, then remove
    // inform conversationManager to remove this sipuri (uri).
}

void User::onBNSyncUserInfoV4 (const QByteArray &data)
{
    qDebug() << data;
    // TODO
    // get node "buddy", if ok, process the data
    // get properties action, (if action is add) user-id, uri, relation-status
    //
}

void User::onBNcontact (const QByteArray &data)
{
    qDebug() << data;
    // TODO
    // get node "application", if ok, process the data
    // get properties uri, user-id, desc, addbuddy-phrase-id
    // inform to create new chat room
}

void User::onBNregistration (const QByteArray &data)
{
    // TODO
    // if event type is deregistered, we nnforced to quit, reset all state
    qDebug() << data;
}

void User::onBNPGGroup (const QByteArray &data)
{
    qDebug() << data;
    // TODO
    // if event is PGGetGroupInfo
    // for each node group, get properties uri, status-code, name, category,
    // current-member-count, limit-member-count, group-rank, max-rank, bulletin,
    // introduce, create-time, get-group-portrait-hds, then update PGgroup info.

    // else if PresenceChanged
    // FIXME
    // get node ?, get some properties uri, iicnickname, identity, user-id
}

void User::onInvite (const QByteArray &data)
{
    /**
     I xxxxYYYY SIP-C/4.0                     *
     F: sip:zzzzzzz@fetion.com.cn;p=4162
     I: -22
     K: text/plain
     K: text/html-fragment
     K: multiparty
     K: nudge
     XI: 2ad4f8d1653943d0ac41e0ade23a88ff
     L: 109
     Q: 200002 I
     AL: buddy
     A: CS address="221.176.31.104:8080;221.176.31.104:443",
     credential="449926111.1164956554"

     s=session
     m=message
     a=user:sip:zzzzzzz@fetion.com.cn;p=4162
     a=user:sip:xxxxYYYY@fetion.com.cn;p=3010
     **/
    int b = data.indexOf ("F: ");
    if (b < 0) return;
    int e = data.indexOf ("\r\n", b);
    QByteArray fromSipuri = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("I: ");
    e = data.indexOf ("\r\n", b);
    QByteArray callId = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("Q: ");
    e = data.indexOf ("\r\n", b);
    QByteArray sequence = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("A: CS address=\"");
    e = data.indexOf (";", b);
    QByteArray address = data.mid (b + 15, e - b - 15);
    b = data.indexOf ("credential=\"");
    e = data.indexOf ("\"", b + 12);
    QByteArray credential = data.mid (b + 12, e - b - 12);
    int seperator = address.indexOf (':');
    QByteArray ip = address.left (seperator);
    quint16 port = address.mid (seperator + 1).toUInt();
    conversationManager->addConversation (fromSipuri);
    // do reply via sipcSocket
    QByteArray reply = "SIP-C/4.0 200 OK\r\nF: ";
    reply.append (fromSipuri);
    reply.append ("\r\nI: ");
    reply.append (callId);
    reply.append ("\r\nQ: ");
    reply.append (sequence);
    reply.append ("\r\n\r\n");
    sipcWrite (reply, sipcSocket);
    QByteArray toSendMsg =
        registerData (info->fetionNumber, info->callId, credential);
    conversationManager->setHost (fromSipuri, ip, port);
    conversationManager->sendData (fromSipuri, toSendMsg);
}

void User::onIncoming (const QByteArray &data)
{
    qDebug() << data;
    // TODO
    // if has node share-content, get properties action,
    // if action is accept, ...
    // else if cancel

    // if has node is-composing.. unknown

    // if has node either nudge or input, reply:
    /*
    Q ByteArray reply = "SIP-C/4.0 *200 OK\r\nF: ";
    reply.append (fromSipuri);
    reply.append ("\r\nI: ");
    reply.append (callId);
    reply.append ("\r\nQ: ");
    reply.append (sequence);
    reply.append ("\r\n\r\n");
    sipcWrite (reply, sipcSocket);
    */
    // if nudge, inform conversationManager to activate or create new
    // conversation and then call Bressein open a chat room
    // if input, then inform user that remote is inputting?
}

void User::onSIPC (const QByteArray &data)
{
    // in openfetion, message is distinguished into by challId.
    // try
    // 1. if callId is pgGroupCallId
    // pg_group_parse_list
    // for each node group, get properties uri, identity, insert PGgroup.
    // 2. if callId is ulist->message->callid --
    // TODO add Qlist to store message
    // store tosend message and remove it if acked, and then save to dataBase
    // if the callId matches unacked msg in list, then remove it from list
    // 3. pggroup is non-empty, foreach pg in pggroup,
    // if pg'inviteCallId matches this callId.(ensure 200 OK).
    // and pg NOT hasAcked, do ack and set hasAcked true. :
    // Do ack:
    // set info->callId to this callId
    // write inviteAckData to server

    // else if pg'getMemberCallId matches this callId, then parse member list
    // and subscribe
}

void User::parsePGGroupMembers (const QByteArray &data)
{
    int b = data.indexOf ("\r\n\r\n");
    QByteArray xml = data.mid (b + 4);
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine, errorColumn;
    bool ok = domDoc.setContent
              (xml, false, &errorMsg, &errorLine, &errorColumn);
    if (not ok)
    {
        qDebug() << "Failed parsePGGroupMembers!";
        qDebug() << errorMsg << errorLine << errorColumn;
        qDebug() << xml;
        return;
    }
    domDoc.normalize();
    QDomElement domRoot = domDoc.documentElement();
    //TODO dunno the structure!
    // from openfetion, there are some nodes with properties like:
    // uri, iicnickname, identity,user-id
}

// TODO move to another thread
void User::onConversationsCheck()
{
    /*
        QTcpSocket *socket;
        for (int i = 0, s = sockets.size(); i < s; ++i)
        {
            qDebug() << "onConversationsCheck";
            socket = sockets.at (i);
            if (socket->state() not_eq QAbstractSocket::ConnectedState)
            {
                printf ("socket closed.");
                break;
            }
            while (socket->bytesAvailable() < (int) sizeof (quint16))
            {
                if (not socket->waitForReadyRead (3000))
                {
                    qDebug() << "onConversationsCheck";
                    qDebug() << "When waitForReadyRead"
                             << sipcSocket->error() << sipcSocket->errorString();
                    break;
                }
            }
            QByteArray responseData = socket->readLine();
            qDebug() << responseData;
            while (not responseData.contains ("L: "))
            {
                while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
                {
                    if (not sipcSocket->waitForReadyRead ())
                    {
                        // TODO handle socket.error() or inform user what happened
                        qDebug() << "onConversationsCheck";
                        qDebug() << "When waitForReadyRead"
                                 << sipcSocket->error() << sipcSocket->errorString();
                        return;
                    }
                }
                responseData.append (sipcSocket->readLine ());
            }
            responseData.append (sipcSocket->readLine ());
            int pos = responseData.indexOf ("L: ");

            if (pos < 0)
            {
                qDebug() << "onConversationsCheck";
                qDebug() << responseData;
                break;
            }
            int pos_ = responseData.indexOf ("\r\n", pos);
            bool ok;
            int length = responseData.mid (pos + 3, pos_ - pos - 3).toUInt (&ok);
            if (not ok)
            {
                qDebug() << "DDDD";
                qDebug() << "not ok" << length;
                qDebug() << responseData;
                break;
            }
            int received = responseData.size();
            pos = responseData.indexOf ("\r\n\r\n");
            while (received < length + pos + 4)
            {
                while (socket->bytesAvailable() < (int) sizeof (quint16))
                {
                    if (not socket->waitForReadyRead ())
                    {
                        // TODO handle socket.error() or inform user what happened
                        qDebug() << "onConversationsCheck";
                        qDebug() << "When waitForReadyRead"
                                 << socket->error() << socket->errorString();
                        return;
                    }
                }
                responseData.append (socket->read (length + pos + 4 - received));
                received = responseData.size();
            }
            qDebug() << "onConversationsCheck";
            qDebug() << QString::fromUtf8 (responseData);
        }*/
}

} // end Bressein

#include "user.moc"

