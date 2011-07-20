/*
    The implement of User
    Copyright (C) 2011  颜烈彬 <slbyan@gmail.com>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <QSslSocket>
#include <QDomDocument>
#include <QHostInfo>
#include "user.h"
#include "userinfo.h"
#include "utils.h"
// N.B all data through socket are utf-8

namespace Bressein
{
    User::User (QByteArray number, QByteArray password)
    {
        sipcSocket = new QTcpSocket (this);
        connect (this, SIGNAL (serverConfigGot()), this, SLOT (sipcRegister()));
        connect (this, SIGNAL (ssiResponseDone()), this, SLOT (getServerConfig()));
        initialize (number, password);
    }

    User::~User()
    {

    }

    bool User::operator== (const User & other)
    {
        return this->info->fetionNumber == other.info->fetionNumber;
    }

//public slots
    void User::setState (StateType&)
    {

    }

    void User::login()
    {
        ssiLogin();
    }

    void User::keepAlive()
    {

    }

    void User::sendMsg (QByteArray& fetionId, QByteArray& message)
    {

    }

    void User::addBuddy (QByteArray& number, QByteArray& info)
    {

    }

//private
    void User::initialize (QByteArray number, QByteArray password)
    {
        // TODO try to load saved configuration for this user
        info = new UserInfo;
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
    }

    void User::ssiLogin()
    {
        //TODO if proxy
        QByteArray password = (hashV4 (info->userId, info->password));
        QByteArray data = ssiLoginData (info->loginNumber, password);
        qDebug() << "Ssi login data";
        qDebug() << data;
        QSslSocket socket;
        socket.connectToHostEncrypted ("uid.fetion.com.cn", 443);

        if (!socket.waitForEncrypted())
        {
            qDebug() << "waitForEncrypted" << socket.errorString();
//             ssiLogin();
            return;
        }

        socket.write (data);

        QByteArray responseData;

        while (socket.waitForReadyRead (-1))
        {
            responseData = socket.readAll();
        }

        handleSsiResponse (responseData);
    }

    void User::handleSsiResponse (QByteArray data)
    {
        if (data.isEmpty())
        {
            qDebug() << "Ssi response is empty!!";
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
        bool ok = domDoc.setContent (xml, false, &errorMsg, &errorLine, &errorColumn);
        qDebug() << ok;

        if (!ok)
        {
            qDebug() << "Failed to parse Ssi response!!";
            qDebug() << errorMsg << errorLine << errorColumn;
            qDebug() << xml;
            return;
        }

        domDoc.normalize();

        QDomElement domRoot = domDoc.documentElement();

        if (domRoot.tagName() == "results")
        {
            if (domRoot.isNull() || !domRoot.hasAttribute ("status-code"))
            {
                qDebug() << "Null or  status-code";
                return;
            }

            QString statusCode = domRoot.attribute ("status-code");

            if (statusCode == "421" || statusCode == "420")
            {
                //420 wrong input validation numbers
                //421 requires validation
                qDebug() << "Authorization needs confirm";

                if (!info->verfication)
                {
                    info->verfication = new UserInfo::Verfication;
                }

                QDomElement domE =  domRoot.firstChildElement ("verification");

                if (domE.hasAttribute ("algorithm") &&
                    domE.hasAttribute ("type=") &&
                    domE.hasAttribute ("text") &&
                    domE.hasAttribute ("tips"))
                {
                    info->verfication->algorithm = domE.attribute ("algorithm").toUtf8();
                    info->verfication->text = domE.attribute ("text").toUtf8();
                    info->verfication->type = domE.attribute ("type").toUtf8();
                    info->verfication->tips = domE.attribute ("tips").toUtf8();
                }

                // TODO re-login with verification
                return;
            }
            else if (statusCode == "401" || statusCode == "404")
            {
                //401 wrong password
                qDebug() << "Authorization error";
                //TODO as user to re-input password
                return;
            }
            else if (statusCode == "200")
            {
                QDomElement domE =  domRoot.firstChildElement ("user");

                if (domE.hasAttribute ("uri") &&
                    domE.hasAttribute ("mobile-no") &&
                    domE.hasAttribute ("user-status") &&
                    domE.hasAttribute ("user-id"))
                {
                    info->sipuri = domE.attribute ("uri").toUtf8();
                    b = info->sipuri.indexOf ("sip:");
                    e = info->sipuri.indexOf ("@", b);
                    info->fetionNumber = info->sipuri.mid (b + 4, e - b - 4);
                    info->mobileNumber = domE.attribute ("mobile-no").toUtf8();
                    info->state = domE.attribute ("user-status").toUtf8();
                    info->userId = domE.attribute ("user-id").toUtf8();
                }

                domE = domE.firstChildElement ("credentials");

                if (!domE.isNull())
                {
                    domE  = domE.firstChildElement ("credential");

                    if (!domE.isNull() && domE.hasAttribute ("domain") && domE.hasAttribute ("c"))
                    {
                        info->credential = domE.attribute ("c").toUtf8();
                    }
                }

                emit ssiResponseDone();

                //TODO emit signal here to invoke getServerConfig
//                 getServerConfig();
            }
        }
    }

    void User::getServerConfig()
    {
        QByteArray body = configData (info->loginNumber);
        qDebug() << "getServerConfig " << body;
        QTcpSocket socket;
        QHostInfo info = QHostInfo::fromName ("nav.fetion.com.cn");
        socket.connectToHost (info.addresses().first().toString(), 80);

        if (!socket.waitForConnected())
        {
            qDebug() << "waitForEncrypted" << socket.errorString();
            return;
        }

        socket.write (body);

        QByteArray responseData;

        while (socket.waitForReadyRead (-1))
        {
            responseData += socket.readAll();
        }

        if (socket.isWritable()&& !socket.isReadable())
            parseServerConfig (responseData);
    }

    void User::parseServerConfig (QByteArray data)
    {
        if (data.isEmpty())
        {
            qDebug() << "Server Configuration response is empty!!";
            return;
        }

        QDomDocument domDoc;

        if (!domDoc.setContent (data))
        {
            qDebug() << "Failed to parse server config response!!";
            qDebug() << data;
            return;
        }

        QDomElement domRoot = domDoc.documentElement();

        if (domRoot.tagName() == "config")
        {
            domRoot = domRoot.firstChildElement ("servers");

            if (!domRoot.isNull() && domRoot.hasAttribute ("version"))
                info->systemconfig.serverVersion = domRoot.attribute ("version").toUtf8();

            domRoot = domRoot.nextSiblingElement ("parameters");

            if (!domRoot.isNull() && domRoot.hasAttribute ("version"))
                info->systemconfig.parametersVersion = domRoot.attribute ("version").toUtf8();

            domRoot = domRoot.nextSiblingElement ("hints");

            if (!domRoot.isNull() && domRoot.hasAttribute ("version"))
                info->systemconfig.hintsVersion = domRoot.attribute ("version").toUtf8();

            //TODO get phrase here

            domRoot = domRoot.nextSiblingElement ("sipc-proxy");

            if (!domRoot.text().isEmpty())
                info->systemconfig.proxyIpPort = domRoot.text().toUtf8();

            domRoot = domRoot.nextSiblingElement ("get-uri");

            if (!domRoot.text().isEmpty())
                info->systemconfig.serverNamePath = domRoot.text().toUtf8();

            //when finish, start sip-c
            emit serverConfigGot();
        }
    }

    void User::sipcRegister()
    {
        sipcSocket->setReadBufferSize (0);

        int seperator = info->systemconfig.proxyIpPort.indexOf (':');
        QString ip = QString (info->systemconfig.proxyIpPort.left (seperator));
        quint16 port = info->systemconfig.proxyIpPort.mid (seperator).toUInt();
        sipcSocket->connectToHost (ip, port);

        if (!sipcSocket->waitForConnected()) /*30 seconds*/
        {
            qDebug() << "#SipcHanlder::run waitForConnected"
            << sipcSocket->errorString();
            return;
        }

        QByteArray toSendMsg ("R fetion.com.cn SIP-C/4.0\r\n");

        toSendMsg.append ("F: ").append (info->fetionNumber).append ("\r\n");
        toSendMsg.append ("I: ").append (QByteArray::number (info->callId++)).append ("\r\n");
        toSendMsg.append ("Q: 2 ").append ("R\r\n");
        toSendMsg.append ("CN: ").append (info->cnonce).append ("\r\n");
        toSendMsg.append ("CL: type=\"pc\" ,version=\"" + PROTOCOL_VERSION + "\"\r\n\r\n");
        int length = 0;

        while (length < toSendMsg.length())
        {
            length += sipcSocket->write (toSendMsg.right (toSendMsg.size() - length));
        }

        sipcSocket->waitForBytesWritten (-1);

        //QCoreApplication::processEvents();
        sipcSocket->flush();

        while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (!sipcSocket->waitForReadyRead (30000))
            {
                qDebug() << "#SipcHanlder::run waitForReadyRead"
                << sipcSocket->error() << sipcSocket->errorString();
                return;
            }
        }

        QByteArray responseData;

        while (sipcSocket->waitForReadyRead (-1))
        {
            responseData = sipcSocket->readAll();
        }

        handleSipcRegisterResponse (responseData);
    }

    void User::handleSipcRegisterResponse (QByteArray data)
    {
        if (!data.startsWith ("SIP-C/4.0 401 Unauthoried"))
        {
            qDebug() << "Wrong Sipc register response.";
            qDebug() << data;
            return;
        }

        int b, e;

        b = data.indexOf ("\",nonce=\"");
        e = data.indexOf ("\",key=\"", b);
        info->nonce = data.mid (b + 9, e - b - 9); // need to store?
        b = data.indexOf ("\",key=\"");
        e = data.indexOf ("\",signature=\"", b);
        info->key = data.mid (b + 7, e - b - 7); // need to store?
        b = data.indexOf ("\",signature=\"");
        e = data.indexOf ("\"", b);
        info->signature = data.mid (b + 14, e - b - 14);
        info->aeskey = QByteArray::fromHex (cnouce (9)).left (32);
        // need to store?
        // generate response
        info->response = RSAPublicEncrypt (info->userId, info->password,
                                           info->nonce, info->aeskey, info->key);
        // DO authorize
        sipcAuthorize();
    }

    void User::sipcAuthorize ()
    {

        if (sipcSocket->state() != QAbstractSocket::ConnectedState)
        {
            qDebug() << "socket closed.";
            return;
        }

        QByteArray toSendMsg ("R fetion.com.cn SIP-C/4.0\r\n");

        toSendMsg.append ("F: ").append (info->fetionNumber).append ("\r\n");
        toSendMsg.append ("I: ").append (QString::number (info->callId++)).append ("\r\n");
        toSendMsg.append ("Q: 2 ").append ("R\r\n");
        toSendMsg.append ("CN: ").append (info->cnonce).append ("\r\n");
        toSendMsg.append ("CL: type=\"pc\" ,version=\"" + PROTOCOL_VERSION + "\"\r\n\r\n");
        int length = 0;

        while (length < toSendMsg.length())
        {
            length += sipcSocket->write (toSendMsg.right (toSendMsg.size() - length));
        }

        sipcSocket->waitForBytesWritten (-1);

        sipcSocket->flush();

        while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (!sipcSocket->waitForReadyRead (30000))
            {
                qDebug() << "#SipcHanlder::run waitForReadyRead" << sipcSocket->error() << sipcSocket->errorString();
                return;
            }
        }

        handleSipcAuthorizeResponse (sipcSocket->readAll());
    }

    void User::handleSipcAuthorizeResponse (QByteArray data)
    {
        //if

    }

    void User::getSsiPic()
    {
        QByteArray data = SsiPicData (info->verfication->algorithm, info->ssic);
        QTcpSocket socket;
        QHostInfo info = QHostInfo::fromName ("nav.fetion.com.cn");
        socket.connectToHost (info.addresses().first().toString(), 80);

        if (!socket.waitForConnected())
        {
            qDebug() << "waitForEncrypted" << socket.errorString();
            return;
        }

        socket.write (data);

        while (socket.waitForReadyRead (-1))
            parseServerConfig (socket.readAll());
    }

}
#include "user.moc"
