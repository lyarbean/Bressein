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

#include <QSslSocket>
#include <QDomDocument>
#include <QHostInfo>

#include <QTimer>
#include <QFuture>
#include "accountinfo.h"
#include "aux.h"
#include "transporter.h"
// N.B. all data through socket are utf-8
// N.B. we call buddyList contacts
namespace Bressein
{
Account::Account (QObject *parent) : QObject (parent), step (NONE)
{
    keepAliveTimer = new QTimer (this);
    messageTimer = new QTimer (this);
    connect (messageTimer, SIGNAL (timeout()),
             this, SLOT (dequeueMessages()));
    connect (messageTimer, SIGNAL (timeout()),
             this, SLOT (dispatchOutbox()));
    connect (messageTimer, SIGNAL (timeout()),
             this, SLOT (dispatchOutbox()));
    messageTimer->start (1000);
    info = new Info (this);
    serverTransporter = new Transporter (0);
    // serverTransporter should be running in its own thread
    conversationManager = new ConversationManager (this);
    connect (serverTransporter, SIGNAL (socketError (const int)),
             this, SLOT (onServerTransportError (const int)));
    connect (this, SIGNAL (ssiResponseParsed()), SLOT (systemConfig()));

    connect (this, SIGNAL (serverConfigParsed()), SLOT (sipcRegister()));
    //TODO move all to queueMessages and process them via onReceiveData
    connect (this, SIGNAL (sipcRegisterParsed()), SLOT (sipcAuthorize()));
    connect (this, SIGNAL (sipcAuthorizeParsed()), SLOT (activateTimer()));
    connect (conversationManager, SIGNAL (receiveData (const QByteArray &)),
             this, SLOT (queueMessages (const QByteArray &)),
             Qt::QueuedConnection);
    connect (serverTransporter, SIGNAL (dataReceived (const QByteArray &)),
             this, SLOT (queueMessages (const QByteArray &)),
             Qt::QueuedConnection);
    this->moveToThread (&workerThread);
    workerThread.start();
}

Account::~Account()
{

}

bool Account::operator== (const Account &other)
{
    return this->info->fetionNumber == other.info->fetionNumber;
}

void Account::setAccount (QByteArray number, QByteArray password)
{
    // TODO try to load saved configuration for this user
    if (number.isEmpty() or password.isEmpty()) return;
    QRegExp expression ("[1-9][0-9]*");
    if (not expression.exactMatch (number))
    {
        qDebug() << "invalid number";
        return;
    }
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

const Bressein::Contacts &Account::getContacts() const
{
    return contacts;
}

/**--------------**/
/** public slots **/
/**--------------**/
void Account::login()
{
    qDebug() << "To Login";
    QMetaObject::invokeMethod (this, "ssiLogin");
}

void Account::loginVerify (const QByteArray &code)
{
    mutex.lock();
    info->verification.code = code;
    mutex.unlock();
    QMetaObject::invokeMethod (this, "ssiVerify", Qt::QueuedConnection);
}

void Account::close()
{
    // note this is called from other thread
    keepAliveTimer->stop();
    keepAliveTimer->deleteLater();
    messageTimer->stop();
    messageTimer->deleteLater();
    serverTransporter->close();
    conversationManager->closeAll();
    disconnect (serverTransporter, SIGNAL (dataReceived (const QByteArray &)),
                this, SLOT (queueMessages (const QByteArray &)));
    disconnect (conversationManager, SIGNAL (receiveData (const QByteArray &)),
                this, SLOT (queueMessages (const QByteArray &)));
    conversationManager->deleteLater();

    if (not contacts.isEmpty())
    {
        QList<ContactInfo *> list = contacts.values();
        while (not list.isEmpty())
            delete list.takeFirst();
        contacts.clear();
    }
    while (not groups.isEmpty())
        delete groups.takeFirst();
    if (info)
    {
        delete info;
    }

    if (workerThread.isRunning())
    {
        workerThread.quit();
        workerThread.wait();
    }
    qDebug() << "clean up self";
//     this->deleteLater();
}


void Account::sendMessage (const QByteArray &toSipuri,
                           const QByteArray &message)
{
    // firstly we check the status of toSipuri
    mutex.lock();
    Letter *letter = new Letter;
    letter->receiver = toSipuri;
    letter->datetime = QDateTime::currentDateTime();
    letter->content = message;
    if (contacts.find (toSipuri).value()->basic.state == StateType::OFFLINE)
    {
        offlineBox.append (letter);
    }
    else
    {
        if (not conversationManager->isOnConversation (toSipuri))
        {
            QMetaObject::invokeMethod (this, "inviteFriend",
                                       Qt::QueuedConnection,
                                       Q_ARG (QByteArray, toSipuri));
        }
        outbox.append (letter);
    }
    mutex.unlock();
}

const Bressein::ContactInfo &Account::getContactInfo (const QByteArray &sipuri)
{
    return * contacts.value (sipuri);
}

/**---------**/
/** private **/
/**---------**/


/**---------------**/
/** private slots **/
/**---------------**/
void Account::keepAlive()
{
    QByteArray toSendMsg = keepAliveData (info->fetionNumber, info->callId);
    serverTransporter->sendData (toSendMsg);
    //send to all conversationa via conversationManager
    // FIXME not that keepAliveData
    qDebug() << "keep chats alive";
    QByteArray chatK = chatKeepAliveData (info->fetionNumber, info->callId);
    conversationManager->sendToAll (chatK);
}

void Account::ssiLogin()
{
    if (info->loginNumber.isEmpty()) return;
    step = SSI;
    //TODO if proxy
    QByteArray password = hashV4 (info->userId, info->password);
    QByteArray data = ssiLoginData (info->loginNumber, password);
    QSslSocket socket (this);
    socket.connectToHostEncrypted (UID_URI, 443);

    if (not socket.waitForEncrypted (-1))
    {
        // TODO handle socket.error() or inform user what happened
        qDebug() << "waitForEncrypted"
                 << socket.error() << socket.errorString();
        if (socket.error() == QAbstractSocket::SslHandshakeFailedError)
        {
            ssiLogin();
        }
        return;
    }
    int length = 0;
    while (length < data.length())
    {
        length += socket.write (data.right (data.size() - length));
    }
    socket.waitForBytesWritten ();
    socket.flush();
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
    length = responseData
             .mid (pos + delimit.size(), pos_ - pos - delimit.size())
             .toUInt (&ok);
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
    parseSsiResponse (responseData);
}

void Account::systemConfig()
{
    qDebug() << "systemConfig";
    step = SYSCONF;
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
    parseServerConfig (responseData);
}


void Account::downloadPortrait (const QByteArray &sipuri)
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
 * CL: type="pc" ,version="3.6.1900"
 **/
void Account::sipcRegister()
{
    step = SIPCR;
    //TODO move to utils
    int seperator = info->systemconfig.proxyIpPort.indexOf (':');
    QByteArray ip = info->systemconfig.proxyIpPort.left (seperator);
    quint16 port = info->systemconfig.proxyIpPort.mid (seperator + 1).toUInt();
    serverTransporter->connectToHost (ip, port);
    QByteArray toSendMsg ("R fetion.com.cn SIP-C/4.0\r\n");
    toSendMsg.append ("F: ").append (info->fetionNumber).append ("\r\n");
    toSendMsg.append ("I: ").append (QByteArray::number (info->callId++));
    toSendMsg.append ("\r\nQ: 2 R\r\nCN: ");
    toSendMsg.append (info->cnonce).append ("\r\nCL: type=\"pc\" ,version=\"");
    toSendMsg.append (PROTOCOL_VERSION + "\"\r\n\r\n");
    serverTransporter->sendData (toSendMsg);
}



/**
 * R fetion.com.cn SIP-C/4.0
 * F: 916*098834
 * I: 1
 * Q: 2 R
 * A: Digest response="5041619..6036118",algorithm="SHA1-sess-v4"
 * AK: ak-value
 * L: 426
 * \r\n\r\n
 * <body>
 **/
void Account::sipcAuthorize()
{
    qDebug() << "sipcAuthorize";
    step = SIPCA;
    QByteArray toSendMsg = sipcAuthorizeData
                           (info->loginNumber, info->fetionNumber,
                            info->userId, info->callId, info->response,
                            info->client.version,
                            info->client.customConfigVersion,
                            info->client.contactVersion, info->state, "");
    qDebug() << toSendMsg;
    serverTransporter->sendData (toSendMsg);
}

// functions for parsing data
void Account::parseSsiResponse (QByteArray &data)
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
        qDebug() << "=========================";
        qDebug() << "ssiLogin" << data;
        qDebug() << "=========================";
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

            QDomElement domE =  domRoot.firstChildElement ("verification");
            if (domE.hasAttribute ("algorithm") and
                domE.hasAttribute ("type=") and
                domE.hasAttribute ("text") and
                domE.hasAttribute ("tips"))
            {
                info->verification.algorithm =
                    domE.attribute ("algorithm").toUtf8();
                info->verification.text = domE.attribute ("text").toUtf8();
                info->verification.type = domE.attribute ("type").toUtf8();
                info->verification.tips = domE.attribute ("tips").toUtf8();
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

void Account::parseSipcRegister (QByteArray &data)
{
    if (not data.startsWith ("SIP-C/4.0 401 Unauthoried"))
    {
        qDebug() << "Wrong Sipc register response.";
        qDebug() << data;
        return;
    }

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
    QByteArray response;
    RSAPublicEncrypt (info->userId, info->password,
                      info->nonce, info->aeskey, info->key, response);
    if (response.isEmpty())
    {
        qDebug() << "response is empty";
        return;
    }
    info->response = response.toUpper();
    emit sipcRegisterParsed();
}


void Account::parseServerConfig (QByteArray &data)
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
        qDebug() << "serverConfigParsed";
        emit serverConfigParsed();
    }
}


void Account::parseSipcAuthorize (QByteArray &data)
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
    qDebug() << "parseSipcAuthorize";
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

void Account::ssiPic()
{
    QByteArray data = SsiPicData (info->verification.algorithm, info->ssic);
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
            info->verification.id = domRoot.attribute ("id").toUtf8();
            info->verification.pic = domRoot.attribute ("pic").toUtf8();
            // base-64 encoded
        }
        // emit signal to inform user to inout code
    }
    else
    {
        // TODO
    }

}

