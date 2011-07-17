/*
    <one line to give the program's name and a brief idea of what it does.>
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
        socket = new QTcpSocket(this);
        socket->
        connect (this, SIGNAL (serverConfigGot()), SLOT (sipcLogin()));
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
        QByteArray data = ssiLoginBody (info->loginNumber, password);
        QSslSocket socket;
        socket.connectToHostEncrypted ("uid.fetion.com.cn", 443);

        if (!socket.waitForEncrypted())
        {
            qDebug() << "waitForEncrypted" << socket.errorString();
            return;
        }

        socket.write (data);

        while (socket.waitForReadyRead (-1))
            handleSsiResponse (QByteArray (socket.readAll().data()));
    }

    void User::handleSsiResponse (QByteArray data)
    {
        if (data.isEmpty())
        {
            qDebug() << "Ssi response is empty!!";
            return;
        }

        int b = data.indexOf ("Set-Cookie: ssic=");

        int e = data.indexOf ("; path", b);
        info->ssic = data.mid (b + 17, e - b - 17);
        QByteArray xml = data.right (data.indexOf ("<?xml version=\"1.0\""));

        QDomDocument domDoc;

        if (!domDoc.setContent (xml))
        {
            qDebug() << "Failed to parse Ssi response!!";
            return;
        }

        domDoc.normalize();

        QDomElement domRoot = domDoc.documentElement();

        if (domRoot.tagName() == "results")
        {
            if (domRoot.hasAttribute ("status-code"))
            {
                qDebug() << "No status-code";
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

                return;
            }
            else if (statusCode == "401" || statusCode == "404")
            {
                //401 wrong password
                qDebug() << "Authorization error";
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

                //TODO emit signal here to invoke getServerConfig
                // time to getServerConfig
            }
        }
    }

    void User::getServerConfig()
    {
        QByteArray body = configBody (info->loginNumber);
        QTcpSocket socket;
        QHostInfo info = QHostInfo::fromName ("nav.fetion.com.cn");
        socket.connectToHost (info.addresses().first().toString(), 80);

        if (!socket.waitForConnected())
        {
            qDebug() << "waitForEncrypted" << socket.errorString();
            return;
        }

        socket.write (body);

        while (socket.waitForReadyRead (-1))
            parseServerConfig (socket.readAll());
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
            qDebug() << "Failed to parse Ssi response!!";
            return;
        }

        QDomElement domRoot = domDoc.documentElement();

        if (domRoot.tagName() == "config")
        {
            domRoot = domRoot.firstChildElement ("servers");

            if (domRoot.hasAttribute ("version"));

            // configServerVersion = domRoot.attribute("version")
            domRoot = domRoot.nextSiblingElement ("parameters");

            if (domRoot.hasAttribute ("version"));

            // configParametersVersion = domRoot.attribute("version")
            domRoot = domRoot.nextSiblingElement ("hints");

            if (domRoot.hasAttribute ("version"));

            // confighintsVersion = domRoot.attribute("version")
            //TODO get phrase here
            domRoot = domRoot.nextSiblingElement ("sipc-proxy");

            if (!domRoot.text().isEmpty());

            // sipcProxyIpPort = domRoot.text();
            domRoot = domRoot.nextSiblingElement ("get-uri");

            if (!domRoot.text().isEmpty());

            // portraitServerNamePath = domRoot.text();
            //when finish, start sip-c
            emit serverConfigGot();
        }
    }

    void User::sipcRegister()
    {
        socket->setReadBufferSize (0);

        int seperator = info->systemconfig.ProxyIpPort.indexOf (':');
        QString ip = QString (info->systemconfig.ProxyIpPort.left (seperator));
        quint16 port = info->systemconfig.ProxyIpPort.mid (seperator).toUInt();
        socket->connectToHost (ip, port);

        if (!socket->waitForConnected()) /*30 seconds*/
        {
            qDebug() << "#SipcHanlder::run waitForConnected" << socket->errorString();
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
            length += socket->write (toSendMsg.right (toSendMsg.size() - length));
        }

        socket->waitForBytesWritten (-1);

        socket->flush();

        while (socket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (!socket->waitForReadyRead (30000))
            {
                qDebug() << "#SipcHanlder::run waitForReadyRead" << socket->error() << socket->errorString();
                return;
            }
        }

        handleSipcRegisterResponse (socket->readAll());
    }

    void User::handleSipcRegisterResponse (QByteArray data)
    {
        if (!data.startsWith("SIP-C/4.0 401 Unauthoried"))
        {
            qDebug() << "Wrong Sipc register response.";
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

        if (socket->state() != QAbstractSocket::ConnectedState )
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
            length += socket->write (toSendMsg.right (toSendMsg.size() - length));
        }

        socket->waitForBytesWritten (-1);

        socket->flush();

        while (socket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (!socket->waitForReadyRead (30000))
            {
                qDebug() << "#SipcHanlder::run waitForReadyRead" << socket->error() << socket->errorString();
                return;
            }
        }
        handleSipcAuthorizeResponse (socket->readAll());
    }
    void User::handleSipcAuthorizeResponse (QByteArray data)
    {

    }


}
