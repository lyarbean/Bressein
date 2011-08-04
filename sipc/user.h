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

#ifndef BRESSEIN_USER_H
#define BRESSEIN_USER_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkProxy>
#include "types.h"
#include "portraitfetcher.h"
class QTimer;
namespace Bressein
{

/** \class User
 * @brief The User is an instance of a sip-c client.
 *
 **/

class User : public QObject
{
    Q_OBJECT

public:
    explicit User (QObject *parent = 0);
    virtual ~User();
    virtual bool operator== (const User &other);
    void setAccount (QByteArray number, QByteArray password);
    const Contacts &getContacts() const;
public slots:

    void login();
    void loginVerify (const QByteArray &code);   //only called when verification required
    void close();
    //TODO may not be called from main thread,
    // use QMetaObject::invokeMethod to dispatch
    // move followings to private slots
    void startChat (const QByteArray &sipUri);
signals:

    void needConfirm();
    void contactsChanged();
//private use
    void ssiResponseParsed();
    void serverConfigParsed();
    void sipcRegisterParsed();
    void sipcAuthorizeParsed();
// The following are executed in worker thread
private:
    void sipcWriteRead (QByteArray &in, QByteArray &out, QTcpSocket *socket);

private slots:
// functions that performance networking
    void keepAlive();
    void receive();
    //called in period when connection established SIP_EVENT_KEEPALIVE
    void ssiLogin();
    void ssiPic();
    void ssiVerify();
    void systemConfig();

    bool downloadPortrait(const QByteArray &sipuri);
    // uploadPortrait(/*file*/);
    void sipcRegister();
    void sipcAuthorize();

    // sipc events
    //SIP_EVENT_CONVERSATION ?
    void contactInfo (const QByteArray &userId);
    // To overload correctly, pass one more argument to distinct
    void contactInfo (const QByteArray &Number, bool mobile);

    //SIP_EVENT_GETCONTACTINFO or by number
    //pg_group_update_group_info
    //void groupInfo;// SIP_EVENT_GETCONTACTINFO

    void addBuddy (const QByteArray &number,
                   QByteArray buddyLists = "0",
                   QByteArray localName = "",
                   QByteArray desc = "",
                   QByteArray phraseId = "0");  //SIP_EVENT_ADDBUDDY
    void deleteBuddy (const QByteArray &userId);   //SIP_EVENT_DELETEBUDDY
    void contactSubscribe();// SIP_EVENT_PRESENCE
    void sendMessage (const QByteArray &toSipuri, const QByteArray &message);
    // SIP_EVENT_CATMESSAGE
    // void sendMessageMyself();// SIP_EVENT_SENDCATMESSAGE
    // void sendMessagePhone SIP_EVENT_SENDCATMESSAGE

    void inviteFriend (const QByteArray &sipUri);
    // SIP_EVENT_STARTCHAT SIP_EVENT_INVITEBUDDY
    void createBuddylist (const QByteArray &name);   // SIP_EVENT_CREATEBUDDYLIST
    void deleteBuddylist (const QByteArray &id);   //SIP_EVENT_DELETEBUDDYLIST
    void renameBuddylist (const QByteArray &id, const QByteArray &name);
    //SIP_EVENT_SETBUDDYLISTINFO
    // void permitPhonenumber SIP_EVENT_SETCONTACTINFO
    // void setDisplayname SIP_EVENT_SETCONTACTINFO
    // void moveGroup SIP_EVENT_SETCONTACTINFO
    // void joinGroup SIP_EVENT_SETCONTACTINFO
    // void leaveGroup SIP_EVENT_SETCONTACTINFO
    // void setMoodphrase SIP_EVENT_SETUSERINFO
    void updateInfo(); //SIP_EVENT_SETUSERINFO
    void setImpresa (const QByteArray &impresa);
    void setMessageStatus (int days);   //SIP_EVENT_SETUSERINFO

    // FIXME
    // directMessage SIP_EVENT_DIRECTSMS SIP_EVENT_SENDDIRECTCATSMS
    // void replayContactRequest SIP_EVENT_HANDLECONTACTREQUEST
    // PG
    // getPgMembers SIP_EVENT_PGGETGROUPMEMBERS
    // getPgList SIP_EVENT_PGGETGROUPLIST
    //void getPgInfo();//SIP_EVENT_PGGETGROUPINFO
    // subsribePg SIP_EVENT_PGPRESENCE
    // sendPgMessage SIP_EVENT_PGSENDCATSMS

    void setClientState (StateType &state);   //SIP_EVENT_SETPRESENCE

// functions for parsing data
    void parseSsiResponse (QByteArray &data);
    void parseServerConfig (QByteArray &data);
    void parseSipcRegister (QByteArray &data);
    void parseSipcAuthorize (QByteArray &data);
//         void parseSsiVerifyResponse (QByteArray &data);
    void activateTimer();
    //TODO inactivateTimer

private:

    // the pimpl
    class  UserInfo;
    typedef UserInfo *Info;
    Info info;
    QThread workerThread;
    QTimer *keepAliveTimer;
    QTimer *receiverTimer;
    Contacts contacts;
    QList<Group *> groups;
    // groups
    //pggroups
    QTcpSocket *sipcSocket;
    // All sip-c transactions are handle through this socket
    QNetworkProxy proxy;
    PortraitFetcher fetcher;
    // TODO make use of proxy
};
}
#endif // USER_H
