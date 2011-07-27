/*
 *  The implement of Bressein User
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

#include <QSslSocket>
#include <QDomDocument>
#include <QHostInfo>
#include <QTimer>
//#include "user.h"
#include "userprivate.h"
#include "utils.h"
// N.B. all data through socket are utf-8
// N.B. we call buddyList a group
namespace Bressein
{
User::User (QByteArray number, QByteArray password, QObject *parent)
        : QObject (parent)
{
    timer = new QTimer (this);
    info = new UserInfo (this);
    sipcSocket = new QTcpSocket (this);
    sipcSocket->setReadBufferSize (0);
    sipcSocket->setSocketOption (QAbstractSocket::KeepAliveOption, 1);
    sipcSocket->setSocketOption (QAbstractSocket::LowDelayOption, 1);
    connect (this, SIGNAL (ssiResponseParsed()), SLOT (systemConfig()));
    connect (this, SIGNAL (serverConfigParsed()), SLOT (sipcRegister()));
    connect (this, SIGNAL (sipcRegisterParsed()), SLOT (sipcAuthorize()));
    connect (this, SIGNAL (sipcAuthorizeParsed()), SLOT (activateTimer()));
    initialize (number, password);
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
    while (!contacts.isEmpty())
        delete contacts.takeFirst();

    qDebug() << "delete groups";
    while (!groups.isEmpty())
        delete groups.takeFirst();
    if (workerThread.isRunning())
    {
        workerThread.quit();
        workerThread.wait();
    }

}

bool User::operator== (const User & other)
{
    return this->info->fetionNumber == other.info->fetionNumber;
}

//public slots

void User::login()
{
    QMetaObject::invokeMethod (this, "ssiLogin");
    // ssiLogin();
}

void User::loginVerify (QByteArray code)
{
    info->verification->code = code;
    QMetaObject::invokeMethod (this, "ssiVerify");
}

void User::close()
{
    // note this is called from main thread
    workerThread.deleteLater();
}

// private
void User::initialize (QByteArray number, QByteArray password)
{
    // TODO try to load saved configuration for this user
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

void User::sipcWriteRead (QByteArray& in, QByteArray& out)
{
    printf ("To write\n");
    printf ("%s\n", in.data());
    int length = 0;
    while (length < in.length())
    {
        length += sipcSocket->write (in.right (in.size() - length));
    }
    sipcSocket->waitForBytesWritten (-1);
    sipcSocket->flush();
    while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (!sipcSocket->waitForReadyRead (-1))
        {
            qDebug() << "When waitForReadyRead"
            << sipcSocket->error() << sipcSocket->errorString();
            timer->stop();
            return;
        }
    }
    // FIXME The sever may send more than the reply
    // Ones is that someone invites this user, and then there comes
    // a piece of invitation too.
    // Fortunately, this case likely happens after keepAlive.
//         while (sipcSocket->waitForReadyRead (-1))
//         {
    out = sipcSocket->readAll();
//         }
    printf ("To read\n");
    printf ("%s\n", out.data());
}

//private slots
void User::keepAlive()
{
    if (sipcSocket->state() != QAbstractSocket::ConnectedState)
    {
        printf ("socket closed.");
        return;
    }
    QByteArray toSendMsg = keepAliveData (info->fetionNumber, info->callId);
    QByteArray responseData;
    qDebug() << "To keepAlive";
    sipcWriteRead (toSendMsg, responseData);

    //TODO Handle any responseData
    // stop timer if failed to send or get no return.
    // qDebug () << responseData;
}

void User::ssiLogin()
{
    //TODO if proxy
    QByteArray password = (hashV4 (info->userId, info->password));
    qDebug() << info->loginNumber;
    QByteArray data = ssiLoginData (info->loginNumber, password);
    QSslSocket socket (this);
    socket.connectToHostEncrypted (UID_URI, 443);

    if (!socket.waitForEncrypted (-1))
    {
        // TODO handle socket.error() or inform user what happened
        qDebug() << "waitForEncrypted" << socket.errorString();
//             ssiLogin();
        return;
    }
    qDebug() << data;
    socket.write (data);
    QByteArray responseData;
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (!socket.waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
            qDebug() << "ssiLogin  waitForReadyRead"
            << socket.error() << socket.errorString();
        }
    }
    qDebug() << "ssiLogin";
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
    {
        if (!socket.waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
            qDebug() << "ssiLogin  waitForReadyRead"
            << socket.error() << socket.errorString();
        }
    }
    //FIXME get more data until <results/> is fully downloaded
    while (socket.waitForReadyRead())
        responseData += socket.readAll();
    parseServerConfig (responseData);
}

/**
 * R fetion.com.cn SIP-C/4.0
 * F : 916098834
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
    if (!sipcSocket->waitForConnected (-1)) /*no time out*/
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
    sipcWriteRead (toSendMsg, responseData);
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
void User::sipcAuthorize ()
{
    if (sipcSocket->state() != QAbstractSocket::ConnectedState)
    {
        printf ("socket closed.");
        return;
    }
    QByteArray toSendMsg = sipcAuthorizeData (info->loginNumber,
            info->fetionNumber,
            info->userId,
            info->callId,
            info->response,
            info->client.version,
            info->client.customConfigVersion,
            info->client.contactVersion,
            info->state);
    int length = 0;
    while (length < toSendMsg.length())
    {
        length +=
            sipcSocket->write (toSendMsg.right (toSendMsg.size() - length));
    }
    sipcSocket->waitForBytesWritten (-1);
    sipcSocket->flush();
    QByteArray responseData;
    while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (!sipcSocket->waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
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
    bool ok = domDoc.setContent (xml, false,
            &errorMsg, &errorLine, &errorColumn);
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

            if (!info->verification)
            {
                info->verification = new UserInfo::Verfication;
            }
            QDomElement domE =  domRoot.firstChildElement ("verification");
            if (domE.hasAttribute ("algorithm") &&
                    domE.hasAttribute ("type=") &&
                    domE.hasAttribute ("text") &&
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
        else if (statusCode == "401" || statusCode == "404")
        {
            //401 wrong password
            //TODO inform user that password is wrong
            //404
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

                if (!domE.isNull() && domE.hasAttribute ("domain") &&
                        domE.hasAttribute ("c"))
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
            info->systemconfig.serverVersion =
                domRoot.attribute ("version").toUtf8();
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
            info->systemconfig.parametersVersion =
                domRoot.attribute ("version").toUtf8();
        domRoot = domRoot.nextSiblingElement ("hints");
        if (!domRoot.isNull() && domRoot.hasAttribute ("version"))
        {
            info->systemconfig.hintsVersion =
                domRoot.attribute ("version").toUtf8();
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
    bool ok = domDoc.setContent (xml, false,
            &errorMsg, &errorLine, &errorColumn);
    if (!ok)
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

        if (!domChild.isNull() && domChild.hasAttribute ("public-ip") &&
                domChild.hasAttribute ("last-login-time") &&
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
            info->client.birthDate =
                domChild.attribute ("birth-date").toUtf8();
            info->client.carrierRegion =
                domChild.attribute ("carrier_region").toUtf8();
            info->client.carrierStatus =
                domChild.attribute ("carrier_status").toUtf8();
            info->client.gender = domChild.attribute ("gender").toUtf8();
            info->client.impresa = domChild.attribute ("impresa").toUtf8();
            info->client.mobileNo = domChild.attribute ("mobile_no").toUtf8();
            info->client.nickname = domChild.attribute ("nickname").toUtf8();
            info->client.smsOnLineStatus =
                domChild.attribute ("sms-online-status").toUtf8();
            info->client.version = domChild.attribute ("version").toUtf8();
        }
        domChild = domChild.nextSiblingElement ("custom-config");
        if (!domChild.isNull() && domChild.hasAttribute ("version"))
        {
            info->client.customConfigVersion =
                domChild.attribute ("version").toUtf8();
            //FIXME is that a xml segment?

            if (!domChild.text().isEmpty())
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

        if (!domChild.isNull() && domChild.hasAttribute ("version"))
        {
            //TODO if the version is the same as that in local cache, means
            // the list is the same too.
            info->client.contactVersion =
                domChild.attribute ("version").toUtf8();
            domGrand = domChild.firstChildElement ("buddy-lists");
            if (!domGrand.isNull())
            {
                domGrand = domGrand.firstChildElement ("buddy-list");
                while (!domGrand.isNull() && domGrand.hasAttribute ("id") &&
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
            if (!domGrand.isNull())
            {
                domGrand = domGrand.firstChildElement ("b");
                while (!domGrand.isNull() &&
                        domGrand.hasAttribute ("f") &&
                        domGrand.hasAttribute ("i") &&
                        domGrand.hasAttribute ("l") &&
                        domGrand.hasAttribute ("n") &&
                        //  domGrand.hasAttribute ("o") &&
                        domGrand.hasAttribute ("p") &&
                        domGrand.hasAttribute ("r") &&
                        domGrand.hasAttribute ("u"))
                {
                    Contact *contact = new Contact;
                    contact->userId = domGrand.attribute ("i").toUtf8();
                    contact->groupId = domGrand.attribute ("l").toUtf8();
                    contact->localName = domGrand.attribute ("n").toUtf8();
//                         contact-> = domGrand.attribute ("o").toUtf8();
                    contact->identity = domGrand.attribute ("p").toUtf8();
                    contact->relationStatus = domGrand.attribute ("r").toUtf8();
                    contact->sipuri = domGrand.attribute ("u").toUtf8();
                    contacts.append (contact);
                    domGrand = domGrand.nextSiblingElement ("b");
                }
            }
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

    if (!socket.waitForConnected (-1))
    {
        //TODO handle exception here
        qDebug() << "waitForEncrypted" << socket.errorString();
        return;
    }
    socket.write (data);
    QByteArray responseData;
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (!socket.waitForReadyRead (-1))
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
    bool ok = domDoc.setContent (xml, false,
            &errorMsg, &errorLine, &errorColumn);
    if (!ok)
    {
        // perhaps need more data from  socket
        qDebug() << "Wrong response";
        return;
    }
    QDomElement domRoot = domDoc.documentElement();
    if (domRoot.tagName() == "results")
    {
        domRoot = domRoot.firstChildElement ("pic-certificate");
        if (!domRoot.isNull() && domRoot.hasAttribute ("id") &&
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

    if (!socket.waitForEncrypted (-1))
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
        if (!socket.waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
            qDebug() << "ssiLogin  waitForReadyRead"
            << socket.error() << socket.errorString();
        }
    }
    responseData = socket.readAll();
    parseSsiResponse (responseData);
}

void User::contactInfo (const QByteArray& userId)
{
    QByteArray toSendMsg = contactInfoData (info->fetionNumber,
            userId, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    //TODO handle responseData
    // carrier-region
}

void User::contactInfo (const QByteArray& Number, bool mobile)
{
    QByteArray toSendMsg = contactInfoData (info->fetionNumber, Number,
            info->callId, mobile);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    //TODO handle responseData
    // carrier-region
}


void User::sendMessage (const QByteArray& toSipuri, const QByteArray& message)
{
    QByteArray toSendMsg = messagedata (info->fetionNumber,
            toSipuri, info->callId, message);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO handle responseData
}

void User::addBuddy (
    const QByteArray &number,
    QByteArray buddyLists,
    QByteArray localName,
    QByteArray desc,
    QByteArray phraseId)
{
    QByteArray toSendMsg = addBuddyData (info->fetionNumber, number,
            info->callId, buddyLists, localName, desc, phraseId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO handle responseData
}

void User::deleteBuddy (const QByteArray& userId)
{
    QByteArray toSendMsg =
        deleteBuddyData (info->fetionNumber, userId, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO handle responseData
}

void User::contactSubscribe()
{
    QByteArray toSendMsg =
        contactSubscribeData (info->fetionNumber, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
}

void User::inviteFriend (const QByteArray& sipUri)
{
    QByteArray toSendMsg = startChatData (info->fetionNumber, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    if (responseData.isEmpty()) return;
    // TODO get tag 'A' and its value which contains ip, port, credential
    QByteArray credential, ip, port;
    toSendMsg.clear();
    responseData.clear();
    toSendMsg = registerData (info->fetionNumber, info->callId, credential);
    QTcpSocket socket (this);
    socket.connectToHost (ip, port.toInt());
    if (!socket.waitForConnected (-1))
    {
        qDebug() << "waitForEncrypted" << socket.errorString();
        return;
    }
    socket.write (toSendMsg);
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (!socket.waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
            qDebug() << "ssiLogin  waitForReadyRead"
            << socket.error() << socket.errorString();
        }
    }
    //FIXME get more data until <results/> is fully downloaded
    while (socket.waitForReadyRead())
        responseData += socket.readAll();
    responseData.clear();
    toSendMsg = invitateData (info->fetionNumber, info->callId, sipUri);
    sipcWriteRead (toSendMsg, responseData);
    // TODO check if 200
}

void User::createBuddylist (const QByteArray &name)
{
    QByteArray toSendMsg =
        createGroupData (info->fetionNumber, info->callId, name);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO handle responseData, get version, name, id
}

void User::deleteBuddylist (const QByteArray& id)
{
    QByteArray toSendMsg =
        deleteGroupData (info->fetionNumber, info->callId, id);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO handle responseData, get version, name, id
}

void User::renameBuddylist (const QByteArray &id, const QByteArray &name)
{
    QByteArray toSendMsg =
        renameGroupData (info->fetionNumber, info->callId, id, name);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO
}

void User::updateInfo ()
{
    QByteArray toSendMsg = updateInfoData (
                info->fetionNumber, info->callId,
                info->client.impresa, info->client.nickname,
                info->client.gender, info->client.customConfig,
                info->client.customConfigVersion);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO handle responseData
}

void User::setImpresa (const QByteArray& impresa)
{
    QByteArray toSendMsg = impresaData (
                info->fetionNumber, info->callId, impresa,
                info->client.version,
                info->client.customConfig,
                info->client.customConfigVersion);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
    // TODO handle responseData
}

void User::setMessageStatus (int days)
{
    QByteArray toSendMsg = messageStatusData (
                info->fetionNumber, info->callId, days);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
}

void User::setClientState (StateType& st)
{
    QByteArray state = QByteArray::number ( (int) st);
    QByteArray toSendMsg = clientStatusData (
                info->fetionNumber, info->callId, state);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData);
}

void User::activateTimer()
{
    connect (timer, SIGNAL (timeout()), this, SLOT (keepAlive()));
    timer->start (40000);
}

}

#include "user.moc"

