/*
    The definition of User
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


#ifndef BRESSEIN_USER_H
#define BRESSEIN_USER_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkProxy>
#include "types.h"

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
    User (QByteArray number, QByteArray password, QObject *parent = 0);
    virtual ~User();
    virtual bool operator== (const User &other);

public slots:

    void login();
    void loginVerify (QByteArray code);   //only called when verification required
    void close();
    //TODO may not be called from main thread,
    // use QMetaObject::invokeMethod to dispatch
    // move followings to private slots
signals:
    void needConfirm();
//private use
    void ssiResponseParsed();
    void serverConfigParsed();
    void sipcRegisterParsed();
    void sipcAuthorizeParsed();
// The following are executed in worker thread
private:
    /**
     * @brief try to load local configuration and initialize info
     *
     * @return void
     **/
    void initialize (QByteArray number, QByteArray password);
    void sipcWriteRead (QByteArray &in, QByteArray &out);

private slots:
// functions that performance networking
    void keepAlive();
    //called in period when connection established SIP_EVENT_KEEPALIVE
    void ssiLogin();
    void ssiPic();
    void ssiVerify();
    void sipcRegister();
    void systemConfig();
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
    QTimer *timer;
    QList<Contact *> contacts;
    QList<Group *> groups;
    // groups
    //pggroups
    QTcpSocket *sipcSocket;
    // All sip-c transactions are handle through this socket
    QNetworkProxy proxy;
    // TODO make use of proxy
};
}
#endif // USER_H
