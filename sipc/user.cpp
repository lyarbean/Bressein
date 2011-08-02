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
//#include "user.h"
#include "userprivate.h"
#include "utils.h"
// N.B. all data through socket are utf-8
// N.B. we call buddyList a group
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
    while (not contacts.isEmpty())
        delete contacts.takeFirst();

    qDebug() << "delete groups";
    while (not groups.isEmpty())
        delete groups.takeFirst();
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

//public slots

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
    workerThread.deleteLater();
}

void User::startChat (const QByteArray &sipUri)
{
    //TODO
    qDebug() << "start Chat called";
    // TODO check if sipUri valid
    QMetaObject::invokeMethod (this, "inviteFriend",
                               Qt::QueuedConnection,
                               Q_ARG (QByteArray, sipUri));
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

const Bressein::Contacts &User::getContacts()
{
    qSort (contacts.begin(), contacts.end(), userIdLessThan);
    return contacts;
}

// private
//
void User::sipcWriteRead (QByteArray &in, QByteArray &out, QTcpSocket *socket)
{
    printf ("To write\n");
    printf ("%s\n", in.data());
    if (socket->state() not_eq QAbstractSocket::ConnectedState)
    {
        printf ("Error: socket is not connected\n");
        // TODO error handle
    }
    int length = 0;
    while (length < in.length())
    {
        length += socket->write (in.right (in.size() - length));
    }
    socket->waitForBytesWritten ();
    socket->flush();
    printf ("written\n");
    while (socket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket->waitForReadyRead ())
        {
            qDebug() << "When waitForReadyRead"
                     << socket->error() << socket->errorString();
            keepAliveTimer->stop();
            receiverTimer->stop();
            return;
        }
    }
    // FIXME The sever may send more than the reply
    // Ones is that someone invites this user, and then there comes
    // a piece of invitation too.
    // Fortunately, this case likely happens after keepAlive.

    out = socket->readAll();
    printf ("To read\n");
    printf ("%s\n", out.data());
}

//private slots
void User::keepAlive()
{
    QByteArray toSendMsg = keepAliveData (info->fetionNumber, info->callId);
    QByteArray responseData;
    qDebug() << "To keepAlive" << info->callId;
    receiverTimer->stop();
    receive();
    receiverTimer->start();
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    qDebug() << QString::fromUtf8 (responseData);
    //TODO Handle any responseData
    // stop timer if failed to send or get no return.
    // qDebug () << responseData;
}

void User::receive()
{
    if (sipcSocket->state() not_eq QAbstractSocket::ConnectedState)
    {
        printf ("socket closed.");
        return;
    }
    while (sipcSocket->bytesAvailable() < (int) sizeof (quint16))
    {
        if (not sipcSocket->waitForReadyRead (3000))
        {
//             qDebug() << "When waitForReadyRead"
//             << sipcSocket->error() << sipcSocket->errorString();
//             receiverTimer->stop();
            return;
        }
    }
    QByteArray responseData = sipcSocket->readAll();
    qDebug() << QString::fromUtf8 (responseData);
    // case 1
    /**
     * B N xxxxyyyy SIP-C/4.0
     * N: SyncUserInfoV4
     * L: 144
     * I: 2
     * Q: 1 BN

     * <events><event type="SyncUserInfo"><user-info><quotas><quota-sendsms>
     * <sendsms count="83"/></quota-sendsms></quotas></user-info></event></events>
     **/
    //FIXME
    // case 2 something like:
    /**
     * <buddy action="add" user-id="" >
     **/
}

