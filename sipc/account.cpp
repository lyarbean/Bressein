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
// N.B. all data through socket are utf-8 encoded
// N.B. we call buddyList contacts
namespace Bressein
{
Account::Account (QObject *parent) : QObject (parent)
{
    keepAliveTimer = new QTimer (this);
    messageTimer = new QTimer (this);
    draftsClearTimer = new QTimer (this);
    connect (messageTimer, SIGNAL (timeout()),
             this, SLOT (dequeueMessages()));
    connect (messageTimer, SIGNAL (timeout()),
             this, SLOT (dispatchOutbox()));
    connect (messageTimer, SIGNAL (timeout()),
             this, SLOT (dispatchOfflineBox()));
    connect (draftsClearTimer, SIGNAL (timeout()),
             this, SLOT (clearDrafts()));
    messageTimer->start (1000);

    publicInfo = new ContactInfo;
    info = new Info (this);
    serverTransporter = new Transporter (0);
    conversationManager = new ConversationManager (this);
    connect (conversationManager, SIGNAL (receiveData (const QByteArray &)),
             this, SLOT (queueMessages (const QByteArray &)),
             Qt::QueuedConnection);
    connect (serverTransporter, SIGNAL (dataReceived (const QByteArray &)),
             this, SLOT (queueMessages (const QByteArray &)),
             Qt::QueuedConnection);
    connect (serverTransporter, SIGNAL (socketError (const int)),
             this, SLOT (onServerTransportError (const int)),
             Qt::QueuedConnection);
    connect (&fetcher, SIGNAL (processed (const QByteArray &)),
             this, SLOT (onPortraitDownloaded (const QByteArray &)),
             Qt::QueuedConnection);
    connect (this, SIGNAL (serverConfigParsed()), SLOT (ssiLogin()));
    connect (this, SIGNAL (ssiResponseParsed()), SLOT (sipcRegister()));
    connect (this, SIGNAL (sipcRegisterParsed()), SLOT (sipcAuthorize()));
    connect (this, SIGNAL (sipcAuthorizeParsed()), SLOT (activateTimer()));
    // move to slave thread
    this->moveToThread (&workerThread);
    workerThread.start();
}

Account::~Account()
{

}

bool Account::operator== (const Account &other)
{
    return this->info->sipuri == other.info->sipuri;
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
        publicInfo->mobileno = number;
    }
    else
    {
        info->fetionNumber = number;
    }
    info->password = password;
    publicInfo->state = StateType::ONLINE;// online
    info->cnouce = cnouce();
    info->callId = 1;
    connected = false;
    groups.clear();
    //add group self
    Group *group = new Group;
    group->groupId = "-1";
    group->groupname = "Myself";
    groups.append (group);
    emit groupChanged (group->groupId, group->groupname);
    group->groupId = "0";
    group->groupname = "Untitled";
    groups.append (group);
    emit groupChanged (group->groupId, group->groupname);
    publicInfo->groupId = "-1";
}

void Account::getFetion (QByteArray &out) const
{
    out = sipToFetion (info->sipuri);
}

void Account::getNickname (QByteArray &name) const
{
    name = publicInfo->nickName;
}

void Account::getContactInfo (const QByteArray &sipuri,
                              ContactInfo &contactInfo)
{
    mutex.lock();
    if (contacts.contains (sipuri))
    {
        contactInfo = * contacts.value (sipuri);
    }
    mutex.unlock();
}
/**--------------**/
/** public slots **/
/**--------------**/
void Account::login()
{
    qDebug() << "To Login";
//     QMetaObject::invokeMethod (this, "ssiLogin", Qt::QueuedConnection);
    QMetaObject::invokeMethod (this, "systemConfig", Qt::QueuedConnection);
}

void Account::loginVerify (const QByteArray &code)
{
    qDebug() << "loginVerify" << code;
    mutex.lock();
    info->verification.code = code;
    mutex.unlock();
    QMetaObject::invokeMethod (this, "ssiLogin", Qt::QueuedConnection);

}