void Account::ssiVerify()
{
    QByteArray password = (hashV4 (info->userId, info->password));
    QByteArray data = ssiVerifyData (info->loginNumber, password,
                                     info->verification.id,
                                     info->verification.code,
                                     info->verification.algorithm);
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

void Account::contactInfo (const QByteArray &userId)
{
    QByteArray toSendMsg = contactInfoData
                           (info->fetionNumber, userId, info->callId);
    serverTransporter->sendData (toSendMsg);
    //TODO handle responseData
    // carrier-region
}

void Account::contactInfo (const QByteArray &Number, bool mobile)
{
    QByteArray toSendMsg = contactInfoData
                           (info->fetionNumber, Number, info->callId, mobile);
    serverTransporter->sendData (toSendMsg);
    //TODO handle responseData
    // carrier-region
}



void Account::addBuddy (
    const QByteArray &number,
    QByteArray buddyLists,
    QByteArray localName,
    QByteArray desc,
    QByteArray phraseId)
{
    QByteArray toSendMsg = addBuddyV4Data (info->fetionNumber, number,
                                           info->callId, buddyLists, localName,
                                           desc, phraseId);
    serverTransporter->sendData (toSendMsg);
    // TODO handle responseData
}

void Account::deleteBuddy (const QByteArray &userId)
{
    QByteArray toSendMsg = deleteBuddyV4Data
                           (info->fetionNumber, userId, info->callId);
    serverTransporter->sendData (toSendMsg);
    // TODO handle responseData
}

void Account::contactSubscribe()
{
    QByteArray toSendMsg = presenceV4Data (info->fetionNumber, info->callId);
    serverTransporter->sendData (toSendMsg);
}

// move another thread -- conversation
void Account::inviteFriend (const QByteArray &sipuri)
{
    toInvite = sipuri;
    QByteArray toSendMsg = startChatData (info->fetionNumber, info->callId);
    serverTransporter->sendData (toSendMsg);
}

void Account::createBuddylist (const QByteArray &name)
{
    QByteArray toSendMsg = createBuddyListData
                           (info->fetionNumber, info->callId, name);

    serverTransporter->sendData (toSendMsg);
    // TODO handle responseData, get version, name, id
}

void Account::deleteBuddylist (const QByteArray &id)
{
    QByteArray toSendMsg = deleteBuddyListData
                           (info->fetionNumber, info->callId, id);
    serverTransporter->sendData (toSendMsg);
    // TODO handle responseData, get version, name, id
}

void Account::renameBuddylist (const QByteArray &id, const QByteArray &name)
{
    QByteArray toSendMsg = setBuddyListInfoData
                           (info->fetionNumber, info->callId, id, name);
    serverTransporter->sendData (toSendMsg);
    // TODO
}

void Account::updateInfo()
{
    QByteArray toSendMsg = setUserInfoV4Data (info->fetionNumber, info->callId,
                                              info->client.impresa,
                                              info->client.nickname,
                                              info->client.gender,
                                              info->client.customConfig,
                                              info->client.customConfigVersion);
    serverTransporter->sendData (toSendMsg);
    // TODO handle responseData
}

void Account::setImpresa (const QByteArray &impresa)
{
    QByteArray toSendMsg = setUserInfoV4Data (info->fetionNumber, info->callId,
                                              impresa, info->client.version,
                                              info->client.customConfig,
                                              info->client.customConfigVersion);
    serverTransporter->sendData (toSendMsg);
    // TODO handle responseData
}

void Account::setMessageStatus (int days)
{
    QByteArray toSendMsg = setUserInfoV4Data
                           (info->fetionNumber, info->callId, days);
    serverTransporter->sendData (toSendMsg);
}

void Account::setClientState (StateType state)
{
    QByteArray statetype = QByteArray::number ( (int) state);
    QByteArray toSendMsg = setPresenceV4Data
                           (info->fetionNumber, info->callId, statetype);
    serverTransporter->sendData (toSendMsg);
}

void Account::activateTimer()
{
    //FIXME should the timer in this thread?
    connect (keepAliveTimer, SIGNAL (timeout()), SLOT (keepAlive()));
    keepAliveTimer->start (60000);
    // call other stuffs right here
    contactSubscribe();
}


void Account::onServerTransportError (const int se)
{
    switch (se)
    {
        case QAbstractSocket::ConnectionRefusedError:
        case QAbstractSocket::RemoteHostClosedError:
        case QAbstractSocket::HostNotFoundError:
            // TODO
            //  close();
            break;
        default:
            break;
    }
}

void Account::queueMessages (const QByteArray &receiveData)
{
    mutex.lock();
    receivedMessages.append (receiveData);
    mutex.unlock();
}

void Account::dequeueMessages()
{
    qDebug() << "dequeueMessages";
    mutex.lock();
    QByteArray data;
    bool empty = receivedMessages.isEmpty();
    if (not empty)
        data = receivedMessages.takeFirst();
    mutex.unlock();
    while (not empty)
    {
        qDebug() << "==============================";
        qDebug() << "while (not empty)";
        qDebug() << data;
        if (not data.isEmpty())
        {
            parseReceivedData (data);
        }
        mutex.lock();
        if (not receivedMessages.isEmpty())
        {
            data = receivedMessages.takeFirst();
        }
        else
        {
            empty = true;
        }
        mutex.unlock();
        qDebug() << "==============================";
    }
}


void Account::dispatchOutbox()
{
    qDebug() << "dispatchLetters";
    mutex.lock();
    Letter *data;
    bool empty = outbox.isEmpty();
    QByteArray sipuri;
    //datetime?
    QByteArray content;
    QByteArray toSendMsg;
    if (not empty)
    {
        data = outbox.takeFirst();
        sipuri = data->receiver;
        content = data->content;
        drafts.append (data);
        toSendMsg = catMsgData (info->fetionNumber, sipuri,
                                info->callId, content);
    }
    mutex.unlock();
    while (not empty)
    {
        qDebug() << "==============================";
        qDebug() << "while (not empty)";
        qDebug() << sipuri;
        qDebug() << content;
        if (not toSendMsg.isEmpty())
        {

            serverTransporter->sendData (toSendMsg);
        }
        mutex.lock();
        if (not outbox.isEmpty())
        {
            data = outbox.takeFirst();
            sipuri = data->receiver;
            content = data->content;
            drafts.append (data);
            toSendMsg = catMsgData (info->fetionNumber, sipuri,
                                    info->callId, content);
        }
        else
        {
            empty = true;
        }
        mutex.unlock();
        qDebug() << "==============================";
    }
}

void Account::dispatchOfflineBox()
{
    qDebug() << "dispatchLetters";
    mutex.lock();
    Letter *data;
    bool empty = offlineBox.isEmpty();
    QByteArray sipuri;
    //datetime?
    QByteArray content;
    QByteArray toSendMsg;
    if (not empty)
    {
        data = offlineBox.takeFirst();
        sipuri = data->receiver;
        content = data->content;
        drafts.append (data);
        toSendMsg = sendCatMsgPhoneData (info->fetionNumber, sipuri,
                                         info->callId, content,"","");
    }
    mutex.unlock();
    while (not empty)
    {
        qDebug() << "==============================";
        qDebug() << "while (not empty)";
        qDebug() << sipuri;
        qDebug() << content;
        if (not toSendMsg.isEmpty())
        {

            serverTransporter->sendData (toSendMsg);
        }
        mutex.lock();
        if (not offlineBox.isEmpty())
        {
            data = offlineBox.takeFirst();
            sipuri = data->receiver;
            content = data->content;
            drafts.append (data);
            toSendMsg = sendCatMsgPhoneData (info->fetionNumber, sipuri,
                                             info->callId, content,"","");
        }
        else
        {
            empty = true;
        }
        mutex.unlock();
        qDebug() << "==============================";
    }
}

void Account::parseReceivedData (const QByteArray &receiveData)
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
    QByteArray data = receiveData;
    int space = data.indexOf (" ");
    QByteArray code = data.left (space);
    qDebug() << "code" << code;
    if (code == "M")
    {
        // handle Message
        onReceivedMessage (data);
    }
    else if (code == "BN")
    {
        int b = data.indexOf ("N: ");
        int e = data.indexOf ("\r\n", b);
        QByteArray event = data.mid (b + 3, e - b - 3);
        if (event == "PresenceV4")
        {
            onBNPresenceV4 (data);
        }
        else if (event == "SyncUserInfoV4")
        {
            onBNSyncUserInfoV4 (data);
        }
        else if (event == "Conversation")
        {
            onBNConversation (data);
        }
        else if (event == "contact")
        {
            onBNcontact (data);
        }
        else if (event == "registration")
        {
            onBNregistration (data);
        }
        else if (event == "PGGroup")
        {
            onBNPGGroup (data);
        }
    }
    else if (code == "I")
    {
        onInvite (data);
    }
    else if (code == "IN")
    {
        onIncoming (data);
    }
    else if (code == "O")
    {
        qDebug() << data;
    }
    else if (code == "SIP-C/4.0")
    {
        if (data.startsWith ("SIP-C/4.0 401 Unauthoried") and
            data.contains ("\r\nW: Digest") and step == SIPCR)
        {
            qDebug() << "parseSipcRegister";
            qDebug() << data;
            qDebug() << "=========";
            parseSipcRegister (data);
        }
        else if (data.startsWith ("SIP-C/4.0 200 OK") and
                 data.contains ("<client") and step == SIPCA)
        {
            parseSipcAuthorize (data);
        }
        else if (data.startsWith ("SIP-C/4.0 200 OK") and
                 data.contains ("A: CS address=")) // right after startChat
        {
            onStartChat (data);
        }
        else
        {
            qDebug() << data;
        }
    }
    else
    {
        qDebug() << data;
    }
//     onReceiveData();
    // blocking?
}