void User::ssiLogin()
{
    qDebug() << "ssiLogin 1";
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
        qDebug() << "waitForEncrypted" << socket.errorString();
//             ssiLogin();
        return;
    }
    qDebug() << data;
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
    //FIXME get more data until <results/> is fully downloaded
    while (socket.waitForReadyRead())
        responseData += socket.readAll();
    parseServerConfig (responseData);
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
    qDebug() << "toSendMsg" << toSendMsg;
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
        if (not sipcSocket->waitForReadyRead (-1))
        {
            // TODO handle socket.error() or inform user what happened
            qDebug() << "sipcAuthorize waitForReadyRead"
                     << sipcSocket->error() << sipcSocket->errorString();
            return;
        }
    }
    while (sipcSocket->waitForReadyRead())
    {
        responseData += sipcSocket->readAll();
    }
    qDebug() << responseData;
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
    qDebug() << QString::fromUtf8 (domDoc.toByteArray (4));
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
    qDebug() << QString::fromUtf8 (domDoc.toByteArray (4));
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
                contacts.clear();
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
                        Contact *contact = new Contact;
                        contact->userId = domGrand.attribute ("i").toUtf8();
                        contact->groupId = domGrand.attribute ("l").toUtf8();
                        contact->localName = domGrand.attribute ("n").toUtf8();
//                         contact-> = domGrand.attribute ("o").toUtf8();
                        contact->identity = domGrand.attribute ("p").toUtf8();
                        contact->relationStatus =
                            domGrand.attribute ("r").toUtf8();
                        contact->sipuri = domGrand.attribute ("u").toUtf8();
                        contacts.append (contact);
                    }
                    domGrand = domGrand.nextSiblingElement ("b");
                }
                qDebug() << "Contacts Changed";
                emit contactsChanged();
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
    bool found = false;
    for (int i = 0; i < info->conversations.size(); ++i)
    {
        if (info->conversations.at (i)->contact.sipuri == toSipuri)
        {
            sipcWriteRead (toSendMsg, responseData,
                           info->conversations.at (i)->socket);
            found = true;
            break;
        }
    }
    if (not found)
    {
        sipcWriteRead (toSendMsg, responseData, sipcSocket);
    }
    // TODO handle responseData
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
}

// move another thread -- conversation
void User::inviteFriend (const QByteArray &sipUri)
{
    QByteArray toSendMsg = startChatData (info->fetionNumber, info->callId);
    QByteArray responseData;
    sipcWriteRead (toSendMsg, responseData, sipcSocket);
    if (responseData.isEmpty() or not responseData.contains ("A: CS address="))
    {
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
    toSendMsg.clear();
    responseData.clear();
    // TODO create new thread for conversation
    // or keep track of the socket
    toSendMsg = registerData (info->fetionNumber, info->callId, credential);
    QTcpSocket *socket = new QTcpSocket (this);
    socket->connectToHost (ip, port);
    if (not socket->waitForConnected (-1))
    {
        qDebug() << "waitForEncrypted" << socket->errorString();
        return;
    }
    sipcWriteRead (toSendMsg, responseData, socket);
    responseData.clear();
    toSendMsg.clear();
    toSendMsg = inviteBuddyData (info->fetionNumber, info->callId, sipUri);
    sipcWriteRead (toSendMsg, responseData, socket);
    qDebug() << responseData;
    // TODO check if 200
    //FIXME Check contact correctly
    UserInfo::Conversation *conversation = new UserInfo::Conversation;
    bool found = false;
    for (int i = 0; i < contacts.size(); i++)
    {
        if (contacts.at (i)->sipuri == sipUri)
        {
            // use deep copy
            conversation->contact = *contacts.at (i);
            found = true;
        }
    }
    if (not found)
    {
        delete conversation;
        delete socket;
        return;
    }
    conversation->socket = socket;
    info->conversations.append (conversation);
    QByteArray message ("Hi!");
    toSendMsg = catMsgData
                (info->fetionNumber, sipUri, info->callId, message);
    responseData.clear();
    sipcWriteRead (toSendMsg, responseData, socket);
    qDebug() << responseData;
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

void User::setClientState (StateType &state)
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
    keepAliveTimer->start (60000);//
    connect (receiverTimer, SIGNAL (timeout()), this, SLOT (receive()));
    receiverTimer->start (3000);
}

}

#include "user.moc"