void Account::close()
{
    // note this is called from other thread
    keepAliveTimer->stop();
    keepAliveTimer->deleteLater();
    messageTimer->stop();
    messageTimer->deleteLater();
    draftsClearTimer->stop();
    draftsClearTimer->deleteLater();
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
    if (publicInfo)
    {
        delete publicInfo;
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
    if (toSipuri == info->sipuri)
    {
        //
        qDebug() << "Msg Self" << toSipuri;
        QByteArray toSendMsg = sendCatMsgSelfData (info->fetionNumber,
                                                   info->sipuri,
                                                   info->callId, message);
        serverTransporter->sendData (toSendMsg);
        return;
    }
    Letter *letter = new Letter;
    letter->receiver = toSipuri;
    letter->datetime = QDateTime::currentDateTime();
    letter->content = message;
    if (contacts.find (toSipuri).value()->state < StateType::HIDDEN)
    {
        qDebug() << "append to offlineBox" << (int) contacts.find (toSipuri).value()->state;
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


void Account::setOnline()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 400));
}

void Account::setRightback()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 300));
}

void Account::setAway()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 100));
}

void Account::setBusy()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 600));
}

void Account::setOutforlunch()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 500));
}

void Account::setOnthephone()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 150));
}

void Account::setMeeting()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 850));
}

void Account::setDonotdisturb()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 800));
}

void Account::setHidden()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, 0));
}

void Account::setOffline()
{
    QMetaObject::invokeMethod (this, "setClientState",
                               Qt::QueuedConnection,
                               Q_ARG (int, -1));
}


/**---------------**/
/** private slots **/
/**---------------**/
void Account::keepAlive()
{
    QByteArray toSendMsg = keepAliveData (info->fetionNumber, info->callId);
    serverTransporter->sendData (toSendMsg);
    //send to all conversationa via conversationManager
    // FIXME not that keepAliveData
    QByteArray chatK = chatKeepAliveData (info->fetionNumber, info->callId);
    conversationManager->sendToAll (chatK);
}

