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

#ifndef BRESSEIN_ACCOUNT_H
#define BRESSEIN_ACCOUNT_H
#include "types.h"
#include "portraitfetcher.h"
#include "conversationmanager.h"
#include <QtCore/QObject>
#include <QtCore/QThread>
#include <QtNetwork/QTcpSocket>
#include <QtNetwork/QNetworkProxy>
#include <QDateTime>

namespace Bressein
{

/** \class User
 * @brief The User is an instance of a sip-c client.
 *
 **/
class Transporter;
class Account : public QObject
{
    Q_OBJECT
    //TODO remove these
//     enum STEP {NONE, SSI, SYSCONF, SIPCR, SIPCA, SIPCP};
public:
    explicit Account (QObject *parent = 0);
    virtual ~Account();
    virtual bool operator== (const Account &other);
    void setAccount (QByteArray number, QByteArray password);
    void getFetion (QByteArray &out) const;
    void getNickname (QByteArray &name) const;
signals:
    void logined();
    void wrongPassword();
    void needConfirm();
    void contactChanged (const QByteArray &);
    void portraitDownloaded (const QByteArray &);
    void verificationPic (const QByteArray &);
    // find a better name
    void groupChanged (const QByteArray &id, const QByteArray &name);
    void incomeMessage (const QByteArray &, //sender
                        const QByteArray &,//date
                        const QByteArray &);//content
    void notSentMessage (const QByteArray &,
                         const QDateTime &,
                         const QByteArray &);
    void contactStateChanged (const QByteArray & , int);
    //private use
    void ssiResponseParsed();
    void serverConfigParsed();
    void sipcRegisterParsed();
    void sipcAuthorizeParsed();

public slots:
    void login();
    // This will be connect to a dialog and call there
    void loginVerify (const QByteArray &code);   //only called when verification required

    // the sender can just call this, regardless of the state of receiver
    void sendMessage (const QByteArray &toSipuri, const QByteArray &message);
    void getContactInfo (const QByteArray &sipuri, ContactInfo &);
    //change states
    void setOnline ();
    void setRightback ();
    void setAway ();
    void setBusy ();
    void setOutforlunch ();
    void setOnthephone ();
    void setMeeting ();
    void setDonotdisturb ();
    void setHidden ();
    void setOffline();
private:
    void parseReceivedData (const QByteArray &in);
    void downloadPortrait (const QByteArray &sipuri);
private slots:
    void activateTimer();
    /**
     * @brief stop dispatching and set state for restore when network is broken.
     *
     * @return void
     **/
    void suspend();
    void onServerTransportError (const int);
    void onPortraitDownloaded (const QByteArray &);
    void queueMessages (const QByteArray &receiveData);
    void dequeueMessages();
    void dispatchOutbox();
    void dispatchOfflineBox();
    void clearDrafts();
    void keepAlive();
    void ssiLogin();
    void ssiPic();
    void systemConfig();
    // uploadPortrait(/*file*/);
    void sipcRegister();
    void sipcAuthorize();
    // sipc events
    //SIP_EVENT_CONVERSATION ?
    void getContactInfo (const QByteArray &userId);
    // To overload correctly, pass one more argument to distinct
    void getContactInfo (const QByteArray &Number, bool mobile);
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

    // SIP_EVENT_CATMESSAGE
    // merged to sendMessage
    // void sendMessageMyself();// SIP_EVENT_SENDCATMESSAGE
    // will merged to sendMessage with indicator
    // void sendMessagePhone SIP_EVENT_SENDCATMESSAGE

    void inviteFriend (const QByteArray &sipuri);
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
    void updateInfo(); //SIP_EQByteArrayVENT_SETUSERINFO
    void setImpresa (const QByteArray &impresa);
    void setMessageStatus (int days);   //SIP_EVENT_SETUSERINFO

    // directMessage SIP_EVENT_DIRECTSMS SIP_EVENT_SENDDIRECTCATSMS
    // void replayContactRequest SIP_EVENT_HANDLECONTACTREQUEST
    // PG
    // getPgMembers SIP_EVENT_PGGETGROUPMEMBERS
    // getPgList SIP_EVENT_PGGETGROUPLIST
    //void getPgInfo();//SIP_EVENT_PGGETGROUPINFO
    // subsribePg SIP_EVENT_PGPRESENCE
    // sendPgMessage SIP_EVENT_PGSENDCATSMS

    void setClientState (int state);   //SIP_EVENT_SETPRESENCE

// functions for parsing data
    void parseSsiResponse (QByteArray &data);
    void parseServerConfig (QByteArray &data);
    void parseSipcRegister (QByteArray &data);
    void parseSipcAuthorize (QByteArray &data);

    void onReceivedMessage (const QByteArray &data);
    void onBNPresenceV4 (const QByteArray &data);
    void onBNConversation (const QByteArray &data);
    void onBNSyncUserInfoV4 (const QByteArray &data);
    void onBNPGGroup (const QByteArray &data);
    void onBNcontact (const QByteArray &data);
    void onBNregistration (const QByteArray &data);
    void onInvite (const QByteArray &data);
    void onInfo (const QByteArray &data);
    void onInfoTransferV4 (const QByteArray &data);
    void onStartChat (const QByteArray &data);
    void onSendReplay (const QByteArray &data);
    void onOption (const QByteArray &data);
    // some functions that helps above on's
    void parsePGGroupMembers (const QByteArray &data);

private:
    ContactInfo *publicInfo; // inserted in contacts, never delete it directly
    // the PIMPL
    class  Info;
    typedef Info *InfoAccess;
    InfoAccess info;
    // the thread that provides event loop
    QThread workerThread;
    QMutex mutex;

    QTimer *keepAliveTimer;
    QTimer *messageTimer;
    QTimer *draftsClearTimer;

    Contacts contacts;
    Groups groups;
    // groups
    //pggroups

    QNetworkProxy proxy;
    PortraitFetcher fetcher;
    Transporter *serverTransporter;
    ConversationManager *conversationManager;

    QList<QByteArray> inbox;
    // a letter
    // receiver date content
    struct Letter
    {
        QByteArray receiver; // a sipuri
        QDateTime datetime;
        QByteArray content;
        int callId; // when to send, it get a callId
    };
    QList<Letter *> outbox;
    QList<Letter *> offlineBox;
    QList<Letter *> drafts; // being processed
    // FIXME if two are to invite.
    QByteArray toInvite;
    // TODO make use of proxy

    bool systemConfigFetched;
    bool connected;
    bool keepAliveAcked;

};
}
#endif // BRESSEIN_ACCOUNT_H