void Account::onReceivedMessage (const QByteArray &data)
{
    // save to dataBase
    // do reply
    qDebug() << data;
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
    QByteArray reply = "SIP-C/4.0 200 OK\r\n";
    if (fromSipuri.contains ("PG"))
    {
        reply.append ("I: ");
        reply.append (callId);
        reply.append ("\r\nQ: ");
        reply.append (sequence);
        reply.append ("\r\nF: ");
        reply.append (fromSipuri);
        reply.append ("\r\n\r\n");
    }
    else
    {
        reply.append ("F: ");
        reply.append (fromSipuri);
        reply.append ("\r\nI: ");
        reply.append (callId);
        reply.append ("\r\nQ: ");
        reply.append (sequence);
        reply.append ("\r\n\r\n");
    }

    // ?fromsiprui , callid , sequence
    if (conversationManager->isOnConversation (fromSipuri))
    {
        conversationManager->sendData (fromSipuri, reply);
    }
    else
    {
        qDebug() << "reply message via sipsocket";
        serverTransporter->sendData (reply);
    }
}

void Account::onBNPresenceV4 (const QByteArray &data)
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
                downloadPortrait (sipuri);
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


void Account::onBNConversation (const QByteArray &data)
{
    qDebug() << "onBNConversation";
    //<events><event type="UserFailed"><member uri="sip:xxxxx@fetion.com.cn;p=aaaa" status="502"/></event></events>
    // when UserEntered, UserFailed, UserLeft, we destroy the conversation corresponding.
    qDebug() << data;
    // if UserLeft, get node member, if ok, get property uri, then remove
    // inform conversationManager to remove this sipuri (uri).
}