void Account::ssiLogin()
{
    if (info->loginNumber.isEmpty()) return;
//     step = SSI;
    //TODO if proxy
    QByteArray password = hashV4 (publicInfo->userId, info->password);
    QByteArray data;
    data = ssiData (info->loginNumber,
                    password,
                    info->verification.id,
                    info->verification.code,
                    info->verification.algorithm);
    QSslSocket socket (this);
    socket.connectToHostEncrypted (UID_URI, 443);
    qDebug() << "ssiLogin" << data;
    if (not socket.waitForEncrypted (8000))
    {
        // TODO handle socket.error() or inform user what happened
        qDebug() << "waitForEncrypted"
                 << socket.error() << socket.errorString();
        if (socket.error() == QAbstractSocket::HostNotFoundError)
        {
            //TODO emit onHostNotFound();
        }
        if (socket.error() == QAbstractSocket::SslHandshakeFailedError or
            socket.error() == QAbstractSocket::SocketTimeoutError or
            socket.error() == QAbstractSocket::UnknownSocketError)
        {
            qDebug() << "waitForEncrypted";
            socket.close();
            ssiLogin();
        }
        // emit login failed
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
    qDebug() << "parseSsiResponse";
    parseSsiResponse (responseData);
}

void Account::systemConfig()
{
    qDebug() << "systemConfig";
//     step = SYSCONF;
    QByteArray body = configData (info->loginNumber);
    qDebug() << body;
    QTcpSocket socket (this);
    QHostInfo info = QHostInfo::fromName ("nav.fetion.com.cn");
    if (info.addresses().isEmpty())
    {
        //FIXME
        return;
    }
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
    qDebug() << "downloadPortrait"<<sipuri;
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
//     step = SIPCR;
    qDebug() << "sipcRegister";

    int seperator = info->systemconfig.proxyIpPort.indexOf (':');
    QByteArray ip = info->systemconfig.proxyIpPort.left (seperator);
    quint16 port = info->systemconfig.proxyIpPort.mid (seperator + 1).toUInt();
    serverTransporter->connectToHost (ip, port);
    QByteArray toSendMsg ("R fetion.com.cn SIP-C/4.0\r\n");
    toSendMsg.append ("F: ").append (info->fetionNumber).append ("\r\n");
    toSendMsg.append ("I: ").append (QByteArray::number (info->callId++));
    toSendMsg.append ("\r\nQ: 2 R\r\nCN: ");
    toSendMsg.append (info->cnouce).append ("\r\nCL: type=\"pc\" ,version=\"");
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
//     step = SIPCA;
    QByteArray ackdata;
    if (not info->verification.code.isEmpty())
    {
        ackdata = "A: Verify algorithm=\"";
        ackdata.append (info->verification.algorithm);
        ackdata.append ("\",type=\"");
        ackdata.append ("GeneralPic");
        ackdata.append ("\",code=\"");
        ackdata.append (info->verification.code);
        ackdata.append ("\",chid=\"");
        ackdata.append (info->verification.id);
        ackdata.append ("\"\r\n");
    }
    QByteArray toSendMsg = sipcAuthorizeData
                           (info->loginNumber, info->fetionNumber,
                            publicInfo->userId, info->callId, info->response,
                            info->client.version,
                            info->client.customConfigVersion,
                            info->client.contactVersion,
                            QByteArray::number (publicInfo->state), ackdata);
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
    qDebug() << QString::fromUtf8 (data);

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
            qDebug() << "requires validation";
            QDomElement domE =  domRoot.firstChildElement ("verification");
            if (domE.hasAttribute ("algorithm") and
                domE.hasAttribute ("type") and
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
        }
        else if (statusCode == "401" or statusCode == "404")
        {
            //401 wrong password
            //TODO inform user that password is wrong
            //404
            //TODO as user to re-input password
            // emit wrong password
            emit wrongPassword();
            return;
        }
        else if (statusCode == "200")
        {
            int b = data.indexOf ("Set-Cookie: ssic=");
            int e = data.indexOf ("; path", b);
            info->ssic = data.mid (b + 17, e - b - 17);
            fetcher.setData (info->systemconfig.portraitServerName,
                             info->systemconfig.portraitServerPath,
                             info->ssic);
            QDomElement domChild =  domRoot.firstChildElement ("user");

            if (domChild.hasAttribute ("uri") and
                domChild.hasAttribute ("mobile-no") and
                domChild.hasAttribute ("user-status") and
                domChild.hasAttribute ("user-id"))
            {
                info->sipuri = domChild.attribute ("uri").toUtf8();
                info->fetionNumber = sipToFetion (info->sipuri);
                publicInfo->mobileno = domChild.attribute ("mobile-no").toUtf8();
                // info->state = domE.attribute ("user-status").toUtf8();
                publicInfo->userId = domChild.attribute ("user-id").toUtf8();
                contacts.insert (info->sipuri, publicInfo);
                emit contactChanged (info->sipuri);
            }
            else
            {
                qDebug() << "fail to pass ssi response";
                ssiLogin();
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
    info->aeskey = QByteArray::fromHex (cnouce (8)).leftJustified (32, 'A', true);
    // need to store?
    // generate response
    // check if emptyq
    bool ok = false;
    // FIXME sometimes RSAPublicEncrypt fails
    while (not ok)
    {
        RSAPublicEncrypt (publicInfo->userId, info->password,
                          info->nonce, info->aeskey, info->key,
                          info->response, ok);
        qDebug() << "RSAPublicEncrypt";
    }
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
                publicInfo->birthdate =
                    domChild.attribute ("birth-date").toUtf8();
                info->client.carrierRegion =
                    domChild.attribute ("carrier-region").toUtf8();
                publicInfo->carrierStatus =
                    domChild.attribute ("carrier-status").toUtf8();
                publicInfo->gender = domChild.attribute ("gender").toUtf8();
                publicInfo->impresa = domChild.attribute ("impresa").toUtf8();
                publicInfo->mobileno =
                    domChild.attribute ("mobile-no").toUtf8();
                publicInfo->nickName =
                    domChild.attribute ("nickname").toUtf8();
                info->client.smsOnLineStatus =
                    domChild.attribute ("sms-online-status").toUtf8();
                info->client.version = domChild.attribute ("version").toUtf8();
                emit contactChanged (info->sipuri);
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
                    emit groupChanged (group->groupId, group->groupname);
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
                        contact->state = StateType::HIDDEN;
                        contact->userId =
                            domGrand.attribute ("i").toUtf8();
                        // get fetionNumber from userId
                        contact->groupId =
                            domGrand.attribute ("l").toUtf8();
                        contact->localName =
                            domGrand.attribute ("n").toUtf8();
//                         contact-> = domGrand.attribute ("o").toUtf8();
                        contact->identity =
                            domGrand.attribute ("p").toUtf8();
                        contact->relationStatus =
                            domGrand.attribute ("r").toUtf8();
                        QByteArray sipuri = domGrand.attribute ("u").toUtf8();
                        contacts.insert (sipuri, contact);
                        downloadPortrait (sipuri);
                        emit contactChanged (sipuri);
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
    emit contactChanged (info->sipuri);
}

void Account::ssiPic()
{
    qDebug() << "ssiPic";
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
    while (socket.bytesAvailable() < (int) sizeof (quint16))
    {
        if (not socket.waitForReadyRead (1000))
        {
            if (socket.error() not_eq QAbstractSocket::SocketTimeoutError)
            {
                qDebug() << "When waitForReadyRead"
                         << socket.error() << socket.errorString();
            }
            return;
        }
    }
    QByteArray responseData = socket.readAll ();
    bool ok;
    int length = 0;
    int pos = 0;
    int pos_ = 0;
    int seperator;
    QByteArray delimit = "Content-Length: ";

    while (not responseData.contains (delimit) or
           not responseData.contains ("\r\n\r\n"))
    {
        responseData.append (socket.readLine());
    }
    seperator = responseData.indexOf ("\r\n\r\n");
    pos = responseData.indexOf (delimit);
    pos_ = responseData.indexOf ("\r\n", pos);
    length = responseData
             .mid (pos + delimit.size(), pos_ - pos - delimit.size())
             .toUInt (&ok);
    int received = responseData.size();
    qDebug() << length;
    while (received < length + seperator + 4)
    {
        while (socket.bytesAvailable() < (int) sizeof (quint16))
        {
            if (not socket.waitForReadyRead (1000))
            {
                if (socket.error() not_eq QAbstractSocket::SocketTimeoutError)
                {
                    qDebug() << "When waitForReadyRead"
                             << socket.error() << socket.errorString();
                }
            }
        }
        responseData.append (socket.read (length + seperator + 4 - received));
        received = responseData.size();
    }
    qDebug() << responseData;
    int b = responseData.indexOf ("<results>");
    int e = responseData.indexOf ("</results>");
    QByteArray xml = responseData.mid (b, e - b + 10);
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine, errorColumn;
    ok = domDoc.setContent
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
            emit verificationPic (info->verification.pic);
        }
        // emit signal to inform user to inout code
    }
    else
    {
        // TODO
    }
    qDebug() << responseData;
}

void Account::ssiVerify()
{
//     QByteArray password = (hashV4 (publicInfo->userId, info->password));
//     QByteArray data = ssiVerifyData (info->loginNumber, password,
//             info->verification.id,
//             info->verification.code,
//             info->verification.algorithm);
//     qDebug() << data;
//     QSslSocket socket (this);
//     socket.connectToHostEncrypted (UID_URI, 443);
//
//     if (not socket.waitForEncrypted (-1))
//     {
//         // TODO handle socket.error() or inform user what happened
//         qDebug() << "waitForEncrypted" << socket.errorString();
//         //             ssiLogin();
//         return;
//     }
//     socket.write (data);
//     QByteArray responseData;
//     while (socket.bytesAvailable() < (int) sizeof (quint16))
//     {
//         if (not socket.waitForReadyRead (-1))
//         {
//             // TODO handle socket.error() or inform user what happened
//             qDebug() << "ssiLogin  waitForReadyRead"
//             << socket.error() << socket.errorString();
//         }
//     }
//     responseData = socket.readAll();
//     parseSsiResponse (responseData);
//     QByteArray ackdata = "A: Verify algorithm=\"";
//     ackdata.append (info->verification.algorithm);
//     ackdata.append ("\",type=\"");
//     ackdata.append ("GeneralPic");
//     ackdata.append ("\",response=\"");
//     ackdata.append (info->verification.code);
//     ackdata.append (",chid=\"");
//     ackdata.append (info->verification.id);
//     ackdata.append ("\"");
//     QByteArray toSendMsg = sipcAuthorizeData
//             (info->loginNumber, info->fetionNumber,
//                     publicInfo->userId, info->callId, info->response,
//                     info->client.version,
//                     info->client.customConfigVersion,
//                     info->client.contactVersion,
//                     QByteArray::number (publicInfo->state), ackdata);
//     serverTransporter->sendData (toSendMsg);
    sipcAuthorizeParsed();
}

void Account::contactInfo (const QByteArray &userId)
{
    QByteArray toSendMsg = contactInfoData
                           (info->fetionNumber, userId, info->callId);
    serverTransporter->sendData (toSendMsg);
}

void Account::contactInfo (const QByteArray &Number, bool mobile)
{
    QByteArray toSendMsg = contactInfoData
                           (info->fetionNumber, Number, info->callId, mobile);
    serverTransporter->sendData (toSendMsg);
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
    qDebug() << "on contactSubscribe";
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
                                              publicInfo->impresa,
                                              publicInfo->nickName,
                                              publicInfo->gender,
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

void Account::setClientState (int state)
{
    publicInfo->state = (StateType) state;
    if (connected)
    {
        QByteArray statetype = QByteArray::number (state);
        QByteArray toSendMsg = setPresenceV4Data
                               (info->fetionNumber, info->callId, statetype);
        serverTransporter->sendData (toSendMsg);
    }
    else
    {
        qDebug() << "relogin";
        ssiLogin();
    }
}

void Account::activateTimer()
{
    //FIXME should the timer in this thread?
    connect (keepAliveTimer, SIGNAL (timeout()), SLOT (keepAlive()));
    connected = true;
    contactSubscribe();
    keepAliveTimer->start (60000);
    draftsClearTimer->start (15000);
    downloadPortrait (info->sipuri);
    emit logined ();
    // call other stuffs right here
}


void Account::onServerTransportError (const int se)
{
    qDebug() << "onServerTransportError" << se;
    switch (se)
    {
        case QAbstractSocket::ConnectionRefusedError:
            login();
            break;
        case QAbstractSocket::RemoteHostClosedError:
            // TODO need confirm from user
            // relogin
            qDebug() << "RemoteHostClosedError";
            keepAliveTimer->stop();
            draftsClearTimer->stop();
            conversationManager->closeAll();
            serverTransporter->stop();
            break;
        case QAbstractSocket::HostNotFoundError:
            qDebug() << "HostNotFoundError";
            // TODO
            //login();
            //  close();
            break;
        case QAbstractSocket::NetworkError:
            qDebug() << "NetworkError";
            break;
        default:
            break;
    }
}

void Account::queueMessages (const QByteArray &receiveData)
{
    mutex.lock();
    inbox.append (receiveData);
    mutex.unlock();
}

void Account::dequeueMessages()
{
    mutex.lock();
    QByteArray data;
    bool empty = inbox.isEmpty();
    if (not empty)
        data = inbox.takeFirst();
    mutex.unlock();
    while (not empty)
    {
        qDebug() << "==============================";
        qDebug() << "dequeueMessages while (not empty)";
        qDebug() << QString::fromUtf8 (data);
        if (not data.isEmpty())
        {
            parseReceivedData (data);
        }
        mutex.lock();
        if (not inbox.isEmpty())
        {
            data = inbox.takeFirst();
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
    // TODO: move to drafts first
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
        drafts.append (data);
        data->callId = info->callId;
        sipuri = data->receiver;
        content = data->content;
        toSendMsg = catMsgData (info->fetionNumber, sipuri,
                                info->callId, content);
    }
    mutex.unlock();
    while (not empty)
    {
        qDebug() << "dispatchOutbox while (not empty)";
        qDebug() << "sipuri" << sipuri;
        qDebug() << "content" << QString::fromUtf8 (content);
        if (not toSendMsg.isEmpty())
        {
            serverTransporter->sendData (toSendMsg);
        }
        mutex.lock();
        if (not outbox.isEmpty())
        {
            data = outbox.takeFirst();
            drafts.append (data);
            data->callId = info->callId;
            sipuri = data->receiver;
            content = data->content;
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
    //     qDebug() << "dispatchOfflineBox";
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
        drafts.append (data);
        data->callId = info->callId;
        sipuri = data->receiver;
        content = data->content;
        toSendMsg = sendCatMsgPhoneData (info->fetionNumber, sipuri,
                                         info->callId, content, "", "");
    }
    mutex.unlock();
    while (not empty)
    {
        qDebug() << "==============================";
        qDebug() << "dispatchOfflineBox while (not empty)";
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
            drafts.append (data);
            data->callId = info->callId;
            sipuri = data->receiver;
            content = data->content;
            toSendMsg = sendCatMsgPhoneData (info->fetionNumber, sipuri,
                                             info->callId, content, "", "");
        }
        else
        {
            empty = true;
        }
        mutex.unlock();
        qDebug() << "==============================";
    }
}

void Account::clearDrafts()
{
    qDebug() << "clearDrafts" << QDateTime::currentDateTime();
    if (drafts.empty())
    {
        return;
    }
    mutex.lock();
    if (not drafts.empty()) // double check
    {
        int s = drafts.size();
        QList<int> deleted;
        for (int i = 0; i < s; ++i)
        {
            if (drafts.at (i)->datetime.toTime_t() + 15 <
                QDateTime::currentDateTime().toTime_t())
            {
                // TODO use notSentMessage
//                 emit notSentMessage (drafts.at (i)->receiver,
//                                      drafts.at (i)->datetime,
//                                      drafts.at (i)->content);
                emit incomeMessage (drafts.at (i)->receiver,
                                    drafts.at (i)->datetime.toString().toUtf8(),
                                    drafts.at (i)->content.append ("NOT SEND"));
                delete drafts.at (i);
                deleted << i;
            }
        }
        for (int i = 0; i < deleted.size(); ++i)
            drafts.removeAt (i);
    }
    mutex.unlock();
}


void Account::onPortraitDownloaded (const QByteArray &sipuri)
{
    emit portraitDownloaded (sipuri);
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
        int b = data.indexOf ("N: ");
        int e = data.indexOf ("\r\n", b);
        QByteArray event = data.mid (b + 3, e - b - 3);
        if (event == "system-message")
        {

        }
        else// if (event == "CatMsg")
        {
            onReceivedMessage (data);
        } // else if (event == "SMS2Fetion") // when send to my self
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
        int b = data.indexOf ("N: ");
        int e = data.indexOf ("\r\n", b);
        QByteArray event = data.mid (b + 3, e - b - 3);
        if (event == "TransferV4")
        {
            onInfoTransferV4 (data);
        }

        else
        {
            onInfo (data);
        }
    }
    else if (code == "O")
    {
        qDebug() << data;
    }
    else if (code == "SIP-C/4.0")
    {
        if (data.startsWith ("SIP-C/4.0 401 Unauthoried") and
            data.contains ("\r\nW: Digest") /*and step == SIPCR*/)
        {
            qDebug() << "parseSipcRegister";
            qDebug() << data;
            qDebug() << "=========";
            parseSipcRegister (data);
        }
        else if (data.startsWith ("SIP-C/4.0 200 OK") and
                 data.contains ("<client") /* and step == SIPCA*/)
        {
            parseSipcAuthorize (data);
        }
        else if (data.startsWith ("SIP-C/4.0 200 OK") and
                 data.contains ("A: CS address=")) // right after startChat
        {
            onStartChat (data);
        }
        else if (data.startsWith ("SIP-C/4.0 200 OK") and
                 data.contains ("XI:"))
        {
            onSendReplay (data);
        }
        else if (data.startsWith ("SIP-C/4.0 280 Send SMS OK"))
        {
            // TODO check quota-frequency
            onSendReplay (data);
        }
        else if (data.contains ("<results><presence><basic"))
        {
            //TODO wrap as a function
            int b = data.indexOf ("value=\"");
            int e = data.indexOf ("\"/",b);
            QByteArray v = data.mid (b + 7, e - b - 7);
            bool ok;
            int value = v.toInt (&ok);
            if (ok)
            {
                publicInfo->state = (StateType) value;
                emit contactChanged (info->sipuri);
            }
        }
        else
        {
            qDebug() << QString::fromUtf8 (data);
        }
    }
    else
    {
        qDebug() << QString::fromUtf8 (data);
    }
//     onReceiveData();
    // blocking?
}

void Account::onReceivedMessage (const QByteArray &data)
{
// TODO distinct private messages from groups messages
    qDebug() << QString::fromUtf8 (data);
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
    if (fromSipuri.startsWith ("tel:"))
    {
        fromSipuri.remove (0, 4);
    }
    emit incomeMessage (fromSipuri, datetime, content);
    QByteArray reply = "SIP-C/4.0 200 OK\r\n";
//     if (fromSipuri.contains ("PG"))
//     {
    reply.append ("I: ");
    reply.append (callId);
    reply.append ("\r\nQ: ");
    reply.append (sequence);
    reply.append ("\r\nF: ");
    reply.append (fromSipuri);
    reply.append ("\r\n\r\n");
//     }
//     else
//     {
//         reply.append ("F: ");
//         reply.append (fromSipuri);
//         reply.append ("\r\nI: ");
//         reply.append (callId);
//         reply.append ("\r\nQ: ");
//         reply.append (sequence);
//         reply.append ("\r\n\r\n");
//     }
    // ?fromsiprui , callid , sequence
    if (contacts.contains (fromSipuri))
    {
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
}

void Account::onBNPresenceV4 (const QByteArray &data)
{
    qDebug() << "onBNPresenceV4 >>>>";
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
            ContactInfo *contactInfo = 0;
            while (not domRoot.isNull())
            {
                contactInfo = 0;
                QByteArray userId = domRoot.attribute ("id").toUtf8();
                child = domRoot.firstChildElement ("p");
                if (not child.isNull() and child.hasAttribute ("su"))
                {
                    sipuri = child.attribute ("su").toUtf8();
                    if (contacts.contains (sipuri))
                    {
                        contactInfo = contacts.value (sipuri);
                    }
                    downloadPortrait (sipuri);
                    contactInfo->userId = userId;
                    contactInfo->mobileno = child.attribute ("m").toUtf8();
                    contactInfo->nickName = child.attribute ("n").toUtf8();
                    contactInfo->impresa = child.attribute ("i").toUtf8();
                    contactInfo->scoreLevel = child.attribute ("l").toUtf8();
                    // FIXME not to downloadPortrait if there is one up to date;
                    // p is the portraitCrc
                    //  contactInfo->imageChanged = child.attribute("p").toUtf8();
                    contactInfo->carrier = child.attribute ("c").toUtf8();
                    contactInfo->carrierStatus =
                        child.attribute ("cs").toUtf8();
                    contactInfo->serviceStatus =
                        child.attribute ("s").toUtf8();
                }
                else if (not contactInfo) // get contactif by userId
                {
                    foreach (ContactInfo *ci, contacts.values())
                    {
                        if (ci->userId == userId)
                        {
                            contactInfo = ci;
                            sipuri = contacts.key (ci);
                            break;
                        }
                    }
                }
                if (not contactInfo)
                {
                    goto next;
                }
                child = domRoot.firstChildElement ("pr");
                if (not child.isNull() and child.hasAttribute ("dt"))
                {
                    contactInfo->devicetype = child.attribute ("dt").toUtf8();
                }
                if (not child.isNull() and child.hasAttribute ("b"))
                {
                    if (contactInfo->state not_eq child.attribute ("b").toInt())
                    {
                        int state = child.attribute ("b").toInt();
                        contactInfo->state = (StateType) state;
                        emit contactStateChanged (sipuri, state);
                    }
                }
                else if (sipuri not_eq info->sipuri)
                {
                    contactInfo->state = StateType::OFFLINE;
                }
                if (not sipuri.isEmpty())
                {
                    emit contactChanged (sipuri);
                    if (contactInfo->state < StateType::HIDDEN)
                    {
                        conversationManager->removeConversation (sipuri);
                    }
                }
                // TODO inform conversationManager to remove one that is OFFLINE
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
    qDebug() << QString::fromUtf8 (data);
    // if UserLeft, get node member, if ok, get property uri, then remove
    // inform conversationManager to remove this sipuri (uri).

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
    QDomElement domChild;
    if (domRoot.tagName() not_eq "events")
    {
        qDebug() << "onBNConversation " << domRoot.tagName();
        return;
    }
    domRoot = domRoot.firstChildElement ("event");
    while (not domRoot.isNull() and domRoot.hasAttribute ("type"))
    {
        if (domRoot.attribute ("type") == "Support")
        {

        }
        else  if (domRoot.attribute ("type") == "UserEntered")
        {
            domChild = domRoot.firstChildElement ("member");
            if (not domChild.isNull() and domChild.hasAttribute ("uri"))
            {
                QByteArray uri = domChild.attribute ("uri").toUtf8();
                //TODO emit contactEnter(uri);
            }
        }
        else  if (domRoot.attribute ("type") == "UserLeft")
        {
            domChild = domRoot.firstChildElement ("member");
            if (not domChild.isNull() and domChild.hasAttribute ("uri"))
            {
                QByteArray uri = domChild.attribute ("uri").toUtf8();
                //TODO emit contactLeft(uri);
            }
        }
        else  if (domRoot.attribute ("type") == "UserFailed")
        {
            domChild = domRoot.firstChildElement ("member");
            if (not domChild.isNull() and domChild.hasAttribute ("uri"))
            {
                QByteArray uri = domChild.attribute ("uri").toUtf8();
                //TODO emit contactLeft(uri);
                //    conversationManager->removeConversation (uri);
            }
        }
        domRoot = domRoot.nextSiblingElement ("event");
    }
}

void Account::onBNSyncUserInfoV4 (const QByteArray &data)
{
    qDebug() << "onBNSyncUserInfoV4";
    qDebug() << QString::fromUtf8 (data);
    // TODO
    // get node "buddy", if ok, process the data
    // get properties action, (if action is add) user-id, uri, relation-status
    //
}

void Account::onBNcontact (const QByteArray &data)
{
    qDebug() << "onBNcontact";
    qDebug() << QString::fromUtf8 (data);
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
    qDebug() << QString::fromUtf8 (data);
}

void Account::onBNPGGroup (const QByteArray &data)
{
    qDebug() << "onBNPGGroup";
    qDebug() << QString::fromUtf8 (data);
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
    reply.append ("200002");
//     reply.append (sequence);
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
        qDebug() << "toInvite is empty ================";
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
    conversationManager->addConversation (sipuri);
    conversationManager->setHost (sipuri, ip, port);
    conversationManager->sendData (sipuri, toSendMsg);
    toSendMsg = inviteBuddyData (info->fetionNumber, info->callId, sipuri);
    conversationManager->sendData (sipuri, toSendMsg);
}

void Account::onSendReplay (const QByteArray &data)
{
    int b, e;
    b = data.indexOf ("I: ");
    e = data.indexOf ("\r\n", b);
    bool ok = false;
    int calledId = data.mid (b + 3, e - b - 3).toInt (&ok);
    if (not ok)
    {
        return;
    }
    mutex.lock();
    if (not drafts.empty())
    {
        int s = drafts.size();
        for (int i = 0; i < s; ++i)
        {
            // TODO add a timer that will check all letters in drafts,
            // if they are not removed within in some seconds, that is,
            // if currentDateTime is 20s greater than datetime of those letters
            if (drafts.at (i)->callId == calledId)
            {
                qDebug() << "onSendReplay" << i;
                delete drafts.at (i);
                drafts.removeAt (i);
                break;
            }
        }
    }
    mutex.unlock();
}

void Account::onInfo (const QByteArray &data)
{
    qDebug() << "onIncoming";
    qDebug() << QString::fromUtf8 (data);;
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

// FIXME don't know how to fix it
void Account::onInfoTransferV4 (const QByteArray &data)
{

    /**
     * IN other SIP-C/4.0
     * I: callId
     * Q: sequence IN
     * F: fromsipuri
     * N: TransferV4
     * L: length
     *
     * <args action="ask"><image method="direct" transfer-id="xxx" name="yyy"
     * size="zzz" /></args>
     *
     **/

    int b = data.indexOf ("F: ");
    int e = data.indexOf ("\r\n", b);
    QByteArray fromSipuri = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("I: ");
    e = data.indexOf ("\r\n", b);
    QByteArray callId = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("Q: ");
    e = data.indexOf ("\r\n", b);
    QByteArray sequence = data.mid (b + 3, e - b - 3);
    b = data.indexOf ("\r\n\r\n");
    QByteArray content = data.mid (b + 4);
//     QByteArray xml = content.mid (b + 4);
    QDomDocument domDoc;
    QString errorMsg;
    int errorLine, errorColumn;
    bool ok = domDoc.setContent
              (content, false, &errorMsg, &errorLine, &errorColumn);
    if (not ok)
    {
        qDebug() << "Failed onInfoTransferV4!";
        qDebug() << errorMsg << errorLine << errorColumn;
        qDebug() << content;
        return;
    }
    domDoc.normalize();
    QDomElement domRoot = domDoc.documentElement();
    domRoot = domRoot.firstChildElement ("args");
    if (domRoot.hasAttribute ("action"))
        qDebug() << domRoot.attribute ("action");
    domRoot = domRoot.firstChildElement ("image");
    QByteArray method = domRoot.attribute ("method").toUtf8();
    QByteArray transfer_id = domRoot.attribute ("transfer-id").toUtf8();
    QByteArray name = domRoot.attribute ("name").toUtf8();
    QByteArray size = domRoot.attribute ("size").toUtf8();
    //TODO do accept
    QByteArray body = content.replace ("ask", "accept");
    QByteArray reply = "IN " +  info->fetionNumber + " SIP-C/4.0\r\n";
    reply.append ("F: ");
    reply.append (fromSipuri);
    reply.append ("\r\nI: ");
    reply.append (callId);
    reply.append ("\r\nQ: ");
    reply.append (sequence);
    reply.append ("\r\nL: ");
    reply.append (QByteArray::number (body.size()));
    reply.append ("\r\n\r\n");
    reply.append (body);
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


void Account::onSipc (const QByteArray &data)
{
    qDebug() << "onSipc";
    qDebug() << QString::fromUtf8 (data);;
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
    qDebug() << QString::fromUtf8 (data);
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
        qDebug() << QString::fromUtf8 (data);;
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

