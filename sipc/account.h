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
    enum STEP {NONE, SSI, SYSCONF, SIPCR, SIPCA, SIPCP};
public:
    explicit Account (QObject *parent = 0);
    virtual ~Account();
    virtual bool operator== (const Account &other);
    void setAccount (QByteArray number, QByteArray password);
    // returns the list of contacts, not including info
    const QList<QByteArray> &getContacts() const;
    //
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
    //private use
    void ssiResponseParsed();
    void serverConfigParsed();
    void sipcRegisterParsed();
    void sipcAuthorizeParsed();

public slots:
    void login();
    // This will be connect to a dialog and call there
    void loginVerify (const QByteArray &code);   //only called when verification required
    void close();

    // the sender can just call this, regardless of the state of receiver
    void sendMessage (const QByteArray &toSipuri, const QByteArray &message);
    const ContactInfo &getContactInfo (const QByteArray &sipuri);
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
    void onServerTransportError (const int);
    void queueMessages (const QByteArray &receiveData);
    void dequeueMessages();
    void dispatchOutbox();
    void dispatchOfflineBox();
    void onPortraitDownloaded (const QByteArray &);
    void keepAlive();
    void ssiLogin();
    void ssiPic();
    void ssiVerify();
    void systemConfig();
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

    // SIP_EVENT_CATMESSAGE
    // void sendMessageMyself();// SIP_EVENT_SENDCATMESSAGE
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
//         void parseSsiVerifyResponse (QByteArray &data);


    void onReceivedMessage (const QByteArray &data);
    //
    void onBNPresenceV4 (const QByteArray &data);
    void onBNConversation (const QByteArray &data);
    void onBNSyncUserInfoV4 (const QByteArray &data);
    void onBNPGGroup (const QByteArray &data);
    void onBNcontact (const QByteArray &data);
    void onBNregistration (const QByteArray &data);
    void onInvite (const QByteArray &data);
    void onInfo (const QByteArray &data);
    void onInfoTransferV4 (const QByteArray &data);
    void onSipc (const QByteArray &data);
    void onStartChat (const QByteArray &data);
    void onOption (const QByteArray &data);
    // some functions that helps above on's
    void parsePGGroupMembers (const QByteArray &data);


private:
    STEP step;
    ContactInfo *publicInfo; //
    // the PIMPL
    class  Info;
    typedef Info *InfoAccess;
    InfoAccess info;
    // the thread that provides event loop
    QThread workerThread;
    QMutex mutex;
    QTimer *keepAliveTimer;
    QTimer *messageTimer;
    Contacts contacts;
    QList<Group *> groups;
    // groups
    //pggroups
    // All sip-c transactions are handle through this socket
    QTcpSocket *sipcSocket;
    //TODO replaced with conversation manager
    ConversationManager *conversationManager;
    QList<QByteArray> inbox;
    // a letter
    // receiver date content
    struct Letter
    {
        QByteArray receiver; // a sipuri
        QDateTime datetime;
        QByteArray content;
    };
    QList<Letter *> outbox;
    QList<Letter *> offlineBox;
    // if one is to send, we move it to drafts first,
    // and then if it is sent successfully, then we delete it.
    QList<Letter *> drafts;
// FIXME if two are to invite.
    QByteArray toInvite;
    // TODO make use of proxy
    QNetworkProxy proxy;
    // TODO enhance to be a networking resource fetcher
    PortraitFetcher fetcher;
    Transporter *serverTransporter;

};
}
#endif // BRESSEIN_ACCOUNT_H