void Account::onBNSyncUserInfoV4 (const QByteArray &data)
{
    qDebug() << "onBNSyncUserInfoV4";
    qDebug() << data;
    // TODO
    // get node "buddy", if ok, process the data
    // get properties action, (if action is add) user-id, uri, relation-status
    //
}

void Account::onBNcontact (const QByteArray &data)
{
    qDebug() << "onBNcontact";
    qDebug() << data;
    // TODO
    // get node "application", if ok, process the data
    // get properties uri, user-id, desc, addbuddy-phrase-id
    // inform to create new chat room
}

void Account::onBNregistration (const QByteArray &data)
{
    qDebug() << "onBNPGGroup";
    // TODO
    // if event type is deregistered, we nnforced to quit, reset all state
    qDebug() << data;
}

void Account::onBNPGGroup (const QByteArray &data)
{
    qDebug() << "onBNPGGroup";
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

void Account::onInvite (const QByteArray &data)
{
    qDebug() << "onInvite";
    qDebug() << receivedMessages;
    qDebug() << "==========================";
    qDebug() << data;
    qDebug() << "==========================";
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
    serverTransporter->sendData (reply);
    QByteArray toSendMsg =
        registerData (info->fetionNumber, info->callId, credential);
    conversationManager->setHost (fromSipuri, ip, port);
    conversationManager->sendData (fromSipuri, toSendMsg);
}

void Account::onStartChat (const QByteArray &data)
{
    // move following code to parsing part
    QByteArray sipuri = toInvite;
    toInvite.clear();
    if (sipuri.isEmpty())
    {
        qDebug() << "toInvite is empty================";
        return;
    }
    int b, e;
    b = data.indexOf ("A: CS address=\"");
    e = data.indexOf (";", b);
    QByteArray address = data.mid (b + 15, e - b - 15);
    b = data.indexOf ("credential=\"");
    e = data.indexOf ("\"", b + 12);
    QByteArray credential = data.mid (b + 12, e - b - 12);
    int seperator = address.indexOf (':');
    QByteArray ip = address.left (seperator);
    quint16 port = address.mid (seperator + 1).toUInt();
    qDebug() << "registerData";
    QByteArray toSendMsg;
    toSendMsg = registerData (info->fetionNumber, info->callId, credential);
    // append conversation to conversation manager
    /** **/
    conversationManager->addConversation (sipuri);
    conversationManager->setHost (sipuri, ip, port);
    conversationManager->sendData (sipuri, toSendMsg);
    toSendMsg = inviteBuddyData (info->fetionNumber, info->callId, sipuri);
    conversationManager->sendData (sipuri, toSendMsg);

    // TODO check if 200
    //FIXME Check contact correctly
    //     QByteArray message ("Hi!");
    //     toSendMsg = catMsgData
    //             (info->fetionNumber, sipuri, info->callId, message);
    //     data.clear();
    //     sipcWriteRead (toSendMsg, data, sipsocket);
    //     qDebug() << data;
    // demo only

    sendMessage (sipuri, "Hello, This is from Bressein!");
}

void Account::onIncoming (const QByteArray &data)
{
    qDebug() << "onIncoming";
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

void Account::onSipc (const QByteArray &data)
{
    qDebug() << "onSipc";
    qDebug() << data;
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

void Account::onOption (const QByteArray &data)
{
    qDebug() << "onOption";
    qDebug() << data;
}

void Account::parsePGGroupMembers (const QByteArray &data)
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



} // end Bressein

#include "account.moc"

