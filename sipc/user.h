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


#ifndef BRESSEIN_USER_H
#define BRESSEIN_USER_H

#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtNetwork/QTcpSocket>

#include "types.h"

class QTimer;

namespace Bressein
{
    // As SSi login is a short-connection, we don't run it in another thread
    // so for, to make the code work ASAP, we are not to provide robust multi threaded
    // a TCPsocket is required to perform sipc transactions
    // as well as a UDPsocket for file transferring.
    // we composite a Http proxy to handle some request.

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
        virtual bool operator== (const User& other);

    public slots:

        void login();
        void close();
        //TODO may not be called from main thread, use QMetaObject::invokeMethod to dispatch
        // move followings to private slots


    signals:
        void ssiResponseParsed();
        void serverConfigParsed();
        void sipcRegisterParsed();
        void sipcAuthorizeParsed();

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
        void keepAlive();//called in period when connection established SIP_EVENT_KEEPALIVE
        void ssiLogin();
        void sipcRegister();
        void systemConfig();
        void sipcAuthorize();
        void ssiVerify();
        // sipc events
        //SIP_EVENT_CONVERSATION ?
        void contactInfo (QByteArray &userId);//SIP_EVENT_GETCONTACTINFO or by number
        // void groupInfo SIP_EVENT_GETCONTACTINFO
        void sendMessage (QByteArray& toSipuri, QByteArray& message);
        void addBuddy (QByteArray& number, QByteArray buddyLists = "0", QByteArray localName = "",
                       QByteArray desc = "",  QByteArray phraseId = "0"); //SIP_EVENT_ADDBUDDY
        // void deleteBuddy() SIP_EVENT_DELETEBUDDY
        // void contactSubscribe(); SIP_EVENT_PRESENCE
        // void sendSms();SIP_EVENT_CATMESSAGE
        // void sendSmsMyself SIP_EVENT_SENDCATMESSAGE
        // void sendSmsPhone SIP_EVENT_SENDCATMESSAGE
        // void inviteFriend SIP_EVENT_STARTCHAT SIP_EVENT_INVITEBUDDY
        // void createBuddyists() SIP_EVENT_CREATEBUDDYLIST
        // void deleteBuddylist SIP_EVENT_DELETEBUDDYLIST
        // void permitPhonenumber SIP_EVENT_SETCONTACTINFO
        // void setDisplayname SIP_EVENT_SETCONTACTINFO
        // void moveGroup SIP_EVENT_SETCONTACTINFO
        // void joinGroup SIP_EVENT_SETCONTACTINFO
        // void leaveGroup SIP_EVENT_SETCONTACTINFO
        // void setMoodphrase SIP_EVENT_SETUSERINFO
        // void updateInfo SIP_EVENT_SETUSERINFO
        // void setSmsStatus SIP_EVENT_SETUSERINFO
        // void renameBuddylist SIP_EVENT_SETBUDDYLISTINFO
        // FIXME
        // directSms SIP_EVENT_DIRECTSMS SIP_EVENT_SENDDIRECTCATSMS
        // void replayContactRequest SIP_EVENT_HANDLECONTACTREQUEST
        // getPgList SIP_EVENT_PGGETGROUPLIST
        // getPgInfo SIP_EVENT_PGGETGROUPINFO
        // subsribePg SIP_EVENT_PGPRESENCE
        // sendPgSms SIP_EVENT_PGSENDCATSMS

        void setState (StateType&);//SIP_EVENT_SETPRESENCE

// functions for parsing data
        void parseSsiResponse (QByteArray &data);
        void parseServerConfig (QByteArray &data);
        void parseSipcRegister (QByteArray &data);
        void parseSipcAuthorize (QByteArray &data);
//         void parseSsiVerifyResponse (QByteArray &data);
        void activateTimer();
        //TODO inactivateTimer

    private:
        QThread workerThread;
        QTimer* timer;

        class  UserInfo;
        typedef UserInfo *Info;
        Info info;
        QList<contact *> contacts;
        QList<group> groups;
        // groups
        //pggroups
        QTcpSocket* sipcSocket; // All sip-c transactions are handle through this socket
    };
}
#endif // USER_H
