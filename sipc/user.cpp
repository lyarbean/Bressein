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
#include <QTimer>
//#include "user.h"
#include "userprivate.h"
#include "utils.h"
// N.B all data through socket are utf-8

namespace Bressein
{
    User::User (QByteArray number, QByteArray password, QObject *parent) : QObject (parent)

    {
        sipcSocket = new QTcpSocket (this);
        sipcSocket->setReadBufferSize (0);
        sipcSocket->setSocketOption (QAbstractSocket::KeepAliveOption, 1);
        sipcSocket->setSocketOption (QAbstractSocket::LowDelayOption, 1);
        connect (this, SIGNAL (ssiResponseParsed()), SLOT (systemConfig()));
        connect (this, SIGNAL (serverConfigParsed()), SLOT (sipcRegister()));
        connect (this, SIGNAL (sipcRegisterParsed()), SLOT (sipcAuthorize()));
        connect (this, SIGNAL (sipcAuthorizeParsed()), SLOT (keepAlive()));
        initialize (number, password);
        this->moveToThread (&workerThread);
        workerThread.start();
    }

    User::~User()
    {

        if (workerThread.isRunning())
        {
            workerThread.quit();
            workerThread.wait();
        }
        if (info) delete info;


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
        QMetaObject::invokeMethod (this, "ssiLogin");
        // ssiLogin();
    }

    void User::close()
    {
        workerThread.deleteLater();
    }

    void User::keepAlive()
    {
        if (sipcSocket->state() != QAbstractSocket::ConnectedState)
        {
            qDebug() << "socket closed.";
            return;
        }

        QByteArray toSendMsg = keepAliveData (info->fetionNumber, info->callId);

        int length = 0;
// TODO wrapped following segment

        while (length < toSendMsg.length())
        {
            length += sipcSocket->write (toSendMsg.right (toSendMsg.size() - length));
        }

        sipcSocket->waitForBytesWritten (-1);

        QByteArray responseData;

        while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (!sipcSocket->waitForReadyRead (-1))
            {
                qDebug() << "sipcAuthorize waitForReadyRead"
                << sipcSocket->error() << sipcSocket->errorString();
                return;
            }
        }

        while (sipcSocket->waitForReadyRead ())
        {
            responseData += sipcSocket->readAll();
        }

        //TODO ensure full of <results/> downloaded
        qDebug() << "To keepAlive";

        qDebug () << responseData;

        QTimer::singleShot (6000, this, SLOT (keepAlive()));
    }

    void User::sendMsg (QByteArray& fetionId, QByteArray& message)
    {

    }

    void User::addBuddy (QByteArray& number, QByteArray& info)
    {

    }

