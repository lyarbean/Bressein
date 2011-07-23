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
        User (QByteArray number, QByteArray password,QObject *parent = 0);
        virtual ~User();
        virtual bool operator== (const User& other);

    public slots:
        void setState (StateType&);
        void login();
        void close();
        void keepAlive();//called in period when connection established
        void sendMessage (QByteArray& toSipuri, QByteArray& message);
        void addBuddy (QByteArray& number, QByteArray buddyLists = "0", QByteArray localName = "", QByteArray desc ="",  QByteArray phraseId = "0");

        //number could either be fetionId or phone number.

    signals:
        void showStatus (QByteArray& message);// the code

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
        void sipcWriteRead(QByteArray& in, QByteArray& out);
    private slots:
// functions that performance networking
        void ssiLogin();
        void sipcRegister();
        void systemConfig();
        void sipcAuthorize();
        void ssiVerify();
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
        typedef UserInfo * Info;
        Info info;
        QList<contact *> contacts;
        // groups
        //pggroups
        QTcpSocket* sipcSocket; // All sip-c transactions are handle through this socket
    };
}
#endif // USER_H