// private
    void User::initialize (QByteArray number, QByteArray password)
    {
        // TODO try to load saved configuration for this user
        info = new UserInfo (this);
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

    void User::ssiLogin()
    {
        //TODO if proxy
        qDebug() << "Begin ssi login";
        QByteArray password = (hashV4 (info->userId, info->password));
        QByteArray data = ssiLoginData (info->loginNumber, password);
        QSslSocket socket (this);
        socket.connectToHostEncrypted (UID_URI, 443);

        if (!socket.waitForEncrypted (-1))
        {
            qDebug() << "waitForEncrypted" << socket.errorString();
//             ssiLogin();
            return;
        }

        socket.write (data);

        QByteArray responseData;

        while (socket.bytesAvailable() < (int) sizeof (quint16))
            if (!socket.waitForReadyRead (-1))
            {
                qDebug() << "ssiLogin  waitForReadyRead"
                << socket.error() << socket.errorString();
            }

        responseData = socket.readAll();

        parseSsiResponse (responseData);
    }

    void User::systemConfig()
    {
        QByteArray body = configData (info->loginNumber);
        QTcpSocket socket (this);
        QHostInfo info = QHostInfo::fromName ("nav.fetion.com.cn");
        socket.connectToHost (info.addresses().first().toString(), 80);

        if (!socket.waitForConnected (-1))
        {
            qDebug() << "waitForEncrypted" << socket.errorString();
            return;
        }

        socket.write (body);

        QByteArray responseData;

        while (socket.bytesAvailable() < (int) sizeof (quint16))
            if (!socket.waitForReadyRead (-1))
            {
                qDebug() << "ssiLogin  waitForReadyRead"
                << socket.error() << socket.errorString();
            }

        //FIXME get more data until <results/> is fully downloaded
        while (socket.waitForReadyRead())
            responseData += socket.readAll();

        parseServerConfig (responseData);
    }

    /** \example
     * R fetion.com.cn SIP-C/4.0
     * F : 916098834
     * I: 1
     * Q: 1 R
     * CN: 1CF1A05B2DD0281755997ADC70F82B16
     * CL: type=”pc” ,version=”3.6.1900″
     **/
    void User::sipcRegister()
    {

        int seperator = info->systemconfig.proxyIpPort.indexOf (':');
        QString ip = QString (info->systemconfig.proxyIpPort.left (seperator));
        quint16 port = info->systemconfig.proxyIpPort.mid (seperator + 1).toUInt();

        sipcSocket->connectToHost (ip, port);

        if (!sipcSocket->waitForConnected (-1)) /*no time out*/
        {
            qDebug() << "#sipcRegister waitForConnected"
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
        qDebug() << "written " << toSendMsg;
        sipcSocket->flush();
        QByteArray responseData;

        while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (!sipcSocket->waitForReadyRead (-1))
            {
                qDebug() << "#sipcRegister  waitForReadyRead"
                << sipcSocket->error() << sipcSocket->errorString();
                return;
            }
        }

        //         while (sipcSocket->waitForReadyRead (-1))
        //         {
        responseData = sipcSocket->readAll();

        //         }

        qDebug() << responseData;

        parseSipcRegister (responseData);
    }



    /** \example
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
    void User::sipcAuthorize ()
    {

        qDebug() << "begin sipcAuthorize";

        if (sipcSocket->state() != QAbstractSocket::ConnectedState)
        {
            qDebug() << "socket closed.";
            return;
        }

        QByteArray body = sipcAuthorizeBody (info->loginNumber,

                                             info->userId,
                                             info->client.version,
                                             info->customConfig,
                                             info->client.contactVersion,
                                             info->state);
        QByteArray toSendMsg ("R fetion.com.cn SIP-C/4.0\r\n");

        toSendMsg.append ("F: ").append (info->fetionNumber).append ("\r\n");
        toSendMsg.append ("I: ").append (QString::number (info->callId++)).append ("\r\n");
        toSendMsg.append ("Q: 2 R\r\n");
        toSendMsg.append ("A: Digest response=\"").append (info->response).append ("\",algorithm=\"SHA1-sess-v4\"\r\n");
        toSendMsg.append ("AK: ak-value\r\n");
        toSendMsg.append ("L: ");
        toSendMsg.append (QByteArray::number (body.size()));
        toSendMsg.append ("\r\n\r\n");
        toSendMsg.append (body);
        int length = 0;
        qDebug() << toSendMsg;

        while (length < toSendMsg.length())
        {
            length += sipcSocket->write (toSendMsg.right (toSendMsg.size() - length));
        }

        sipcSocket->waitForBytesWritten (-1);
        sipcSocket->flush();

        QByteArray responseData;

        while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
        {
            if (!sipcSocket->waitForReadyRead (-1))
            {
                qDebug() << "sipcAuthorize waitForReadyRead"
                << sipcSocket->error() << sipcSocket->errorString();
                return;
            }
        }

        while (sipcSocket->waitForReadyRead ())
        {
            responseData += sipcSocket->readAll();
        }

        //TODO ensure full of <results/> downloaded
        parseSipcAuthorize (responseData);
    }

// functions for parsing data
    void User::parseSsiResponse (QByteArray &data)
    {

        qDebug() << "Begin parse Ssi";

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

        if (!ok)
        {
            qDebug() << "Failed to parse Ssi response!!";
            qDebug() << errorMsg << errorLine << errorColumn;
            qDebug() << xml;
            return;
        }

        domDoc.normalize();
        qDebug() << QString::fromUtf8 (domDoc.toByteArray (4));
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
                    // info->state = domE.attribute ("user-status").toUtf8();
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

                emit ssiResponseParsed();
            }
        }
    }

    void User::parseSipcRegister (QByteArray &data)
    {

        if (!data.startsWith ("SIP-C/4.0 401 Unauthoried"))
        {
            qDebug() << "Wrong Sipc register response.";
            qDebug() << data;
            return;
        }

        qDebug() << QString::fromUtf8 (data);
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
        emit sipcRegisterParsed();
    }


    void User::parseServerConfig (QByteArray &data)
    {
        if (data.isEmpty())
        {
            qDebug() << "Server Configuration response is empty!!";
            return;
        }

        QDomDocument domDoc;

        QByteArray xml = data.mid (data.indexOf ("<?xml version=\"1.0\""));
        bool ok = domDoc.setContent (xml);

        if (!ok)
        {
            qDebug() << "Failed to parse server config response!!";
            qDebug() << xml;
            qDebug() << "==============================";
            return;
        }
        domDoc.normalize();
        qDebug() << QString::fromUtf8 (domDoc.toByteArray (4));
        QDomElement domRoot = domDoc.documentElement();

        QDomElement domChild;

        if (domRoot.tagName() == "config")
        {
            domRoot = domRoot.firstChildElement ("servers");

            if (!domRoot.isNull() && domRoot.hasAttribute ("version"))
            {
                // process children
                info->systemconfig.serverVersion = domRoot.attribute ("version").toUtf8();
                domChild = domRoot.firstChildElement ("sipc-proxy");

                if (!domChild.isNull() && !domChild.text().isEmpty())
                {
                    info->systemconfig.proxyIpPort = domChild.text().toUtf8();
                }

                domChild = domRoot.firstChildElement ("get-uri");

                if (!domChild.isNull() && !domChild.text().isEmpty())
                {
                    info->systemconfig.serverNamePath = domChild.text().toUtf8();
                }
            }

            domRoot = domRoot.nextSiblingElement ("parameters");

            if (!domRoot.isNull() && domRoot.hasAttribute ("version"))
                info->systemconfig.parametersVersion = domRoot.attribute ("version").toUtf8();

            domRoot = domRoot.nextSiblingElement ("hints");

            if (!domRoot.isNull() && domRoot.hasAttribute ("version"))
            {
                info->systemconfig.hintsVersion = domRoot.attribute ("version").toUtf8();
                domChild = domRoot.firstChildElement ("addbuddy-phrases");
                QDomElement domGrand = domChild.firstChildElement ("phrases");

                while (!domGrand.isNull() && domGrand.hasAttribute ("id"))
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
        bool ok = domDoc.setContent (xml, false, &errorMsg, &errorLine, &errorColumn);

        if (!ok)
        {
            // perhaps need more data from  socket
            qDebug() << "Wrong sipc authorize response";
            return;
        }

        // begin to parse
        domDoc.normalize();

        QDomElement domRoot = domDoc.documentElement();

        QDomElement domChild;

        if (domRoot.tagName() == "results")
        {
            domChild = domRoot.firstChildElement ("client");

            if (!domChild.isNull() && domChild.hasAttribute ("public-ip") &&
                domChild.hasAttribute ("last-login-time") &&
                domChild.hasAttribute ("last-login-ip"))
            {
                info->client.publicIp = domChild.attribute ("public-ip").toUtf8();
                info->client.lastLoginIp = domChild.attribute ("last-login-ip").toUtf8();
                info->client.lastLoginTime = domChild.attribute ("last-login-time").toUtf8();
            }

            domChild = domRoot.firstChildElement ("user-info");

            domChild = domChild.firstChildElement ("personal");

            if (!domChild.isNull() && domChild.hasAttribute ("user-id") &&
                domChild.hasAttribute ("carrier") && //should be CMCC?
                domChild.hasAttribute ("version") &&
                domChild.hasAttribute ("nickname") &&
                domChild.hasAttribute ("gender") &&
                domChild.hasAttribute ("birth-date") &&
                domChild.hasAttribute ("mobile-no") &&
                domChild.hasAttribute ("sms-online-status") &&
                domChild.hasAttribute ("carrier-region") &&
                domChild.hasAttribute ("carrier-status") &&
                domChild.hasAttribute ("impresa"))
            {
                info->client.birthDate = domChild.attribute ("birth-date").toUtf8();
                info->client.carrierRegion = domChild.attribute ("carrier_region").toUtf8();
                info->client.carrierStatus = domChild.attribute ("carrier_status").toUtf8();
                info->client.gender = domChild.attribute ("gender").toUtf8();
                info->client.impresa = domChild.attribute ("impresa").toUtf8();
                info->client.mobileNo = domChild.attribute ("mobile_no").toUtf8();
                info->client.nickname = domChild.attribute ("nickname").toUtf8();
                info->client.smsOnLineStatus = domChild.attribute ("sms-online-status").toUtf8();
                info->client.version = domChild.attribute ("version").toUtf8();
            }

            domChild = domChild.nextSiblingElement ("custom-config");

            if (!domChild.isNull() && domChild.hasAttribute ("version"))
            {
                info->client.customConfigVersion = domChild.attribute ("version").toUtf8();
                //FIXME is that a xml segment?

                if (!domChild.text().isEmpty())
                    info->customConfig = domChild.text().toUtf8();
            }

            domChild = domChild.nextSiblingElement ("contact-list");

// < contact-list version = "363826266" >
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
// o: group id, p: identity
// r; relationStatus, u: sipuri

            if (!domChild.isNull() && domChild.hasAttribute ("version"))
            {
                //TODO if the version is the same as that in local cache, means
                // the list is the same too.
                info->client.contactVersion = domChild.attribute ("version").toUtf8();
                // for now, simply store this segment, we use it for view
                // there are some properties
                // groupCount
                //TODO
                // there is chat-friends and blacklist
            }

            domChild = domChild.nextSiblingElement ("quotas");

            if (!domChild.isNull())
            {
                domChild = domChild.firstChildElement ("quota-frequency");

                if (!domChild.isNull())
                {
                    domChild = domChild.firstChildElement ("frequency");

                    if (!domChild.isNull() &&
                        domChild.hasAttribute ("day-limit") &&
                        domChild.hasAttribute ("day-count") &&
                        domChild.hasAttribute ("month-limit") &&
                        domChild.hasAttribute ("month-count"))
                    {
                        info->client.smsDayLimit = domChild.attribute ("day-limit").toUtf8();
                        info->client.smsDayCount = domChild.attribute ("day-count").toUtf8();
                        info->client.smsMonthCount = domChild.attribute ("month-limit").toUtf8();
                        info->client.smsMonthCount = domChild.attribute ("month-count").toUtf8();
                    }
                }
            }

            emit sipcAuthorizeParsed();
        }
    }

    void User::ssiVerify()
    {
        QByteArray data = SsiPicData (info->verfication->algorithm, info->ssic);
        QTcpSocket socket;
        QHostInfo info = QHostInfo::fromName ("nav.fetion.com.cn");
        socket.connectToHost (info.addresses().first().toString(), 80);

        if (!socket.waitForConnected (-1))
        {
            //TODO handle exception here
            qDebug() << "waitForEncrypted" << socket.errorString();
            return;
        }

        socket.write (data);

        QByteArray responseData;

        while (socket.bytesAvailable() < (int) sizeof (quint16))
            if (!socket.waitForReadyRead (-1))
            {
                qDebug() << "ssiLogin  waitForReadyRead"
                << socket.error() << socket.errorString();
            }

        responseData += socket.readAll();

//TODO
//         parseServerConfig (responseData);
    }

}

#include "user.moc"

